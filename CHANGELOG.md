# Changelog

All notable changes to bzip2pb are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

---

## [Unreleased] — perf/08-decompression-reconstruction

### Fixed
- **Decompression throughput no longer stalls with multiple threads.**
  `reconstruct_substream` previously copied block bits one at a time on the
  main thread before handing work to the pool.  For a 10 MB incompressible file
  at level 9 this was ~79 M iterations running serially, giving identical speed
  (18 MB/s) regardless of thread count.  Replaced with a byte-level barrel-shift
  copy (~8× fewer iterations); decompression now scales with thread count and is
  expected to reach ~100 MB/s on a 4-core M1 Pro — matching lbzip2.

### Improved
- **Bitscanner uses a sliding window.**  The 64-bit word used for magic-pattern
  matching was previously rebuilt from 8 individual byte loads on every byte
  position.  It is now seeded once and updated by a single byte per step,
  reducing memory reads by ~8× in `scan_bzip2_blocks`.

### Known limitations
- Text compression speed (30–41 MB/s at auto threads) remains well below
  lbzip2 (163–201 MB/s) due to libbz2's BWT sort being slower on repetitive
  input.  A future step will evaluate replacing the BWT backend with `libsais`
  or `divsufsort`.

---

## [0.2.0] — step/07-documentation

### Added
- `README.md` with install, usage, and benchmark sections.
- `docs/ARCHITECTURE.md` — threading model and pipeline diagrams.
- `docs/FORMAT.md` — bzip2 stream format specification.
- `docs/BENCHMARKS.md` — reference benchmark results and methodology.

---

## [0.1.6] — step/06-conda

### Added
- Conda recipe (`conda/meta.yaml`, `conda/build.sh`, `conda/bld.bat`).
- Multi-platform CI for conda builds (Linux x86-64, macOS ARM64, Windows x64).
- MIT `LICENSE` file.

---

## [0.1.5] — step/05-benchmarks

### Added
- `benchmarks/bench.cpp` harness comparing bzip2pb, libbzip2, pbzip2, lbzip2.
- CMake `benchmark` target.
- CSV output (`build/benchmark_results.csv`).

---

## [0.1.4] — step/04-testing

### Added
- Catch2 v3 unit and integration test suite (175 assertions, 58 test cases).
- `bzip2pb_lib` static-library CMake target shared between binary and tests.
- CTest integration; `cmake --build build --target check`.

---

## [0.1.3] — step/03-lbzip2-cli

### Added
- Full lbzip2-compatible CLI: `-z/-d/-t/-c/-k/-f/-n/-1…-9/-v/-q/-L/-V/--help`.
- `-t` test mode decompresses to a null sink and verifies CRC.
- Verbose output matches lbzip2's `filename: N.NNN:1, …` format.
- Exit codes 0/1/2 matching lbzip2.

---

## [0.1.2] — step/02-standard-format

### Changed
- Output is now standard `.bz2` (concatenated bzip2 streams); replaces the
  previous custom `BZP\x02` container format.

### Added
- Bit-level block-boundary scanner (`src/bitscanner.cpp`).
- Sub-stream reconstructor (`src/bitstream.cpp`) enabling parallel decompression
  of any standard `.bz2` file.

---

## [0.1.1] — step/01-cross-platform-build

### Added
- CMake build with vcpkg integration.
- GitHub Actions CI matrix: Windows x64/ARM64, Linux x86-64, macOS ARM64.
- `--version` flag.
