// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CODEC_PNG_OPS_SIMD_IMPL_P_H_INCLUDED
#define BLEND2D_CODEC_PNG_OPS_SIMD_IMPL_P_H_INCLUDED

#include <blend2d/axl/commons.h>
#include <blend2d/axl/mem_ops.h>
#include <blend2d/codec/png_ops_p.h>
#include <blend2d/simd/simd_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_codec_impl
//! \{

namespace bl::Png::Ops {
namespace {

// Codecs - Png - Ops - SimdImpl
// =============================

// Precalculates D == 3C - B. This is a constant that only relies on the scanline above, thus it's fully vectorized.
static BL_INLINE SIMD::Vec8xU16 v_precalc_d(const SIMD::Vec8xU16& b, const SIMD::Vec8xU16& c) noexcept {
  using namespace SIMD;

  Vec8xU16 c_plus_c = add_i16(c, c);
  Vec8xU16 c_minus_b = sub_i16(c, b);
  return add_i16(c_plus_c, c_minus_b);
}

static BL_INLINE SIMD::Vec8xU16 v_paeth(const SIMD::Vec8xU16& a, const SIMD::Vec8xU16& b, const SIMD::Vec8xU16& c, const SIMD::Vec8xU16& d) noexcept {
  using namespace SIMD;

  Vec8xU16 hi = max_i16(a, b);
  Vec8xU16 threshold = sub_i16(d, a);
  Vec8xU16 lo = min_i16(a, b);

  Vec8xU16 pred_hi = cmp_gt_i16(hi, threshold);
  Vec8xU16 pred_lo = cmp_gt_i16(threshold, lo);

  Vec8xU16 t0 = blendv_u8(lo, c, pred_hi);
  return blendv_u8(hi, t0, pred_lo);
}

template<uint32_t Shift>
static BL_INLINE SIMD::Vec16xU8 v_sllb_addb(const SIMD::Vec16xU8& a) noexcept {
  using namespace SIMD;

  Vec16xU8 t = sllb_u128<Shift>(a);
  return add_i8(a, t);
}

// Paeth in the byte domain. The distance to C is the sum of the two absolute differences if
// A-C and B-C have the same sign, and their absolute difference otherwise. Saturating the sum
// preserves the comparisons, since the distances to A and B never exceed 255.
static BL_INLINE SIMD::Vec16xU8 v_paeth_u8(const SIMD::Vec16xU8& a, const SIMD::Vec16xU8& b, const SIMD::Vec16xU8& c) noexcept {
  using namespace SIMD;
  Vec16xU8 pa = abs_diff_u8(b, c);
  Vec16xU8 pb = abs_diff_u8(a, c);
  Vec16xU8 opposite = xor_(cmp_gt_u8(c, a), cmp_gt_u8(c, b));
  Vec16xU8 pc = blendv_u8(adds_u8(pa, pb), abs_diff_u8(pa, pb), opposite);
  Vec16xU8 use_a = cmp_le_u8(pa, min_u8(pb, pc));
  Vec16xU8 use_b = cmp_le_u8(pb, pc);
  return blendv_u8(blendv_u8(c, b, use_b), a, use_a);
}

template<uint32_t Filter>
static BL_INLINE SIMD::Vec16xU8 v_diagonal_predict(const SIMD::Vec16xU8& a, const SIMD::Vec16xU8& b, const SIMD::Vec16xU8& c) noexcept {
  using namespace SIMD;
  if constexpr (Filter == kFilterTypePaeth) {
    return v_paeth_u8(a, b, c);
  }
  else {
    return hadd_u8(a, b);
  }
}

// Codecs - Png - Ops - Forward Filter
// ==================================

template<size_t BPP, uint32_t Filter>
static BL_INLINE void filter_row_simd(uint8_t* p, const uint8_t* u, size_t size) noexcept {
  using namespace SIMD;

  constexpr size_t kPrefix = Filter == kFilterTypeUp ? 0u : BPP;
  size_t i = size;

  // Work backwards so each vector can still read the original pixels to its left.
  while (i >= kPrefix + 16u) {
    i -= 16u;
    Vec16xU8 current = loadu<Vec16xU8>(p + i);
    Vec16xU8 prediction;

    if constexpr (Filter == kFilterTypeSub) {
      prediction = loadu<Vec16xU8>(p + i - BPP);
    }
    else if constexpr (Filter == kFilterTypeUp) {
      prediction = loadu<Vec16xU8>(u + i);
    }
    else if constexpr (Filter == kFilterTypeAvg0) {
      Vec16xU8 left = loadu<Vec16xU8>(p + i - BPP);
      Vec16xU8 zero = make_zero<Vec16xU8>();
      prediction = v_diagonal_predict<kFilterTypeAvg>(left, zero, zero);
    }
    else {
      Vec16xU8 left = loadu<Vec16xU8>(p + i - BPP);
      Vec16xU8 up = loadu<Vec16xU8>(u + i);
      if constexpr (Filter == kFilterTypePaeth) {
        Vec16xU8 upper_left = loadu<Vec16xU8>(u + i - BPP);
        prediction = v_paeth_u8(left, up, upper_left);
      }
      else {
        prediction = v_diagonal_predict<kFilterTypeAvg>(left, up, up);
      }
    }

    storeu(p + i, sub_i8(current, prediction));
  }

  while (i > kPrefix) {
    i--;
    uint32_t prediction;
    if constexpr (Filter == kFilterTypeSub)
      prediction = p[i - BPP];
    else if constexpr (Filter == kFilterTypeUp)
      prediction = u[i];
    else if constexpr (Filter == kFilterTypeAvg0)
      prediction = p[i - BPP] >> 1;
    else if constexpr (Filter == kFilterTypePaeth)
      prediction = apply_paeth_filter(p[i - BPP], u[i], u[i - BPP]);
    else
      prediction = apply_avg_filter(p[i - BPP], u[i]);

    p[i] = uint8_t(uint32_t(p[i]) - prediction);
  }

  // A missing left pixel is zero. Sub and first-row Average leave this prefix unchanged.
  if constexpr (Filter == kFilterTypeAvg || Filter == kFilterTypePaeth) {
    while (i) {
      i--;
      uint32_t prediction = Filter == kFilterTypeAvg ? u[i] >> 1 : u[i];
      p[i] = uint8_t(uint32_t(p[i]) - prediction);
    }
  }
}

template<size_t BPP, uint32_t Filter>
static BL_INLINE void filter_image_simd(uint8_t* p, size_t bpl, uint32_t h) noexcept {
  size_t size = bpl - 1u;
  p += size_t(h - 1u) * bpl;

  // Preserve each unfiltered row until the row below has consumed it.
  for (uint32_t y = h;;) {
    p[0] = uint8_t(Filter);
    uint8_t* row = p + 1;

    if constexpr (Filter == kFilterTypeSub) {
      filter_row_simd<BPP, kFilterTypeSub>(row, nullptr, size);
    }
    else if constexpr (Filter == kFilterTypeUp) {
      if (y > 1u)
        filter_row_simd<BPP, kFilterTypeUp>(row, p - bpl + 1, size);
    }
    else if constexpr (Filter == kFilterTypeAvg) {
      if (y > 1u)
        filter_row_simd<BPP, kFilterTypeAvg>(row, p - bpl + 1, size);
      else
        filter_row_simd<BPP, kFilterTypeAvg0>(row, nullptr, size);
    }
    else if constexpr (Filter == kFilterTypePaeth) {
      if (y > 1u)
        filter_row_simd<BPP, kFilterTypePaeth>(row, p - bpl + 1, size);
      else
        filter_row_simd<BPP, kFilterTypeSub>(row, nullptr, size);
    }

    if (--y == 0u)
      break;

    p -= bpl;
  }
}

template<uint32_t BPP>
static BLResult BL_CDECL filter_simd_impl(uint8_t* p, size_t bpp, size_t bpl, uint32_t h, uint32_t filter_type) noexcept {
  BL_ASSERT(bpp == BPP);
  BL_ASSERT(bpl > BPP);
  BL_ASSERT(h > 0u);
  axl::maybe_unused(bpp);

  if (filter_type == kFilterTypeDefault) {
    filter_type = kFilterTypePaeth;
  }

  switch (filter_type) {
    case kFilterTypeNone:  filter_image_simd<BPP, kFilterTypeNone >(p, bpl, h); break;
    case kFilterTypeSub:   filter_image_simd<BPP, kFilterTypeSub  >(p, bpl, h); break;
    case kFilterTypeUp:    filter_image_simd<BPP, kFilterTypeUp   >(p, bpl, h); break;
    case kFilterTypeAvg:   filter_image_simd<BPP, kFilterTypeAvg  >(p, bpl, h); break;
    case kFilterTypePaeth: filter_image_simd<BPP, kFilterTypePaeth>(p, bpl, h); break;
    default:
      return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  return BL_SUCCESS;
}

// Codecs - Png - Ops - Forward Filter With Repeat Masks
// ====================================================

#if BL_TARGET_ARCH_ARM == 64 && !defined(BL_TARGET_BIG_ENDIAN)

// Isolate the left edge so every vector in the interior has unconditional loads.
template<size_t BPP, bool FirstRow, bool FirstVector>
static BL_INLINE SIMD::Vec16xU8 filter_paeth_mask_vector(uint8_t* p, size_t bpl) noexcept {
  using namespace SIMD;

  Vec16xU8 current = loadu<Vec16xU8>(p);
  Vec16xU8 left;
  if constexpr (FirstVector)
    left = sllb_u128<BPP>(current);
  else
    left = loadu<Vec16xU8>(p - BPP);

  Vec16xU8 prediction = left;
  if constexpr (!FirstRow) {
    const uint8_t* u = p - bpl;
    Vec16xU8 up = loadu<Vec16xU8>(u);
    Vec16xU8 upper_left;
    if constexpr (FirstVector)
      upper_left = sllb_u128<BPP>(up);
    else
      upper_left = loadu<Vec16xU8>(u - BPP);

    prediction = v_paeth_u8(left, up, upper_left);
  }

  Vec16xU8 residual = sub_i8(current, prediction);
  storeu(p, residual);
  return residual;
}

template<size_t BPP, bool FirstRow, bool FirstChunk>
static BL_INLINE SIMD::Vec16xU8 filter_paeth_mask_chunk(uint8_t* p, size_t bpl, uint64_t* mask, const SIMD::Vec16xU8& next) noexcept {
  using namespace SIMD;

  const Vec16xU8 weights = make128_u8(0x80u, 0x40u, 0x20u, 0x10u, 0x08u, 0x04u, 0x02u, 0x01u);
  Vec16xU8 r3 = filter_paeth_mask_vector<BPP, FirstRow, false>(p + 48, bpl);
  Vec16xU8 e3 = and_(cmp_eq_u8(r3, alignr_u128<1>(next, r3)), weights);
  Vec16xU8 r2 = filter_paeth_mask_vector<BPP, FirstRow, false>(p + 32, bpl);
  Vec16xU8 e2 = and_(cmp_eq_u8(r2, alignr_u128<1>(r3, r2)), weights);

  // Start packing while the lower vectors are filtered to shorten the dependency chain
  // and live ranges. Four equality vectors become one compact 64-bit mask.
  Vec16xU8 cd = addp_i8(e2, e3);
  Vec16xU8 r1 = filter_paeth_mask_vector<BPP, FirstRow, false>(p + 16, bpl);
  Vec16xU8 e1 = and_(cmp_eq_u8(r1, alignr_u128<1>(r2, r1)), weights);
  Vec16xU8 r0 = filter_paeth_mask_vector<BPP, FirstRow, FirstChunk>(p, bpl);
  Vec16xU8 e0 = and_(cmp_eq_u8(r0, alignr_u128<1>(r1, r0)), weights);
  Vec16xU8 ab = addp_i8(e0, e1);
  Vec16xU8 sum = addp_i8(ab, cd);
  sum = addp_i8(sum, sum);
  storeu_64(mask, sum);
  return r0;
}

template<size_t BPP, bool FirstRow>
static BL_INLINE void filter_paeth_mask_row(uint8_t* row, size_t bpl, size_t size, uint64_t* masks) noexcept {
  using namespace SIMD;

  size_t i = size;
  size_t full_vectors = size & ~size_t(15);
  uint16_t tail = 0;
  uint8_t right = 0;

  if (size & 63u) {
    masks[size >> 6u] = 0;
  }

  // Scalar bytes align the vector groups to mask words. Traverse backwards so the
  // left and upper-left bytes still contain their original samples.
  while (i > full_vectors) {
    --i;
    uint32_t left = i >= BPP ? row[i - BPP] : 0u;
    uint32_t prediction = left;
    if constexpr (!FirstRow) {
      const uint8_t* u = row - bpl;
      prediction = apply_paeth_filter(left, u[i], i >= BPP ? u[i - BPP] : 0u);
    }

    uint8_t residual = uint8_t(uint32_t(row[i]) - prediction);
    row[i] = residual;
    if (i + 1u < size && residual == right) {
      tail |= uint16_t(1u << (i & 15u));
    }
    right = residual;
  }

  if (size & 15u) {
    axl::storeu_u16(reinterpret_cast<uint8_t*>(masks) + (full_vectors >> 4u) * 2u, tail);
  }

  Vec16xU8 next = make128_u8(right);
  while (i & 63u) {
    i -= 16u;
    Vec16xU8 residual = i ? filter_paeth_mask_vector<BPP, FirstRow, false>(row + i, bpl)
                          : filter_paeth_mask_vector<BPP, FirstRow, true>(row, bpl);
    uint16_t bits = uint16_t(extract_mask_bits_i8(cmp_eq_u8(residual, alignr_u128<1>(next, residual))));
    axl::storeu_u16(reinterpret_cast<uint8_t*>(masks) + (i >> 4u) * 2u, bits);
    next = residual;
  }

  uint8_t* p = row + i;
  uint64_t* m = masks + (i >> 6u);
  if (i > 128u) {
    uint8_t* limit = row + 128u;
    do {
      p -= 128u;
      m -= 2u;
      next = filter_paeth_mask_chunk<BPP, FirstRow, false>(p + 64u, bpl, m + 1, next);
      next = filter_paeth_mask_chunk<BPP, FirstRow, false>(p, bpl, m, next);
    } while (p > limit);
  }

  if (i > 64u) {
    uint8_t* limit = row + 64u;
    while (p > limit) {
      p -= 64u;
      --m;
      next = filter_paeth_mask_chunk<BPP, FirstRow, false>(p, bpl, m, next);
    }
  }

  if (p != row) {
    filter_paeth_mask_chunk<BPP, FirstRow, true>(row, bpl, masks, next);
  }

  // The last comparison has no successor and must never extend across rows.
  masks[(size - 1u) >> 6u] &= ~(uint64_t(1) << ((size - 1u) & 63u));
}

template<size_t BPP>
static BL_NOINLINE void filter_paeth_with_masks(uint8_t* p, size_t bpl, uint32_t h, uint64_t* masks) noexcept {
  size_t size = bpl - 1u;
  size_t mask_stride = FastEncoder::repeat_mask_stride(bpl);

  for (uint32_t y = h; y--;) {
    uint8_t* row = p + size_t(y) * bpl + 1u;
    uint64_t* row_masks = masks + size_t(y) * mask_stride;
    row[-1] = uint8_t(kFilterTypePaeth);
    if (y) {
      filter_paeth_mask_row<BPP, false>(row, bpl, size, row_masks);
    }
    else {
      filter_paeth_mask_row<BPP, true>(row, bpl, size, row_masks);
    }
  }
}

#endif // BL_TARGET_ARCH_ARM == 64 && !BL_TARGET_BIG_ENDIAN

static BL_INLINE void build_repeat_masks_simd(const uint8_t* p, size_t bpl, uint32_t h, uint64_t* masks) noexcept {
#if defined(BL_TARGET_OPT_AVX512)
  size_t size = bpl - 1u;
  size_t mask_stride = FastEncoder::repeat_mask_stride(bpl);

  for (uint32_t y = 0; y < h; y++, p += bpl, masks += mask_stride) {
    const uint8_t* row = p + 1u;
    uint32_t x = 0;

    // A whole mask word needs 65 source bytes, including the successor of
    // its final byte. Both unmasked loads stay within this scanline.
    while (size - x > 64u) {
      __m512i bytes = _mm512_loadu_si512(row + x);
      __m512i next = _mm512_loadu_si512(row + x + 1u);
      masks[x >> 6u] = uint64_t(_mm512_cmpeq_epi8_mask(bytes, next));
      x += 64u;
    }

    // Compare only bytes with a successor. Masked loads suppress accesses
    // beyond the row, and the comparison clears its final and padding bits.
    size_t count = size - x - 1u;
    uint64_t tail = 0;

    if (count) {
      __mmask64 valid = __mmask64((uint64_t(1) << count) - 1u);
      __m512i bytes = _mm512_maskz_loadu_epi8(valid, row + x);
      __m512i next = _mm512_maskz_loadu_epi8(valid, row + x + 1u);
      tail = uint64_t(_mm512_mask_cmpeq_epi8_mask(valid, bytes, next));
    }

    masks[x >> 6u] = tail;
  }
#elif (BL_TARGET_ARCH_ARM == 64 && !defined(BL_TARGET_BIG_ENDIAN)) || defined(BL_SIMD_FEATURE_EXTRACT_SIGN_BITS)
  using namespace SIMD;
  size_t size = bpl - 1u;
  size_t mask_stride = FastEncoder::repeat_mask_stride(bpl);
  for (uint32_t y = 0; y < h; y++, p += bpl, masks += mask_stride) {
    const uint8_t* row = p + 1u;
    size_t x = 0;
    masks[(size - 1u) >> 6u] = 0;

    // Load the successor vector only while all sixteen comparisons are inside
    // the row. The last byte has no successor and retains its zero mask bit.
    while (size - x > 64u) {
      Vec16xU8 e0 = cmp_eq_u8(loadu<Vec16xU8>(row + x     ), loadu<Vec16xU8>(row + x +  1u));
      Vec16xU8 e1 = cmp_eq_u8(loadu<Vec16xU8>(row + x + 16u), loadu<Vec16xU8>(row + x + 17u));
      Vec16xU8 e2 = cmp_eq_u8(loadu<Vec16xU8>(row + x + 32u), loadu<Vec16xU8>(row + x + 33u));
      Vec16xU8 e3 = cmp_eq_u8(loadu<Vec16xU8>(row + x + 48u), loadu<Vec16xU8>(row + x + 49u));

      masks[x >> 6u] = extract_mask_bits_i8(e0, e1, e2, e3);

      x += 64u;
    }

    while (size - x > 16u) {
      Vec16xU8 equality = cmp_eq_u8(loadu<Vec16xU8>(row + x), loadu<Vec16xU8>(row + x + 1u));
      uint16_t bits = uint16_t(extract_mask_bits_i8(equality));
      axl::storeu_u16(reinterpret_cast<uint8_t*>(masks) + (x >> 4u) * 2u, bits);
      x += 16u;
    }

    uint64_t tail = 0;
    uint64_t tail_word = uint64_t(x) >> 6u;

    for (; x + 1u < size; x++) {
      tail |= uint64_t(row[x] == row[x + 1u]) << (x & 63u);
    }

    masks[tail_word] |= tail;
  }
#else
  FastEncoder::build_repeat_masks(p, bpl, h, masks);
#endif
}

template<size_t BPP>
static BLResult BL_CDECL filter_with_masks_simd_impl(uint8_t* p, size_t bpp, size_t bpl, uint32_t h, uint32_t filter_type, uint64_t* masks) noexcept {
  BL_ASSERT(bpp == BPP);
  BL_ASSERT(bpl > BPP);
  BL_ASSERT(h > 0u);

#if BL_TARGET_ARCH_ARM == 64 && !defined(BL_TARGET_BIG_ENDIAN)
  if constexpr (BPP == 1u || BPP == 3u || BPP == 4u) {
    if (filter_type == kFilterTypePaeth || filter_type == kFilterTypeDefault) {
      filter_paeth_with_masks<BPP>(p, bpl, h, masks);
      return BL_SUCCESS;
    }
  }
#endif

  BL_PROPAGATE(filter_simd_impl<BPP>(p, bpp, bpl, h, filter_type));
  build_repeat_masks_simd(p, bpl, h, masks);
  return BL_SUCCESS;
}

template<size_t BPP, uint32_t Filter>
static BL_INLINE void inverse_filter_row_part(uint8_t* p, const uint8_t* u, size_t begin, size_t end) noexcept {
  for (size_t x = begin; x < end; x++) {
    for (size_t k = 0; k < BPP; k++) {
      size_t i = x * BPP + k;
      uint32_t a = x ? p[i - BPP] : 0u;
      uint32_t b = u[i];

      if constexpr (Filter == kFilterTypePaeth) {
        p[i] = apply_sum_filter(p[i], apply_paeth_filter(a, b, x ? u[i - BPP] : 0u));
      }
      else {
        p[i] = apply_sum_filter(p[i], apply_avg_filter(a, b));
      }
    }
  }
}

template<size_t BPP>
static BL_INLINE uint32_t load_diagonal_pixel(const uint8_t* p) noexcept {
  if constexpr (BPP == 3)
    return axl::loadu_u24(p);
  else
    return axl::loadu_u32(p);
}

static BL_INLINE SIMD::Vec16xU8 load_diagonal_four(const uint8_t* p, size_t delta) noexcept {
  using namespace SIMD;
  Vec16xU8 p01 = interleave_lo_u32(loadu_32<Vec16xU8>(p), loadu_32<Vec16xU8>(p + delta));
  Vec16xU8 p23 = interleave_lo_u32(loadu_32<Vec16xU8>(p + delta * 2), loadu_32<Vec16xU8>(p + delta * 3));
  return interleave_lo_u64(p01, p23);
}

template<size_t BPP>
static BL_INLINE void store_diagonal_pixel(uint8_t* p, const SIMD::Vec16xU8& v) noexcept {
  using namespace SIMD;
  if constexpr (BPP == 3) {
    storeu_16(p, v);
    store_8(p + 2, srlb_u128<2>(v));
  }
  else {
    storeu_32(p, v);
  }
}

template<size_t BPP>
static BL_INLINE void store_diagonal_four(uint8_t* p, size_t delta, const SIMD::Vec16xU8& v) noexcept {
  using namespace SIMD;
  store_diagonal_pixel<BPP>(p, v);
  store_diagonal_pixel<BPP>(p + delta, srlb_u128<4>(v));
  store_diagonal_pixel<BPP>(p + delta * 2, srlb_u128<8>(v));
  store_diagonal_pixel<BPP>(p + delta * 3, srlb_u128<12>(v));
}

// Rows run one pixel behind their predecessor, so the left and upper neighbours both belong
// to the previous diagonal. Each group of four pixels fills a vector, with a spare byte for RGB.
// Decode the small triangles at either edge separately to keep bounds checks out of the loop.
template<size_t BPP, uint32_t Filter, uint32_t Rows>
static BL_NOINLINE void inverse_filter_diagonal(uint8_t* p, const uint8_t* u, size_t bpl) noexcept {
  using namespace SIMD;
  constexpr uint32_t kStart = Rows - 1;

  size_t stride = size_t(bpl) + 1u;
  size_t delta = stride - BPP;
  size_t width = bpl / BPP;
  size_t end = width - uint32_t(BPP == 3);

  for (uint32_t r = 0; r < Rows; r++) {
    inverse_filter_row_part<BPP, Filter>(p + stride * r, r ? p + stride * (r - 1) : u, 0, kStart - r);
  }

  uint32_t left[8] {};
  uint32_t upper_left[8] {};

  for (uint32_t r = 0; r < Rows - 1; r++) {
    uint32_t offset = (kStart - r - 1) * BPP;
    left[r] = load_diagonal_pixel<BPP>(p + stride * r + offset);
    upper_left[r] = load_diagonal_pixel<BPP>((r ? p + stride * (r - 1) : u) + offset);
  }

  Vec16xU8 a0 = make128_u32<Vec16xU8>(left[3], left[2], left[1], left[0]);
  Vec16xU8 c0 = make128_u32<Vec16xU8>(upper_left[3], upper_left[2], upper_left[1], upper_left[0]);
  Vec16xU8 a1 = make128_u32<Vec16xU8>(left[7], left[6], left[5], left[4]);
  Vec16xU8 c1 = make128_u32<Vec16xU8>(upper_left[7], upper_left[6], upper_left[5], upper_left[4]);

  uint8_t* d = p + kStart * BPP;
  for (size_t x = kStart; x < end; x++, d += BPP) {
    Vec16xU8 b0 = or_(sllb_u128<4>(a0), loadu_32<Vec16xU8>(u + x * BPP));
    Vec16xU8 q0 = add_i8(load_diagonal_four(d, delta), v_diagonal_predict<Filter>(a0, b0, c0));
    if constexpr (Rows == 8) {
      Vec16xU8 b1 = alignr_u128<12>(a1, a0);
      Vec16xU8 q1 = add_i8(load_diagonal_four(d + delta * 4, delta), v_diagonal_predict<Filter>(a1, b1, c1));
      store_diagonal_four<BPP>(d + delta * 4, delta, q1);
      a1 = q1;
      c1 = b1;
    }
    store_diagonal_four<BPP>(d, delta, q0);
    a0 = q0;
    c0 = b0;
  }

  for (uint32_t r = 0; r < Rows; r++) {
    inverse_filter_row_part<BPP, Filter>(p + stride * r, r ? p + stride * (r - 1) : u, end - r, width);
  }
}

template<size_t BPP>
BLResult BL_CDECL inverse_filter_simd_impl(uint8_t* p, size_t bpp, size_t bpl, uint32_t h) noexcept {
  using namespace SIMD;

  // Only used by asserts, unused in release mode.
  axl::maybe_unused(bpp);

  BL_ASSERT(bpp == BPP);
  BL_ASSERT(bpl > 1u);
  BL_ASSERT(h > 0u);

  uint32_t y = h;
  uint8_t* u = nullptr;

  // Subtract one BYTE that is used to store the `filter` ID - it's always processed and not part of pixel data.
  bpl--;

  // First row uses a special filter that doesn't access the previous row,
  // which is assumed to contain all zeros.
  uint32_t filter_type = *p++;

  if (filter_type >= kFilterTypeCount) {
    filter_type = kFilterTypeNone;
  }

  filter_type = simplify_filter_of_first_row(filter_type);

  for (;;) {
    if constexpr (BPP == 3 || BPP == 4) {
      if ((filter_type == kFilterTypePaeth || filter_type == kFilterTypeAvg) && y >= 4 && bpl >= BPP * 32u) {
        uint32_t rows = 1;
        while (rows < bl_min(y, uint32_t(8)) && p[size_t(bpl + 1u) * rows - 1u] == filter_type) {
          rows++;
        }

        if (rows >= 4) {
          uint32_t count;
          if (filter_type == kFilterTypePaeth && rows == 8) {
            inverse_filter_diagonal<BPP, kFilterTypePaeth, 8>(p, u, bpl);
            count = 8;
          }
          else {
            if (filter_type == kFilterTypePaeth)
              inverse_filter_diagonal<BPP, kFilterTypePaeth, 4>(p, u, bpl);
            else
              inverse_filter_diagonal<BPP, kFilterTypeAvg, 4>(p, u, bpl);
            count = 4;
          }
          p += size_t(bpl + 1u) * count - 1u;
          y -= count - 1u;
          if (--y == 0)
            break;
          u = p - bpl;
          filter_type = *p++;
          if (filter_type >= kFilterTypeCount)
            filter_type = kFilterTypeNone;
          continue;
        }
      }
    }

    switch (filter_type) {
      // This is one of the easiest filters to parallelize. Although it looks like the data dependency
      // is too high, it's simply additions, which are really easy to parallelize. The following formula:
      //
      //     Y1' = BYTE(Y1 + Y0')
      //     Y2' = BYTE(Y2 + Y1')
      //     Y3' = BYTE(Y3 + Y2')
      //     Y4' = BYTE(Y4 + Y3')
      //
      // Expanded to (with byte casts removed, as they are implicit in our case):
      //
      //     Y1' = Y1 + Y0'
      //     Y2' = Y2 + Y1 + Y0'
      //     Y3' = Y3 + Y2 + Y1 + Y0'
      //     Y4' = Y4 + Y3 + Y2 + Y1 + Y0'
      //
      // The size of the register doesn't matter here. The Y0' dependency has been omitted to make the
      // flow cleaner, however, it can be added to Y1 before processing or it can be shifted to the
      // first cell so the first addition would be performed against [Y0', Y1, Y2, Y3].
      case kFilterTypeSub: {
        size_t i = bpl - BPP;

        if (i >= 32) {
          // Align to 16-BYTE boundary.
          size_t j = uint32_t(axl::align_up_diff(uintptr_t(p + BPP), 16));
          for (i -= j; j != 0; j--, p++)
            p[BPP] = apply_sum_filter(p[BPP], p[0]);

          if constexpr (BPP == 1) {
            Vec16xU8 p0, p1, p2, p3;
            Vec16xU8 t2;

            // Process 64 BYTEs at a time.
            p0 = cast_from_u32<Vec16xU8>(p[0]);
            while (i >= 64) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 1));
              p1 = loada<Vec16xU8>(p + 17);
              p2 = loada<Vec16xU8>(p + 33);
              p3 = loada<Vec16xU8>(p + 49);

              p0 = v_sllb_addb<1>(p0);
              p2 = v_sllb_addb<1>(p2);
              p0 = v_sllb_addb<2>(p0);
              p2 = v_sllb_addb<2>(p2);
              p0 = v_sllb_addb<4>(p0);
              p2 = v_sllb_addb<4>(p2);
              p0 = v_sllb_addb<8>(p0);
              p2 = v_sllb_addb<8>(p2);
              storea<Vec16xU8>(p + 1, p0);

              p0 = srlb_u128<15>(p0);
              t2 = srlb_u128<15>(p2);
              p1 = add_i8(p1, p0);
              p3 = add_i8(p3, t2);

              p1 = v_sllb_addb<1>(p1);
              p3 = v_sllb_addb<1>(p3);
              p1 = v_sllb_addb<2>(p1);
              p3 = v_sllb_addb<2>(p3);
              p1 = v_sllb_addb<4>(p1);
              p3 = v_sllb_addb<4>(p3);
              p1 = v_sllb_addb<8>(p1);
              p3 = v_sllb_addb<8>(p3);
              storea<Vec16xU8>(p + 17, p1);

              p1 = interleave_hi_u8(p1, p1);
              p1 = interleave_hi_u16(p1, p1);
              p1 = swizzle_u32<3, 3, 3, 3>(p1);

              p2 = add_i8(p2, p1);
              p3 = add_i8(p3, p1);

              storea<Vec16xU8>(p + 33, p2);
              storea<Vec16xU8>(p + 49, p3);
              p0 = srlb_u128<15>(p3);

              p += 64;
              i -= 64;
            }

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 1));
              p0 = v_sllb_addb<1>(p0);
              p0 = v_sllb_addb<2>(p0);
              p0 = v_sllb_addb<4>(p0);
              p0 = v_sllb_addb<8>(p0);

              storea(p + 1, p0);
              p0 = srlb_u128<15>(p0);

              p += 16;
              i -= 16;
            }
          }
          else if constexpr (BPP == 2) {
            Vec16xU8 p0, p1, p2, p3;
            Vec16xU8 t2;

            // Process 64 BYTEs at a time.
            p0 = cast_from_u32<Vec16xU8>(axl::loada_u16(p));
            while (i >= 64) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 2));
              p1 = loada<Vec16xU8>(p + 18);
              p2 = loada<Vec16xU8>(p + 34);
              p3 = loada<Vec16xU8>(p + 50);

              p0 = v_sllb_addb<2>(p0);
              p2 = v_sllb_addb<2>(p2);
              p0 = v_sllb_addb<4>(p0);
              p2 = v_sllb_addb<4>(p2);
              p0 = v_sllb_addb<8>(p0);
              p2 = v_sllb_addb<8>(p2);
              storea(p + 2, p0);

              p0 = srlb_u128<14>(p0);
              t2 = srlb_u128<14>(p2);
              p1 = add_i8(p1, p0);
              p3 = add_i8(p3, t2);

              p1 = v_sllb_addb<2>(p1);
              p3 = v_sllb_addb<2>(p3);
              p1 = v_sllb_addb<4>(p1);
              p3 = v_sllb_addb<4>(p3);
              p1 = v_sllb_addb<8>(p1);
              p3 = v_sllb_addb<8>(p3);
              storea(p + 18, p1);

              p1 = interleave_hi_u16(p1, p1);
              p1 = swizzle_u32<3, 3, 3, 3>(p1);

              p2 = add_i8(p2, p1);
              p3 = add_i8(p3, p1);

              storea(p + 34, p2);
              storea(p + 50, p3);
              p0 = srlb_u128<14>(p3);

              p += 64;
              i -= 64;
            }

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 2));
              p0 = v_sllb_addb<2>(p0);
              p0 = v_sllb_addb<4>(p0);
              p0 = v_sllb_addb<8>(p0);

              storea(p + 2, p0);
              p0 = srlb_u128<14>(p0);

              p += 16;
              i -= 16;
            }
          }
          else if constexpr (BPP == 3) {
            Vec16xU8 p0, p1, p2, p3;
            Vec16xU8 t0, t2;

#if defined(BL_TARGET_OPT_SSSE3) || defined(BL_TARGET_OPT_ASIMD)
            Vec16xU8 ext3b = make128_u64<Vec16xU8>(0xFFFF0A09080A0908u, 0xFFFF020100020100u);
#else
            Vec16xU8 ext3b = make128_u32<Vec16xU8>(0x01000001u);
#endif

            // Process 64 BYTEs at a time.
            p0 = cast_from_u32<Vec16xU8>(axl::loadu_u32(p) & 0x00FFFFFFu);
            while (i >= 64) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 3));
              p1 = loada<Vec16xU8>(p + 19);
              p2 = loada<Vec16xU8>(p + 35);

              p0 = v_sllb_addb<3>(p0);
              p2 = v_sllb_addb<3>(p2);
              p0 = v_sllb_addb<6>(p0);
              p2 = v_sllb_addb<6>(p2);
              p0 = v_sllb_addb<12>(p0);
              p2 = v_sllb_addb<12>(p2);

              p3 = loada<Vec16xU8>(p + 51);
              t0 = srlb_u128<13>(p0);
              t2 = srlb_u128<13>(p2);

              p1 = add_i8(p1, t0);
              p3 = add_i8(p3, t2);

              p1 = v_sllb_addb<3>(p1);
              p3 = v_sllb_addb<3>(p3);
              p1 = v_sllb_addb<6>(p1);
              p3 = v_sllb_addb<6>(p3);
              p1 = v_sllb_addb<12>(p1);
              p3 = v_sllb_addb<12>(p3);
              storea(p + 3, p0);

              p0 = swizzle_u32<3, 3, 3, 3>(p1);
              p0 = srli_u32<8>(p0);

#if defined(BL_TARGET_OPT_SSSE3) || defined(BL_TARGET_OPT_ASIMD)
              p0 = swizzlev_u8(p0, ext3b);
#else
              p0 = mulw_u32(p0, ext3b);
#endif

              p0 = swizzle_lo_u16<0, 2, 1, 0>(p0);
              p0 = swizzle_hi_u16<1, 0, 2, 1>(p0);

              storea(p + 19, p1);
              p2 = add_i8(p2, p0);
              p0 = swizzle_u32<1, 3, 2, 1>(p0);

              storea(p + 35, p2);
              p0 = add_i8(p0, p3);

              storea(p + 51, p0);
              p0 = srlb_u128<13>(p0);

              p += 64;
              i -= 64;
            }

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 3));

              p0 = v_sllb_addb<3>(p0);
              p0 = v_sllb_addb<6>(p0);
              p0 = v_sllb_addb<12>(p0);

              storea(p + 3, p0);
              p0 = srlb_u128<13>(p0);

              p += 16;
              i -= 16;
            }
          }
          else if constexpr (BPP == 4) {
            Vec16xU8 p0, p1, p2, p3;
            Vec16xU8 t2;

            // Process 64 BYTEs at a time.
            p0 = cast_from_u32<Vec16xU8>(axl::loada_u32(p));
            while (i >= 64) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 4));
              p1 = loada<Vec16xU8>(p + 20);
              p2 = loada<Vec16xU8>(p + 36);
              p3 = loada<Vec16xU8>(p + 52);

              p0 = v_sllb_addb<4>(p0);
              p2 = v_sllb_addb<4>(p2);
              p0 = v_sllb_addb<8>(p0);
              p2 = v_sllb_addb<8>(p2);
              storea(p + 4, p0);

              p0 = srlb_u128<12>(p0);
              t2 = srlb_u128<12>(p2);

              p1 = add_i8(p1, p0);
              p3 = add_i8(p3, t2);

              p1 = v_sllb_addb<4>(p1);
              p3 = v_sllb_addb<4>(p3);
              p1 = v_sllb_addb<8>(p1);
              p3 = v_sllb_addb<8>(p3);

              p0 = swizzle_u32<3, 3, 3, 3>(p1);
              storea(p + 20, p1);

              p2 = add_i8(p2, p0);
              p0 = add_i8(p0, p3);

              storea(p + 36, p2);
              storea(p + 52, p0);
              p0 = srlb_u128<12>(p0);

              p += 64;
              i -= 64;
            }

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 4));

              p0 = v_sllb_addb<4>(p0);
              p0 = v_sllb_addb<8>(p0);
              storea(p + 4, p0);
              p0 = srlb_u128<12>(p0);

              p += 16;
              i -= 16;
            }
          }
          else if constexpr (BPP == 6) {
            Vec16xU8 p0, p1, p2, p3;
            Vec16xU8 t2;

            p0 = loadu_64<Vec16xU8>(p);
            p0 = slli_i64<16>(p0);
            p0 = srli_u64<16>(p0);

            // Process 64 BYTEs at a time.
            while (i >= 64) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 6));
              p1 = loada<Vec16xU8>(p + 22);
              p2 = loada<Vec16xU8>(p + 38);

              p0 = v_sllb_addb<6>(p0);
              p2 = v_sllb_addb<6>(p2);
              p0 = v_sllb_addb<12>(p0);
              p2 = v_sllb_addb<12>(p2);

              p3 = loada<Vec16xU8>(p + 54);
              storea(p + 6, p0);

              p0 = srlb_u128<10>(p0);
              t2 = srlb_u128<10>(p2);

              p1 = add_i8(p1, p0);
              p3 = add_i8(p3, t2);

              p1 = v_sllb_addb<6>(p1);
              p3 = v_sllb_addb<6>(p3);
              p1 = v_sllb_addb<12>(p1);
              p3 = v_sllb_addb<12>(p3);

              p0 = dup_hi_u64(p1);
              p0 = swizzle_lo_u16<1, 3, 2, 1>(p0);
              p0 = swizzle_hi_u16<2, 1, 3, 2>(p0);

              storea(p + 22, p1);
              p2 = add_i8(p2, p0);
              p0 = swizzle_u32<1, 3, 2, 1>(p0);

              storea(p + 38, p2);
              p0 = add_i8(p0, p3);

              storea(p + 54, p0);
              p0 = srlb_u128<10>(p0);

              p += 64;
              i -= 64;
            }

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 6));

              p0 = v_sllb_addb<6>(p0);
              p0 = v_sllb_addb<12>(p0);

              storea(p + 6, p0);
              p0 = srlb_u128<10>(p0);

              p += 16;
              i -= 16;
            }
          }
          else if constexpr (BPP == 8) {
            Vec16xU8 p0, p1, p2, p3;
            Vec16xU8 t2;

            // Process 64 BYTEs at a time.
            p0 = loadu_64<Vec16xU8>(p);
            while (i >= 64) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 8));
              p1 = loada<Vec16xU8>(p + 24);
              p2 = loada<Vec16xU8>(p + 40);
              p3 = loada<Vec16xU8>(p + 56);

              p0 = v_sllb_addb<8>(p0);
              p2 = v_sllb_addb<8>(p2);
              storea(p + 8, p0);

              p0 = srlb_u128<8>(p0);
              t2 = dup_hi_u64(p2);
              p1 = add_i8(p1, p0);

              p1 = v_sllb_addb<8>(p1);
              p3 = v_sllb_addb<8>(p3);
              p0 = dup_hi_u64(p1);
              p3 = add_i8(p3, t2);
              storea(p + 24, p1);

              p2 = add_i8(p2, p0);
              p0 = add_i8(p0, p3);

              storea(p + 40, p2);
              storea(p + 56, p0);
              p0 = srlb_u128<8>(p0);

              p += 64;
              i -= 64;
            }

            // Process 16 BYTEs at a time.
            while (i >= 16) {
              p0 = add_i8(p0, loada<Vec16xU8>(p + 8));
              p0 = v_sllb_addb<8>(p0);

              storea(p + 8, p0);
              p0 = srlb_u128<8>(p0);

              p += 16;
              i -= 16;
            }
          }
        }

        for (; i != 0; i--, p++) {
          p[BPP] = apply_sum_filter(p[BPP], p[0]);
        }

        p += BPP;
        break;
      }

      // This is actually the easiest filter that doesn't require any kind of specialization for a particular BPP.
      case kFilterTypeUp: {
        BL_ASSERT(u != nullptr);

        size_t i = bpl;

        if (i >= 24) {
          // Align to 16-BYTE boundary.
          size_t j = uint32_t(axl::align_up_diff(uintptr_t(p), 16));
          for (i -= j; j != 0; j--, p++, u++) {
            p[0] = apply_sum_filter(p[0], u[0]);
          }

          // Process 64 BYTEs at a time.
          while (i >= 64) {
            Vec16xU8 u0 = loadu<Vec16xU8>(u +  0);
            Vec16xU8 u1 = loadu<Vec16xU8>(u + 16);
            Vec16xU8 p0 = add_i8(u0, loada<Vec16xU8>(p +  0));
            Vec16xU8 p1 = add_i8(u1, loada<Vec16xU8>(p + 16));

            Vec16xU8 u2 = loadu<Vec16xU8>(u + 32);
            Vec16xU8 u3 = loadu<Vec16xU8>(u + 48);
            Vec16xU8 p2 = add_i8(u2, loada<Vec16xU8>(p + 32));
            Vec16xU8 p3 = add_i8(u3, loada<Vec16xU8>(p + 48));

            storea(p +  0, p0);
            storea(p + 16, p1);
            storea(p + 32, p2);
            storea(p + 48, p3);

            p += 64;
            u += 64;
            i -= 64;
          }

          // Process 8 BYTEs at a time.
          while (i >= 8) {
            Vec16xU8 u0 = loadu_64<Vec16xU8>(u);
            Vec16xU8 p0 = loada_64<Vec16xU8>(p);

            storea_64(p, add_i8(p0, u0));

            p += 8;
            u += 8;
            i -= 8;
          }
        }

        for (; i != 0; i--, p++, u++) {
          p[0] = apply_sum_filter(p[0], u[0]);
        }
        break;
      }

      // This filter is extremely difficult for low BPP values as there is a huge sequential data dependency,
      // I didn't succeeded to solve it. 1-3 BPP implementations are pretty bad and I would like to hear about
      // a way to improve those. The implementation for 4 BPP and more is pretty good, as there is less data
      // dependency between individual bytes.
      //
      // Sequential Approach:
      //
      //     Y1' = byte((2*Y1 + U1 + Y0') >> 1)
      //     Y2' = byte((2*Y2 + U2 + Y1') >> 1)
      //     Y3' = byte((2*Y3 + U3 + Y2') >> 1)
      //     Y4' = byte((2*Y4 + U4 + Y3') >> 1)
      //     Y5' = ...
      //
      // Expanded, `U1 + Y0'` replaced with `U1`:
      //
      //     Y1' = byte((2*Y1 + U1) >> 1)
      //     Y2' = byte((2*Y2 + U2 + byte((2*Y1 + U1) >> 1)) >> 1)
      //     Y3' = byte((2*Y3 + U3 + byte((2*Y2 + U2 + byte((2*Y1 + U1) >> 1)) >> 1)) >> 1)
      //     Y4' = byte((2*Y4 + U4 + byte((2*Y3 + U3 + byte((2*Y2 + U2 + byte((2*Y1 + U1) >> 1)) >> 1)) >> 1)) >> 1)
      //     Y5' = ...
      case kFilterTypeAvg: {
        BL_ASSERT(u != nullptr);

        if constexpr (BPP == 1) {
          // This is one of the most difficult AVG filters. 1-BPP has a huge sequential dependency, which is
          // nearly impossible to parallelize. The code below is the best I could have written, it's a mixture
          // of C++ and SIMD. Maybe using a pure C would be even better than this code, but, I tried to take
          // advantage of 8 BYTE fetches at least. Unrolling the loop any further doesn't lead to an improvement.
          uint32_t t0 = 0;
          uint32_t t1;

          // Process 8 BYTE chunks at a time.
          size_t i = bpl;
          while (i >= 8) {
            Vec16xU8 p0 = loadu_64_u8_u16<Vec16xU8>(p);
            Vec16xU8 u0 = loadu_64_u8_u16<Vec16xU8>(u);

            p0 = slli_i16<1>(p0);
            p0 = add_i16(p0, u0);

            t1 = cast_to_u32(p0);
            p0 = srlb_u128<4>(p0);
            t0 = ((t0 + t1) >> 1) & 0xFF; t1 >>= 16;
            p[0] = uint8_t(t0);

            t0 = ((t0 + t1) >> 1) & 0xFF;
            t1 = cast_to_u32(p0);
            p0 = srlb_u128<4>(p0);
            p[1] = uint8_t(t0);

            t0 = ((t0 + t1) >> 1) & 0xFF; t1 >>= 16;
            p[2] = uint8_t(t0);

            t0 = ((t0 + t1) >> 1) & 0xFF;
            t1 = cast_to_u32(p0);
            p0 = srlb_u128<4>(p0);
            p[3] = uint8_t(t0);

            t0 = ((t0 + t1) >> 1) & 0xFF; t1 >>= 16;
            p[4] = uint8_t(t0);

            t0 = ((t0 + t1) >> 1) & 0xFF;
            t1 = cast_to_u32(p0);
            p[5] = uint8_t(t0);

            t0 = ((t0 + t1) >> 1) & 0xFF; t1 >>= 16;
            p[6] = uint8_t(t0);

            t0 = ((t0 + t1) >> 1) & 0xFF;
            p[7] = uint8_t(t0);

            p += 8;
            u += 8;
            i -= 8;
          }

          while (i != 0u) {
            t0 = (uint32_t(p[0]) + apply_avg_filter(t0, u[0])) & 0xFFu;
            p[0] = uint8_t(t0);

            p++;
            u++;
            i--;
          }
          break;

        }
        else if constexpr (BPP == 2) {
          // Keep both channel carries in the low byte of separate 16-bit lanes.
          uint32_t t0 = 0;
          size_t i = bpl;

          // Process 8 BYTE chunks at a time.
          while (i >= 8) {
            Vec16xU8 p0 = loadu_64_u8_u16<Vec16xU8>(p);
            Vec16xU8 u0 = loadu_64_u8_u16<Vec16xU8>(u);
            p0 = add_i16(slli_i16<1>(p0), u0);

            // Each lane sum is at most 1020, so addition cannot carry into the next lane.
            // The mask also removes the bit shifted from the upper lane into bit 15.
            uint32_t t2 = cast_to_u32(p0);
            p0 = srlb_u128<4>(p0);
            t0 = ((t0 + t2) >> 1) & 0x00FF00FFu;
            p[0] = uint8_t(t0);
            p[1] = uint8_t(t0 >> 16);

            t2 = cast_to_u32(p0);
            p0 = srlb_u128<4>(p0);
            t0 = ((t0 + t2) >> 1) & 0x00FF00FFu;
            p[2] = uint8_t(t0);
            p[3] = uint8_t(t0 >> 16);

            t2 = cast_to_u32(p0);
            p0 = srlb_u128<4>(p0);
            t0 = ((t0 + t2) >> 1) & 0x00FF00FFu;
            p[4] = uint8_t(t0);
            p[5] = uint8_t(t0 >> 16);

            t2 = cast_to_u32(p0);
            t0 = ((t0 + t2) >> 1) & 0x00FF00FFu;
            p[6] = uint8_t(t0);
            p[7] = uint8_t(t0 >> 16);

            p += 8;
            u += 8;
            i -= 8;
          }

          uint32_t t1 = t0 >> 16;
          t0 &= 0xFFu;
          for (; i != 0; i -= 2, p += 2, u += 2) {
            t0 = uint8_t(uint32_t(p[0]) + apply_avg_filter(t0, u[0]));
            t1 = uint8_t(uint32_t(p[1]) + apply_avg_filter(t1, u[1]));
            p[0] = uint8_t(t0);
            p[1] = uint8_t(t1);
          }
          break;
        }
        else if constexpr (BPP == 3) {
          uint32_t t0 = 0;
          uint32_t t1 = 0;
          uint32_t t2 = 0;
          for (uint32_t i = 0; i < bpl; i += 3) {
            t0 = uint8_t(uint32_t(p[i + 0]) + apply_avg_filter(t0, u[i + 0]));
            t1 = uint8_t(uint32_t(p[i + 1]) + apply_avg_filter(t1, u[i + 1]));
            t2 = uint8_t(uint32_t(p[i + 2]) + apply_avg_filter(t2, u[i + 2]));
            p[i + 0] = uint8_t(t0);
            p[i + 1] = uint8_t(t1);
            p[i + 2] = uint8_t(t2);
          }
          p += bpl;
          break;
        }
        else {
          for (uint32_t i = 0; i < BPP; i++) {
            p[i] = apply_sum_filter(p[i], u[i] >> 1);
          }

          u += BPP;

          size_t i = bpl - BPP;
          if (i >= 32) {
            // Align to 16-BYTE boundary.
            size_t j = uint32_t(axl::align_up_diff(uintptr_t(p + BPP), 16));

            for (i -= j; j != 0; j--, p++, u++) {
              p[BPP] = apply_sum_filter(p[BPP], apply_avg_filter(p[0], u[0]));
            }

            if constexpr (BPP == 4) {
              Vec16xU8 m00FF = make128_u32<Vec16xU8>(0x00FF00FFu);
              Vec16xU8 m01FF = make128_u32<Vec16xU8>(0x01FF01FFu);
              Vec16xU8 t1 = unpack_lo64_u8_u16(loada_32<Vec16xU8>(p));

              // Process 16 BYTEs at a time.
              while (i >= 16) {
                Vec16xU8 p0, p1;
                Vec16xU8 u0, u1;

                p1 = loada<Vec16xU8>(p + 4);
                u1 = loadu<Vec16xU8>(u);

                p0 = unpack_lo64_u8_u16(p1);         // LO | Unpack Ln
                p1 = unpack_hi64_u8_u16(p1);         // HI | Unpack Ln
                p0 = slli_i16<1>(p0);                // LO | << 1

                u0 = unpack_lo64_u8_u16(u1);         // LO | Unpack Up
                p0 = add_i16(p0, t1);                // LO | Add Last

                p0 = add_i16(p0, u0);                // LO | Add Up
                p0 = p0 & m01FF;                     // LO | & 0x01FE

                u1 = unpack_hi64_u8_u16(u1);         // HI | Unpack Up
                t1 = sllb_u128<8>(p0);               // LO | Get Last
                p0 = slli_i16<1>(p0);                // LO | << 1

                p1 = slli_i16<1>(p1);                // HI | << 1
                p0 = add_i16(p0, t1);                // LO | Add Last
                p0 = srli_u16<2>(p0);                // LO | >> 2

                p1 = add_i16(p1, u1);                // HI | Add Up
                p0 = p0 & m00FF;                     // LO | & 0x00FF
                t1 = srlb_u128<8>(p0);               // LO | Get Last

                p1 = add_i16(p1, t1);                // HI | Add Last
                p1 = p1 & m01FF;                     // HI | & 0x01FE

                t1 = sllb_u128<8>(p1);               // HI | Get Last
                p1 = slli_i16<1>(p1);                // HI | << 1

                t1 = add_i16(t1, p1);                // HI | Add Last
                t1 = srli_u16<2>(t1);                // HI | >> 2
                t1 = t1 & m00FF;                     // HI | & 0x00FF

                p0 = packz_128_u16_u8(p0, t1);
                t1 = srlb_u128<8>(t1);               // HI | Get Last
                storea(p + 4, p0);

                p += 16;
                u += 16;
                i -= 16;
              }
            }
            else if constexpr (BPP == 6) {
              Vec16xU8 t1 = loadu_64<Vec16xU8>(p);

              // Process 16 BYTEs at a time.
              while (i >= 16) {
                Vec16xU8 p0, p1, p2;
                Vec16xU8 u0, u1, u2;

                u0 = loadu<Vec16xU8>(u);
                t1 = unpack_lo64_u8_u16(t1);
                p0 = loada<Vec16xU8>(p + 6);

                p1 = srlb_u128<6>(p0);               // P1 | Extract
                u1 = srlb_u128<6>(u0);               // P1 | Extract

                p2 = srlb_u128<12>(p0);              // P2 | Extract
                u2 = srlb_u128<12>(u0);              // P2 | Extract

                p0 = unpack_lo64_u8_u16(p0);         // P0 | Unpack
                u0 = unpack_lo64_u8_u16(u0);         // P0 | Unpack

                p1 = unpack_lo64_u8_u16(p1);         // P1 | Unpack
                u1 = unpack_lo64_u8_u16(u1);         // P1 | Unpack

                p2 = unpack_lo64_u8_u16(p2);         // P2 | Unpack
                u2 = unpack_lo64_u8_u16(u2);         // P2 | Unpack

                u0 = add_i16(u0, t1);                // P0 | Add Last
                u0 = srli_u16<1>(u0);                // P0 | >> 1
                p0 = add_i8(p0, u0);                 // P0 | Add (Up+Last)/2

                u1 = add_i16(u1, p0);                // P1 | Add P0
                u1 = srli_u16<1>(u1);                // P1 | >> 1
                p1 = add_i8(p1, u1);                 // P1 | Add (Up+Last)/2

                u2 = add_i16(u2, p1);                // P2 | Add P1
                u2 = srli_u16<1>(u2);                // P2 | >> 1
                p2 = add_i8(p2, u2);                 // P2 | Add (Up+Last)/2

                p0 = sllb_u128<4>(p0);
                p0 = packz_128_u16_u8(p0, p1);
                p0 = sllb_u128<2>(p0);
                p0 = srlb_u128<4>(p0);

                p2 = packz_128_u16_u8(p2, p2);
                p2 = sllb_u128<12>(p2);
                p0 = p0 | p2;

                storea(p + 6, p0);
                t1 = srlb_u128<10>(p0);

                p += 16;
                u += 16;
                i -= 16;
              }
            }
            else if constexpr (BPP == 8) {
              // Process 16 BYTEs at a time.
              Vec16xU8 t1 = unpack_lo64_u8_u16(loadu_64<Vec16xU8>(p));

              while (i >= 16) {
                Vec16xU8 p0, p1;
                Vec16xU8 u0, u1;

                u1 = loadu<Vec16xU8>(u);
                p1 = loada<Vec16xU8>(p + 8);

                u0 = unpack_lo64_u8_u16(u1);         // LO | Unpack Up
                p0 = unpack_lo64_u8_u16(p1);         // LO | Unpack Ln

                u0 = add_i16(u0, t1);                // LO | Add Last
                p1 = unpack_hi64_u8_u16(p1);         // HI | Unpack Ln
                u0 = srli_u16<1>(u0);                // LO | >> 1
                u1 = unpack_hi64_u8_u16(u1);         // HI | Unpack Up

                p0 = add_i8(p0, u0);                 // LO | Add (Up+Last)/2
                u1 = add_i16(u1, p0);                // HI | Add LO
                u1 = srli_u16<1>(u1);                // HI | >> 1
                p1 = add_i8(p1, u1);                 // HI | Add (Up+LO)/2

                p0 = packz_128_u16_u8(p0, p1);
                t1 = p1;                             // HI | Get Last
                storea(p + 8, p0);

                p += 16;
                u += 16;
                i -= 16;
              }
            }
          }

          for (; i != 0; i--, p++, u++) {
            p[BPP] = apply_sum_filter(p[BPP], apply_avg_filter(p[0], u[0]));
          }

          p += BPP;
          break;
        }
      }

      case kFilterTypePaeth: {
        BL_ASSERT(u != nullptr);

        // Paeth:
        //
        //   [C] [B]
        //   [A] [P] <- Current pixel
        //
        // Per pixel computation:
        //
        //   Q = (P + Paeth(A, B, C)) & 0xFF

        if constexpr (BPP == 1) {
          uint32_t sa0 = 0;
          uint32_t sc0 = 0;

          for (size_t i = 0; i < bpl; i++) {
            uint32_t sb0 = u[i];
            sa0 = (uint32_t(p[i]) + apply_paeth_filter(sa0, sb0, sc0)) & 0xFFu;
            sc0 = sb0;
            p[i] = uint8_t(sa0);
          }

          p += bpl;
        }
        else if constexpr (BPP == 2) {
          uint32_t sa0 = 0;
          uint32_t sa1 = 0;
          uint32_t sc0 = 0;
          uint32_t sc1 = 0;

          size_t i = bpl;
          while (i) {
            // Must hold as `bytes_per_line % 2 == 0`
            BL_ASSERT(i >= 2);

            uint32_t sb0 = u[0];
            uint32_t sb1 = u[1];

            sa0 = (uint32_t(p[0]) + apply_paeth_filter(sa0, sb0, sc0)) & 0xFFu;
            sa1 = (uint32_t(p[1]) + apply_paeth_filter(sa1, sb1, sc1)) & 0xFFu;

            sc0 = sb0;
            sc1 = sb1;

            p[0] = uint8_t(sa0);
            p[1] = uint8_t(sa1);

            p += 2;
            u += 2;
            i -= 2;
          }
        }
        else if constexpr (BPP == 3) {
          Vec8xU16 va0 = make_zero<Vec8xU16>();
          Vec8xU16 vc0 = make_zero<Vec8xU16>();
          Vec8xU16 vmask = make128_u64<Vec8xU16>(0x0000000000000000u, 0x0000FFFFFFFFFFFFu);

          // Process 12 BYTEs at a time (but load 16 BYTEs at a time for simplicity).
          size_t i = bpl;
          while (i >= 16) {
            Vec8xU16 vb0 = loadu_128<Vec8xU16>(u);
            Vec8xU16 vp0 = loadu_128<Vec8xU16>(p);

            vc0 = or_(vc0, sllb_u128<3>(vb0));

            Vec8xU16 vb1 = srlb_u128<6>(vb0);
            Vec8xU16 vc1 = srlb_u128<6>(vc0);
            Vec8xU16 vp1 = srlb_u128<6>(vp0);

            vb0 = unpack_lo64_u8_u16(vb0);
            vb1 = unpack_lo64_u8_u16(vb1);
            vc0 = unpack_lo64_u8_u16(vc0);
            vc1 = unpack_lo64_u8_u16(vc1);
            vp0 = unpack_lo64_u8_u16(vp0);
            vp1 = unpack_lo64_u8_u16(vp1);

            Vec8xU16 vd0 = v_precalc_d(vb0, vc0);
            Vec8xU16 vd1 = v_precalc_d(vb1, vc1);

            Vec8xU16 vq0 = add_i8(v_paeth(            (va0), vb0, vc0, vd0), vp0);
            Vec8xU16 vq1 = add_i8(v_paeth(sllb_u128<6>(vq0), vb0, vc0, vd0), vp0);
            Vec8xU16 vq2 = add_i8(v_paeth(srlb_u128<6>(vq1), vb1, vc1, vd1), vp1);
            Vec8xU16 vq3 = add_i8(v_paeth(sllb_u128<6>(vq2), vb1, vc1, vd1), vp1);

            vq0 = blendv_u8(vq1, vq0, vmask);
            vq2 = blendv_u8(vq3, vq2, vmask);

            vq0 = packz_128_u16_u8(vq0);
            va0 = and_(srlb_u128<6>(vq2), vmask);

            vq2 = packz_128_u16_u8(vq2);
            vc0 = srli_u64<24>(and_(packz_128_u16_u8(vb1), vmask));

            vq0 = or_(and_(vq0, vmask), slli_u64<48>(vq2));
            storeu_64(p + 0, vq0);
            storeu_32(p + 8, srli_u64<16>(vq2));

            p += 12;
            u += 12;
            i -= 12;
          }

          // Process 3 BYTEs at a time (but load 4 bytes at once to avoid byte loads).
          vc0 = unpack_lo64_u8_u16(vc0);
          vmask = make128_u64<Vec8xU16>(0x0000000000000000u, 0x000000000000FFFFu);

          while (i) {
            // Must hold as `bytes_per_line % 3 == 0`
            BL_ASSERT(i >= 3);

            Vec8xU16 vb0 = unpack_lo64_u8_u16(loadu_32<Vec8xU16>(u));
            Vec8xU16 vp0 = unpack_lo64_u8_u16(loadu_32<Vec8xU16>(p - 1));

            Vec8xU16 vd0 = v_precalc_d(vb0, vc0);
            va0 = add_i8(v_paeth(va0, vb0, vc0, vd0), srli_u64<16>(vp0));
            vc0 = vb0;
            storeu_32(p - 1, packz_128_u16_u8(or_(slli_u64<16>(va0), and_(vp0, vmask))));

            p += 3;
            u += 3;
            i -= 3;
          }
        }
        else if constexpr (BPP == 4) {
          Vec8xU16 va0 = make_zero<Vec8xU16>();
          Vec8xU16 vc0 = make_zero<Vec8xU16>();

          size_t i = bpl;
          while (i >= 16) {
            Vec8xU16 vb = loadu_128<Vec8xU16>(u);
            Vec8xU16 vp0 = loadu_128<Vec8xU16>(p);

            vc0 = or_(vc0, sllb_u128<4>(vb));

            Vec8xU16 vb1 = unpack_hi64_u8_u16(vb);
            Vec8xU16 vc1 = unpack_hi64_u8_u16(vc0);
            Vec8xU16 vp1 = unpack_hi64_u8_u16(vp0);
            Vec8xU16 vb0 = unpack_lo64_u8_u16(vb);

            vc0 = unpack_lo64_u8_u16(vc0);
            vp0 = unpack_lo64_u8_u16(vp0);

            Vec8xU16 vd0 = v_precalc_d(vb0, vc0);
            Vec8xU16 vd1 = v_precalc_d(vb1, vc1);

            Vec8xU16 vq0, vq1, vq2;
            vq0 = add_i8(v_paeth(        (va0), vb0, vc0, vd0), vp0);
            vq1 = add_i8(v_paeth(swap_u64(vq0), vb0, vc0, vd0), vp0);
            vq2 = add_i8(v_paeth(swap_u64(vq1), vb1, vc1, vd1), vp1);
            va0 = add_i8(v_paeth(swap_u64(vq2), vb1, vc1, vd1), vp1);

            vq0 = shuffle_u64<1, 0>(vq0, vq1);
            vq2 = shuffle_u64<1, 0>(vq2, va0);
            va0 = srlb_u128<8>(va0);

            vq0 = packz_128_u16_u8(vq0, vq2);
            vc0 = srlb_u128<12>(vb);

            storeu_128(p, vq0);

            p += 16;
            u += 16;
            i -= 16;
          }

          vc0 = unpack_lo64_u8_u16(vc0);
          while (i) {
            // Must hold as `bytes_per_line % 4 == 0`
            BL_ASSERT(i >= 4);

            Vec8xU16 vb0 = unpack_lo64_u8_u16(loadu_32<Vec8xU16>(u));
            Vec8xU16 vp0 = unpack_lo64_u8_u16(loadu_32<Vec8xU16>(p));

            Vec8xU16 vd0 = v_precalc_d(vb0, vc0);
            va0 = add_i8(v_paeth(va0, vb0, vc0, vd0), vp0);
            vc0 = vb0;

            storeu_32(p, packz_128_u16_u8(va0));

            p += 4;
            u += 4;
            i -= 4;
          }
        }
        else if constexpr (BPP == 6) {
          Vec8xU16 va0 = make_zero<Vec8xU16>();
          Vec8xU16 vc0 = make_zero<Vec8xU16>();

          size_t i = bpl;
          while (i >= 12) {
            Vec8xU16 vu0 = loadu_128<Vec8xU16>(u);
            Vec8xU16 vp0 = interleave_lo_u64(loadu_64<Vec8xU16>(p), loadu_32<Vec8xU16>(p + 8));

            Vec8xU16 vu1 = unpack_lo64_u8_u16(srlb_u128<6>(vu0));
            Vec8xU16 vp1 = unpack_lo64_u8_u16(srlb_u128<6>(vp0));

            vu0 = unpack_lo64_u8_u16(vu0);
            vp0 = unpack_lo64_u8_u16(vp0);

            Vec8xU16 vd0 = v_precalc_d(vu0, vc0);
            Vec8xU16 vd1 = v_precalc_d(vu1, vu0);

            vp0 = add_i8(v_paeth(va0, vu0, vc0, vd0), vp0);
            va0 = add_i8(v_paeth(vp0, vu1, vu0, vd1), vp1);

            vp0 = srlb_u128<2>(packz_128_u16_u8(sllb_u128<4>(vp0), va0));
            vc0 = vu1;

            storeu_64(p + 0, vp0);
            storeu_32(p + 8, swizzle_u32<2, 2, 2, 2>(vp0));

            p += 12;
            u += 12;
            i -= 12;
          }

          if (i) {
            // Must hold as `bytes_per_line % 6 == 0`
            BL_ASSERT(i == 6);

            Vec8xU16 vb0 = loadu_64_u8_u16<Vec8xU16>(u);
            Vec8xU16 vp0 = loadu_64<Vec8xU16>(p - 2);
            Vec8xU16 vd0 = v_precalc_d(vb0, vc0);

            vp0 = add_i8(vp0, slli_u64<16>(packz_128_u16_u8(v_paeth(va0, vb0, vc0, vd0))));
            storeu_64(p - 2, vp0);
            p += 6;
          }
        }
        else if constexpr (BPP == 8) {
          Vec8xU16 va0 = make_zero<Vec8xU16>();
          Vec8xU16 vc0 = make_zero<Vec8xU16>();

          size_t i = bpl;
          while (i >= 16) {
            Vec8xU16 vu0 = loadu_128<Vec8xU16>(u);
            Vec8xU16 vp0 = loadu_128<Vec8xU16>(p);

            Vec8xU16 vu1 = unpack_hi64_u8_u16(vu0);
            Vec8xU16 vp1 = unpack_hi64_u8_u16(vp0);

            vu0 = unpack_lo64_u8_u16(vu0);
            vp0 = unpack_lo64_u8_u16(vp0);

            Vec8xU16 vd0 = v_precalc_d(vu0, vc0);
            Vec8xU16 vd1 = v_precalc_d(vu1, vu0);

            vp0 = add_i8(v_paeth(va0, vu0, vc0, vd0), vp0);
            va0 = add_i8(v_paeth(vp0, vu1, vu0, vd1), vp1);

            vp0 = packz_128_u16_u8(vp0, va0);
            vc0 = vu1;

            storeu_128(p, vp0);

            p += 16;
            u += 16;
            i -= 16;
          }

          if (i) {
            // Must hold as `bytes_per_line % 8 == 0`
            BL_ASSERT(i == 8);

            Vec8xU16 vb0 = loadu_64_u8_u16<Vec8xU16>(u);
            Vec8xU16 vp0 = loadu_64_u8_u16<Vec8xU16>(p);
            Vec8xU16 vd0 = v_precalc_d(vb0, vc0);

            va0 = add_i8(v_paeth(va0, vb0, vc0, vd0), vp0);
            storeu_64(p, packz_128_u16_u8(va0));
            p += 8;
          }
        }
        break;
      }

      // This filter is artificial and only possible for the very first row, so there is no need to have it optimized.
      case kFilterTypeAvg0: {
        for (size_t i = bpl - BPP; i != 0; i--, p++) {
          p[BPP] = apply_sum_filter(p[BPP], p[0] >> 1);
        }
        p += BPP;
        break;
      }

      case kFilterTypeNone:
      default:
        p += bpl;
        break;
    }

    if (--y == 0) {
      break;
    }

    u = p - bpl;
    filter_type = *p++;

    if (filter_type >= kFilterTypeCount) {
      filter_type = kFilterTypeNone;
    }
  }

  return BL_SUCCESS;
}

void init_simd_functions(FunctionTable& ft) noexcept {
  ft.filter[1] = filter_simd_impl<1>;
  ft.filter[2] = filter_simd_impl<2>;
  ft.filter[3] = filter_simd_impl<3>;
  ft.filter[4] = filter_simd_impl<4>;
  ft.filter[6] = filter_simd_impl<6>;
  ft.filter[8] = filter_simd_impl<8>;

  ft.filter_with_masks[1] = filter_with_masks_simd_impl<1>;
  ft.filter_with_masks[2] = filter_with_masks_simd_impl<2>;
  ft.filter_with_masks[3] = filter_with_masks_simd_impl<3>;
  ft.filter_with_masks[4] = filter_with_masks_simd_impl<4>;
  ft.filter_with_masks[6] = filter_with_masks_simd_impl<6>;
  ft.filter_with_masks[8] = filter_with_masks_simd_impl<8>;

  ft.inverse_filter[1] = inverse_filter_simd_impl<1>;
  ft.inverse_filter[2] = inverse_filter_simd_impl<2>;
  ft.inverse_filter[3] = inverse_filter_simd_impl<3>;
  ft.inverse_filter[4] = inverse_filter_simd_impl<4>;
  ft.inverse_filter[6] = inverse_filter_simd_impl<6>;
  ft.inverse_filter[8] = inverse_filter_simd_impl<8>;
}

} // {anonymous}
} // {bl::Png::Ops}

//! \}
//! \endcond

#endif // BLEND2D_CODEC_PNG_OPS_SIMD_IMPL_P_H_INCLUDED
