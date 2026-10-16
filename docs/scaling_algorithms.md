# How the scaling works

The shared CPU scaler resizes a 32-bit image with nearest-neighbor or bilinear filtering. Each window starts with nearest. Call `mfb_set_stretch_mode(window, MFB_STRETCH_BILINEAR)` to select bilinear at run time, or pass `MFB_STRETCH_NEAREST` to switch back. Both filters are always available.

Backends use `stretch_image_cached` with a cache stored in the window. The `stretch_image` helper uses a temporary cache for one call. This page explains how both filters work and how the scaler keeps and reuses its tables.

A pixel is 32 bits: blue in the low byte, then green, red and alpha (the byte order of Windows, macOS, Wayland and MiniFB). The code assumes a little-endian processor, which every MiniFB target is. Below, a **texel** is a pixel of the source image, and a **pixel** is one of the destination.

## What one call does

1. Reuse or build the **column table**, when the filter needs one: for each destination column, which source column (or columns, and with which weight) it uses. The table depends on the source and destination widths and the filter.
2. Choose the **level** once for the whole call: scalar, SSE2, AVX2 or NEON, from the processor and the width of the row.
3. Go through the destination rows from top to bottom. For each row, find which source row (or rows, and with which weight) it uses, and fill it in the cheapest way that gives the same pixels.

## How much faster it is

Compared with the previous `stretch_image`, measured with window sizes from 320x240 to 3840x2160:

| Processor | Code | Nearest | Bilinear |
| --- | --- | --- | --- |
| x86-64, Intel from Haswell to Alder Lake (i7-4980HQ, i5-6300U, i5-10310U, i7-10850H, i7-1265U); GCC, MSVC, clang | SSE2, and AVX2 for bilinear | 1.0 to 4.7 times | 3 to 12 times |
| 32-bit x86 (i3 M 380, 32-bit Windows); MSVC | scalar | 1.1 to 4.7 times | 1.7 to 3.1 times |
| ARM, Android (Cortex-X1, A78, A73, A55 and A53; 32 and 64 bits); clang | scalar, and NEON for bilinear | 1.1 to 7.7 times | 1.6 to 15 times |
| Apple M1 Pro, macOS; Apple clang | scalar, and NEON for bilinear | 1.8 to 5.6 times | 4.7 to 13 times |

The gains are largest with integer factors in nearest (x2, x3, x4) and when enlarging in bilinear. They are smallest when reducing, and at 4K, where most of the time goes into writing the image to memory. On ARM, nearest uses the scalar code, which the compiler turns in part into NEON on its own; bilinear has its own NEON code. Two cases were slower than before: nearest when reducing with clang on Alder Lake (by 1 or 2 %), and nearest on a Cortex-A73 running 32-bit code, with a viewport.

## Where each pixel samples

Both filters sample the source under the **center** of each destination pixel, as a GPU does. For destination column `x`, the center is at

```cpp
sample(x) = (x + 0.5) * src_width / dst_width - 0.5        // in source texels
```

- **Nearest** takes the texel that contains the center: `floor((2 * x + 1) * src_width / (2 * dst_width))`.
- **Bilinear** mixes the two texels around `sample(x)`, and clamps `sample(x)` to the first and the last texel, as `GL_CLAMP_TO_EDGE` does in OpenGL.

Rows work in the same way, with the heights.

Sampling at the center keeps the image in place. With an integer factor every texel is repeated exactly that many times: enlarged by 3, texel 0 gives pixels 0, 1 and 2, texel 1 gives 3, 4 and 5, and so on. The previous code sampled at the left edge of each pixel with a truncated fixed-point step, and could give texel 0 four times and move the whole image one pixel.

![Enlarging 3 texels to 9 pixels, sampling at the center and at the left edge](scaling_algorithms/sampling.svg)

### Exact positions with integers: the DDA

A fixed-point step such as 16.16 is not exact: the step is truncated, the error adds up along the row, and near the edge of a texel the sample takes the wrong one. Instead, the code computes the exact division column by column with two integers, a quotient and a remainder. This is a DDA (digital differential analyzer), the same idea as Bresenham's line algorithm.

For nearest, the texel of column `x` is `(2 * x + 1) * src_width` divided by `2 * dst_width`. From one column to the next, the dividend grows by `2 * src_width`, so the code only adds:

```cpp
// Once per call: column 0, and the step from one column to the next, each divided by the divisor.
divisor        = 2 * dst_width;
index          = src_width / divisor;
remainder      = src_width % divisor;
step_index     = (2 * src_width) / divisor;
step_remainder = (2 * src_width) % divisor;

// Once per column: add the step, and move one texel on when the remainder reaches the divisor.
index     += step_index;
remainder += step_remainder;
if (remainder >= divisor) {
    remainder -= divisor;
    ++index;
}
```

For example, with `src_width = 3` and `dst_width = 5`, the divisor is `2 * 5 = 10`, and the columns get the texels 0, 0, 1, 2, 2. That is exactly `floor((2 * x + 1) * 3 / (2 * 5))` for `x` from 0 to 4. There is no rounding error and no division per column, and in nearest every value fits in 32 bits for sizes below 2^30. Bilinear computes `sample(x)` in the same way, in 1/256 of a texel, with 64-bit values; it only does so once per column and once per row, so the cost does not matter.

![The DDA for 3 texels and 5 pixels](scaling_algorithms/dda.svg)

### Tables

Every row uses the same source columns, so the column positions are computed once and kept in a table:

- Nearest: one source index per destination column.
- Bilinear: the two source columns and the horizontal weight of each destination column.

Reading the table is cheaper than stepping an offset in every row, so being exact costs nothing per pixel. The table and the memory for the bilinear row buffers are kept in the window's cache between calls. The allocation only grows when more space is needed.

Changing either width or the filter invalidates the column table. Switching filters must rebuild it even at the same size: nearest stores one source index per column, while bilinear stores two indices and a weight. The allocation is kept when the filter changes, and the bilinear row buffers are filled again from the current source image on each call.

If the memory cannot be allocated, `stretch_image` still draws: it computes the columns pixel by pixel instead of reading a table, which is slower but gives the same pixels, and it logs a warning once.

## Nearest

For each destination row, in this order:

1. If the row uses the same source row as the row above, it is a copy of that row (`memcpy`). Enlarged by 4, three of every four rows are copies.
2. Otherwise, if the destination is exactly 2, 3 or 4 times as wide as the source, each texel is written that many times, without the table.
3. Otherwise, each pixel reads the texel that the table gives. The loop goes through the table and the destination with pointers, 4 pixels per iteration. Compilers do not unroll it on their own in the same way, and with a 32-bit index some of them spend extra instructions on each pixel to compute `x + 1`, `x + 2` and `x + 3` and convert them to 64 bits.

**SIMD.** Case 2 repeats 4 texels at a time (SSE2 `unpacklo/hi_epi32` for x2, `pshufd` for x3 and x4). Case 3 reads 4 texels one by one and writes them with one store: x86 has no fast gather (`vpgather` is slow on processors with the microcode for Downfall), and 8 texels at a time with AVX2 gain too little to be worth another path. Large images are limited by memory bandwidth, so SIMD gains less there.

## Bilinear

### The formula: 8-bit weights, one rounding

The DDA gives the sample position in 1/256 of a texel. Its high bits are the source column, and its low 8 bits are the weight `fx` of the next column, from 0 to 255. Rows work the same way and give `fy`. At the last column or row, both texels are the same one. Per channel:

```cpp
top    = p00 * (256 - fx) + p01 * fx           // at most 65280: fits in 16 bits
bottom = p10 * (256 - fx) + p11 * fx
result = (top * (256 - fy) + bottom * fy + 0x8000) >> 16
```

The weights add up to 256 in each direction, so the sum before the shift is the result multiplied by 65536 (2^16). Adding `0x8000`, which is half of that, makes the shift round to the nearest value instead of truncating: it is the `+ 0.5` of rounding.

Nothing is rounded until the end, so a flat color stays exact, and the result is at most 1 level from the exact bilinear value, rounded. 8-bit weights are what let `top` and `bottom` fit in 16 bits, which the SIMD code needs.

![The four texels around a bilinear sample and the two passes](scaling_algorithms/bilinear.svg)

### Two passes, with row reuse

The filter is separable, so it is done in two passes:

1. **Horizontal pass.** A source row is interpolated across the destination columns and kept as 16 bits per channel: that is `top` or `bottom` above.
2. **Vertical pass.** Each destination row mixes two of those rows with `fy`.

Two row buffers are kept. The source rows only move down, so the bottom row of one destination row is often the top row of the next one, and is reused instead of computed again. When enlarging by `k`, the horizontal work goes down about `k` times. When reducing, no row is reused, and the two passes cost about as much as the direct formula.

![Ten destination rows from four source rows, with four horizontal passes](scaling_algorithms/row_reuse.svg)

### Horizontal pass: two channels per multiplication

The horizontal formula is rewritten as `p0 * 256 + (p1 - p0) * fx`. Each texel is split into two pairs of channels, each pair in the two 16-bit halves of a `uint32_t`:

```cpp
blue_red    =  p       & 0x00ff00ff;     // 0x00RR00BB
green_alpha = (p >> 8) & 0x00ff00ff;     // 0x00AA00GG
```

One multiplication then handles two channels. A half can go below 0 in between, but each half ends between 0 and 65280, so the whole value is exact modulo 2^32 and the halves do not overlap.

All four channels would also fit in one `uint64_t`, but that is not faster. Splitting the pixel into a `uint64_t` and joining it again takes more instructions than it saves, 64-bit constants do not fit inside the instructions, and on 32-bit processors each 64-bit multiplication takes several instructions. The two 32-bit operations, instead, do not depend on each other, so the processor runs them at the same time.

![A pixel split into two pairs of 16-bit channels, and the order of the channels in the rows](scaling_algorithms/swar.svg)

In SIMD the same formula runs in 16-bit lanes with `pmullw`, which is exact modulo 2^16 for the same reason. Green and alpha come from `psrlw 8`, without a mask. NEON does the same with `vmlaq_u16`, and reads the three fields of 4 columns with one `vld3q_u32`.

### Vertical pass: splitting into bytes

`top * (256 - fy) + bottom * fy` needs 24 bits, which does not fit in the 16-bit lanes of `pmullw`. So the SIMD code splits each 16-bit value into its high and low bytes:

```cpp
hi = (top >> 8)   * (256 - fy) + (bottom >> 8)   * fy      // at most 65280
lo = (top & 0xff) * (256 - fy) + (bottom & 0xff) * fy      // at most 65280
result = (hi + 128 + (lo >> 8)) >> 8
```

Both sums fit in 16 bits, and the result is exactly the formula above: `(256 * hi + lo + 0x8000) >> 16`, where the constant `0x8000` is `128 << 8`. It was checked for every possible pair of row values.

NEON does not need the split. `vmull_u16` and `vmlal_u16` multiply 16-bit lanes into 32-bit sums, and `vrshrn_n_u32(sum, 16)` adds the `0x8000`, shifts and narrows in one instruction.

### The order of the channels in the rows

The scalar and the SIMD passes keep the 16-bit channels of a row in a different order:

- Scalar: blue, red, green, alpha. The horizontal pass stores the two pairs as the multiplications leave them.
- SIMD: blue, green, red, alpha, the byte order of a pixel, so the vertical pass only has to pack the results to bytes.

That is why the level is chosen once per call and used for both passes: a row filled by one level can only be mixed by the same level.

In AVX2, `pack` and `unpack` work inside each 128-bit half, so after them the pixels are in the order 0 1 4 5 2 3 6 7, and one `vpermq` or `vperm2i128` puts them back in order.

![The order of the pixels after the AVX2 pack, and after vpermq](scaling_algorithms/avx2_order.svg)

## Common to both filters

- **Levels.** SSE2 is part of every 64-bit x86 processor. AVX2 is only used for bilinear, when the processor and the operating system support x86-64-v3. NEON is only used for bilinear, on every 64-bit ARM processor and on 32-bit ARM builds with NEON. Other processors, and 32-bit x86, use the scalar code; compilers often vectorize parts of it on their own.
- **The end of a row.** SIMD rows are done in blocks of 4 pixels (SSE2, NEON) or 8 (AVX2). The last block ends where the row ends and may overlap the one before it, which writes the same pixels twice. Reads stay inside the source row, because the texels are read through the tables. Rows narrower than one block use the scalar code.
- **No aligned prologue.** Loads and stores are unaligned; on current processors they cost about the same.
- **Limits.** Nearest does not draw sizes of 2^30 or more.

## How it is checked

- A fuzz test runs the code on thousands of random inputs and checks each result. Here it compares every level with the scalar code on sizes from 1 up, offsets, pitches longer than the row, integer factors, reductions and sizes of 0. The images are placed right before a protected page of memory, so that any read or write past the image stops the program. It also runs under AddressSanitizer.
- The formulas (8-bit weights, the halves of the horizontal pass, the byte split of the vertical pass) were checked for every possible input.
- `tests/scaling.c` checks both filters against an integer reference calculation. It also checks switching filters at the same size with one cache, source and destination offsets, padded rows and dimensions of 65,536 pixels.
