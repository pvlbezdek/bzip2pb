# Step 1 complete: Cross-Platform Build & CI

## What was done
- Bumped CMake minimum to 3.20 and project version to 0.2.0
- Added platform/arch detection in CMakeLists.txt; generates `src/version.h` into build tree
- Added per-platform compile flags: MSVC gets `/W4 /WX /O2 /EHsc`; Linux/macOS get `-Wall -Wextra -Werror -O2`
- Added macOS universal binary option (`-DBZIP2PB_UNIVERSAL=ON`)
- Added Linux static link option (`-DBZIP2PB_STATIC=ON`)
- Replaced hardcoded VS18 vcpkg path in `CMakePresets.json` with `$env{VCPKG_INSTALLATION_ROOT}`
- Added hidden `vcpkg-base` preset; all other presets inherit from it
- Added four CI presets: `ci-windows-latest-x64`, `ci-windows-latest-ARM64`, `ci-ubuntu-latest-x64`, `ci-macos-latest-arm64`
- Created `src/version.h.in` with version, platform, and arch macros
- Added `--version` / `-V` flag to `main.cpp` — prints `bzip2pb 0.2.0 (Windows x64)`
- Created `.github/workflows/ci.yml` with 4-target matrix

## Key files changed
- `CMakeLists.txt` — cross-platform flags, version.h generation, install target
- `CMakePresets.json` — replaced hardcoded path; added CI presets
- `src/version.h.in` — new: version/platform/arch macros for @ONLY substitution
- `src/main.cpp` — added `#include "version.h"`, `--version`/`-V` handling
- `.github/workflows/ci.yml` — new: 4-target CI matrix

## Decisions made
- `VCPKG_INSTALLATION_ROOT` used for toolchain path — this env var is pre-set on all GitHub-hosted runners; local Windows developers must set it manually (e.g., `C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg`)
- CI Windows presets use `Visual Studio 17 2022` (GitHub `windows-latest` runner has VS 2022); local presets keep `Visual Studio 18 2026`
- ARM64 CI job is marked `cross_compile: true` — smoke test is skipped because no ARM64 runner is available; the build step validates compilation only
- `-Werror` added for Linux/macOS; if CI surfaces warnings from existing code they must be fixed in Step 2 or a hotfix commit on this branch

## Known issues / TODOs
- vcpkg manifest mode causes cmake configure to download/build bzip2 from source on CI, which is slow (~2–3 min). Add caching in a later step.
- Windows ARM64 runtime cannot be tested in CI (documented limitation, matches lbzip2 approach)
- Linux/macOS CI not yet verified locally; `-Werror` may reveal minor GCC/Clang warnings in existing source

## What the next agent must know
- Branch `step/02-standard-format` starts from `main` after this PR is merged
- Step 2 removes the custom `BZP\x02` format from `compress.cpp` / `decompress.cpp` and `common.h`
- Step 2 introduces `src/bitscanner.h/.cpp` and `src/bitstream.h/.cpp` — see `docs/DEVELOPMENT_PLAN.md` Step 2 for full spec
- Do NOT break the `-Werror` / `/WX` flags added in Step 1; new code must compile warning-free on all platforms
- The `bzip2pb_lib` static library refactor (needed for Step 4 testing) is NOT in Step 2 — it's in Step 4
