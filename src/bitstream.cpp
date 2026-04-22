#include "bitstream.h"
#include <cstring>

namespace {

class BitWriter {
    std::vector<uint8_t>& buf_;
    int bit_pos_; // 0 = MSB of the current output byte

public:
    explicit BitWriter(std::vector<uint8_t>& buf, int initial_bit_pos = 0)
        : buf_(buf), bit_pos_(initial_bit_pos) {}

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

// Copy n_bits from src starting at src_bit_start into buf_ (which must be
// byte-aligned at the call site, i.e. bit_pos == 0).  Returns the number of
// trailing bits written into the last output byte (0 = last byte is complete).
// Operates byte-by-byte with a barrel shift — ~8× faster than bit-by-bit.
static unsigned copy_bits_byte_aligned(
    std::vector<uint8_t>& buf,
    const uint8_t*         src,
    size_t                 src_size_bytes,
    size_t                 src_bit_start,
    size_t                 n_bits)
{
    if (n_bits == 0) return 0;

    const size_t   src_byte  = src_bit_start / 8;
    const unsigned shift     = static_cast<unsigned>(src_bit_start % 8);
    const size_t   n_full    = n_bits / 8;          // complete output bytes
    const unsigned tail      = static_cast<unsigned>(n_bits % 8);  // 0..7 leftover bits

    buf.reserve(buf.size() + n_full + (tail ? 1u : 0u));

    if (shift == 0) {
        // Source is byte-aligned: bulk copy then handle tail.
        const size_t avail = (src_byte < src_size_bytes)
                             ? src_size_bytes - src_byte : 0;
        const size_t copy_full = (n_full <= avail) ? n_full : avail;
        const size_t old_size = buf.size();
        buf.resize(old_size + copy_full);
        std::memcpy(buf.data() + old_size, src + src_byte, copy_full);

        if (tail > 0 && src_byte + n_full < src_size_bytes) {
            // Keep only the top `tail` bits of the next source byte.
            buf.push_back(src[src_byte + n_full]
                          & static_cast<uint8_t>(0xFFu << (8u - tail)));
        }
    } else {
        // Source straddles byte boundaries: one barrel-shift per output byte.
        const unsigned rshift = 8u - shift;
        for (size_t i = 0; i < n_full; ++i) {
            const size_t b = src_byte + i;
            if (b >= src_size_bytes) break;
            const uint8_t lo = (b + 1 < src_size_bytes) ? (src[b + 1] >> rshift) : 0u;
            buf.push_back(static_cast<uint8_t>((src[b] << shift) | lo));
        }
        if (tail > 0) {
            const size_t b = src_byte + n_full;
            if (b < src_size_bytes) {
                uint8_t byte_val = src[b] << shift;
                if (b + 1 < src_size_bytes)
                    byte_val |= src[b + 1] >> rshift;
                // Keep only the top `tail` bits.
                buf.push_back(byte_val & static_cast<uint8_t>(0xFFu << (8u - tail)));
            }
        }
    }

    return tail;
}

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

    // 2. Copy block bits (block magic + payload) using byte-level barrel shift.
    //    The output is byte-aligned here (right after the 4-byte header).
    const size_t n_bits = next_boundary_bit - block_start_bit;
    const unsigned tail_bits = copy_bits_byte_aligned(
        result, src, src_size_bytes, block_start_bit, n_bits);

    // 3. Continue appending EOS magic and CRC; resume at the partial last byte.
    BitWriter w(result, static_cast<int>(tail_bits));

    // End-of-stream magic: 0x177245385090 (48 bits, digits of sqrt(pi))
    w.write_bits(0x177245385090ULL, 48);

    // 4. Combined-stream CRC.
    //    For a single-block sub-stream: combined = rotate_left_1(0) ^ block_crc = block_crc.
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
