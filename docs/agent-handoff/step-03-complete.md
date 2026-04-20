# Step 3 complete: lbzip2-Compatible CLI

## What was done

- Extracted argument parser into `src/cli.h` + `src/cli.cpp` (`ParsedArgs parse_args()`)
- Renamed thread-count flag from `-p` to `-n` / `--parallel`; kept `-p` as hidden backwards-compat alias
- Changed `-c` from "compress" to `--stdout` (write to stdout, implies `-k`); `-z` / `--compress` now explicitly selects compression
- Added `-t` / `--test`: decompress to null sink, verify CRC, exit 0 = OK / 2 = corrupt
- Added `-q` / `--quiet`: suppresses all warnings and error messages
- Added `-L` / `--license`: prints MIT license text and exits
- Implemented exit codes: 0 = success, 1 = warning (e.g. output exists), 2 = error (I/O or corruption)
- Added `--` end-of-options marker and `-` as stdin/stdout filename
- Added `.tbz2` → `.tar` and `.tbz` → `.tar` extension handling on decompress
- Implemented lbzip2-style verbose output: `name:  R:1,  B bits/byte, S% saved, N in, N out.`
- Added `compress_file_stdout()` to `compress.cpp`
- Added `decompress_file_stdout()`, `decompress_file_test()`, `decompress_stdin_test()` to `decompress.cpp`
- Removed debug verbose prints from `do_compress` / `do_decompress` (now handled in `main.cpp`)
- All changes compile warning-free under MSVC /WX (verified locally on Windows x64)

## Key files changed

- `src/common.h` — added `quiet`, `test`, `to_stdout` fields to `Options`
- `src/cli.h` — new file: `ParsedArgs` struct, `parse_args()` declaration
- `src/cli.cpp` — new file: full argument parser with all lbzip2-compatible flags
- `src/main.cpp` — rewritten: uses `cli.h`, lbzip2 verbose stats, proper exit codes, all new modes
- `src/compress.h` / `src/compress.cpp` — added `compress_file_stdout()`
- `src/decompress.h` / `src/decompress.cpp` — added stdout/test/stdin-test variants + `NullSink`/`NullOStream`
- `CMakeLists.txt` — added `src/cli.cpp` to source list

## Decisions made

- `-c` means `--stdout` (lbzip2 convention), not `--compress` as in the old code — **breaking change from v0.1 CLI**
- Verbose stats are computed in `main.cpp` using `std::filesystem::file_size` before/after the operation; stdout/test modes skip the ratio line (no output file to measure)
- `NullSink` / `NullOStream` live in `decompress.cpp` (only consumers of test mode)
- exit(2) for unknown options/bad values via `parse_args`; `main()` itself returns 1 for soft warnings and 2 for exceptions

## Known issues / TODOs

- Verbose stats not printed for `-c` (stdout) or `-t` (test) modes — only per-file file-to-file mode gets ratio output; this matches Step 3 scope
- Stdin/stdout verbose stats not supported (no file to measure before/after)
- No formal tests yet (Step 4)

## What the next agent must know

- CLI is now lbzip2-compatible; all new flags are implemented and tested
- The old `-c` flag (meaning compress) is gone; scripts using `-c` for compress must switch to `-z`
- `bzip2pb_lib` static library target (needed by Step 4's test suite) is not yet extracted — Step 4 must refactor `CMakeLists.txt` to split sources into a library target
- Build command (Windows): set `VCPKG_INSTALLATION_ROOT` then `cmake --preset windows-release && cmake --build build --preset release`
- All four CI matrix targets still expected to compile warning-free
