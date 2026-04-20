#include <catch2/catch_test_macros.hpp>
#include "compress.h"
#include "decompress.h"
#include <bzlib.h>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
#include "../testdata.h"

// Compress using libbzip2 directly (single stream, no threading).
static std::vector<uint8_t> bz2_compress_buf(const std::vector<uint8_t>& input,
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

// Run decompress_stream on 'compressed' and return decompressed bytes.
static std::vector<uint8_t> decompress_via_stream(
        const std::vector<uint8_t>& compressed) {
    std::istringstream in(std::string(
        reinterpret_cast<const char*>(compressed.data()), compressed.size()));
    std::ostringstream out;
    Options opts;
    decompress_stream(in, out, opts);
    const std::string s = out.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}

// Run compress_stream + decompress_stream end-to-end.
static std::vector<uint8_t> roundtrip(const std::vector<uint8_t>& input,
                                       const Options& opts = Options{}) {
    std::istringstream in_c(std::string(
        reinterpret_cast<const char*>(input.data()), input.size()));
    std::ostringstream out_c;
    compress_stream(in_c, out_c, opts);
    const std::string cs = out_c.str();

    std::istringstream in_d(cs);
    std::ostringstream out_d;
    decompress_stream(in_d, out_d, opts);
    const std::string ds = out_d.str();
    return std::vector<uint8_t>(ds.begin(), ds.end());
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("decompress_stream inverts compress_stream") {
    const auto input = TestData::compressible(8192);
    CHECK(roundtrip(input) == input);
}

TEST_CASE("decompress_stream handles libbzip2-compressed input") {
    const auto input = TestData::text_like(4096);
    const auto compressed = bz2_compress_buf(input);
    const auto recovered = decompress_via_stream(compressed);
    CHECK(recovered == input);
}

TEST_CASE("decompress_stream levels 1 through 9") {
    const auto input = TestData::random_binary(4096, 99u);
    for (int level = 1; level <= 9; ++level) {
        Options opts;
        opts.block_level = level;
        CHECK(roundtrip(input, opts) == input);
    }
}

TEST_CASE("decompress_stream with 1 thread") {
    const auto input = TestData::compressible(32768);
    Options opts;
    opts.num_threads = 1;
    CHECK(roundtrip(input, opts) == input);
}

TEST_CASE("decompress_stream rejects invalid header") {
    const std::vector<uint8_t> garbage = {0x00, 0x01, 0x02, 0x03, 0x04};
    std::istringstream in(std::string(
        reinterpret_cast<const char*>(garbage.data()), garbage.size()));
    std::ostringstream out;
    Options opts;
    CHECK_THROWS_AS(decompress_stream(in, out, opts), std::runtime_error);
}

TEST_CASE("decompress_stream rejects truncated stream") {
    const auto input = TestData::compressible(4096);
    auto compressed = bz2_compress_buf(input);
    compressed.resize(compressed.size() / 2);  // truncate

    std::istringstream in(std::string(
        reinterpret_cast<const char*>(compressed.data()), compressed.size()));
    std::ostringstream out;
    Options opts;
    CHECK_THROWS(decompress_stream(in, out, opts));
}
