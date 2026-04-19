# Agent Handoff — Current State

**Status:** Plan written, initial scaffold committed. Ready for Step 1.

## Repository state

- Branch: `main`
- Last commit: initial scaffold (thread pool, compress, decompress, main, CMake)
- CI: none yet
- Build: works locally on Windows x64 with vcpkg + MSVC (VS 18 2026)
- Tests: none
- Format: **currently uses custom `BZP\x02` format — Step 2 changes this**

## What exists

| File                  | Notes                                            |
|-----------------------|--------------------------------------------------|
| `src/common.h`        | Options struct + old custom format constants     |
| `src/thread_pool.h/cpp` | Generic C++17 thread pool — keep as-is        |
| `src/compress.h/cpp`  | Parallel compression — outputs custom format     |
| `src/decompress.h/cpp`| Parallel decompression — reads custom format     |
| `src/main.cpp`        | Basic CLI (-c/-d/-k/-f/-p/-b/-v/-1..-9)         |
| `CMakeLists.txt`      | Single-platform MSVC, vcpkg, no install target   |
| `CMakePresets.json`   | Windows-only presets with hardcoded VS18 path    |
| `vcpkg.json`          | bzip2 dependency with baseline                   |

## Next agent: Step 1 — Cross-Platform Build & CI

See `docs/DEVELOPMENT_PLAN.md` Step 1 for full instructions.

**Key tasks:**
1. Refactor CMakeLists.txt for Windows/Linux/macOS/ARM64
2. Replace hardcoded VS18 path in CMakePresets.json with env-var-based presets
3. Add GitHub Actions CI matrix (4 targets)
4. Add `src/version.h.in` and `--version` flag
5. Verify no warnings on Linux/macOS (add `-Werror`)

**Caution:** Do NOT change the compression/decompression logic in Step 1.
The format change happens in Step 2.

**Branch to create:** `step/01-cross-platform-build`
