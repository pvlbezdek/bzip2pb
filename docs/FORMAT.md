# bzip2 File Format

This document describes the bzip2 file format as relevant to bzip2pb's parallel decompression implementation. The authoritative source is Julian Seward's bzip2 source code (bzlib.c).

---

## Stream structure

A bzip2 file is a sequence of one or more concatenated streams. Each stream is:

```
"BZh"              3 bytes  — stream magic
'1'–'9'            1 byte   — block size digit (block size = digit × 100 KB)
[Block]+           variable — one or more compressed blocks
EOS marker        10 bytes  — end-of-stream (see below)
```

**Concatenated streams** — the bzip2 format explicitly permits multiple streams concatenated without any separator. `bzip2 -d` and all parallel variants handle this correctly. bzip2pb's compressor produces exactly one stream per input block, so the output of compressing a 1 MB file at level 9 (900 KB blocks) is two concatenated streams.

---

## Block format (bit stream)

Blocks are NOT byte-aligned. The block data is a continuous bit stream within the file. A block starts immediately after the preceding block (or after the 4-byte stream header for the first block in a stream) at whatever bit offset the previous data ends.

```
0x314159265359      48 bits  — block magic (first 6 significant digits of π)
block_crc           32 bits  — CRC32 of the uncompressed block data
randomised           1 bit   — always 0 in modern bzip2
origPtr             24 bits  — BWT origin pointer
symMap             varies    — Huffman symbol map
selectors          varies    — Huffman tree selector array
trees              varies    — Huffman trees (2–6 trees)
data               varies    — Huffman-coded + MTF + RLE data
```

The block data encodes: RLE1 expansion → BWT → MTF → RLE2 → Huffman. Decompression reverses these stages. bzip2pb delegates all of this to `BZ2_bzBuffToBuffDecompress`.

---

## End-of-stream (EOS) marker

The EOS marker immediately follows the last block in a stream:

```
0x177245385090      48 bits  — EOS magic (first 6 significant digits of √π)
combined_crc        32 bits  — combined CRC32 of all blocks in the stream
[0–7 padding bits]           — zero bits to pad to byte boundary
```

**Combined CRC** — bzip2pb sets this to zero when constructing sub-streams for parallel decompression. libbzip2's `BZ2_bzBuffToBuffDecompress` does not validate the combined CRC when `small=0`; it only validates the per-block CRC embedded in each block header.

---

## Block boundary scanning

### Why scanning is required

Because block boundaries fall at arbitrary bit positions, there is no index or offset table to consult. The only way to find where block N ends and block N+1 begins is to scan the bit stream for the 48-bit block magic `0x314159265359`.

### Worked example

```
File bytes (hex):

  Offset  0:  42 5A 68 39   → "BZh9" stream header (32 bits)
  Offset  4:  31 41 59 26 53 59 ...  → block magic starts here at bit 32
```

The block magic `0x314159265359` at byte offset 4 = bit offset 32. This is the common case (first block of first stream).

For a file with two concatenated streams, the second stream header starts immediately after the EOS marker of the first stream, which ends at an arbitrary bit position. The second block magic therefore appears at a non-multiple-of-8 bit offset.

Example where the second block magic is shifted by 3 bits (starts at bit 35 from its stream header):

```
Bytes at stream-2 header + 4:
  06 28 2B 24 CA B2 ...
  ^--- top 3 bits are tail of the EOS marker padding
  bits 3–50: 0x314159265359 (block magic)
```

The scanner loads 8 bytes as a `uint64_t` and tests all 8 shifts, catching the magic at any bit offset.

### Scanner complexity

- Operations per input byte: 16 comparisons (8 bit offsets × 2 magic values)
- 100 MB file: ~800 million comparisons
- Typical time: ~0.2 s on a modern CPU (purely sequential; not parallelised)

---

## Sub-stream reconstruction

To decompress block N in parallel, bzip2pb extracts the block bits and wraps them in a minimal valid bzip2 stream:

1. Write `"BZh"` + `block_size_digit` (copied from the original stream header)
2. Copy bits `[block_start_bit, next_boundary_bit)` from the source buffer — this includes the block magic, block CRC, and all compressed data up to (but not including) the next block or EOS magic
3. Write EOS magic `0x177245385090` as 48 bits
4. Write combined CRC = `0x00000000` as 32 bits
5. Pad to byte boundary with zero bits

The reconstructed stream is a complete, valid bzip2 stream containing exactly one block. libbzip2 decompresses it and verifies the per-block CRC embedded in step 2. The zero combined CRC in step 4 is accepted without error.

---

## Comparison with bzip2pb v0.1 custom format

Earlier versions of bzip2pb used a proprietary format:

```
"BZP\x02"          4 bytes  — custom magic
num_blocks         4 bytes  — block count (little-endian)
[block_size, data] varies   — each block preceded by its size
```

This format was not compatible with any other tool. Version 0.2.0 removed it entirely in favour of the standard bzip2 format described above.
