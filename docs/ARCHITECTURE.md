# bzip2pb — Architecture

## Overview

bzip2pb is a parallel bzip2 compressor and decompressor. The core idea is the same in both directions: divide the work into independent chunks, process them concurrently on a thread pool, and reassemble the results in order.

---

## Thread pool

`src/thread_pool.h / .cpp` — a fixed-size pool of worker threads backed by a task queue. The number of threads defaults to `std::thread::hardware_concurrency()` and is overridden by `-n N`.

Tasks are submitted as `std::function<void()>` and return a `std::future<T>`. The main thread submits all tasks up front, then collects futures in submission order, which preserves output ordering without blocking the workers.

---

## Compression pipeline

```
stdin / file
     │
     ▼
┌──────────────────────────────────────────────┐
│  Read loop (main thread)                     │
│  Reads N × 100 KB chunks sequentially        │
└──────────────────────┬───────────────────────┘
                       │  N chunks
                       ▼
         ┌─────────────────────────────┐
         │       Thread pool           │
         │  Each worker calls          │
         │  BZ2_bzBuffToBuffCompress() │
         │  → a complete bzip2 stream  │
         └──────────┬──────────────────┘
                    │  N compressed streams (futures, in order)
                    ▼
┌──────────────────────────────────────────────┐
│  Write loop (main thread)                    │
│  Collects futures in submission order        │
│  Writes each compressed stream directly      │
│  (no header or size prefix)                  │
└──────────────────────────────────────────────┘
     │
     ▼
stdout / file.bz2
```

**Key property:** Each chunk is compressed into a *complete, self-contained bzip2 stream* (stream header + block + EOS marker). The output file is the concatenation of these streams. The bzip2 format explicitly permits concatenated streams, so any standard decompressor handles the output correctly.

**Block size** — controlled by `-1` through `-9`; each level N uses chunks of N × 100 KB. The final chunk may be smaller.

---

## Decompression pipeline

Parallel decompression of a bzip2 file is harder than compression because the block boundaries are not stored at a known byte offset — they must be discovered by scanning the bit stream.

```
stdin / file.bz2
     │
     ▼
┌──────────────────────────────────────────────┐
│  Load entire file into memory                │
│  (required for bit-level random access)      │
└──────────────────────┬───────────────────────┘
                       │
                       ▼
┌──────────────────────────────────────────────┐
│  Bit-level block scanner                     │
│  scan_bzip2_blocks()  src/bitscanner.cpp     │
│                                              │
│  Scans for 48-bit magic patterns:            │
│    block magic  0x314159265359               │
│    EOS magic    0x177245385090               │
│                                              │
│  Returns a sorted list of (bit_offset, type) │
└──────────────────────┬───────────────────────┘
                       │  K block boundaries
                       ▼
┌──────────────────────────────────────────────┐
│  Sub-stream reconstruction                   │
│  reconstruct_substream()  src/bitstream.cpp  │
│                                              │
│  For each pair of adjacent boundaries:       │
│  1. Prepend "BZh" + block_size_digit         │
│  2. Copy bits [start, end) from source       │
│  3. Append EOS magic + zero CRC + padding    │
└──────────────────────┬───────────────────────┘
                       │  K reconstructed sub-streams
                       ▼
         ┌─────────────────────────────┐
         │       Thread pool           │
         │  Each worker calls          │
         │  BZ2_bzBuffToBuffDecompress │
         └──────────┬──────────────────┘
                    │  K decompressed chunks (futures, in order)
                    ▼
┌──────────────────────────────────────────────┐
│  Write loop (main thread)                    │
│  Collects futures in submission order        │
│  Writes decompressed data                    │
└──────────────────────────────────────────────┘
     │
     ▼
stdout / file
```

**Memory model** — the entire compressed file is read into RAM before decompression begins. This is required because blocks can start at any bit offset, making seek-and-read impractical. This matches lbzip2's approach. For files larger than available RAM, a chunked approach would be needed (not yet implemented).

---

## Bit-level block scanner

The core of the decompression pipeline. See also [FORMAT.md](FORMAT.md) for the bzip2 block format.

### Algorithm

The scanner loads 8 bytes at a time as a big-endian `uint64_t`, then tests all 8 possible bit shifts (0–7) within that byte by left-shifting and masking to 48 bits:

```
word = bytes[b..b+7] as big-endian uint64
for d in 0..7:
    candidate = (word << d) & 0xFFFFFFFFFFFF0000
    if candidate == 0x3141592653590000:  → block magic at bit (b*8 + d)
    if candidate == 0x1772453850900000:  → EOS magic at bit (b*8 + d)
```

This performs 16 comparisons per input byte. For a 100 MB file this is approximately 800 million comparisons, completing in roughly 0.2 s on a modern CPU.

The scanner starts scanning at byte offset 4 (after the 4-byte stream header) to avoid spurious matches in the header bytes.

After scanning, results are sorted by bit offset and deduplicated (adjacent sliding windows can report the same match twice).

### Why bit offsets matter

In a bzip2 file, blocks are NOT byte-aligned. The block data is a continuous bit stream; the first block starts at bit 32 (right after the 4-byte stream header), but subsequent blocks end wherever the Huffman coding of the previous block ends — which is almost never on a byte boundary. The scanner must therefore operate at bit granularity.

---

## Source map

| File | Purpose |
|------|---------|
| `src/main.cpp` | Entry point; file/stdin dispatch, verbose stats |
| `src/cli.h/.cpp` | Argument parsing; prints usage/version/license |
| `src/common.h` | `Options` struct shared across all modules |
| `src/compress.h/.cpp` | Parallel compression via thread pool |
| `src/decompress.h/.cpp` | Parallel decompression; orchestrates scanner + pool |
| `src/bitscanner.h/.cpp` | Bit-level magic scanner |
| `src/bitstream.h/.cpp` | Sub-stream reconstruction (bit copy + re-framing) |
| `src/thread_pool.h/.cpp` | Fixed-size worker thread pool |
| `src/version.h.in` | Version/platform template; filled by CMake |
