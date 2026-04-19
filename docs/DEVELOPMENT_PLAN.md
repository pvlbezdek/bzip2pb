# bzip2pb — Development Plan

Each step runs in an isolated branch, concludes with a pull request to `main`,
and leaves a handoff file in `docs/agent-handoff/` for the next agent.

---

## Quick Reference

| Step | Branch                         | Focus                                    |
|------|--------------------------------|------------------------------------------|
| 1    | `step/01-cross-platform-build` | CMake, CI matrix, packaging skeleton     |
| 2    | `step/02-standard-format`      | Standard .bz2 output + bit-level scanner |
| 3    | `step/03-lbzip2-cli`           | Full lbzip2-compatible CLI               |
| 4    | `step/04-testing`              | Catch2 suite, CTest target               |
| 5    | `step/05-benchmarks`           | Speed/ratio benchmarks vs bzip2 family   |
| 6    | `step/06-conda`                | Conda recipe, multi-platform build       |
| 7    | `step/07-documentation`        | README, architecture docs, man page      |

---

## Agent Instructions (all steps)

1. Read `docs/agent-handoff/current.md` before starting work.
2. Create your branch from `main` (`git checkout -b step/0N-...`).
3. Commit at logical checkpoints (not just at the end).
4. When done, write `docs/agent-handoff/step-0N-complete.md` (see template below).
5. Copy it over `docs/agent-handoff/current.md`.
6. Open a PR to `main`; PR title must match the step name exactly.
7. Do **not** merge — leave the merge for the project owner.

### Handoff file template

```markdown
# Step N complete: <title>

## What was done
- Bullet list of changes

## Key files changed
- path/to/file — reason

## Decisions made
- Decision → rationale

## Known issues / TODOs
- Any deferred work

## What the next agent must know
- Critical context for step N+1
```

---

## Step 1 — Cross-Platform Build & CI

**Branch:** `step/01-cross-platform-build`
**Reads:** nothing (first step)
**Writes:** `docs/agent-handoff/step-01-complete.md`

### Goals

1. CMake works on all four targets without code changes:
   - Windows x64 (MSVC)
   - Windows ARM64 (MSVC cross-compile)
   - Linux x86_64 (GCC ≥ 11 and Clang ≥ 14)
   - macOS ARM64 (Apple Clang)

2. GitHub Actions CI runs on every push/PR:
   - Matrix: `[windows-latest, ubuntu-latest, macos-latest]`
   - Windows ARM64: cross-compiled on `windows-latest` (no native runner available)
   - Each job: configure → build → smoke test (`bzip2pb --version`)

3. vcpkg integration is clean and reproducible for all platforms.

4. A `--version` flag prints `bzip2pb <version> (<platform> <arch>)`.

### Detailed tasks

#### CMakeLists.txt improvements

```cmake
# Add at top level
cmake_minimum_required(VERSION 3.20)
project(bzip2pb VERSION 0.2.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# Platform detection
include(CheckCXXCompilerFlag)

# Architecture-specific flags
if(MSVC)
    # ARM64 cross-compile: set via -DCMAKE_GENERATOR_PLATFORM=ARM64
    target_compile_options(bzip2pb PRIVATE /W4 /WX /O2 /EHsc)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64")
        target_compile_definitions(bzip2pb PRIVATE BZIP2PB_ARCH_ARM64)
    endif()
elseif(APPLE)
    target_compile_options(bzip2pb PRIVATE -Wall -Wextra -Werror -O2)
    # Universal binary option (x86_64;arm64)
    option(BZIP2PB_UNIVERSAL "Build universal binary (macOS)" OFF)
    if(BZIP2PB_UNIVERSAL)
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
    endif()
else()  # Linux
    target_compile_options(bzip2pb PRIVATE -Wall -Wextra -Werror -O2)
    # Static linking option for portable Linux binary
    option(BZIP2PB_STATIC "Link statically" OFF)
    if(BZIP2PB_STATIC)
        target_link_options(bzip2pb PRIVATE -static-libgcc -static-libstdc++)
    endif()
endif()

# Version info embedded in binary
configure_file(src/version.h.in src/version.h @ONLY)
```

#### Windows ARM64

Cross-compile on `windows-latest` runner:
```yaml
- name: Configure (ARM64)
  run: cmake -B build-arm64 -G "Visual Studio 17 2022" -A ARM64
       -DCMAKE_TOOLCHAIN_FILE=...vcpkg.cmake
       -DVCPKG_TARGET_TRIPLET=arm64-windows
```
Note: runtime testing of the ARM64 binary in CI is not possible; the build step
validates compilation only. Document this limitation clearly.

#### GitHub Actions matrix

File: `.github/workflows/ci.yml`

```yaml
name: CI
on: [push, pull_request]

jobs:
  build:
    strategy:
      matrix:
        include:
          - os: windows-latest
            triplet: x64-windows
            arch: x64
          - os: windows-latest
            triplet: arm64-windows
            arch: ARM64
            cross_compile: true
          - os: ubuntu-latest
            triplet: x64-linux
            arch: x64
          - os: macos-latest
            triplet: arm64-osx
            arch: arm64
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - name: Install vcpkg bzip2
        run: vcpkg install bzip2 --triplet=${{ matrix.triplet }}
      - name: Configure
        run: cmake --preset ci-${{ matrix.os }}-${{ matrix.arch }}
      - name: Build
        run: cmake --build build --config Release
      - name: Smoke test
        if: ${{ !matrix.cross_compile }}
        run: ./build/Release/bzip2pb --version
```

#### CMakePresets.json additions

Add CI presets for each platform (no hardcoded local paths — use env vars for
vcpkg root).

#### Files to create/modify

- `CMakeLists.txt` — refactor as above
- `CMakePresets.json` — add CI presets
- `src/version.h.in` — version template
- `.github/workflows/ci.yml` — CI matrix
- `vcpkg.json` — ensure `builtin-baseline` is set

### Success criteria

- `cmake --preset windows-release && cmake --build build --preset release` works
  locally on Windows.
- All four matrix targets compile without warnings (`-Werror` / `/WX`).
- `bzip2pb --version` exits 0 on all non-cross-compile targets.

---

## Step 2 — Standard bzip2 Format + Bit-Level Block Scanner

**Branch:** `step/02-standard-format`
**Reads:** `docs/agent-handoff/step-01-complete.md`
**Writes:** `docs/agent-handoff/step-02-complete.md`

This is the most technically complex step. Take time to understand the bzip2
file format before writing code.

### bzip2 file format primer

```
Stream:
  "BZh"                        3 bytes  — stream magic
  '1'–'9'                      1 byte   — block size digit (100KB units)
  [Block]+                     variable — one or more compressed blocks
  End-of-stream marker         10 bytes (see below)

Block (NOT byte-aligned — this is a bit-stream):
  0x314159265359               48 bits  — block magic (digits of π)
  block_crc                    32 bits  — CRC32 of uncompressed data
  randomised                    1 bit   — always 0 in modern bzip2
  origPtr                      24 bits  — BWT starting pointer
  [BWT + MTF + RLE + Huffman data]       — variable length

End-of-stream marker (NOT byte-aligned):
  0x177245385090               48 bits  — EOS magic (digits of √π)
  combined_crc                 32 bits
  [0–7 padding bits]                    — pad to byte boundary
```

**Key fact:** Block and EOS boundaries are bit-aligned, not byte-aligned.
This means a block can start at ANY bit position within the file.

### Part A — Standard bzip2 output (compression side)

Replace the current custom `BZP\x02` format with standard bzip2 output.

**Strategy:** Compress each input chunk independently into a complete bzip2
stream (what we do now). Concatenate the resulting streams. The result is a
valid bzip2 file (the standard allows multiple concatenated streams; standard
`bzip2 -d` handles them correctly).

This is a **small change** to `compress.cpp`:
- Remove the custom header write (`BZIP2PB_MAGIC`, `num_blocks`, etc.)
- Write each compressed block directly without size prefix
- Each `BZ2_bzBuffToBuffCompress` output is already a complete bzip2 stream

```cpp
// compress.cpp — new do_compress() core loop
for (auto& blk : compressed_blocks) {
    // blk is already a complete bzip2 stream; write directly
    out.write(reinterpret_cast<const char*>(blk.data()), blk.size());
}
// No header needed
```

**Verification:** `bzip2pb -c file | bzip2 -d` must reproduce `file`.

### Part B — Bit-level block boundary scanner (decompression side)

This enables parallel decompression of ANY standard .bz2 file.

#### Algorithm overview

```
1. Read entire compressed file into memory buffer.
2. Parse stream header ("BZh" + digit) → record block_size_digit.
3. Scan the buffer bit-by-bit for the 48-bit block magic 0x314159265359.
   - Also watch for EOS magic 0x177245385090.
   - Record each match as (byte_offset, bit_offset) pair.
4. Result: ordered list of block start positions in bits.
5. For each block i:
   a. Extract bits from block_start[i] to block_start[i+1] (or EOS).
   b. Reconstruct a valid bzip2 stream:
      - Prepend "BZh" + digit (from original stream header)
      - Append the block's bits
      - Append EOS marker (0x177245385090) + CRC(0) + padding
   c. Submit to thread pool: BZ2_bzBuffToBuffDecompress(reconstructed)
6. Collect results in order, write to output.
```

#### Bit scanner implementation

```cpp
// src/bitscanner.h — core data structure
struct BlockBoundary {
    size_t bit_offset;   // bit position in file (0 = MSB of first byte)
    bool   is_eos;       // true if this is an end-of-stream marker
};

// Scan buffer for bzip2 block/EOS magic
// Returns sorted list of boundaries
std::vector<BlockBoundary> scan_bzip2_blocks(
    const uint8_t* data, size_t size_bytes);
```

```cpp
// Efficient 64-bit word scanner
// Load 8 bytes into a uint64_t, extract 6-byte window at each bit offset
std::vector<BlockBoundary> scan_bzip2_blocks(
    const uint8_t* data, size_t size_bytes)
{
    static constexpr uint64_t BLOCK_MAGIC = 0x3141592653590000ULL;
    static constexpr uint64_t EOS_MAGIC   = 0x1772453850900000ULL;
    static constexpr uint64_t MASK48      = 0xFFFFFFFFFFFF0000ULL;

    std::vector<BlockBoundary> result;
    // Skip 4-byte stream header ("BZh" + digit)
    size_t start_byte = 4;

    for (size_t b = start_byte; b + 8 <= size_bytes; ++b) {
        // Load 8 bytes as big-endian uint64
        uint64_t word = 0;
        for (int i = 0; i < 8; ++i)
            word = (word << 8) | data[b + i];

        // Check all 8 bit offsets within this byte
        for (int d = 0; d < 8; ++d) {
            uint64_t shifted = (word << d) & MASK48;
            if (shifted == BLOCK_MAGIC)
                result.push_back({ (b * 8) + d, false });
            else if (shifted == EOS_MAGIC)
                result.push_back({ (b * 8) + d, true });
        }
    }
    // Sort and deduplicate (overlapping windows can double-report)
    std::sort(result.begin(), result.end(),
              [](auto& a, auto& b){ return a.bit_offset < b.bit_offset; });
    result.erase(std::unique(result.begin(), result.end(),
                             [](auto& a, auto& b){
                                 return a.bit_offset == b.bit_offset; }),
                 result.end());
    return result;
}
```

#### Sub-stream reconstruction

```cpp
// src/bitstream.h — bit-level copy utility
// Copies 'n_bits' starting at bit offset 'src_bit' from 'src'
// into a new byte vector, prepended with the bzip2 stream header
// and appended with the EOS marker + zero CRC + padding.
std::vector<uint8_t> reconstruct_substream(
    const uint8_t* src,
    size_t src_size_bytes,
    size_t block_start_bit,    // bit offset of block magic in src
    size_t next_boundary_bit,  // bit offset of next magic (or file end)
    char block_size_digit);    // '1'–'9' from original stream header
```

The reconstruction:
1. Write `"BZh"` + `block_size_digit`.
2. Copy bits `[block_start_bit, next_boundary_bit)` from `src`.
3. Write EOS magic `0x177245385090` as bits.
4. Write 32-bit combined CRC = 0 (libbzip2 will compute during decompress
   and validate against the per-block CRC embedded in the block itself).
5. Pad to byte boundary with 0 bits.

**Important:** The per-block CRC (`block_crc` field, 32 bits after the block
magic) is INSIDE the block bits and will be verified by libbzip2. We only need
to append a placeholder combined-stream CRC; setting it to 0 is accepted by
libbzip2 when `small=0, verbosity=0`.

#### Updated decompress flow

```cpp
void do_decompress(std::istream& in, std::ostream& out, const Options& opts) {
    // Read entire file into memory (needed for bit scanning)
    std::vector<uint8_t> compressed(
        std::istreambuf_iterator<char>(in), {});

    // Validate stream header
    if (compressed.size() < 4 ||
        compressed[0] != 'B' || compressed[1] != 'Z' || compressed[2] != 'h' ||
        compressed[3] < '1'  || compressed[3] > '9')
        throw std::runtime_error("Not a valid bzip2 file");

    char digit = static_cast<char>(compressed[3]);

    // Find block boundaries
    auto boundaries = scan_bzip2_blocks(compressed.data(), compressed.size());
    if (boundaries.empty())
        throw std::runtime_error("No bzip2 blocks found");

    // Reconstruct and decompress each block in parallel (sliding window)
    ...
}
```

#### Memory considerations

Loading the entire compressed file into RAM is required for bit-level random
access. For files larger than available RAM, a chunked approach would be needed
(future work). Document this limitation clearly; it matches lbzip2's behaviour.

### Part C — Remove old custom format

Delete all references to `BZIP2PB_MAGIC`, `BZIP2PB_VERSION`, and the custom
header read/write code. The new tool reads and writes only standard bzip2.

### Files to create/modify

- `src/bitscanner.h` + `src/bitscanner.cpp` — bit-level magic scanner
- `src/bitstream.h` + `src/bitstream.cpp` — sub-stream reconstructor
- `src/compress.cpp` — remove custom header, output raw bzip2 streams
- `src/decompress.cpp` — replace header parse with bit scanner
- `src/common.h` — remove old magic/version constants

### Success criteria

- `bzip2pb file && bzip2 -d file.bz2` reproduces `file`.
- `bzip2 file && bzip2pb -d file.bz2` reproduces `file`.
- `bzip2pb file && bzip2pb -d file.bz2` reproduces `file`.
- All three round-trips work for files at each compression level 1–9.
- All three round-trips work for: empty file, 1-byte file, 1MB, 100MB.

---

## Step 3 — lbzip2-Compatible CLI

**Branch:** `step/03-lbzip2-cli`
**Reads:** `docs/agent-handoff/step-02-complete.md`
**Writes:** `docs/agent-handoff/step-03-complete.md`

### lbzip2 parameter set to implement

| Flag              | Long form          | Description                                      |
|-------------------|--------------------|--------------------------------------------------|
| `-z`              | `--compress`       | Force compress (default)                         |
| `-d`              | `--decompress`     | Decompress                                       |
| `-t`              | `--test`           | Test: decompress to /dev/null, verify CRC        |
| `-c`              | `--stdout`         | Write to stdout, keep input                      |
| `-k`              | `--keep`           | Keep input files (do not delete after success)   |
| `-f`              | `--force`          | Overwrite existing output; compress non-regular  |
| `-n N`            | `--parallel N`     | Use N worker threads (default: all cores)        |
| `-1` … `-9`       |                    | Compression level / block size (100KB–900KB)     |
| `-v`              | `--verbose`        | Print filename, compression ratio, time          |
| `-q`              | `--quiet`          | Suppress warnings                                |
| `-L`              | `--license`        | Print license and exit                           |
| `-V`              | `--version`        | Print version and exit                           |
|                   | `--help`           | Print usage and exit                             |
| `--`              |                    | End of options; remaining args are filenames     |
| `-`               |                    | Filename meaning stdin/stdout                    |

**Differences from current CLI:**
- `-n` instead of `-p` for thread count (lbzip2 convention)
- `-z` explicitly selects compression
- `-t` test mode (decompress to null, verify integrity)
- `-c` writes to stdout without deleting input (implies `-k`)
- `-L` license output
- `-V` / `--version` standardised
- `-v` outputs: `filename: N.NNN:1, N bytes, N.NN bits/byte, N.N% saved`
- `--` end-of-options marker
- `-` as filename = stdin (compress) / stdin (decompress)
- Multiple input files processed in sequence

### Verbose output format

Match lbzip2's exact format for scripting compatibility:

```
  filename.txt:  1.523:1,  6.564 bits/byte, 34.34% saved, 102400 in, 67234 out.
```

### Exit codes

| Code | Meaning                                      |
|------|----------------------------------------------|
| 0    | Success                                      |
| 1    | Warning (e.g., input file not regular)       |
| 2    | Error (e.g., corrupt data, I/O failure)      |

### `-t` test mode

```cpp
// Decompress to a null sink — verifies CRC without writing output
class NullSink : public std::streambuf {
    std::streamsize xsputn(const char*, std::streamsize n) override { return n; }
    int overflow(int c) override { return c; }
};
```

### Output filename convention

Match bzip2/lbzip2:

| Mode        | Input          | Output              |
|-------------|----------------|---------------------|
| compress    | `file`         | `file.bz2`          |
| compress    | stdin (`-`)    | stdout              |
| decompress  | `file.bz2`     | `file`              |
| decompress  | `file.tbz2`    | `file.tar`          |
| decompress  | `file.tbz`     | `file.tar`          |
| decompress  | `file` (other) | `file.out`          |
| decompress  | stdin (`-`)    | stdout              |

### Refactor `main.cpp`

Current argument parser is adequate but needs extending. Consider splitting into:
- `src/cli.h` + `src/cli.cpp` — argument parsing, returns `Options`
- `src/main.cpp` — just calls parse + dispatch

### Files to create/modify

- `src/main.cpp` — extend CLI, add exit codes, verbose format
- `src/cli.h` + `src/cli.cpp` — extracted argument parser
- `src/common.h` — extend `Options` struct
- `src/compress.cpp` — verbose output, timing
- `src/decompress.cpp` — test mode, verbose output

### Success criteria

- `bzip2pb -h` output matches lbzip2's help format closely.
- `bzip2pb -t file.bz2` exits 0 for valid file, 2 for corrupt.
- `bzip2pb -v file` prints compression statistics.
- `bzip2pb -c file | bzip2pb -d -c -` reproduces file on stdout.
- `echo test | bzip2pb | bzip2pb -d` round-trips on all platforms.

---

## Step 4 — Testing Suite

**Branch:** `step/04-testing`
**Reads:** `docs/agent-handoff/step-03-complete.md`
**Writes:** `docs/agent-handoff/step-04-complete.md`

### Framework

Use **Catch2 v3** (header-only mode via vcpkg: `catch2`).
Add to `vcpkg.json`:
```json
"dependencies": ["bzip2", "catch2"]
```

### CMake integration

```cmake
find_package(Catch2 3 REQUIRED)
include(CTest)
include(Catch)

add_executable(bzip2pb_tests
    tests/unit/test_bitscanner.cpp
    tests/unit/test_compress.cpp
    tests/unit/test_decompress.cpp
    tests/unit/test_cli.cpp
    tests/integration/test_roundtrip.cpp
    tests/integration/test_compatibility.cpp
    tests/integration/test_edge_cases.cpp
)
target_link_libraries(bzip2pb_tests PRIVATE
    bzip2pb_lib   # extract library target from main executable
    Catch2::Catch2WithMain
    BZip2::BZip2
)
catch_discover_tests(bzip2pb_tests)
```

**Important:** Refactor `bzip2pb` sources into a `bzip2pb_lib` static library
target linked by both the `bzip2pb` executable and `bzip2pb_tests`. This avoids
recompiling sources and makes `main()` testable.

### Test corpus generation

Generate all test data deterministically at test time (no large files committed):

```cpp
// tests/testdata.h
namespace TestData {
    // Deterministic pseudo-random binary data (seeded)
    std::vector<uint8_t> random_binary(size_t bytes, uint32_t seed = 42);
    // Highly compressible (repeated pattern)
    std::vector<uint8_t> compressible(size_t bytes);
    // Already compressed (low entropy)
    std::vector<uint8_t> incompressible(size_t bytes);
    // Text-like (printable ASCII with word patterns)
    std::vector<uint8_t> text_like(size_t bytes);
}
```

### Unit tests

#### `tests/unit/test_bitscanner.cpp`

```cpp
TEST_CASE("scan_bzip2_blocks finds block in minimal bzip2 stream") {
    // Create a real bzip2 stream using BZ2_bzBuffToBuffCompress
    // Verify scan finds exactly one block magic at expected bit offset
}

TEST_CASE("scan_bzip2_blocks finds multiple blocks in concatenated streams") {
    // Compress two independent chunks, concatenate
    // Verify two block magics are found
}

TEST_CASE("reconstruct_substream produces decompressible output") {
    // Take a known bzip2 stream, scan for block,
    // reconstruct sub-stream, verify BZ2_bzBuffToBuffDecompress succeeds
}

TEST_CASE("bit scanner handles files not ending on byte boundary") {
    // Edge case: file where EOS marker + CRC requires non-zero padding
}
```

#### `tests/unit/test_compress.cpp`

```cpp
TEST_CASE("compress_block produces valid bzip2 stream") {
    // BZ2_bzBuffToBuffDecompress succeeds on output of compress_block()
}
TEST_CASE("compress_block level 1 through 9") { ... }
TEST_CASE("compress_block empty input returns empty stream") { ... }
TEST_CASE("compress_block single byte input") { ... }
```

#### `tests/unit/test_decompress.cpp`

```cpp
TEST_CASE("decompress_block exact inverse of compress_block") { ... }
TEST_CASE("decompress returns correct data for each level 1-9") { ... }
```

#### `tests/unit/test_cli.cpp`

```cpp
TEST_CASE("parse_args: -n sets thread count") { ... }
TEST_CASE("parse_args: -1 through -9 set block level") { ... }
TEST_CASE("parse_args: -- stops option parsing") { ... }
TEST_CASE("parse_args: unknown option returns error") { ... }
TEST_CASE("parse_args: -c implies -k") { ... }
```

### Integration tests

#### `tests/integration/test_roundtrip.cpp`

```cpp
// Round-trip: bzip2pb compress → bzip2pb decompress
TEST_CASE("roundtrip: random binary 0 bytes")   { ... }
TEST_CASE("roundtrip: random binary 1 byte")    { ... }
TEST_CASE("roundtrip: random binary 1 KB")      { ... }
TEST_CASE("roundtrip: random binary 1 MB")      { ... }
TEST_CASE("roundtrip: random binary 10 MB")     { ... }
TEST_CASE("roundtrip: exactly one block")       { ... }
TEST_CASE("roundtrip: exactly two blocks")      { ... }
TEST_CASE("roundtrip: compressible data")       { ... }
TEST_CASE("roundtrip: incompressible data")     { ... }
TEST_CASE("roundtrip: all levels 1-9")          { ... }
TEST_CASE("roundtrip: 1 thread")                { ... }
TEST_CASE("roundtrip: 4 threads")               { ... }
TEST_CASE("roundtrip: max threads")             { ... }
```

#### `tests/integration/test_compatibility.cpp`

```cpp
// Cross-tool compatibility using libbzip2 API directly
TEST_CASE("bzip2pb output decompressible by libbzip2") {
    // Compress with bzip2pb, decompress with BZ2_bzBuffToBuffDecompress
}
TEST_CASE("libbzip2 output decompressible by bzip2pb") {
    // Compress with BZ2_bzBuffToBuffCompress, decompress with bzip2pb
}
TEST_CASE("multi-stream file decompresses correctly") {
    // Manually concatenate two bzip2 streams, decompress with bzip2pb
}
```

#### `tests/integration/test_edge_cases.cpp`

```cpp
TEST_CASE("decompress corrupt file throws") { ... }
TEST_CASE("decompress truncated file throws") { ... }
TEST_CASE("decompress empty file succeeds with empty output") { ... }
TEST_CASE("-t test mode: valid file exits 0") { ... }
TEST_CASE("-t test mode: corrupt file exits 2") { ... }
TEST_CASE("compress then decompress preserves exact bytes") { ... }
```

### CMake targets

```cmake
# Run all tests
add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>
    DEPENDS bzip2pb_tests
)
# Alias for make test
enable_testing()
```

Users run: `cmake --build build --target check`
Or: `ctest --test-dir build -C Release --output-on-failure`

### CI integration

Add test step to `.github/workflows/ci.yml`:
```yaml
- name: Test
  if: ${{ !matrix.cross_compile }}
  run: ctest --test-dir build -C Release --output-on-failure
```

### Files to create/modify

- `tests/` directory (new)
- `tests/unit/test_bitscanner.cpp`
- `tests/unit/test_compress.cpp`
- `tests/unit/test_decompress.cpp`
- `tests/unit/test_cli.cpp`
- `tests/integration/test_roundtrip.cpp`
- `tests/integration/test_compatibility.cpp`
- `tests/integration/test_edge_cases.cpp`
- `tests/testdata.h`
- `CMakeLists.txt` — add `bzip2pb_lib` target, test targets
- `vcpkg.json` — add `catch2`

### Success criteria

- `cmake --build build --target check` passes on all platforms.
- No test failures on any of the four CI targets.
- Code coverage ≥ 80% for `src/bitscanner.cpp` and `src/compress.cpp`
  (measured with gcov/llvm-cov on Linux; report in handoff).

---

## Step 5 — Benchmarking

**Branch:** `step/05-benchmarks`
**Reads:** `docs/agent-handoff/step-04-complete.md`
**Writes:** `docs/agent-handoff/step-05-complete.md`

### Goals

1. Measure and report: compression speed (MB/s), decompression speed (MB/s),
   compression ratio, wall-clock time.
2. Thread-scaling analysis: 1, 2, 4, 8, N threads.
3. Compare against bzip2 (via libbzip2 API — always available) and optionally
   pbzip2/lbzip2 if found on `PATH`.
4. Output: human-readable table + machine-readable CSV.
5. Dedicated CMake target: `cmake --build build --target benchmark`.

### Corpus

Generated at benchmark time (not committed), with configurable seed:

| Name          | Size    | Type                              |
|---------------|---------|-----------------------------------|
| `text_1m`     | 1 MB    | Text-like (high compressibility)  |
| `binary_1m`   | 1 MB    | Random binary (low compressibility)|
| `text_10m`    | 10 MB   | Text-like                         |
| `binary_10m`  | 10 MB   | Random binary                     |
| `text_100m`   | 100 MB  | Text-like                         |
| `binary_100m` | 100 MB  | Random binary                     |

### Benchmark harness

```cpp
// benchmarks/bench.cpp
struct BenchResult {
    std::string tool;
    std::string corpus;
    int         threads;
    int         level;
    double      compress_mb_s;
    double      decompress_mb_s;
    double      ratio;           // compressed_size / original_size
    double      compress_ms;
    double      decompress_ms;
};

// Run bzip2pb at each thread count using internal API (not subprocess)
BenchResult bench_bzip2pb(const std::string& corpus_path, int threads, int level);

// Run libbzip2 single-threaded via BZ2_bzBuffToBuffCompress/Decompress
BenchResult bench_libbzip2(const std::string& corpus_path, int level);

// Run external tool (pbzip2/lbzip2) via subprocess if found on PATH
// Returns std::nullopt if tool not available
std::optional<BenchResult> bench_external(
    const std::string& tool,    // "pbzip2" or "lbzip2"
    const std::string& corpus_path,
    int threads, int level);
```

### Thread-scaling measurement

```
Threads:   1     2     4     8    16    auto
Speed:   120   230   440   820  1510  1620 MB/s
```

### Output format

Terminal output:
```
bzip2pb benchmark — text_10m (10.0 MB)
═══════════════════════════════════════════════════════════════════════
Tool          Threads  Level  Compr.Speed  Decomp.Speed  Ratio
───────────────────────────────────────────────────────────────────────
bzip2pb            1      9     112 MB/s      310 MB/s   0.312
bzip2pb            4      9     430 MB/s     1180 MB/s   0.312
bzip2pb           16      9    1480 MB/s     3900 MB/s   0.312
bzip2 (lib)        1      9     108 MB/s      290 MB/s   0.311
pbzip2             4      9     410 MB/s     1050 MB/s   0.312  [found]
lbzip2             4      9     420 MB/s     1100 MB/s   0.312  [found]
═══════════════════════════════════════════════════════════════════════
```

CSV output (`benchmark_results.csv`):
```
tool,corpus,threads,level,compress_mb_s,decompress_mb_s,ratio,compress_ms,decompress_ms
bzip2pb,text_10m,1,9,112.4,309.8,0.312,88.9,32.3
...
```

### CMake target

```cmake
add_executable(bzip2pb_bench benchmarks/bench.cpp)
target_link_libraries(bzip2pb_bench PRIVATE bzip2pb_lib BZip2::BZip2)

add_custom_target(benchmark
    COMMAND bzip2pb_bench --csv benchmark_results.csv
    DEPENDS bzip2pb_bench
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
```

### Files to create

- `benchmarks/bench.cpp` — benchmark harness
- `benchmarks/bench.h` — types and declarations
- `benchmarks/corpus.h` — corpus generation (reuses `tests/testdata.h`)
- `CMakeLists.txt` — add `bzip2pb_bench` + `benchmark` target

### Success criteria

- `cmake --build build --target benchmark` runs without errors on all platforms.
- Output includes bzip2pb results and libbzip2 baseline on all platforms.
- External tools section prints `[not found]` gracefully when absent.
- CSV is written to `build/benchmark_results.csv`.

---

## Step 6 — Conda Package

**Branch:** `step/06-conda`
**Reads:** `docs/agent-handoff/step-05-complete.md`
**Writes:** `docs/agent-handoff/step-06-complete.md`

### Goal

Users can install the binary with:
```
conda install -c <channel> bzip2pb
```

### Structure

```
conda/
├── meta.yaml        — conda recipe
├── build.sh         — Linux/macOS build script
└── bld.bat          — Windows build script
```

### `conda/meta.yaml`

```yaml
{% set version = "0.2.0" %}

package:
  name: bzip2pb
  version: {{ version }}

source:
  path: ..

build:
  number: 0
  # No Python dependency — pure C++ binary
  # Compatible with any Python >= 3.8 environment
  run_constrained:
    - python >=3.8

requirements:
  build:
    - {{ compiler('cxx') }}
    - cmake >=3.20
    - ninja
  host:
    - bzip2

test:
  commands:
    - bzip2pb --version
    - echo "hello world" | bzip2pb | bzip2pb -d

about:
  home: https://github.com/pvlbezdek/bzip2pb
  license: MIT
  license_file: LICENSE
  summary: Parallel bzip2 compressor and decompressor
  description: |
    bzip2pb compresses and decompresses files using the bzip2 algorithm
    with multi-threaded parallelism. Output is fully compatible with
    standard bzip2 tools.

extra:
  recipe-maintainers:
    - pvlbezdek
```

### `conda/build.sh` (Linux / macOS)

```bash
#!/bin/bash
set -ex
cmake -B build -S . \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$PREFIX \
    -DBZIP2_INCLUDE_DIR=$PREFIX/include \
    -DBZIP2_LIBRARIES=$PREFIX/lib/libbz2.a
cmake --build build
cmake --install build
```

### `conda/bld.bat` (Windows)

```bat
cmake -B build -S . ^
    -G "Ninja" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX="%LIBRARY_PREFIX%" ^
    -DBZIP2_INCLUDE_DIR="%LIBRARY_INC%" ^
    -DBZIP2_LIBRARIES="%LIBRARY_LIB%\bz2.lib"
cmake --build build
cmake --install build
```

### CMakeLists.txt install target

Ensure the install target is correct:
```cmake
install(TARGETS bzip2pb
    RUNTIME DESTINATION bin           # Windows/Linux/macOS binary
    BUNDLE  DESTINATION bin           # macOS .app bundles (if applicable)
)
```

### GitHub Actions workflow for conda build

File: `.github/workflows/conda.yml`

```yaml
name: Conda Build
on:
  push:
    tags: ['v*']
  workflow_dispatch:

jobs:
  conda-build:
    strategy:
      matrix:
        include:
          - os: ubuntu-latest
            platform: linux-64
          - os: macos-latest
            platform: osx-arm64
          - os: windows-latest
            platform: win-64
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - uses: conda-incubator/setup-miniconda@v3
        with:
          auto-activate-base: true
          python-version: "3.10"
      - name: Install conda-build
        run: conda install conda-build
      - name: Build
        run: conda build conda/
      - name: Upload artifact
        uses: actions/upload-artifact@v4
        with:
          name: conda-pkg-${{ matrix.platform }}
          path: ${{ env.CONDA_PREFIX }}/conda-bld/**/*.tar.bz2
```

### Files to create/modify

- `conda/meta.yaml`
- `conda/build.sh`
- `conda/bld.bat`
- `CMakeLists.txt` — verify `install()` target
- `.github/workflows/conda.yml`
- `LICENSE` — add MIT license file if not present

### Success criteria

- `conda build conda/` succeeds on Linux, macOS, and Windows.
- `conda install --use-local bzip2pb` works and `bzip2pb --version` prints
  the version.
- CI conda workflow produces artifacts for all three platforms.

---

## Step 7 — README & Documentation

**Branch:** `step/07-documentation`
**Reads:** `docs/agent-handoff/step-06-complete.md`
**Writes:** `docs/agent-handoff/step-07-complete.md` (final)

### Files to create

#### `README.md` (root)

Sections:
1. **Header** — name, one-line description, CI badge, conda badge
2. **Features** — bullet list (parallel, standard .bz2 compatible, lbzip2 CLI,
   all platforms, conda)
3. **Quick start** — 5 lines showing install + basic compress/decompress
4. **Installation**
   - Conda (recommended): `conda install -c <channel> bzip2pb`
   - Build from source: prerequisites per platform, step-by-step commands
     for Windows, Linux, macOS
5. **Usage** — full option table matching lbzip2's man page style; examples for
   every major mode (compress, decompress, test, stdout, multiple files)
6. **Performance** — brief comparison table (link to `docs/BENCHMARKS.md`)
7. **Compatibility** — what .bz2 files it can read/write; notes on standard
   bzip2 interop
8. **Building tests** — `cmake --build build --target check`
9. **Running benchmarks** — `cmake --build build --target benchmark`
10. **Platform notes** — Windows ARM64 cross-compile caveat; macOS universal
    binary option
11. **License**

#### `docs/ARCHITECTURE.md`

- Threading model diagram (ASCII)
- Compression pipeline: read → thread pool → write
- Decompression pipeline: read → bit scan → thread pool → write
- File format note: "outputs standard bzip2 (concatenated streams)"
- Bit-level scanner: algorithm explanation with diagram
- Memory model: entire compressed file in RAM for decompression

#### `docs/FORMAT.md`

- Complete bzip2 format specification (stream header, block header, EOS)
- Block boundary scanning algorithm with worked example
- Sub-stream reconstruction procedure
- Comparison with custom bzip2pb v0.1 format (historical note)

#### `docs/BENCHMARKS.md`

- Methodology
- Hardware used for reference measurements
- Result tables and scaling graphs (ASCII or linked images)
- Instructions for reproducing

### Success criteria

- `README.md` renders correctly on GitHub.
- All build commands in README are tested and work.
- CI badge in README shows green.
- No broken links.

---

## Appendix A — Technical Reference

### bzip2 block magic scanner — worked example

```
File bytes (hex): 42 5A 68 39  [stream header "BZh9"]
                  31 41 59 26 53 59 ...  [block starts here at bit 32]
```

The block magic `0x314159265359` at byte offset 4 = bit offset 32.

If the block were shifted by 3 bits (starts at bit 35):
```
Bytes at offset 4: 06 28 2B 24 CA B2 ...
                   ^^^ first 3 bits are tail of previous block
                   remaining bits: 0x314159265359...
```

The scanner loads 8 bytes as a `uint64_t`, left-shifts by 0–7, masks to 48 bits,
and compares. Total operations per byte: 16 comparisons (8 offsets × 2 magics).
For a 100MB file: ~800M comparisons, completing in ~0.2s on a modern CPU.

### Platform-specific notes

#### Windows ARM64

- Cross-compile only; no native CI runner available.
- Use `cmake -G "Visual Studio 18 2026" -A ARM64`.
- The bzip2 library must be built for arm64-windows triplet in vcpkg.
- `BZ2_bzBuffToBuffCompress` is not CPU-architecture-specific and works
  correctly on ARM64.

#### macOS Universal Binary

- Enable with `-DBZIP2PB_UNIVERSAL=ON`.
- Requires both `x86_64` and `arm64` slices of libbzip2.
- vcpkg does not natively produce universal binaries; build libbzip2
  separately for each arch and `lipo` them, or use Homebrew's universal build.

#### Linux Static Binary

- Enable with `-DBZIP2PB_STATIC=ON`.
- Produces a portable binary with no shared library dependencies.
- Useful for distribution without package managers.

---

## Appendix B — Dependency Matrix

| Dependency | Version   | Usage                     | Provided via     |
|------------|-----------|---------------------------|------------------|
| libbzip2   | ≥ 1.0.6   | Core compression           | vcpkg / conda    |
| Catch2     | 3.x       | Testing (dev only)         | vcpkg            |
| CMake      | ≥ 3.20    | Build system               | VS / system      |
| Python     | ≥ 3.8     | Conda env compatibility    | conda            |
| ninja      | any       | Fast builds (optional)     | conda-build      |

---

*Last updated: Step 0 (plan creation). Each agent updates the relevant step
section with actual outcomes.*
