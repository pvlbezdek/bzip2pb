#include "bitstream.h"

namespace {

class BitWriter {
    std::vector<uint8_t>& buf_;
    int bit_pos_ = 0; // 0 = MSB of the current output byte

public:
    explicit BitWriter(std::vector<uint8_t>& buf) : buf_(buf) {}

    void write_bit(int bit) {
        if (bit_pos_ == 0) buf_.push_back(0u);
        if (bit) buf_.back() |= static_cast<uint8_t>(0x80u >> static_cast<unsigned>(bit_pos_));
        if (++bit_pos_ == 8) bit_pos_ = 0;
    }

    // Write n_bits of value MSB-first (bzip2 bit-stream convention).
    void write_bits(uint64_t value, int n_bits) {
        for (int i = n_bits - 1; i >= 0; --i)
            write_bit(static_cast<int>((value >> static_cast<unsigned>(i)) & 1u));
    }
};

} // namespace

std::vector<uint8_t> reconstruct_substream(
    const uint8_t* src,
    size_t         src_size_bytes,
    size_t         block_start_bit,
    size_t         next_boundary_bit,
    char           block_size_digit)
{
    std::vector<uint8_t> result;
    // Reserve header + block bits (estimate) + EOS + CRC + 1 pad byte
    result.reserve(4 + (next_boundary_bit - block_start_bit) / 8 + 12);

    // 1. bzip2 stream header: "BZh" + block-size digit
    result.push_back('B');
    result.push_back('Z');
    result.push_back('h');
    result.push_back(static_cast<uint8_t>(block_size_digit));

    BitWriter w(result);

    // 2. Copy block bits verbatim (includes block magic + block payload).
    //    Each bit is extracted MSB-first from src.
    const size_t n_bits = next_boundary_bit - block_start_bit;
    for (size_t i = 0; i < n_bits; ++i) {
        const size_t abs_bit  = block_start_bit + i;
        const size_t byte_idx = abs_bit / 8;
        if (byte_idx >= src_size_bytes) break;
        const int val = (src[byte_idx] >> (7 - static_cast<int>(abs_bit % 8))) & 1;
        w.write_bit(val);
    }

    // 3. End-of-stream magic: 0x177245385090 (48 bits, digits of sqrt(pi))
    w.write_bits(0x177245385090ULL, 48);

    // 4. Combined-stream CRC.
    //    bzip2 format: combined = rotate_left_1(prev_combined) ^ block_crc for each block.
    //    For our single-block sub-stream: combined = rotate_left_1(0) ^ block_crc = block_crc.
    //    The block_crc occupies bits [48, 80) relative to block_start_bit.
    uint32_t block_crc = 0;
    for (int i = 0; i < 32; ++i) {
        const size_t ab  = block_start_bit + 48 + static_cast<size_t>(i);
        if (ab / 8 >= src_size_bytes) break;
        const int v = (src[ab / 8] >> (7 - static_cast<int>(ab % 8))) & 1;
        block_crc = (block_crc << 1) | static_cast<uint32_t>(v);
    }
    w.write_bits(static_cast<uint64_t>(block_crc), 32);

    // 5. Padding: remaining bits of the last byte are already 0 (BitWriter
    //    initialises each new byte to 0 before setting individual bits).

    return result;
}
