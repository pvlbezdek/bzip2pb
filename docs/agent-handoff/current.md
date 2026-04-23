# Step 11 complete: libbzip2 Internal Optimizations

## What was done
- Compiled libbzip2 1.0.8 with explicit `-O3 -funroll-loops` / `/O2` flags.
- Replaced `BZ2_blockSort` (O(N²logN) Bentley-Sedgewick + fallback) with libsais
  v2.10.4 O(N) suffix-array construction on the doubled string T+T.
- Vendored libsais into `third_party/libsais/`.
- Bumped version 0.2.3 → 0.2.4.

## Key files changed
- `third_party/libsais/libsais.h` / `libsais.c` — vendored libsais 2.10.4 (new)
- `CMakeLists.txt` — libsais sources + include dir; optimization flags
- `third_party/bzip2/blocksort.c` — `BZ2_blockSort` rewritten
- `CHANGELOG.md` — `[0.2.4]` entry
- `docs/DEVELOPMENT_PLAN.md` — Step 11 section updated

## Decisions made
- Doubled-string T+T approach used to obtain cyclic SA from libsais (suffix SA).
- See `docs/agent-handoff/step-11-complete.md` for full rationale.

## Known issues / TODOs
- Extra malloc(~7 MB) per block sort — can be eliminated in a future step.
- CRC optimization deferred.

## What the next agent must know
- All 58 tests pass.  Version 0.2.4.  Branch `perf/11-libbzip2-optimizations`.
