#include "bench.h"
#include "compress.h"
#include "decompress.h"
#include "testdata.h"
#include <bzlib.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using Clock  = std::chrono::steady_clock;

// ── timing helpers ────────────────────────────────────────────────────────────

static double to_ms(Clock::duration d)
{
    return std::chrono::duration<double, std::milli>(d).count();
}

static double mb_per_sec(size_t bytes, double ms)
{
    if (ms <= 0.0 || bytes == 0) return 0.0;
    return (static_cast<double>(bytes) / (1024.0 * 1024.0)) / (ms / 1000.0);
}

// ── bench_bzip2pb ─────────────────────────────────────────────────────────────

BenchResult bench_bzip2pb(const std::vector<uint8_t>& data,
                          const std::string& corpus_name,
                          int threads, int level)
{
    Options opts;
    opts.block_level = level;
    opts.num_threads = static_cast<unsigned>(threads < 0 ? 0 : threads);

    std::string raw(reinterpret_cast<const char*>(data.data()), data.size());

    // Compress
    std::istringstream ci(raw);
    std::ostringstream co;
    auto t0 = Clock::now();
    compress_stream(ci, co, opts);
    auto t1 = Clock::now();
    std::string compressed = co.str();

    // Decompress
    opts.decompress = true;
    std::istringstream di(compressed);
    std::ostringstream dout;
    auto t2 = Clock::now();
    decompress_stream(di, dout, opts);
    auto t3 = Clock::now();

    double cmpr_ms  = to_ms(t1 - t0);
    double dcmpr_ms = to_ms(t3 - t2);

    BenchResult r;
    r.tool            = "bzip2pb";
    r.corpus          = corpus_name;
    r.threads         = threads;
    r.level           = level;
    r.compress_ms     = cmpr_ms;
    r.decompress_ms   = dcmpr_ms;
    r.compress_mb_s   = mb_per_sec(data.size(), cmpr_ms);
    r.decompress_mb_s = mb_per_sec(data.size(), dcmpr_ms);
    r.ratio           = static_cast<double>(compressed.size()) /
                        static_cast<double>(data.size() > 0 ? data.size() : 1);
    return r;
}

// ── bench_libbzip2 ────────────────────────────────────────────────────────────

BenchResult bench_libbzip2(const std::vector<uint8_t>& data,
                           const std::string& corpus_name,
                           int level)
{
    unsigned dest_len = static_cast<unsigned>(data.size() * 2 + 600);
    std::vector<char> compressed(dest_len);

    auto t0 = Clock::now();
    int rc = BZ2_bzBuffToBuffCompress(
        compressed.data(), &dest_len,
        // C API does not declare input const even though it is not modified
        const_cast<char*>(reinterpret_cast<const char*>(data.data())),
        static_cast<unsigned>(data.size()),
        level, 0, 0);
    auto t1 = Clock::now();
    if (rc != BZ_OK)
        throw std::runtime_error("libbzip2 compress failed: " + std::to_string(rc));

    unsigned decomp_len = static_cast<unsigned>(data.size() + 1);
    std::vector<char> decompressed(decomp_len);

    auto t2 = Clock::now();
    rc = BZ2_bzBuffToBuffDecompress(
        decompressed.data(), &decomp_len,
        compressed.data(), dest_len, 0, 0);
    auto t3 = Clock::now();
    if (rc != BZ_OK)
        throw std::runtime_error("libbzip2 decompress failed: " + std::to_string(rc));

    double cmpr_ms  = to_ms(t1 - t0);
    double dcmpr_ms = to_ms(t3 - t2);

    BenchResult r;
    r.tool            = "bzip2 (lib)";
    r.corpus          = corpus_name;
    r.threads         = 1;
    r.level           = level;
    r.compress_ms     = cmpr_ms;
    r.decompress_ms   = dcmpr_ms;
    r.compress_mb_s   = mb_per_sec(data.size(), cmpr_ms);
    r.decompress_mb_s = mb_per_sec(data.size(), dcmpr_ms);
    r.ratio           = static_cast<double>(dest_len) /
                        static_cast<double>(data.size() > 0 ? data.size() : 1);
    return r;
}

// ── bench_external (Unix only) ────────────────────────────────────────────────

#ifdef _WIN32
// pbzip2 and lbzip2 are not available on Windows
std::optional<BenchResult> bench_external(const std::string&,
                                          const std::vector<uint8_t>&,
                                          const std::string&,
                                          int, int)
{
    return std::nullopt;
}
#else

static bool tool_on_path(const std::string& tool)
{
    std::string cmd = "which " + tool + " > /dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

static std::string thread_flag(const std::string& tool, int threads)
{
    if (threads <= 0) return "";
    if (tool == "pbzip2") return " -p" + std::to_string(threads);
    if (tool == "lbzip2") return " -n " + std::to_string(threads);
    return "";
}

std::optional<BenchResult> bench_external(const std::string& tool,
                                          const std::vector<uint8_t>& data,
                                          const std::string& corpus_name,
                                          int threads, int level)
{
    if (!tool_on_path(tool))
        return std::nullopt;

    fs::path tmp_dir  = fs::temp_directory_path();
    fs::path raw_path = tmp_dir / "bzip2pb_bench_raw.bin";
    fs::path bz2_path = tmp_dir / "bzip2pb_bench_raw.bz2";

    // Write raw data
    {
        std::ofstream f(raw_path, std::ios::binary);
        f.write(reinterpret_cast<const char*>(data.data()),
                static_cast<std::streamsize>(data.size()));
    }

    // Benchmark compress: tool reads raw_path, writes to /dev/null
    std::string tf        = thread_flag(tool, threads);
    std::string comp_cmd  = tool + tf + " -" + std::to_string(level)
                          + " -k -c \"" + raw_path.string() + "\" > /dev/null 2>&1";
    auto t0 = Clock::now();
    std::system(comp_cmd.c_str());
    auto t1 = Clock::now();

    // Pre-build a .bz2 file with libbzip2 for the decompress benchmark
    unsigned bz2_size = static_cast<unsigned>(data.size() * 2 + 600);
    {
        std::vector<char> tmp(bz2_size);
        BZ2_bzBuffToBuffCompress(
            tmp.data(), &bz2_size,
            const_cast<char*>(reinterpret_cast<const char*>(data.data())),
            static_cast<unsigned>(data.size()), level, 0, 0);
        std::ofstream bf(bz2_path, std::ios::binary);
        bf.write(tmp.data(), bz2_size);
    }

    // Benchmark decompress: tool reads bz2_path, writes to /dev/null
    std::string dcmp_cmd = tool + tf + " -d -k -c \""
                         + bz2_path.string() + "\" > /dev/null 2>&1";
    auto t2 = Clock::now();
    std::system(dcmp_cmd.c_str());
    auto t3 = Clock::now();

    // Cleanup
    std::error_code ec;
    fs::remove(raw_path, ec);
    fs::remove(bz2_path, ec);

    double cmpr_ms  = to_ms(t1 - t0);
    double dcmpr_ms = to_ms(t3 - t2);

    BenchResult r;
    r.tool            = tool;
    r.corpus          = corpus_name;
    r.threads         = threads;
    r.level           = level;
    r.compress_ms     = cmpr_ms;
    r.decompress_ms   = dcmpr_ms;
    r.compress_mb_s   = mb_per_sec(data.size(), cmpr_ms);
    r.decompress_mb_s = mb_per_sec(data.size(), dcmpr_ms);
    r.ratio           = static_cast<double>(bz2_size) /
                        static_cast<double>(data.size() > 0 ? data.size() : 1);
    return r;
}
#endif  // _WIN32

// ── output helpers ────────────────────────────────────────────────────────────

static constexpr int TABLE_WIDTH = 76;

static void print_separator(char ch)
{
    std::cout << std::string(TABLE_WIDTH, ch) << "\n";
}

static void print_header()
{
    print_separator('=');
    char buf[TABLE_WIDTH + 8];
    std::snprintf(buf, sizeof(buf),
        "%-16s %7s  %5s  %12s  %12s  %6s",
        "Tool", "Threads", "Level",
        "Compr.Speed", "Decomp.Speed", "Ratio");
    std::cout << buf << "\n";
    print_separator('-');
}

static void print_row(const BenchResult& r)
{
    std::string thr = (r.threads == 0) ? "auto" : std::to_string(r.threads);
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "%-16s %7s  %5d  %9.0f MB/s  %9.0f MB/s  %6.3f",
        r.tool.c_str(), thr.c_str(), r.level,
        r.compress_mb_s, r.decompress_mb_s, r.ratio);
    std::cout << buf << "\n";
}

static void write_csv(const std::string& path,
                      const std::vector<BenchResult>& results)
{
    std::ofstream csv(path);
    csv << "tool,corpus,threads,level,"
           "compress_mb_s,decompress_mb_s,ratio,"
           "compress_ms,decompress_ms\n";
    for (auto& r : results) {
        csv << r.tool       << ","
            << r.corpus     << ","
            << r.threads    << ","
            << r.level      << ","
            << std::fixed << std::setprecision(2)
            << r.compress_mb_s   << ","
            << r.decompress_mb_s << ","
            << std::setprecision(4) << r.ratio << ","
            << std::setprecision(2)
            << r.compress_ms  << ","
            << r.decompress_ms << "\n";
    }
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    std::string csv_path;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--csv" && i + 1 < argc)
            csv_path = argv[++i];
        else if (std::string(argv[i]) == "--help") {
            std::cout << "Usage: bzip2pb_bench [--csv <file>]\n"
                         "  --csv <file>  Write results to CSV in addition to stdout\n";
            return 0;
        }
    }

    const int LEVEL       = 9;
    const int EXT_THREADS = 4;   // thread count passed to external tools

    struct Corpus {
        std::string        name;
        size_t             size;
        bool               is_text;
        std::vector<int>   thread_counts;  // 0 = hardware_concurrency
    };

    const std::vector<Corpus> corpora = {
        {"text_1m",      1ul * 1024 * 1024,   true,  {1, 2, 4, 8, 0}},
        {"binary_1m",    1ul * 1024 * 1024,   false, {1, 2, 4, 8, 0}},
        {"text_10m",    10ul * 1024 * 1024,   true,  {1, 4, 0}},
        {"binary_10m",  10ul * 1024 * 1024,   false, {1, 4, 0}},
        {"text_100m",  100ul * 1024 * 1024,   true,  {1, 0}},
        {"binary_100m",100ul * 1024 * 1024,   false, {1, 0}},
    };

    std::vector<BenchResult> all_results;

    for (auto& corpus : corpora) {
        double mb = static_cast<double>(corpus.size) / (1024.0 * 1024.0);

        std::cout << "\nGenerating " << corpus.name << " ("
                  << std::fixed << std::setprecision(0) << mb << " MB)...\n";
        std::cout.flush();

        std::vector<uint8_t> data = corpus.is_text
            ? TestData::text_like(corpus.size)
            : TestData::random_binary(corpus.size);

        std::cout << "bzip2pb benchmark -- " << corpus.name
                  << " (" << std::fixed << std::setprecision(1) << mb << " MB)\n";
        print_header();

        // bzip2pb at various thread counts
        for (int n : corpus.thread_counts) {
            auto r = bench_bzip2pb(data, corpus.name, n, LEVEL);
            print_row(r);
            std::cout.flush();
            all_results.push_back(r);
        }

        // libbzip2 single-threaded baseline
        {
            auto r = bench_libbzip2(data, corpus.name, LEVEL);
            print_row(r);
            std::cout.flush();
            all_results.push_back(r);
        }

        // External tools (Unix only; returns nullopt on Windows)
        for (const char* tool : {"pbzip2", "lbzip2"}) {
            auto r = bench_external(tool, data, corpus.name, EXT_THREADS, LEVEL);
            if (r) {
                print_row(*r);
                all_results.push_back(*r);
            } else {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%-16s [not found]", tool);
                std::cout << buf << "\n";
            }
            std::cout.flush();
        }

        print_separator('=');
    }

    if (!csv_path.empty()) {
        write_csv(csv_path, all_results);
        std::cout << "\nCSV results written to: " << csv_path << "\n";
    }

    return 0;
}
