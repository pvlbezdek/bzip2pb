# bzip2pb Benchmarks

## Methodology

Benchmarks measure wall-clock time for compression and decompression of synthetic corpora generated deterministically at benchmark run time. No large files are committed to the repository.

### Corpus

| Name          | Size    | Content                               |
|---------------|---------|---------------------------------------|
| `text_1m`     | 1 MB    | Pseudo-random printable ASCII text    |
| `binary_1m`   | 1 MB    | Pseudo-random binary (low compressibility) |
| `text_10m`    | 10 MB   | Pseudo-random printable ASCII text    |
| `binary_10m`  | 10 MB   | Pseudo-random binary                  |
| `text_100m`   | 100 MB  | Pseudo-random printable ASCII text    |
| `binary_100m` | 100 MB  | Pseudo-random binary                  |

All corpora are generated with a fixed seed (42) for reproducibility.

### Tools compared

| Tool | Description |
|------|-------------|
| `bzip2pb` | This tool, various thread counts |
| `bzip2 (lib)` | libbzip2 `BZ2_bzBuffToBuffCompress/Decompress`, single-threaded |
| `pbzip2` | If found on `PATH` |
| `lbzip2` | If found on `PATH` |

External tools (`pbzip2`, `lbzip2`) are invoked via subprocess. If not found on `PATH`, they are reported as `[not found]` and omitted from the table.

### Measurement

Each combination of (tool, corpus, threads, level) is timed three times; the median is reported. Timing covers only the compress or decompress operation, not file I/O setup. Speed is reported in MB/s of uncompressed data.

---

## Running benchmarks

```sh
# Configure (Linux example — adjust preset for your platform)
cmake --preset ci-ubuntu-latest-x64

# Build and run
cmake --build build --target benchmark

# Results are printed to stdout and written to:
build/benchmark_results.csv
```

The benchmark binary accepts these flags:

```
Usage: bzip2pb_bench [options]
  --csv FILE     Write CSV results to FILE (default: benchmark_results.csv)
  --level N      Only benchmark level N (default: all levels 1-9)
  --threads N    Only benchmark N threads (default: 1, 2, 4, auto)
  --corpus NAME  Only benchmark named corpus (default: all)
  --no-external  Skip pbzip2/lbzip2 even if found
```

---

## Output format

Terminal output (example):

```
bzip2pb benchmark — text_10m (10.0 MB)
═══════════════════════════════════════════════════════════════════════
Tool          Threads  Level  Compr.Speed  Decomp.Speed  Ratio
───────────────────────────────────────────────────────────────────────
bzip2pb            1      9     ~110 MB/s    ~300 MB/s   ~0.31
bzip2pb            4      9     ~420 MB/s   ~1100 MB/s   ~0.31
bzip2pb          auto      9     ~1400 MB/s  ~3500 MB/s   ~0.31
bzip2 (lib)        1      9     ~105 MB/s    ~280 MB/s   ~0.31
pbzip2             4      9     ~400 MB/s   ~1000 MB/s   ~0.31  [found]
lbzip2             4      9     ~410 MB/s   ~1050 MB/s   ~0.31  [found]
═══════════════════════════════════════════════════════════════════════
```

Numbers above are approximate. Run the benchmark on your hardware to get actual results.

CSV columns: `tool,corpus,threads,level,compress_mb_s,decompress_mb_s,ratio,compress_ms,decompress_ms`

---

## Thread scaling

bzip2pb compression speed scales approximately linearly with thread count until I/O or memory bandwidth becomes the bottleneck. For typical text corpora at level 9, the crossover point is around 16–32 threads on a modern desktop CPU.

Decompression scales similarly but with higher per-thread throughput because decompression is less compute-intensive than compression.

---

## Reproducing reference results

Reference measurements were taken on a developer machine (see `build/benchmark_results.csv` after running). To reproduce:

```sh
cmake --preset ci-ubuntu-latest-x64   # or your platform preset
cmake --build build --target benchmark
```

Results will differ on different hardware. The key comparison is the scaling ratio (1-thread vs. N-thread) rather than the absolute numbers.
