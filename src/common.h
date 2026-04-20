#pragma once
#include <cstdint>

struct Options {
    int      block_level   = 9;   // 1-9: bzip2 compression level AND chunk size (N * 100 KB)
    unsigned num_threads   = 0;   // 0 = std::thread::hardware_concurrency()
    bool     keep          = false;
    bool     force         = false;
    bool     verbose       = false;
    bool     quiet         = false;   // -q: suppress warnings
    bool     decompress    = false;
    bool     test          = false;   // -t: decompress to null sink (verify integrity)
    bool     to_stdout     = false;   // -c: write to stdout, keep input
};
