# Step 9 complete: Parallel Sub-Stream Reconstruction

## What was done
- Moved `reconstruct_substream` from the main dispatch thread into each pool
  task so reconstruction and decompression execute concurrently across all
  worker threads.
- The source `compressed` buffer is `const` and read-only after loading; no
  locks or atomics are needed.

## Key files changed
- `src/decompress.cpp` — `reconstruct_substream` call moved inside pool lambda;
  raw pointer + size captured instead of a pre-built `std::vector`
- `CMakeLists.txt` — version bumped 0.2.1 → 0.2.2
- `CHANGELOG.md` — `[0.2.2]` entry added
- `docs/DEVELOPMENT_PLAN.md` — Step 9 row added to quick-reference table;
  Step 9 section appended and marked ✓ COMPLETE

## Decisions made
- Raw pointer capture (`const uint8_t*` + `size_t`) preferred over
  `std::shared_ptr<std::vector>` — avoids ref-count overhead; safe because
  `compressed` outlives all futures (drained before it goes out of scope).
- No output-buffer pre-allocation in this step — kept scope minimal.

## Known issues / TODOs
- Binary decompression (auto threads) still ~25–35% behind lbzip2; the
  remaining gap is likely inside libbzip2's own BZ2 decode path.
- Text compression gap (5–7× vs lbzip2) requires BWT backend replacement
  (libsais/divsufsort) — deferred.
- Output assembly still copies each block's result vector into the stream;
  pre-allocating a single output buffer and writing directly to computed offsets
  could remove one memcpy pass.

## What the next agent must know
- All existing Catch2 assertions pass unchanged after this change.
- `CHANGELOG.md` keeps a `[0.2.2]` entry at the top under Keep-a-Changelog
  format.
- Branch is `perf/09-parallel-reconstruction`; open a PR to `main`.
