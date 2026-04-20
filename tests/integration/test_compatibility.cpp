#include <catch2/catch_test_macros.hpp>
#include "compress.h"
#include "decompress.h"
#include <bzlib.h>
#include <cstdint>
#include <sstream>
#include <vector>
#include "../testdata.h"

// Compress with bzip2pb (compress_stream), decompress with raw libbzip2.
static std::vector<uint8_t> bzip2pb_compress(const std::vector<uint8_t>& input,
                                              const Options& opts = Options{}) {
    std::istringstream in(std::string(
        reinterpret_cast<const char*>(input.data()), input.size()));
    std::ostringstream out;
    compress_stream(in, out, opts);
    const std::string s = out.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}

// Compress with raw libbzip2, decompress with bzip2pb (decompress_stream).
static std::vector<uint8_t> libbzip2_compress(const std::vector<uint8_t>& input,
                                               int level = 9) {
    unsigned int out_size =
        static_cast<unsigned int>(input.size() * 1.02 + 600);
    std::vector<uint8_t> output(out_size);
    const int ret = BZ2_bzBuffToBuffCompress(
        reinterpret_cast<char*>(output.data()), &out_size,
        const_cast<char*>(reinterpret_cast<const char*>(input.data())),
        static_cast<unsigned int>(input.size()),
        level, 0, 30);
    REQUIRE(ret == BZ_OK);
    output.resize(out_size);
    return output;
}

static std::vector<uint8_t> libbzip2_decompress(const std::vector<uint8_t>& src,
                                                  size_t max_out) {
    unsigned int out_size = static_cast<unsigned int>(max_out);
    std::vector<uint8_t> output(out_size);
    const int ret = BZ2_bzBuffToBuffDecompress(
        reinterpret_cast<char*>(output.data()), &out_size,
        const_cast<char*>(reinterpret_cast<const char*>(src.data())),
        static_cast<unsigned int>(src.size()),
        0, 0);
    REQUIRE(ret == BZ_OK);
    output.resize(out_size);
    return output;
}

static std::vector<uint8_t> bzip2pb_decompress(const std::vector<uint8_t>& compressed) {
    std::istringstream in(std::string(
        reinterpret_cast<const char*>(compressed.data()), compressed.size()));
    std::ostringstream out;
    Options opts;
    decompress_stream(in, out, opts);
    const std::string s = out.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("bzip2pb output is decompressible by libbzip2") {
    const auto input = TestData::text_like(16 * 1024);
    const auto compressed = bzip2pb_compress(input);

    const auto recovered = libbzip2_decompress(compressed, input.size() + 1024);
    CHECK(recovered == input);
}

TEST_CASE("libbzip2 output is decompressible by bzip2pb") {
    const auto input = TestData::compressible(16 * 1024);
    const auto compressed = libbzip2_compress(input);

    const auto recovered = bzip2pb_decompress(compressed);
    CHECK(recovered == input);
}

TEST_CASE("multi-stream file (two concatenated libbzip2 streams) decompresses correctly") {
    const auto chunk1 = TestData::text_like(8 * 1024);
    const auto chunk2 = TestData::compressible(8 * 1024);
    const auto c1 = libbzip2_compress(chunk1);
    const auto c2 = libbzip2_compress(chunk2);

    std::vector<uint8_t> concat;
    concat.insert(concat.end(), c1.begin(), c1.end());
    concat.insert(concat.end(), c2.begin(), c2.end());

    const auto recovered = bzip2pb_decompress(concat);

    std::vector<uint8_t> expected;
    expected.insert(expected.end(), chunk1.begin(), chunk1.end());
    expected.insert(expected.end(), chunk2.begin(), chunk2.end());

    CHECK(recovered == expected);
}

TEST_CASE("bzip2pb output at each level 1-9 is decompressible by libbzip2") {
    const auto input = TestData::random_binary(8 * 1024, 7u);
    for (int level = 1; level <= 9; ++level) {
        Options opts;
        opts.block_level = level;
        const auto compressed = bzip2pb_compress(input, opts);
        const auto recovered = libbzip2_decompress(compressed, input.size() + 1024);
        CHECK(recovered == input);
    }
}
