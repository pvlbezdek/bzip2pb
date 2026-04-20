#pragma once
#include <cstdint>
#include <vector>

struct BlockBoundary {
    size_t bit_offset;
    bool   is_eos;
};

// Scan a bzip2 file buffer for block and end-of-stream magic patterns.
// Skips the 4-byte stream header. Returns boundaries sorted by bit_offset.
std::vector<BlockBoundary> scan_bzip2_blocks(const uint8_t* data, size_t size_bytes);
