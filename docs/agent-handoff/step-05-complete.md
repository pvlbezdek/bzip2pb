# Step 5 complete: Benchmarking

## What was done

- Created `benchmarks/bench.h` — `BenchResult` struct and three function declarations
- Created `benchmarks/bench.cpp` — full benchmark harness with `main()`:
  - `bench_bzip2pb()` — times `compress_stream`/`decompress_stream` via the internal API
  - `bench_libbzip2()` — times `BZ2_bzBuffToBuffCompress`/`BZ2_bzBuffToBuffDecompress` as a single-threaded baseline
  - `bench_external()` — times `pbzip2`/`lbzip2` via `system()` subprocess (Unix only; returns `nullopt` on Windows)
  - `main()` — generates six corpora, runs all benchmarks, prints a formatted table per corpus, writes optional CSV
- Added `bzip2pb_bench` executable target and `benchmark` custom target to `CMakeLists.txt`

## Key files changed

- `benchmarks/bench.h` — new: BenchResult, function declarations
- `benchmarks/bench.cpp` — new: complete benchmark harness + main()
- `CMakeLists.txt` — added `bzip2pb_bench` executable + `benchmark` target

## Decisions made

- External-tool benchmarking (`pbzip2`/`lbzip2`) is Unix-only via `#ifdef _WIN32`; returns `[not found]` silently on Windows since neither tool is typically available there
- Corpus data is generated lazily inside the loop (one at a time) to keep peak memory at ~200 MB (100 MB input + 100 MB compressed buffer) rather than pre-allocating all 222 MB simultaneously
- Thread-scaling coverage is reduced for large corpora to keep runtime reasonable:
  - 1 MB: threads 1, 2, 4, 8, auto
  - 10 MB: threads 1, 4, auto
  - 100 MB: threads 1, auto
- `bzip2pb_lib` public include dirs provide `src/` automatically; `benchmarks/` and `tests/` (for `testdata.h`) are added as PRIVATE includes on the bench target
- `$<TARGET_FILE:bzip2pb_bench>` used in the `benchmark` custom-target COMMAND so Windows MSVC multi-config builds (where the binary lives in `Release/`) are resolved correctly

## Known issues / TODOs

- Very compressible text corpora (text_like uses a 9-word repeat) compress to < 0.001 ratio, which shows as `0.000` with the current 3-decimal display; this is cosmetic and correct
- The 100 MB corpora take several minutes to run; the benchmark is a manual target and is not added to CI
- External tool subprocess timing includes process-launch overhead (tens of milliseconds); meaningful only for corpora ≥ 10 MB

## What the next agent must know

- Run: `cmake --build build --target benchmark` — produces console table and `build/benchmark_results.csv`
- `bzip2pb_bench --help` describes the `--csv` flag
- `bzip2pb_lib` and `testdata.h` are the two dependencies; the pattern is identical to how `bzip2pb_tests` was wired up in step 4
- Step 6 is Conda packaging; the relevant CMake `install()` target already exists in `CMakeLists.txt`
