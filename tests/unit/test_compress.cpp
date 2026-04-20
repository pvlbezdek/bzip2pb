#include <catch2/catch_test_macros.hpp>
#include "compress.h"
#include <bzlib.h>
#include <cstdint>
#include <sstream>
#include <vector>
#include "../testdata.h"

// Decompress a raw bzip2 buffer with libbzip2; returns decompressed bytes.
static std::vector<uint8_t> bz2_decompress_buf(const std::vector<uint8_t>& src,
                                                 size_t max_out) {
    unsigned int out_size = static_cast<unsigned int>(max_out);
    std::vector<uint8_t> out(out_size);
    const int ret = BZ2_bzBuffToBuffDecompress(
        reinterpret_cast<char*>(out.data()), &out_size,
        const_cast<char*>(reinterpret_cast<const char*>(src.data())),
        static_cast<unsigned int>(src.size()),
        0, 0);
    REQUIRE(ret == BZ_OK);
    out.resize(out_size);
    return out;
}

// Run compress_stream on 'input' and return the compressed bytes.
static std::vector<uint8_t> compress_via_stream(const std::vector<uint8_t>& input,
                                                  const Options& opts) {
    std::istringstream in(std::string(
        reinterpret_cast<const char*>(input.data()), input.size()));
    std::ostringstream out;
    compress_stream(in, out, opts);
    const std::string s = out.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("compress_stream produces a valid bzip2 stream") {
    const auto input = TestData::compressible(4096);
    Options opts;
    const auto compressed = compress_via_stream(input, opts);

    REQUIRE_FALSE(compressed.empty());
    // Must begin with bzip2 stream header
    REQUIRE(compressed.size() >= 4);
    CHECK(compressed[0] == 'B');
    CHECK(compressed[1] == 'Z');
    CHECK(compressed[2] == 'h');
    CHECK(compressed[3] >= '1');
    CHECK(compressed[3] <= '9');

    // libbzip2 must be able to decompress it back
    const auto recovered =
        bz2_decompress_buf(compressed, input.size() + 1024);
    CHECK(recovered == input);
}

TEST_CASE("compress_stream levels 1 through 9 all produce valid output") {
    const auto input = TestData::text_like(8192);
    for (int level = 1; level <= 9; ++level) {
        Options opts;
        opts.block_level = level;
        const auto compressed = compress_via_stream(input, opts);

        REQUIRE_FALSE(compressed.empty());
        CHECK(compressed[3] == static_cast<uint8_t>('0' + level));

        const auto recovered =
            bz2_decompress_buf(compressed, input.size() + 1024);
        CHECK(recovered == input);
    }
}

TEST_CASE("compress_stream with single thread produces valid output") {
    const auto input = TestData::random_binary(16384);
    Options opts;
    opts.num_threads = 1;
    const auto compressed = compress_via_stream(input, opts);

    const auto recovered =
        bz2_decompress_buf(compressed, input.size() + 1024);
    CHECK(recovered == input);
}

TEST_CASE("compress_stream with 4 threads produces valid output") {
    const auto input = TestData::compressible(65536);
    Options opts;
    opts.num_threads = 4;
    const auto compressed = compress_via_stream(input, opts);

    const auto recovered =
        bz2_decompress_buf(compressed, input.size() + 1024);
    CHECK(recovered == input);
}

TEST_CASE("compress_stream empty input produces valid empty bzip2 stream") {
    const std::vector<uint8_t> input;
    Options opts;
    const auto compressed = compress_via_stream(input, opts);
    // Empty input must produce a non-empty valid bzip2 stream (header + EOS),
    // not zero bytes, so that decompress can round-trip it correctly.
    REQUIRE_FALSE(compressed.empty());
    const auto recovered = bz2_decompress_buf(compressed, 1);
    CHECK(recovered.empty());
}

TEST_CASE("compress_stream single byte input") {
    const std::vector<uint8_t> input = {0x42};
    Options opts;
    const auto compressed = compress_via_stream(input, opts);

    REQUIRE_FALSE(compressed.empty());
    const auto recovered =
        bz2_decompress_buf(compressed, 64);
    CHECK(recovered == input);
}
