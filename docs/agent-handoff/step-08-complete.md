# Step 8 complete: Decompression Throughput & Scanner Speed

## What was done
- Replaced bit-by-bit block-bit copy in `reconstruct_substream` with a
  byte-level barrel-shift copy (`copy_bits_byte_aligned`), eliminating the
  serial O(n_bits) bottleneck that prevented decompression from scaling with
  thread count.
- Added `initial_bit_pos` parameter to `BitWriter` so EOS-magic and CRC writes
  resume correctly after the bulk copy leaves a partial last byte.
- Replaced the 8-byte word-rebuild inner loop in `scan_bzip2_blocks` with a
  sliding window (1 memory read per step instead of 8).
- Added `CHANGELOG.md` (root) documenting all changes since project start.
- Added Step 8 entry to `docs/DEVELOPMENT_PLAN.md` and marked it complete.

## Key files changed
- `src/bitstream.cpp` — `copy_bits_byte_aligned` replaces O(n_bits) bit loop
- `src/bitscanner.cpp` — sliding window for the 64-bit word
- `docs/DEVELOPMENT_PLAN.md` — Step 8 entry added and marked ✓ COMPLETE
- `CHANGELOG.md` — created; documents all steps 0.1.1 → current

## Decisions made
- Byte-level (not 64-bit-word-level) copy chosen for simplicity and safety;
  the `b+1` look-ahead is always within bounds for non-tail bytes.
- `BitWriter::initial_bit_pos` defaults to 0 so all existing call sites are
  unchanged.
- Compression speed gap vs lbzip2 intentionally deferred — requires replacing
  libbz2's BWT sort and is a larger scope change.

## Known issues / TODOs
- **Text compression** is still 5–7× behind lbzip2 due to libbz2's
  Bentley-Sedgewick BWT sort degrading on repetitive input.  Future work:
  integrate `libsais` or `divsufsort` as the BWT backend.
- Decompression improvement not yet re-benchmarked on target hardware
  (M1 Pro macOS); expected ~100 MB/s at 4 threads for binary data.

## What the next agent must know
- All 175 Catch2 assertions still pass after the changes.
- `CHANGELOG.md` uses Keep-a-Changelog format; new entries go under
  `[Unreleased]` until a version tag is cut.
- The `perf/08-decompression-reconstruction` branch is the working branch;
  a PR to `main` is open but not yet merged.
