#include "bitscanner.h"
#include <algorithm>

std::vector<BlockBoundary> scan_bzip2_blocks(const uint8_t* data, size_t size_bytes) {
    // Block magic: 0x314159265359  (leading digits of pi)
    // EOS magic:   0x177245385090  (leading digits of sqrt(pi))
    // Packed into the upper 48 bits of a uint64 for shift-comparison.
    static constexpr uint64_t BLOCK_MAGIC = 0x3141592653590000ULL;
    static constexpr uint64_t EOS_MAGIC   = 0x1772453850900000ULL;
    static constexpr uint64_t MASK48      = 0xFFFFFFFFFFFF0000ULL;

    std::vector<BlockBoundary> result;

    // Start after the 4-byte stream header ("BZh" + digit).
    // Load 8 bytes as big-endian uint64 and check all 8 bit offsets per byte.
    for (size_t b = 4; b + 8 <= size_bytes; ++b) {
        uint64_t word = 0;
        for (int k = 0; k < 8; ++k)
            word = (word << 8) | data[b + static_cast<size_t>(k)];

        for (int d = 0; d < 8; ++d) {
            const uint64_t shifted = (word << static_cast<unsigned>(d)) & MASK48;
            if (shifted == BLOCK_MAGIC)
                result.push_back({ b * 8 + static_cast<size_t>(d), false });
            else if (shifted == EOS_MAGIC)
                result.push_back({ b * 8 + static_cast<size_t>(d), true });
        }
    }

    std::sort(result.begin(), result.end(),
              [](const BlockBoundary& lhs, const BlockBoundary& rhs) {
                  return lhs.bit_offset < rhs.bit_offset;
              });
    result.erase(
        std::unique(result.begin(), result.end(),
                    [](const BlockBoundary& lhs, const BlockBoundary& rhs) {
                        return lhs.bit_offset == rhs.bit_offset;
                    }),
        result.end());

    return result;
}
