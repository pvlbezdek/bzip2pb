#include <catch2/catch_test_macros.hpp>
#include "bitscanner.h"
#include "bitstream.h"
#include <bzlib.h>
#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::vector<uint8_t> bz2_compress(const std::vector<uint8_t>& input,
                                          int level = 9) {
    unsigned int out_size =
        static_cast<unsigned int>(input.size() * 1.02 + 600);
    std::vector<uint8_t> output(out_size);
    const int ret = BZ2_bzBuffToBuffCompress(
        reinterpret_cast<char*>(output.data()), &out_size,
        // libbzip2 doesn't modify src, but the API isn't const-correct
        const_cast<char*>(reinterpret_cast<const char*>(input.data())),
        static_cast<unsigned int>(input.size()),
        level, 0, 30);
    REQUIRE(ret == BZ_OK);
    output.resize(out_size);
    return output;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("scan_bzip2_blocks finds block and EOS in a minimal stream") {
    const std::vector<uint8_t> input(1024, static_cast<uint8_t>('A'));
    const auto compressed = bz2_compress(input);

    const auto boundaries =
        scan_bzip2_blocks(compressed.data(), compressed.size());

    REQUIRE_FALSE(boundaries.empty());

    bool has_block = false, has_eos = false;
    for (const auto& b : boundaries) {
        if (!b.is_eos) has_block = true;
        if (b.is_eos)  has_eos   = true;
    }
    CHECK(has_block);
    CHECK(has_eos);
    // Block must precede EOS
    CHECK(boundaries.front().bit_offset < boundaries.back().bit_offset);
}

TEST_CASE("scan_bzip2_blocks finds two block magics in concatenated streams") {
    const std::vector<uint8_t> chunk1(200, static_cast<uint8_t>('A'));
    const std::vector<uint8_t> chunk2(200, static_cast<uint8_t>('B'));
    const auto c1 = bz2_compress(chunk1);
    const auto c2 = bz2_compress(chunk2);

    std::vector<uint8_t> concat;
    concat.insert(concat.end(), c1.begin(), c1.end());
    concat.insert(concat.end(), c2.begin(), c2.end());

    const auto boundaries =
        scan_bzip2_blocks(concat.data(), concat.size());

    int block_count = 0;
    for (const auto& b : boundaries)
        if (!b.is_eos) ++block_count;

    CHECK(block_count >= 2);
}

TEST_CASE("reconstruct_substream produces decompressible output") {
    const std::vector<uint8_t> input(512, static_cast<uint8_t>('X'));
    const auto compressed = bz2_compress(input);

    const auto boundaries =
        scan_bzip2_blocks(compressed.data(), compressed.size());
    REQUIRE_FALSE(boundaries.empty());

    // Locate first non-EOS boundary and the following boundary.
    size_t block_bit = 0, next_bit = compressed.size() * 8;
    bool found = false;
    for (size_t i = 0; i < boundaries.size(); ++i) {
        if (!boundaries[i].is_eos) {
            block_bit = boundaries[i].bit_offset;
            if (i + 1 < boundaries.size())
                next_bit = boundaries[i + 1].bit_offset;
            found = true;
            break;
        }
    }
    REQUIRE(found);

    const char digit = static_cast<char>(compressed[3]);
    const auto sub = reconstruct_substream(
        compressed.data(), compressed.size(),
        block_bit, next_bit, digit);

    unsigned int out_size = static_cast<unsigned int>(input.size() + 256u);
    std::vector<uint8_t> output(out_size);
    const int ret = BZ2_bzBuffToBuffDecompress(
        reinterpret_cast<char*>(output.data()), &out_size,
        const_cast<char*>(reinterpret_cast<const char*>(sub.data())),
        static_cast<unsigned int>(sub.size()),
        0, 0);
    CHECK(ret == BZ_OK);
    output.resize(out_size);
    CHECK(output == input);
}

TEST_CASE("scan_bzip2_blocks returns boundaries sorted by bit_offset") {
    const std::vector<uint8_t> chunk(300, static_cast<uint8_t>('Z'));
    const auto c1 = bz2_compress(chunk);
    const auto c2 = bz2_compress(chunk);

    std::vector<uint8_t> concat;
    concat.insert(concat.end(), c1.begin(), c1.end());
    concat.insert(concat.end(), c2.begin(), c2.end());

    const auto boundaries =
        scan_bzip2_blocks(concat.data(), concat.size());

    for (size_t i = 1; i < boundaries.size(); ++i)
        CHECK(boundaries[i].bit_offset > boundaries[i - 1].bit_offset);
}
