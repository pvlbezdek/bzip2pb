# Step 11 complete: libbzip2 Internal Optimizations

## What was done

### 11.1 — Compiler optimization flags
Added explicit per-target optimization flags to `CMakeLists.txt` for the
`bzip2_vendored` static library:
- MSVC: `/O2` (already the Release default but now explicit and applies to all configs)
- GCC / Clang: `-O3 -funroll-loops`

### 11.2 — Replace BWT sort with libsais O(N) suffix-array construction
Vendored libsais v2.10.4 (Apache-2.0) into `third_party/libsais/` (two files).
Replaced bzip2's Bentley-Sedgewick `mainSort` / exponential-radix `fallbackSort`
in `third_party/bzip2/blocksort.c` with a call to `libsais()`.

**Algorithm detail (cyclic vs suffix SA):**
bzip2 requires a *cyclic* rotation sort; libsais computes a *suffix* sort.
These differ when suffix T[i..N-1] is a prefix of T[j..N-1].  The fix:
call `libsais()` on the doubled string T+T (length 2N), then filter the
SA to entries < N.  Suffix i < N of T+T starts with cyclic rotation i, so
the suffix order restricted to [0..N-1] equals the cyclic rotation order.

The second copy of T is written into `block[N..2N-1]` at the start of
`BZ2_blockSort`; arr2 is allocated as `(N+34)*4` bytes so `block[N..2N-1]`
is always within bounds.  A `malloc(2N × int32)` holds the temporary SA and
is freed before the function returns.  `compress.c` is unchanged.

## Key files changed
- `third_party/libsais/libsais.h` — vendored libsais 2.10.4 header (new)
- `third_party/libsais/libsais.c` — vendored libsais 2.10.4 source (new)
- `CMakeLists.txt` — libsais added to `bzip2_vendored`; optimization flags added
- `third_party/bzip2/blocksort.c` — `BZ2_blockSort` replaced with libsais doubled-string approach
- `CHANGELOG.md` — `[0.2.4]` entry added
- `docs/DEVELOPMENT_PLAN.md` — Step 11 section updated with actual implementation notes

## Decisions made
- Doubled-string T+T approach chosen over `libsais_bwt` (suffix BWT) because
  suffix BWT ≠ cyclic BWT → BZ_DATA_ERROR on decompression.
- `libsais_int` with sentinel (bytes shifted to [1..256], sentinel=0) was also
  tried and rejected; the sentinel doesn't simulate cyclic wrap-around correctly.
- `generateMTFValues` in `compress.c` left unchanged (reads `block[ptr[i]-1]`
  with wrap-around) — this correctly computes `BWT[i] = T[(SA[i]-1+N)%N]`.
- Peak extra memory per block sort: 2N bytes (second T copy in arr2) +
  8N bytes (SA malloc) ≈ 9 MB for a 900 KB block.  Freed before Huffman stage.
- libsais license: Apache-2.0, compatible with project's MIT license.

## Known issues / TODOs
- The malloc(2N int32) per block adds ~7 MB temporary allocation per thread.
  Future optimization: reuse arr1/arr2 for SA storage to eliminate the malloc.
- CRC computation (per-byte table lookup in `bzlib_private.h`) is deferred
  as noted in the analysis table.

## What the next agent must know
- All 58 Catch2 assertions pass on branch `perf/11-libbzip2-optimizations`.
- Version is 0.2.4.
- The PR should be merged to `main`.
- `third_party/libsais/` must remain in git (Apache-2.0 vendored, ~500 KB).
