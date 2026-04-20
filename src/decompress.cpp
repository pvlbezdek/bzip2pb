#include "decompress.h"
#include "bitscanner.h"
#include "bitstream.h"
#include "thread_pool.h"
#include <bzlib.h>
#include <deque>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <vector>

// Discards all written bytes — used for -t test mode.
class NullSink : public std::streambuf {
protected:
    std::streamsize xsputn(const char*, std::streamsize n) override { return n; }
    int_type overflow(int_type c) override { return c; }
};
class NullOStream : public std::ostream {
    NullSink sink_;
public:
    NullOStream() : std::ostream(&sink_) {}
};

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

void decompress_stream(std::istream& in, std::ostream& out, const Options& opts) {
    // Load entire compressed file into memory (required for bit-level random access).
    // Memory usage: equal to the compressed file size — same model as lbzip2.
    const std::vector<uint8_t> compressed(
        std::istreambuf_iterator<char>(in), {});

    // Validate bzip2 stream header
    if (compressed.size() < 4 ||
        compressed[0] != 'B' || compressed[1] != 'Z' || compressed[2] != 'h' ||
        compressed[3] < '1'  || compressed[3] > '9')
        throw std::runtime_error("Not a valid bzip2 file");

    const char digit = static_cast<char>(compressed[3]);

    // Locate all block and EOS magic patterns via 64-bit word sliding scan
    const auto boundaries = scan_bzip2_blocks(compressed.data(), compressed.size());
    if (boundaries.empty())
        throw std::runtime_error("No bzip2 blocks found in file");

    const unsigned num_threads =
        opts.num_threads > 0 ? opts.num_threads : std::thread::hardware_concurrency();
    const unsigned threads = num_threads > 0 ? num_threads : 1u;
    const size_t   window  = threads * 2;
    const size_t   file_bits = compressed.size() * 8;

    ThreadPool pool(threads);
    std::deque<std::future<std::vector<uint8_t>>> futures;
    size_t num_blocks = 0;

    auto drain = [&](size_t limit) {
        while (futures.size() > limit) {
            const auto data = futures.front().get();
            futures.pop_front();
            out.write(reinterpret_cast<const char*>(data.data()),
                      static_cast<std::streamsize>(data.size()));
        }
    };

    for (size_t i = 0; i < boundaries.size(); ++i) {
        if (boundaries[i].is_eos) continue;

        // Next boundary (block or EOS) defines where this block's bits end.
        const size_t next_bit = (i + 1 < boundaries.size())
                                ? boundaries[i + 1].bit_offset
                                : file_bits;

        // Reconstruct a minimal self-contained bzip2 stream for this one block.
        auto sub = reconstruct_substream(
            compressed.data(), compressed.size(),
            boundaries[i].bit_offset, next_bit, digit);

        // Decompress the sub-stream on a pool thread.
        futures.push_back(pool.submit(
            [sub = std::move(sub)]() mutable -> std::vector<uint8_t> {
                // Max uncompressed size per bzip2 block is 900 KB (block level 9).
                unsigned int out_size = 900u * 1024u + 1024u;
                std::vector<uint8_t> output(out_size);
                const int ret = BZ2_bzBuffToBuffDecompress(
                    reinterpret_cast<char*>(output.data()), &out_size,
                    reinterpret_cast<char*>(sub.data()),
                    static_cast<unsigned int>(sub.size()),
                    0, 0);
                if (ret != BZ_OK)
                    throw std::runtime_error(
                        "BZ2_bzBuffToBuffDecompress error: " + std::to_string(ret));
                output.resize(out_size);
                return output;
            }));
        ++num_blocks;
        drain(window);
    }
    drain(0);

    (void)num_blocks; // verbose output handled by caller
}

void decompress_file(const std::string& input_path,
                     const std::string& output_path,
                     const Options& opts) {
    std::ifstream in(input_path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open: " + input_path);
    std::ofstream out(output_path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot create: " + output_path);
    decompress_stream(in, out, opts);
}

void decompress_file_stdout(const std::string& input_path, const Options& opts) {
    std::ifstream in(input_path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open: " + input_path);
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    decompress_stream(in, std::cout, opts);
}

void decompress_file_test(const std::string& input_path, const Options& opts) {
    std::ifstream in(input_path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open: " + input_path);
    NullOStream null_out;
    decompress_stream(in, null_out, opts);
}

void decompress_stdin(const Options& opts) {
#ifdef _WIN32
    _setmode(_fileno(stdin),  _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    decompress_stream(std::cin, std::cout, opts);
}

void decompress_stdin_test(const Options& opts) {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    NullOStream null_out;
    decompress_stream(std::cin, null_out, opts);
}
