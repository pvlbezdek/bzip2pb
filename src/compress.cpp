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

    int ret = BZ2_bzBuffToBuffCompress(
        reinterpret_cast<char*>(output.data()), &out_size,
        reinterpret_cast<char*>(input.data()),
        static_cast<unsigned int>(input.size()),
        level, 0, 30);

    if (ret != BZ_OK)
        throw std::runtime_error("BZ2_bzBuffToBuffCompress error: " + std::to_string(ret));

    output.resize(out_size);
    return output;
}

static void write_le32(std::ostream& os, uint32_t v) {
    uint8_t buf[4] = {
        static_cast<uint8_t>(v),
        static_cast<uint8_t>(v >> 8),
        static_cast<uint8_t>(v >> 16),
        static_cast<uint8_t>(v >> 24)};
    os.write(reinterpret_cast<char*>(buf), 4);
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
        auto n = static_cast<size_t>(in.gcount());
        if (n == 0) break;
        std::vector<uint8_t> chunk(buf.data(), buf.data() + n);
        futures.push_back(pool.submit(compress_block, std::move(chunk), opts.block_level));
        drain(window);
    }
    drain(0);

    const uint32_t num_blocks = static_cast<uint32_t>(compressed_blocks.size());

    // Write header
    out.write(BZIP2PB_MAGIC.data(), 4);
    uint8_t ver = BZIP2PB_VERSION;
    out.write(reinterpret_cast<char*>(&ver), 1);
    write_le32(out, block_size);
    write_le32(out, num_blocks);

    // Write size-prefixed compressed blocks
    for (auto& blk : compressed_blocks) {
        write_le32(out, static_cast<uint32_t>(blk.size()));
        out.write(reinterpret_cast<const char*>(blk.data()),
                  static_cast<std::streamsize>(blk.size()));
    }

    if (opts.verbose)
        std::cerr << "Compressed " << num_blocks << " block(s) with "
                  << threads << " thread(s), block size " << block_size << " B\n";
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

void compress_stdin(const Options& opts) {
#ifdef _WIN32
    _setmode(_fileno(stdin),  _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    do_compress(std::cin, std::cout, opts);
}
