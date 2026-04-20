# Agent Handoff — Current State

**Status:** Step 2 complete (PR open). Ready for Step 3.

## Repository state

- Branch: `main` (Step 2 PR pending merge)
- Last merged step: Step 2 — Standard bzip2 Format + Bit-Level Block Scanner
- CI: GitHub Actions matrix (4 targets, will test Linux/macOS cross-tool compat)
- Build: works locally on Windows x64
- Tests: none yet (Step 4)
- Format: **standard bzip2 — output decompressible by standard bzip2 tools**

## What exists

| File                      | Notes                                                             |
|---------------------------|-------------------------------------------------------------------|
| `src/common.h`            | Options struct only (custom format constants removed)             |
| `src/thread_pool.h/cpp`   | Generic C++17 thread pool — keep as-is                           |
| `src/compress.h/cpp`      | Parallel compression — outputs standard multi-stream bzip2        |
| `src/decompress.h/cpp`    | Parallel decompression — bit scanner + sub-stream reconstruction  |
| `src/bitscanner.h/cpp`    | 64-bit word scanner for block/EOS magic                          |
| `src/bitstream.h/cpp`     | Sub-stream reconstructor (includes correct combined CRC)          |
| `src/main.cpp`            | CLI: -c/-d/-k/-f/-p/-b/-v/-V/-1..-9/--version/--help            |
| `src/version.h.in`        | Version/platform/arch template                                    |
| `CMakeLists.txt`          | Cross-platform, v0.2.0, -Werror on Linux/macOS                   |
| `CMakePresets.json`       | Local + 4 CI presets                                              |
| `.github/workflows/ci.yml`| 4-target CI matrix                                               |
| `vcpkg.json`              | bzip2 dependency with baseline                                    |

## Next agent: Step 3 — lbzip2-Compatible CLI

See `docs/DEVELOPMENT_PLAN.md` Step 3 for full instructions.

**Key tasks:**
1. Rename `-p` to `-n` for thread count (keep `-p` as hidden alias for backwards compat)
2. Add `-z` / `--compress` to explicitly select compression
3. Add `-t` / `--test` mode (decompress to null sink)
4. Make `-c` / `--stdout` write to stdout and imply `-k`
5. Add `-q` / `--quiet` and `-L` / `--license` flags
6. Match lbzip2 verbose output format exactly
7. Add exit codes: 0=success, 1=warning, 2=error
8. Handle `--` end-of-options, `-` as stdin/stdout filename
9. Handle `.tbz2`/`.tbz` → `.tar` extension for decompress
10. Extract CLI parser to `src/cli.h` + `src/cli.cpp`

**Caution:** Do NOT change bitscanner, bitstream, compress, or decompress logic.
All new code must compile warning-free (MSVC /WX and GCC/Clang -Werror).

**Branch to create:** `step/03-lbzip2-cli`

**Local build:**
```powershell
$env:VCPKG_INSTALLATION_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
cmake --preset windows-release
cmake --build build --preset release
```
