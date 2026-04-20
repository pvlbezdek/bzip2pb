# Agent Handoff — Current State

**Status:** Step 1 complete (PR open). Ready for Step 2.

## Repository state

- Branch: `main` (Step 1 PR pending merge)
- Last merged step: Step 1 — Cross-Platform Build & CI
- CI: GitHub Actions matrix configured (4 targets)
- Build: works locally on Windows x64 (VCPKG_INSTALLATION_ROOT must be set)
- Tests: none yet
- Format: **still uses custom `BZP\x02` format — Step 2 changes this**

## What exists

| File                      | Notes                                                    |
|---------------------------|----------------------------------------------------------|
| `src/common.h`            | Options struct + custom format constants (remove in Step 2) |
| `src/thread_pool.h/cpp`   | Generic C++17 thread pool — keep as-is                  |
| `src/compress.h/cpp`      | Parallel compression — outputs custom format             |
| `src/decompress.h/cpp`    | Parallel decompression — reads custom format             |
| `src/main.cpp`            | CLI with --version/-V added                              |
| `src/version.h.in`        | Version/platform/arch template; cmake generates version.h |
| `CMakeLists.txt`          | Cross-platform, v0.2.0, -Werror on Linux/macOS           |
| `CMakePresets.json`       | vcpkg-base + local + 4 CI presets (env-var toolchain)    |
| `.github/workflows/ci.yml`| 4-target CI matrix                                       |
| `vcpkg.json`              | bzip2 dependency with baseline                           |

## Next agent: Step 2 — Standard bzip2 Format + Bit-Level Block Scanner

See `docs/DEVELOPMENT_PLAN.md` Step 2 for full instructions.

**Key tasks:**
1. Remove custom `BZP\x02` header from `compress.cpp`; write raw concatenated bzip2 streams
2. Remove custom header read from `decompress.cpp`; replace with bit-level block scanner
3. Create `src/bitscanner.h` + `src/bitscanner.cpp` — 64-bit word scanner for block/EOS magic
4. Create `src/bitstream.h` + `src/bitstream.cpp` — sub-stream reconstructor
5. Clean up `src/common.h` (remove `BZIP2PB_MAGIC`, `BZIP2PB_VERSION`, `DEFAULT_BLOCK_SIZE`)

**Caution:** Do NOT change CMake build, CI, or CLI flags in Step 2.
All new code must compile warning-free (both MSVC /WX and GCC/Clang -Werror).

**Branch to create:** `step/02-standard-format`

**Local build note:** Set `VCPKG_INSTALLATION_ROOT` before using the preset:
```
$env:VCPKG_INSTALLATION_ROOT = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
cmake --preset windows-release
cmake --build build --preset release
```
