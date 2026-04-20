#include "compress.h"
#include "thread_pool.h"
#include <bzlib.h>
#include <deque>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static std::vector<uint8_t> compress_block(std::vector<uint8_t> input, int level) {
    if (input.empty()) return {};
    unsigned int out_size = static_cast<unsigned int>(input.size() * 1.02 + 600);
    std::vector<uint8_t> output(out_size);

    const int ret = BZ2_bzBuffToBuffCompress(
        reinterpret_cast<char*>(output.data()), &out_size,
        reinterpret_cast<char*>(input.data()),
        static_cast<unsigned int>(input.size()),
        level, 0, 30);

    if (ret != BZ_OK)
        throw std::runtime_error("BZ2_bzBuffToBuffCompress error: " + std::to_string(ret));

    output.resize(out_size);
    return output;
}

static void do_compress(std::istream& in, std::ostream& out, const Options& opts) {
    const uint32_t block_size =
        static_cast<uint32_t>(opts.block_level) * 100u * 1024u;
    const unsigned num_threads =
        opts.num_threads > 0 ? opts.num_threads : std::thread::hardware_concurrency();
    const unsigned threads = num_threads > 0 ? num_threads : 1u;
    const size_t   window  = threads * 2;

    ThreadPool pool(threads);
    std::deque<std::future<std::vector<uint8_t>>> futures;
    std::vector<std::vector<uint8_t>> compressed_blocks;

    auto drain = [&](size_t limit) {
        while (futures.size() > limit) {
            compressed_blocks.push_back(futures.front().get());
            futures.pop_front();
        }
    };

    std::vector<uint8_t> buf(block_size);
    while (true) {
        in.read(reinterpret_cast<char*>(buf.data()), block_size);
        const auto n = static_cast<size_t>(in.gcount());
        if (n == 0) break;
        std::vector<uint8_t> chunk(buf.data(), buf.data() + n);
        futures.push_back(pool.submit(compress_block, std::move(chunk), opts.block_level));
        drain(window);
    }
    drain(0);

    // Each compressed_block is already a complete bzip2 stream produced by
    // BZ2_bzBuffToBuffCompress.  Concatenating them yields a valid multi-stream
    // .bz2 file per the bzip2 specification.
    for (const auto& blk : compressed_blocks)
        out.write(reinterpret_cast<const char*>(blk.data()),
                  static_cast<std::streamsize>(blk.size()));

    (void)opts; // verbose output handled by caller
}

void compress_file(const std::string& input_path,
                   const std::string& output_path,
                   const Options& opts) {
    std::ifstream in(input_path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open: " + input_path);
    std::ofstream out(output_path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot create: " + output_path);
    do_compress(in, out, opts);
}

void compress_file_stdout(const std::string& input_path, const Options& opts) {
    std::ifstream in(input_path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open: " + input_path);
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    do_compress(in, std::cout, opts);
}

void compress_stdin(const Options& opts) {
#ifdef _WIN32
    _setmode(_fileno(stdin),  _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    do_compress(std::cin, std::cout, opts);
}
