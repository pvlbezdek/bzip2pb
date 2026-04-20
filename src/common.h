#pragma once
#include <cstdint>

struct Options {
    int      block_level   = 9;   // 1-9: bzip2 compression level AND chunk size (N * 100 KB)
    unsigned num_threads   = 0;   // 0 = std::thread::hardware_concurrency()
    bool     keep          = false;
    bool     force         = false;
    bool     verbose       = false;
    bool     decompress    = false;
};
