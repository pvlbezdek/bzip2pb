#include "decompress.h"
#include "thread_pool.h"
#include <bzlib.h>
#include <cstring>
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

static std::vector<uint8_t> decompress_block(std::vector<uint8_t> compressed,
                                              uint32_t max_out) {
    std::vector<uint8_t> output(max_out);
    unsigned int out_size = max_out;

    int ret = BZ2_bzBuffToBuffDecompress(
        reinterpret_cast<char*>(output.data()), &out_size,
        reinterpret_cast<char*>(compressed.data()),
        static_cast<unsigned int>(compressed.size()),
        0, 0);

    if (ret != BZ_OK)
        throw std::runtime_error("BZ2_bzBuffToBuffDecompress error: " + std::to_string(ret));

    output.resize(out_size);
    return output;
}

static uint32_t read_le32(std::istream& is) {
    uint8_t buf[4];
    is.read(reinterpret_cast<char*>(buf), 4);
    if (is.gcount() != 4)
        throw std::runtime_error("Unexpected end of file");
    return static_cast<uint32_t>(buf[0])
         | (static_cast<uint32_t>(buf[1]) << 8)
         | (static_cast<uint32_t>(buf[2]) << 16)
         | (static_cast<uint32_t>(buf[3]) << 24);
}

static void do_decompress(std::istream& in, std::ostream& out, const Options& opts) {
    // Verify magic
    char magic[4];
    in.read(magic, 4);
    if (in.gcount() != 4 || std::memcmp(magic, BZIP2PB_MAGIC.data(), 4) != 0)
        throw std::runtime_error(
            "Not a bzip2pb file — use standard bzip2 for regular .bz2 files");

    uint8_t version;
    in.read(reinterpret_cast<char*>(&version), 1);
    if (version != BZIP2PB_VERSION)
        throw std::runtime_error("Unsupported bzip2pb version: " +
                                 std::to_string(version));

    const uint32_t block_size  = read_le32(in);
    const uint32_t num_blocks  = read_le32(in);
    const unsigned num_threads =
        opts.num_threads > 0 ? opts.num_threads : std::thread::hardware_concurrency();
    const unsigned threads = num_threads > 0 ? num_threads : 1u;
    const size_t   window  = threads * 2;

    ThreadPool pool(threads);
    std::deque<std::future<std::vector<uint8_t>>> futures;

    auto drain = [&](size_t limit) {
        while (futures.size() > limit) {
            auto data = futures.front().get();
            futures.pop_front();
            out.write(reinterpret_cast<const char*>(data.data()),
                      static_cast<std::streamsize>(data.size()));
        }
    };

    for (uint32_t i = 0; i < num_blocks; ++i) {
        uint32_t csize = read_le32(in);
        std::vector<uint8_t> compressed(csize);
        in.read(reinterpret_cast<char*>(compressed.data()), csize);
        if (static_cast<uint32_t>(in.gcount()) != csize)
            throw std::runtime_error("Truncated block " + std::to_string(i));
        futures.push_back(
            pool.submit(decompress_block, std::move(compressed), block_size));
        drain(window);
    }
    drain(0);

    if (opts.verbose)
        std::cerr << "Decompressed " << num_blocks << " block(s) with "
                  << threads << " thread(s)\n";
}

void decompress_file(const std::string& input_path,
                     const std::string& output_path,
                     const Options& opts) {
    std::ifstream in(input_path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open: " + input_path);
    std::ofstream out(output_path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot create: " + output_path);
    do_decompress(in, out, opts);
}

void decompress_stdin(const Options& opts) {
#ifdef _WIN32
    _setmode(_fileno(stdin),  _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    do_decompress(std::cin, std::cout, opts);
}
