#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct BenchResult {
    std::string tool;
    std::string corpus;
    int         threads;        // 0 = hardware_concurrency (auto)
    int         level;
    double      compress_mb_s;
    double      decompress_mb_s;
    double      ratio;          // compressed_bytes / original_bytes
    double      compress_ms;
    double      decompress_ms;
};

// Benchmark bzip2pb using the internal compress_stream / decompress_stream API.
// threads=0 uses hardware_concurrency().
BenchResult bench_bzip2pb(const std::vector<uint8_t>& data,
                          const std::string& corpus_name,
                          int threads, int level);

// Benchmark libbzip2 single-threaded via BZ2_bzBuffToBuffCompress/Decompress.
BenchResult bench_libbzip2(const std::vector<uint8_t>& data,
                           const std::string& corpus_name,
                           int level);

// Benchmark an external parallel tool (pbzip2 / lbzip2) via subprocess.
// Returns nullopt when the tool is not found on PATH.
std::optional<BenchResult> bench_external(const std::string& tool,
                                          const std::vector<uint8_t>& data,
                                          const std::string& corpus_name,
                                          int threads, int level);
