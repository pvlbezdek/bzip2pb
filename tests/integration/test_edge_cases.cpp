#include <catch2/catch_test_macros.hpp>
#include "compress.h"
#include "decompress.h"
#include <bzlib.h>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>
#include "../testdata.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::vector<uint8_t> compress_to_vec(const std::vector<uint8_t>& input,
                                             const Options& opts = Options{}) {
    std::istringstream in(std::string(
        reinterpret_cast<const char*>(input.data()), input.size()));
    std::ostringstream out;
    compress_stream(in, out, opts);
    const std::string s = out.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}

static std::vector<uint8_t> decompress_from_vec(
        const std::vector<uint8_t>& compressed,
        const Options& opts = Options{}) {
    std::istringstream in(std::string(
        reinterpret_cast<const char*>(compressed.data()), compressed.size()));
    std::ostringstream out;
    decompress_stream(in, out, opts);
    const std::string s = out.str();
    return std::vector<uint8_t>(s.begin(), s.end());
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("decompress corrupt data throws std::runtime_error") {
    std::vector<uint8_t> corrupt = {'B', 'Z', 'h', '9',
        0x31, 0x41, 0x59, 0x26, 0x53, 0x59,  // block magic
        0xFF, 0xFF, 0xFF, 0xFF,               // garbage CRC / data
        0x00, 0x00, 0x00, 0x00
    };
    // Pad to something that passes the header check but has no valid block data
    corrupt.resize(64, 0x00);

    std::istringstream in(std::string(
        reinterpret_cast<const char*>(corrupt.data()), corrupt.size()));
    std::ostringstream out;
    Options opts;
    CHECK_THROWS(decompress_stream(in, out, opts));
}

TEST_CASE("decompress truncated file throws") {
    const auto input = TestData::compressible(4096);
    auto compressed = compress_to_vec(input);
    compressed.resize(compressed.size() / 3);

    std::istringstream in(std::string(
        reinterpret_cast<const char*>(compressed.data()), compressed.size()));
    std::ostringstream out;
    Options opts;
    CHECK_THROWS(decompress_stream(in, out, opts));
}

TEST_CASE("compress then decompress preserves exact byte sequence") {
    const auto input = TestData::random_binary(32768, 999u);
    const auto recovered = decompress_from_vec(compress_to_vec(input));
    CHECK(recovered == input);
}

TEST_CASE("decompress empty input throws (not a valid bzip2 file)") {
    const std::vector<uint8_t> empty;
    std::istringstream in(std::string(
        reinterpret_cast<const char*>(empty.data()), empty.size()));
    std::ostringstream out;
    Options opts;
    CHECK_THROWS_AS(decompress_stream(in, out, opts), std::runtime_error);
}

TEST_CASE("decompress bad magic header throws") {
    const std::vector<uint8_t> bad_magic = {'X', 'Z', 'h', '9', 0, 0, 0, 0};
    std::istringstream in(std::string(
        reinterpret_cast<const char*>(bad_magic.data()), bad_magic.size()));
    std::ostringstream out;
    Options opts;
    CHECK_THROWS_AS(decompress_stream(in, out, opts), std::runtime_error);
}

TEST_CASE("null bytes round-trip correctly") {
    const std::vector<uint8_t> input(4096, 0x00);
    CHECK(decompress_from_vec(compress_to_vec(input)) == input);
}

TEST_CASE("all-0xFF bytes round-trip correctly") {
    const std::vector<uint8_t> input(4096, 0xFF);
    CHECK(decompress_from_vec(compress_to_vec(input)) == input);
}

TEST_CASE("exactly block_level * 100KB round-trips at level 1") {
    // Exactly one block at level 1 (100 KB)
    Options opts;
    opts.block_level = 1;
    const auto input = TestData::compressible(100 * 1024);
    CHECK(decompress_from_vec(compress_to_vec(input, opts), opts) == input);
}

TEST_CASE("slightly over one block spills into second block") {
    Options opts;
    opts.block_level = 1;
    // 100 KB + 1 byte forces a second block
    const auto input = TestData::compressible(100 * 1024 + 1);
    CHECK(decompress_from_vec(compress_to_vec(input, opts), opts) == input);
}
