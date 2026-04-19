#pragma once
#include <array>
#include <cstdint>
#include <string>

// File format:
//   [4]  magic = "BZP\x02"
//   [1]  version = 1
//   [4]  block_size  (uint32 LE) — uncompressed bytes per block
//   [4]  num_blocks  (uint32 LE)
//   per block:
//     [4]  compressed_size (uint32 LE)
//     [N]  compressed data (a complete bzip2 stream)

inline constexpr std::array<char, 4> BZIP2PB_MAGIC = {'B', 'Z', 'P', '\x02'};
inline constexpr uint8_t BZIP2PB_VERSION = 1;
inline constexpr uint32_t DEFAULT_BLOCK_SIZE = 900 * 1024;

struct Options {
    int      block_level   = 9;   // 1-9: bzip2 compression level AND chunk size (N * 100 KB)
    unsigned num_threads   = 0;   // 0 = std::thread::hardware_concurrency()
    bool     keep          = false;
    bool     force         = false;
    bool     verbose       = false;
    bool     decompress    = false;
};
