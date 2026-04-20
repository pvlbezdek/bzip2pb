#include <catch2/catch_test_macros.hpp>
#include "compress.h"
#include "decompress.h"
#include <cstdint>
#include <sstream>
#include <vector>
#include "../testdata.h"

// Compress then decompress; return the recovered bytes.
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

TEST_CASE("roundtrip: empty input") {
    const std::vector<uint8_t> input;
    CHECK(roundtrip(input) == input);
}

TEST_CASE("roundtrip: 1 byte") {
    const std::vector<uint8_t> input = {0xAB};
    CHECK(roundtrip(input) == input);
}

TEST_CASE("roundtrip: 1 KB random binary") {
    const auto input = TestData::random_binary(1024);
    CHECK(roundtrip(input) == input);
}

TEST_CASE("roundtrip: 1 MB random binary") {
    const auto input = TestData::random_binary(1024 * 1024);
    CHECK(roundtrip(input) == input);
}

TEST_CASE("roundtrip: compressible data") {
    const auto input = TestData::compressible(512 * 1024);
    CHECK(roundtrip(input) == input);
}

TEST_CASE("roundtrip: incompressible data") {
    const auto input = TestData::incompressible(64 * 1024);
    CHECK(roundtrip(input) == input);
}

TEST_CASE("roundtrip: text-like data") {
    const auto input = TestData::text_like(256 * 1024);
    CHECK(roundtrip(input) == input);
}

TEST_CASE("roundtrip: level 1 (smallest block)") {
    Options opts;
    opts.block_level = 1;
    const auto input = TestData::compressible(200 * 1024);
    CHECK(roundtrip(input, opts) == input);
}

TEST_CASE("roundtrip: level 5") {
    Options opts;
    opts.block_level = 5;
    const auto input = TestData::text_like(200 * 1024);
    CHECK(roundtrip(input, opts) == input);
}

TEST_CASE("roundtrip: level 9 (largest block)") {
    Options opts;
    opts.block_level = 9;
    const auto input = TestData::random_binary(200 * 1024, 77u);
    CHECK(roundtrip(input, opts) == input);
}

TEST_CASE("roundtrip: 1 thread") {
    Options opts;
    opts.num_threads = 1;
    const auto input = TestData::compressible(128 * 1024);
    CHECK(roundtrip(input, opts) == input);
}

TEST_CASE("roundtrip: 4 threads") {
    Options opts;
    opts.num_threads = 4;
    const auto input = TestData::random_binary(512 * 1024, 55u);
    CHECK(roundtrip(input, opts) == input);
}

TEST_CASE("roundtrip: 2 bytes") {
    const std::vector<uint8_t> input = {0x00, 0xFF};
    CHECK(roundtrip(input) == input);
}
