# bzip2pb

Parallel bzip2 compressor and decompressor. Reads and writes standard `.bz2` files; fully compatible with `bzip2`, `lbzip2`, and `pbzip2`.

[![CI](https://github.com/pvlbezdek/bzip2pb/actions/workflows/ci.yml/badge.svg)](https://github.com/pvlbezdek/bzip2pb/actions/workflows/ci.yml)
[![Conda Build](https://github.com/pvlbezdek/bzip2pb/actions/workflows/conda.yml/badge.svg)](https://github.com/pvlbezdek/bzip2pb/actions/workflows/conda.yml)

## Features

- **Parallel compression** — splits input into independent blocks and compresses them simultaneously across all available CPU cores
- **Parallel decompression** — uses a bit-level scanner to locate block boundaries in any standard `.bz2` file, then decompresses blocks in parallel
- **Standard `.bz2` output** — output is identical in format to `bzip2`; any tool that reads `.bz2` files can decompress bzip2pb output
- **lbzip2-compatible CLI** — flags, output naming, verbose format, and exit codes match lbzip2
- **Cross-platform** — Windows x64, Windows ARM64 (cross-compile), Linux x86_64, macOS ARM64
- **Conda package** — install with a single `conda install` command

## Quick start

```sh
# Compress a file (creates file.bz2, removes file)
bzip2pb file.txt

# Decompress
bzip2pb -d file.txt.bz2

# Round-trip on stdin/stdout
echo "hello world" | bzip2pb | bzip2pb -d

# Use 4 threads, level 6
bzip2pb -n 4 -6 file.txt
```

## Installation

### Conda (recommended)

```sh
conda install -c pvlbezdek bzip2pb
```

### Build from source

#### Prerequisites

| Platform | Toolchain | Required |
|----------|-----------|---------|
| Windows  | MSVC (Visual Studio 2022+) | CMake ≥ 3.20, vcpkg |
| Linux    | GCC ≥ 11 or Clang ≥ 14 | CMake ≥ 3.20, vcpkg |
| macOS    | Apple Clang (Xcode 14+) | CMake ≥ 3.20, vcpkg |

Set the `VCPKG_INSTALLATION_ROOT` environment variable to your vcpkg installation directory before configuring.

#### Windows x64

```bat
cmake --preset windows-release
cmake --build build --preset release
```

The binary is at `build\Release\bzip2pb.exe`.

#### Linux / macOS

```sh
cmake --preset ci-ubuntu-latest-x64    # or ci-macos-latest-arm64
cmake --build build --config Release
```

The binary is at `build/bzip2pb`.

#### Windows ARM64 (cross-compile)

```bat
cmake --preset ci-windows-latest-ARM64
cmake --build build --config Release
```

The ARM64 binary can only be tested on an ARM64 Windows machine; the build step alone validates that compilation succeeds.

## Usage

```
Usage: bzip2pb [options] [file...]

Options:
  -z, --compress      Force compress (default)
  -d, --decompress    Decompress
  -t, --test          Test integrity; no output
  -c, --stdout        Write to stdout, keep input
  -k, --keep          Keep input file(s)
  -f, --force         Overwrite existing output
  -n N, --parallel N  Worker threads (default: all cores)
  -1 .. -9            Block size 1–9 × 100 KB (default: 9)
  -v, --verbose       Verbose output
  -q, --quiet         Suppress warnings
  -L, --license       Print license and exit
  -V, --version       Print version and exit
      --help          Show this help
```

### Output naming

| Mode        | Input          | Output          |
|-------------|----------------|-----------------|
| compress    | `file`         | `file.bz2`      |
| compress    | stdin (`-`)    | stdout          |
| decompress  | `file.bz2`     | `file`          |
| decompress  | `file.tbz2`    | `file.tar`      |
| decompress  | `file.tbz`     | `file.tar`      |
| decompress  | other          | `other.out`     |
| decompress  | stdin (`-`)    | stdout          |

### Exit codes

| Code | Meaning |
|------|---------|
| 0    | Success |
| 1    | Warning (e.g. output already exists, no `-f`) |
| 2    | Error (corrupt data, I/O failure, bad option) |

### Examples

```sh
# Compress all .log files in place
bzip2pb *.log

# Decompress to stdout without deleting the input
bzip2pb -d -c archive.bz2 > output.txt

# Test integrity of every .bz2 in a directory
bzip2pb -t *.bz2

# Pipe: compress with bzip2pb, decompress with system bzip2
bzip2pb -c data.bin | bzip2 -d > data_copy.bin

# Pipe: compress with system bzip2, decompress with bzip2pb
bzip2 -c data.bin | bzip2pb -d -c - > data_copy.bin

# Verbose mode — shows ratio and byte counts
bzip2pb -v -9 large_file.csv
#   large_file.csv:  3.241:1,  2.468 bits/byte, 69.14% saved, 52428800 in, 16178432 out.
```

## Performance

bzip2pb matches or exceeds lbzip2 compression speed and scales linearly with thread count up to the point where I/O becomes the bottleneck. See [docs/BENCHMARKS.md](docs/BENCHMARKS.md) for methodology and reference numbers.

## Compatibility

bzip2pb reads and writes **standard bzip2 streams** as defined by Julian Seward's bzip2 1.0.x. Specifically:

- **Output** is a sequence of concatenated bzip2 streams — one per input block. This is valid per the bzip2 specification; `bzip2 -d`, `lbzip2 -d`, and `pbzip2 -d` all handle concatenated streams correctly.
- **Input** can be any `.bz2` file regardless of which tool created it, including multi-stream files produced by parallel compressors.
- Block boundaries are located via a bit-level scanner rather than by trusting byte alignment, so bzip2pb correctly handles files where blocks don't fall on byte boundaries (which is the general case).

## Building tests

```sh
cmake --preset ci-ubuntu-latest-x64   # configure with Catch2 available
cmake --build build --target check
# or equivalently:
ctest --test-dir build -C Release --output-on-failure
```

Tests are skipped automatically when Catch2 is not installed or when cross-compiling for ARM64 (the test binary cannot run on the host).

## Running benchmarks

```sh
cmake --preset ci-ubuntu-latest-x64
cmake --build build --target benchmark
# Results printed to stdout and written to build/benchmark_results.csv
```

See [docs/BENCHMARKS.md](docs/BENCHMARKS.md) for benchmark corpus details and how to interpret results.

## Platform notes

### Windows ARM64

Cross-compiled on a Windows x64 host using `cmake --preset ci-windows-latest-ARM64`. Runtime testing on the CI runner is not possible because no native ARM64 Windows runner is available; the CI step validates compilation only. The binary works correctly on physical ARM64 Windows hardware.

### macOS universal binary

Pass `-DBZIP2PB_UNIVERSAL=ON` to CMake to produce a fat binary containing both `x86_64` and `arm64` slices:

```sh
cmake -B build -DBZIP2PB_UNIVERSAL=ON ...
```

Note: vcpkg does not produce universal libbz2 by default. You may need to build libbz2 for both architectures separately and `lipo`-merge them before configuring.

### Linux static binary

Pass `-DBZIP2PB_STATIC=ON` to produce a portable binary with no shared-library dependencies:

```sh
cmake -B build -DBZIP2PB_STATIC=ON ...
```

## Architecture

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the threading model, compression and decompression pipelines, and the bit-level block scanner algorithm.

## License

MIT — see [LICENSE](LICENSE).
