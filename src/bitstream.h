#pragma once
#include <cstdint>
#include <vector>

// Reconstruct a self-contained bzip2 stream from a single block extracted
// from a larger file buffer.  The result can be passed directly to
// BZ2_bzBuffToBuffDecompress.
//
// src               — full compressed file buffer
// src_size_bytes    — length of src in bytes
// block_start_bit   — bit offset of the block magic (0x314159265359) in src
// next_boundary_bit — bit offset of the next magic (block or EOS), or end-of-file
// block_size_digit  — '1'–'9' from the original stream header
std::vector<uint8_t> reconstruct_substream(
    const uint8_t* src,
    size_t         src_size_bytes,
    size_t         block_start_bit,
    size_t         next_boundary_bit,
    char           block_size_digit);
