// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_OPENTYPE_OT_GLYF_SIMD_IMPL_P_H_INCLUDED
#define BLEND2D_OPENTYPE_OT_GLYF_SIMD_IMPL_P_H_INCLUDED

#include <blend2d/axl/commons.h>
#include <blend2d/axl/mem_ops.h>
#include <blend2d/axl/scopedbuffer_p.h>
#include <blend2d/core/font_p.h>
#include <blend2d/core/matrix_p.h>
#include <blend2d/core/path_p.h>
#include <blend2d/geometry/commons_p.h>
#include <blend2d/opentype/ot_face_p.h>
#include <blend2d/opentype/ot_glyf_p.h>
#include <blend2d/opentype/ot_glyf_simd_data_p.h>
#include <blend2d/simd/simd_p.h>
#include <blend2d/tables/tables_p.h>

#if 0
  #include <blend2d/tracing/instrument_p.h>
  #define BL_OPENTYPE_PROFILE_SECTION(...) __VA_ARGS__;
#else
  #define BL_OPENTYPE_PROFILE_SECTION(...)
#endif

//! \cond INTERNAL
//! \addtogroup blend2d_opentype_impl
//! \{

namespace bl::OpenType::GlyfImpl {

// OpenType::GlyfImpl - GetGlyphOutlinesSimdImpl [SSE4.2 & AVX2 & ASIMD]
// =====================================================================

namespace {

using namespace SIMD;

BL_OPENTYPE_PROFILE_SECTION(struct LoopTimingData {
  Tracing::Count glyf_simple_count;
  Tracing::Count glyf_compound_count;
  Tracing::Count flag_non_repeats_full;
  Tracing::Count flag_non_repeats_half;
  Tracing::Count flag_has_repeats;
  Tracing::Time loop1_expand_flags;
  Tracing::Time loop2_transform_flags;
  Tracing::Time loop3_decode_vertices;
  Tracing::Time loop4_emit_path;

  ~LoopTimingData() {
    if (glyf_simple_count.value > 0u) {
      printf("\n=== [[Tracing]] OpenType - Outline TTF Decoder ===\n");
      BL_TRACING_OUTPUT(glyf_simple_count);
      BL_TRACING_OUTPUT(glyf_compound_count);
      BL_TRACING_OUTPUT(flag_non_repeats_full);
      BL_TRACING_OUTPUT(flag_non_repeats_half);
      BL_TRACING_OUTPUT(flag_has_repeats);
      BL_TRACING_OUTPUT(loop1_expand_flags);
      BL_TRACING_OUTPUT(loop2_transform_flags);
      BL_TRACING_OUTPUT(loop3_decode_vertices);
      BL_TRACING_OUTPUT(loop4_emit_path);
    }
  }
} trace_ot_glyf)

// There are some differences between X86 and ARM we have to address. In general the implementation is pretty
// similar, however, extracting MSB bits from 8-bit elements of a vector is different and in general ARM lacks
// some instructions that X86 supports natively, and doing a full emulation just is not good for performance.
//
// So, instead of a full emulation, we provide two implementations for X86 and ARM that use a slightly different
// approach, but the result is the same.
#if BL_TARGET_ARCH_X86

struct OffCurveSplineAcc {
  Vec4xU32 acc {};
  Vec16xU8 v_0x8080 {};

  BL_INLINE void init() noexcept {
    v_0x8080 = common_table.p_8080808080808080.as<Vec16xU8>();
  }

  BL_INLINE void accumulate_flags(const Vec16xU8& vf) noexcept {
    Vec16xU8 isolated = andnot(vf, v_0x8080);
    acc += vec_cast<Vec4xU32>(sad_u8_u64(isolated, v_0x8080));
  }

  BL_INLINE size_t get() const noexcept {
    Vec4xU32 result = acc + swap_u64(acc);
    return extract_u32<0>(result) >> 7;
  }
};

#elif BL_TARGET_ARCH_ARM

// ARM implementation uses narrowing shift to pack 2x8 bits into 2x4 bits, which can be then converted to a
// GP register predicate, which we can test. This is a pretty good approach that expands to only slightly more
// instructions than X86 approach.
struct OffCurveSplineAcc {
  Vec8xU16 acc0 {};
  Vec8xU16 acc1 {};

  BL_INLINE void init() noexcept {}

  BL_INLINE void accumulate_flags(const Vec16xU8& vf) noexcept {
    Vec16xU8 isolated = srli_u8<7>(vf);
    acc0 = addw_lo_u8_to_u16(acc0, isolated);
    acc1 = addw_hi_u8_to_u16(acc1, isolated);
  }

  BL_INLINE size_t get() const noexcept {
    Vec8xU16 acc = acc0 + acc1;
    uint32x4_t sum_q = vaddl_u16(vget_low_u16(acc.v), vget_high_u16(acc.v));
#if BL_TARGET_ARCH_ARM >= 64
    return vaddvq_u32(sum_q);
#else
    uint32x2_t sum_d = vadd_u32(vget_low_u32(sum_q), vget_high_u32(sum_q));
    sum_d = vadd_u32(sum_d, vrev64_u32(sum_d));
    return vget_lane_u32(sum_d, 0);
#endif
  }
};

#else
#error "bl::OpenType::GlyfImpl - missing support for target architecture"
#endif

template<typename VecT>
static BL_INLINE VecT fix_swizzle_predicate(const VecT& p, const VecT& v_0x0F0F) noexcept {
  return p & v_0x0F0F;
}

static BL_INLINE VecPair<Vec16xU8> aggregate_vertex_sizes(const Vec16xU8& vf, const Vec16xU8& xy_sizes_predicate, const Vec16xU8& v_0x0F0F) noexcept {
  Vec16xU8 pred = fix_swizzle_predicate(vf, v_0x0F0F);
  Vec16xU8 yx_sizes = swizzlev_u8(xy_sizes_predicate, pred); // [H   G   F   E   D   C   B   A]

  yx_sizes = yx_sizes + slli_i64<8>(yx_sizes);               // [H:G G:F F:E E:D D:C C:B B:A A]
  yx_sizes = yx_sizes + slli_i64<16>(yx_sizes);              // [H:E G:D F:C E:B D:A C:A B:A A]

  Vec16xU8 y_sizes = srli_u64<4>(yx_sizes) & v_0x0F0F;       // Y sizes separated from YX sizes.
  Vec16xU8 x_sizes = yx_sizes & v_0x0F0F;                    // X sizes separated from YX sizes.

  y_sizes = y_sizes + slli_i64<32>(y_sizes);                 // [H:A G:A F:A E:A D:A C:A B:A A]
  x_sizes = x_sizes + slli_i64<32>(x_sizes);                 // [H:A G:A F:A E:A D:A C:A B:A A]

  return {x_sizes, y_sizes};
}

static BL_INLINE Vec4xU32 sums_from_aggregated_sizes_of_8_bytes(const VecPair<Vec16xU8>& sizes) noexcept {
  return vec_u32(srli_u64<56>(shuffle_u32<1, 3, 1, 3>(sizes[0], sizes[1])));
}

static BL_INLINE Vec4xU32 sums_from_aggregated_sizes_of_16_bytes(const VecPair<Vec16xU8>& sizes) noexcept {
  return vec_u32(srli_u32<24>(shuffle_u32<1, 3, 1, 3>(sizes[0], sizes[1])));
}

struct DecodedVertex {
  int16_t x;
  int16_t y;
};

static BL_INLINE Vec2xF64 transform_decoded_vertex(const DecodedVertex* decoded_vertex, const Vec2xF64& m00_m11, const Vec2xF64& m10_m01) noexcept {
  Vec2xF64 vtx = cvt_2xi32_f64(vec_i32(unpack_lo64_i16_i32(loada_32<Vec8xI16>(decoded_vertex))));
  Vec2xF64 rev = swap_f64(vtx);
  return madd(rev, m10_m01, vtx * m00_m11);
}

template<typename Condition>
static BL_INLINE void append_vertex_if(PathAppender& appender, const Vec2xF64& vtx, Condition condition) noexcept {
  storeu(appender._vertex_ptr, vtx);
  appender._advance_vertex_ptr(size_t(condition));
}

static BL_INLINE void append_vertex_1x(PathAppender& appender, const Vec2xF64& vtx) noexcept {
  storeu(appender._vertex_ptr, vtx);
  appender._advance_vertex_ptr(1u);
}

static BL_INLINE void append_vertex_cmd(PathAppender& appender, BLPathCmd cmd, const Vec2xF64& vtx) noexcept {
  storeu(appender._vertex_ptr, vtx);
  appender._append_simple_command(cmd);
  appender._advance_vertex_ptr(1u);
}

static BL_INLINE void append_quad(PathAppender& appender, const Vec2xF64& v0, const Vec2xF64& v1) noexcept {
  storeu(appender._vertex_ptr + 0u, v0);
  storeu(appender._vertex_ptr + 1u, v1);
  appender._advance_vertex_ptr(2u);
  appender._append_simple_command(BL_PATH_CMD_QUAD);
}

// Calculates 4 path commands from processed TT flags (our shuffled format).
static BL_INLINE uint32_t path_cmds_from_vec_flags(uint32_t f) noexcept {
  return (f >> kVFToPathCmdRShift) & axl::repeat_byte<uint32_t>(kVFToPathCmdMask);
}

static BL_INLINE BLResult get_glyph_outlines_simd_impl(
  const BLFontFaceImpl* face_impl,
  BLGlyphId glyph_id,
  const BLMatrix2D* transform,
  BLPath* out,
  size_t* contour_count_out,
  ScopedBuffer* tmp_buffer
) noexcept {
  using namespace SIMD;

  using Simple = GlyfTable::Simple;
  using Compound = GlyfTable::Compound;

#ifdef BL_TARGET_OPT_AVX2
  constexpr uint32_t kDataAlignment = 32;
#else
  constexpr uint32_t kDataAlignment = 16;
#endif

  const OTFaceImpl* ot_face_impl = static_cast<const OTFaceImpl*>(face_impl);

  if (BL_UNLIKELY(glyph_id >= ot_face_impl->face_info.glyph_count)) {
    return bl_make_error(BL_ERROR_INVALID_GLYPH);
  }

  RawTable glyf_table = ot_face_impl->glyf.glyf_table;
  RawTable loca_table = ot_face_impl->glyf.loca_table;
  uint32_t loca_offset_size = ot_face_impl->loca_offset_size();

  const uint8_t* g_ptr = nullptr;
  size_t remaining_size = 0;
  size_t compound_level = 0;

  // Only matrix and compound_flags are important in the root entry.
  CompoundEntry compound_data[CompoundEntry::kMaxLevel];
  compound_data[0].g_ptr = nullptr;
  compound_data[0].remaining_size = 0;
  compound_data[0].compound_flags = Compound::kArgsAreXYValues;
  compound_data[0].transform = *transform;

  PathAppender appender;
  size_t contour_count_total = 0;

  for (;;) {
    size_t offset;
    size_t end_off;
    size_t remaining_size_after_glyph_data;

    // NOTE: Maximum glyph_id is 65535, so we are always safe here regarding
    // multiplying the `glyph_id` by 2 or 4 to calculate the correct index.
    if (loca_offset_size == 2) {
      size_t index = size_t(glyph_id) * 2u;
      if (BL_UNLIKELY(index + sizeof(UInt16) * 2u > loca_table.size)) {
        goto InvalidData;
      }
      offset = uint32_t(reinterpret_cast<const UInt16*>(loca_table.data + index + 0)->value()) * 2u;
      end_off = uint32_t(reinterpret_cast<const UInt16*>(loca_table.data + index + 2)->value()) * 2u;
    }
    else {
      size_t index = size_t(glyph_id) * 4u;
      if (BL_UNLIKELY(index + sizeof(UInt32) * 2u > loca_table.size)) {
        goto InvalidData;
      }
      offset = reinterpret_cast<const UInt32*>(loca_table.data + index + 0)->value();
      end_off = reinterpret_cast<const UInt32*>(loca_table.data + index + 4)->value();
    }

    remaining_size_after_glyph_data = glyf_table.size - end_off;

    // Simple or Empty Glyph
    // ---------------------

    if (BL_UNLIKELY(offset >= end_off || end_off > glyf_table.size)) {
      // Only ALLOWED when `offset == end_off`.
      if (BL_UNLIKELY(offset != end_off || end_off > glyf_table.size)) {
        goto InvalidData;
      }
    }
    else {
      g_ptr = glyf_table.data + offset;
      remaining_size = end_off - offset;

      if (BL_UNLIKELY(remaining_size < sizeof(GlyfTable::GlyphData))) {
        goto InvalidData;
      }

      int contour_count_signed = reinterpret_cast<const GlyfTable::GlyphData*>(g_ptr)->number_of_contours();
      if (contour_count_signed > 0) {
        BL_OPENTYPE_PROFILE_SECTION(trace_ot_glyf.glyf_simple_count.increment())

        size_t contour_count = size_t(unsigned(contour_count_signed));
        axl::OverflowFlag of{};

        // Minimum data size is:
        //   10                       [GlyphData header]
        //   (number_of_contours * 2) [end_pts_of_contours]
        //   2                        [instruction_length]
        g_ptr += sizeof(GlyfTable::GlyphData);
        remaining_size = axl::sub_overflow(remaining_size, sizeof(GlyfTable::GlyphData) + contour_count * 2u + 2u, of);
        if (BL_UNLIKELY(axl::did_overflow(of))) {
          goto InvalidData;
        }

        const UInt16* contour_array = reinterpret_cast<const UInt16*>(g_ptr);
        g_ptr += contour_count * 2u;
        contour_count_total += contour_count;

        // We don't use hinting instructions, so skip them.
        size_t instruction_count = axl::loadu_u16_be(g_ptr);
        remaining_size = axl::sub_overflow(remaining_size, instruction_count, of);
        if (BL_UNLIKELY(axl::did_overflow(of))) {
          goto InvalidData;
        }

        g_ptr += 2u + instruction_count;
        const uint8_t* g_end = g_ptr + remaining_size;

        // Number of vertices in TrueType sense (could be less than a number of points required by BLPath
        // representation, especially if TT outline contains consecutive off-curve points).
        size_t tt_vertex_count = size_t(contour_array[contour_count - 1].value()) + 1u;

        // Only try to decode vertices if there is more than 1.
        if (tt_vertex_count > 1u) {
          // Allocate Temporary Data for Vertex Decoding
          // -------------------------------------------

          size_t aligned_vertex_count = axl::align_up(tt_vertex_count, kDataAlignment) + kDataAlignment;
          uint8_t* f_data_ptr = static_cast<uint8_t*>(tmp_buffer->alloc(aligned_vertex_count * 3 + kDataAlignment));

          if (BL_UNLIKELY(!f_data_ptr)) {
            return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
          }

          f_data_ptr = axl::align_up(f_data_ptr, kDataAlignment);

          // Sizes of x_coordinates[] and y_coordinates[] arrays in TrueType data.
          uint8_t* x_pred_ptr = f_data_ptr + aligned_vertex_count;
          uint8_t* y_pred_ptr = x_pred_ptr + aligned_vertex_count;

          constexpr uint8_t kNopFlags = uint8_t(Simple::kOnCurvePoint |
                                                Simple::kXIsSameOrXByteIsPositive |
                                                Simple::kYIsSameOrYByteIsPositive);

          // 1. Loop - Expand Repeated TrueType Flags
          // ----------------------------------------
          BL_OPENTYPE_PROFILE_SECTION(auto t_loop1_start = trace_ot_glyf.loop1_expand_flags.start())

          // TrueType flags can be repeated, which generally make SIMD processing difficult, because the number of
          // repeats can vary between 0 to 255 (+ the initial flag, thus each decoded flag can repeated from 1 to 256).
          //
          // Instead of trying to do this in a SIMD loop, we simply preprocess the flags in a way that repeated flags
          // are expanded and REPEAT bytes discarded. This makes it possible to load and process flags with SIMD
          // instructions.
          //
          // NOTE: Scalar loop seems the best here. On 64-bit machine scalar loops can process 2 bit-words, which
          // translates to 16 TT flags. Since repeats are non-predictable and TT encoders use them it's better than
          // trying to use a SIMD loop when it comes to latency. Also, in general fonts don't have that many TT flags
          // per glyph, so we won't really benefit much if the loop processed double the flags, for example, because
          // one repeat in data would essentially ruin the benefit of a wider pipeline.

          // A simple loop which expands repeated flags first. In general we want to read BLBitWord quantities, which
          // means that we can read beyond at most `sizeof(BLBitWord)-1` bytes. The only case where there won't be
          // enough space is case in which this glyph would be at the end of 'glyf' table data and it would be very
          // small or unusual. So detect this case and switch to a very simple loop if that happens.
          if (remaining_size + remaining_size_after_glyph_data >= tt_vertex_count + sizeof(BLBitWord) * 2u) {
            constexpr BLBitWord tt_repeat_mask = axl::repeat_byte<BLBitWord>(Simple::kRepeatFlag);

            size_t i = 0;

            BL_NOUNROLL
            while (i < tt_vertex_count) {
              BLBitWord tt_flags_0 = axl::loadu<BLBitWord>(g_ptr);
              BLBitWord tt_flags_1 = axl::loadu<BLBitWord>(g_ptr + sizeof(BLBitWord));
              BLBitWord tt_flags_combined = tt_flags_0 | tt_flags_1;

              axl::storeu(f_data_ptr + i + 0                , tt_flags_0);
              axl::storeu(f_data_ptr + i + sizeof(BLBitWord), tt_flags_1);

              // Good case - no repeats.
              if ((tt_flags_combined & tt_repeat_mask) == 0u) {
                BL_OPENTYPE_PROFILE_SECTION(trace_ot_glyf.flag_non_repeats_full.increment())
                i += sizeof(BLBitWord) * 2u;
                g_ptr += sizeof(BLBitWord) * 2u;
                continue;
              }

              // Good case - the first bit-word has no repeats, the second has repeats.
              if ((tt_flags_0 & tt_repeat_mask) == 0u) {
                // There is a chance that we have just finished the loop as the repeats are part of vertex data that
                // follows. This is possible as we load words here unconditionally.
                BL_OPENTYPE_PROFILE_SECTION(trace_ot_glyf.flag_non_repeats_half.increment())

                i += sizeof(BLBitWord);
                g_ptr += sizeof(BLBitWord);

                if (i >= tt_vertex_count) {
                  break;
                }

                tt_flags_0 = tt_flags_1;
              }

              BLBitWord repeat_index = axl::ctz(tt_flags_0 & tt_repeat_mask) / 8u;
              i += repeat_index;
              g_ptr += repeat_index;

              if (i >= tt_vertex_count) {
                break;
              }

              BL_OPENTYPE_PROFILE_SECTION(trace_ot_glyf.flag_has_repeats.increment())

              tt_flags_0 = axl::repeat_byte<BLBitWord>((tt_flags_0 >> (repeat_index * 8u)) & (0xFFu ^ Simple::kRepeatFlag));
              repeat_index = i;
              size_t repeat_count = size_t(g_ptr[1]) + 1u;

              i += repeat_count;
              if (BL_UNLIKELY(i > tt_vertex_count)) {
                goto InvalidData;
              }

              BL_NOUNROLL
              while (repeat_index < i) {
                axl::storeu(f_data_ptr + repeat_index, tt_flags_0);
                repeat_index += sizeof(BLBitWord);
              }

              g_ptr += 2; // Advance both 'flags' byte and 'repeat' byte.
            }

            // Correct g_ptr as we may have advanced further than we should have.
            g_ptr -= (i - tt_vertex_count);
            if (g_ptr > g_end) {
              goto InvalidData;
            }
          }
          else {
            size_t i = 0;

            BL_NOUNROLL
            while (i < tt_vertex_count) {
              if (BL_UNLIKELY(g_ptr == g_end)) {
                goto InvalidData;
              }

              BLBitWord tt_flags = BLBitWord(*g_ptr++);
              f_data_ptr[i++] = uint8_t(tt_flags);

              if ((tt_flags & Simple::kRepeatFlag) == 0u) {
                continue;
              }

              tt_flags = axl::repeat_byte<BLBitWord>(tt_flags ^ Simple::kRepeatFlag);

              // Overwrite the previous byte as we don't want to have REPEAT flag in expanded flags data.
              f_data_ptr[i - 1] = uint8_t(tt_flags);

              if (BL_UNLIKELY(g_ptr == g_end)) {
                goto InvalidData;
              }

              size_t repeat_count = size_t(*g_ptr++);
              size_t repeat_index = i;

              i += repeat_count;
              if (BL_UNLIKELY(i > tt_vertex_count)) {
                goto InvalidData;
              }

              BL_NOUNROLL
              while (repeat_index < i) {
                axl::storeu(f_data_ptr + repeat_index, tt_flags);
                repeat_index += sizeof(BLBitWord);
              }
            }
          }

          BL_OPENTYPE_PROFILE_SECTION(trace_ot_glyf.loop1_expand_flags.end(t_loop1_start))

          // 2. Loop - Transform TrueType Flags and Generate Decode Predicates
          // -----------------------------------------------------------------
          BL_OPENTYPE_PROFILE_SECTION(auto t_loop2_start = trace_ot_glyf.loop2_transform_flags.start())

          // We need 3 temporary buffers:
          //
          //  - f_data_ptr - Converted flags data. These flags represent the same flags as used by TrueType, however,
          //                 the bits representing each vertex are different so they can be used with VPSHUFB/TBL.
          //  - x_pred_ptr - Buffer that is used to calculate predicates for X coordinates.
          //  - y_pred_ptr - Buffer that is used to calculate predicates for Y coordinates.
          //
          // The `x_pred_ptr` and `y_pred_ptr` buffers contain data grouped for 8 flags. Each byte contains the size
          // of the coordinate (either 0, 1, or 2 bytes are used in TrueType data) aggregated in the following way:
          //
          // Input coordinate sizes     = [A B C D E F G H]
          // Aggregated in [x|y]PredPtr = [A A+B A+B+C A+B+C+D A+B+C+D+E A+B+C+D+E+F A+B+C+D+E+F+G A+B+C+D+E+F+G+H]
          //
          // The aggregated sizes are very useful, because they describe where each vertex starts in decode buffer.

          size_t x_coords_size;
          size_t y_coords_size;

          OffCurveSplineAcc off_curve_spline_acc;
          size_t off_curve_spline_count;

          Vec16xU8 v_0x0F0F = common_table.p_0F0F0F0F0F0F0F0F.as<Vec16xU8>();
          off_curve_spline_acc.init();

          {
            // Make the rest of the buffer non-contributing to the accumulators that we aggregate to.
            Vec16xU8 vz = make128_u8<Vec16xU8>(kNopFlags);

            storeu(f_data_ptr + tt_vertex_count, vz);
            storeu(f_data_ptr + tt_vertex_count + 16, vz);

            Vec16xU8 v_0x0101 = common_table.p_0101010101010101.as<Vec16xU8>();
            Vec16xU8 v_0x3030 = common_table.p_3030303030303030.as<Vec16xU8>();

            Vec16xU8 v_convert_flags_predicate = loada<Vec16xU8>(convert_flags_predicate);
            Vec16xU8 v_sizes_per_xy_predicate = loada<Vec16xU8>(sizes_per_xy_predicate);

            Vec4xU32 v_sum_xy0 = make_zero<Vec4xU32>();
            Vec4xU32 v_sum_xy1 = make_zero<Vec4xU32>();

            BL_NOUNROLL
            for (size_t i = 0; i < tt_vertex_count; i += 32) {
              // Load flags into SIMD register.
              Vec16xU8 va0 = loada<Vec16xU8>(f_data_ptr + i +  0);
              Vec16xU8 va1 = loada<Vec16xU8>(f_data_ptr + i + 16);

              // VecFlags = PrevOnFlags
              Vec16xU8 vf0 = alignr_u128<15>(va0, vz);
              Vec16xU8 vf1 = alignr_u128<15>(va1, va0);

              // VecFlags = TT_Flags | PrevOnFlags
#if defined(BL_TARGET_ARCH_X86)
              vf0 = (slli_u16<3>(vf0 & v_0x0101) | va0) & v_0x0F0F;
              vf1 = (slli_u16<3>(vf1 & v_0x0101) | va1) & v_0x0F0F;
#else
              vf0 = slli_u8<3>(vf0) | va0;
              vf1 = slli_u8<3>(vf1) | va1;
#endif

              // VecFlags = [EmitMid, EmitCmd, Off, On, 1, 1, YByte, XByte]
              vf0 = swizzlev_u8(v_convert_flags_predicate, vf0);
              vf1 = swizzlev_u8(v_convert_flags_predicate, vf1);

              // Isolate XSame and YSame flags.
              Vec16xU8 vb0 = srli_u16<2>((va0 & v_0x3030));
              Vec16xU8 vb1 = srli_u16<2>((va1 & v_0x3030));

              // Accumulate off-spline curves for precise vertex allocation.
              off_curve_spline_acc.accumulate_flags(vf0);
              off_curve_spline_acc.accumulate_flags(vf1);

              // VecFlags = [EmitMid, EmitCmd, Off, On, !YSame, !XSame, YByte, XByte]
              vf0 = vf0 ^ vb0;
              vf1 = vf1 ^ vb1;

              // Vertex sizes for X and Y [0, 1, 2].
              VecPair<Vec16xU8> vs0 = aggregate_vertex_sizes(vf0, v_sizes_per_xy_predicate, v_0x0F0F);
              VecPair<Vec16xU8> vs1 = aggregate_vertex_sizes(vf1, v_sizes_per_xy_predicate, v_0x0F0F);

              storea(f_data_ptr + i +  0, vf0);
              storea(f_data_ptr + i + 16, vf1);

              storea(x_pred_ptr + i +  0, vs0[0]);
              storea(x_pred_ptr + i + 16, vs1[0]);

              storea(y_pred_ptr + i +  0, vs0[1]);
              storea(y_pred_ptr + i + 16, vs1[1]);

              vz = va1;
              v_sum_xy0 += sums_from_aggregated_sizes_of_16_bytes(vs0);
              v_sum_xy1 += sums_from_aggregated_sizes_of_16_bytes(vs1);
            }

            // Finally, calculate the size of x_coordinates[] and y_coordinates[] arrays.
            v_sum_xy0 += v_sum_xy1;
            v_sum_xy0 += srli_u64<32>(v_sum_xy0);
            x_coords_size = extract_u16<0>(v_sum_xy0);
            y_coords_size = extract_u16<4>(v_sum_xy0);
          }

          off_curve_spline_count = off_curve_spline_acc.get();
          remaining_size = axl::sub_overflow(axl::ptr_byte_diff(g_ptr, g_end), x_coords_size + y_coords_size, of);

          // This makes the static analysis happy about not using `remaining_size` afterwards. We don't want to discard
          // the result of the subtraction as that could possibly introduce an issue in the future if code depending
          // on `remaining_size` is added below.
          axl::maybe_unused(remaining_size);

          if (BL_UNLIKELY(axl::did_overflow(of))) {
            goto InvalidData;
          }

          BL_OPENTYPE_PROFILE_SECTION(trace_ot_glyf.loop2_transform_flags.end(t_loop2_start))

          // 3. Loop - Read TrueType Vertex Data
          // -----------------------------------

          // See otglyf.cpp for more details about how to calculate the byte_capacity_to_reserve:
          constexpr size_t extra_vertex_count = 16;
          size_t reserve_vertex_count = contour_count * 2 + tt_vertex_count + off_curve_spline_count + extra_vertex_count;
          size_t byte_capacity_to_reserve = reserve_vertex_count * (sizeof(BLPoint) + 1u) + contour_count;

          // Increase max_vertex_count if the path was not allocated yet - this avoids a possible realloc of compound glyphs.
          if (out->byte_capacity() == 0 && compound_level > 0u) {
            byte_capacity_to_reserve += 64u * (sizeof(BLPoint) + 1u);
          }

          BL_PROPAGATE(appender.begin_append(out, byte_capacity_to_reserve));

          // Temporary data where 16-bit coordinates (per X and Y) are stored before they are converted to double precision.
          // Appender vertex array is used, but the start pointer is calculated in a way that these two pointers won't overlap
          // once the vertices are processed and converted to floats.
          DecodedVertex* decoded_vertex_array = axl::align_down(
            reinterpret_cast<DecodedVertex*>(appender._vertex_ptr + reserve_vertex_count) - axl::align_up(tt_vertex_count, kDataAlignment), kDataAlignment);

          BL_OPENTYPE_PROFILE_SECTION(auto t_loop3_start = trace_ot_glyf.loop3_decode_vertices.start())
          {
            // Since we know exactly how many bytes both vertex arrays consume we can decode both X and Y coordinates at
            // the same time. This gives us also the opportunity to start appending to BLPath immediately.
            const uint8_t* y_ptr = g_ptr + x_coords_size;

            // LO+HI predicate is added to interleaved predicates.
            Vec16xU8 v_lo_hi_pred_inc = make128_u16<Vec16xU8>(uint16_t(0x0041u));

            // These are predicates we need to combine with x_pred and y_pred to get the final predicate for VPSHUFB/TBL.
            Vec16xU8 v_decode_op_x_imm = loada<Vec16xU8>(decode_op_x_table);
            Vec16xU8 v_decode_op_y_imm = loada<Vec16xU8>(decode_op_y_table);

            // NOTE: It's super unlikely that there won't be 16 bytes available after the end of x/y coordinates. Basically
            // only last glyph could be affected. However, we still need to check whether the bytes are there as we cannot just
            // read outside of the glyph table.
            if (BL_LIKELY(remaining_size_after_glyph_data >= 16)) {
              // Common case - uses at most 16-byte reads ahead, processes 16 vertices at a time.
#ifdef BL_TARGET_OPT_AVX2
              Vec32xU8 v_0x0F0F_256 = broadcast_i128<Vec32xU8>(v_0x0F0F);
              Vec32xU8 v_lo_hi_pred_inc256 = broadcast_i128<Vec32xU8>(v_lo_hi_pred_inc);
              size_t i = 0;

              // Process 32 vertices at a time.
              if (tt_vertex_count > 16) {
                Vec32xU8 v_decode_op_x_imm_256 = broadcast_i128<Vec32xU8>(v_decode_op_x_imm);
                Vec32xU8 v_decode_op_y_imm_256 = broadcast_i128<Vec32xU8>(v_decode_op_y_imm);

                do {
                  Vec16xU8 x_vertices_initial_0 = loadu<Vec16xU8>(g_ptr);
                  Vec16xU8 y_vertices_initial_0 = loadu<Vec16xU8>(y_ptr);

                  g_ptr += x_pred_ptr[i + 7];
                  y_ptr += y_pred_ptr[i + 7];

                  Vec32xU8 f_data = fix_swizzle_predicate(loada<Vec32xU8>(f_data_ptr + i), v_0x0F0F_256);
                  Vec32xU8 x_pred = slli_i64<8>(loada<Vec32xU8>(x_pred_ptr + i));
                  Vec32xU8 y_pred = slli_i64<8>(loada<Vec32xU8>(y_pred_ptr + i));

                  x_pred += swizzlev_u8(v_decode_op_x_imm_256, f_data);
                  y_pred += swizzlev_u8(v_decode_op_y_imm_256, f_data);

                  Vec16xU8 x_vertices_initial_1 = loadu<Vec16xU8>(g_ptr);
                  Vec16xU8 y_vertices_initial_1 = loadu<Vec16xU8>(y_ptr);

                  g_ptr += x_pred_ptr[i + 15];
                  y_ptr += y_pred_ptr[i + 15];

                  Vec32xU8 x_pred_0 = interleave_lo_u8(x_pred, x_pred);
                  Vec32xU8 x_pred_1 = interleave_hi_u8(x_pred, x_pred);
                  Vec32xU8 y_pred_0 = interleave_lo_u8(y_pred, y_pred);
                  Vec32xU8 y_pred_1 = interleave_hi_u8(y_pred, y_pred);

                  Vec16xI16 x_vertices_0 = make256_128<Vec16xI16>(loadu<Vec16xU8>(g_ptr), x_vertices_initial_0);
                  Vec16xI16 y_vertices_0 = make256_128<Vec16xI16>(loadu<Vec16xU8>(y_ptr), y_vertices_initial_0);

                  g_ptr += x_pred_ptr[i + 23];
                  y_ptr += y_pred_ptr[i + 23];

                  x_pred_0 += v_lo_hi_pred_inc256;
                  x_pred_1 += v_lo_hi_pred_inc256;
                  y_pred_0 += v_lo_hi_pred_inc256;
                  y_pred_1 += v_lo_hi_pred_inc256;

                  Vec16xI16 x_vertices_1 = make256_128<Vec16xI16>(loadu<Vec16xU8>(g_ptr), x_vertices_initial_1);
                  Vec16xI16 y_vertices_1 = make256_128<Vec16xI16>(loadu<Vec16xU8>(y_ptr), y_vertices_initial_1);

                  g_ptr += x_pred_ptr[i + 31];
                  y_ptr += y_pred_ptr[i + 31];

                  x_vertices_0 = swizzlev_u8(x_vertices_0, x_pred_0);
                  y_vertices_0 = swizzlev_u8(y_vertices_0, y_pred_0);
                  x_vertices_1 = swizzlev_u8(x_vertices_1, x_pred_1);
                  y_vertices_1 = swizzlev_u8(y_vertices_1, y_pred_1);

                  x_pred_0 = srai_i16<15>(slli_i16<2>(x_pred_0));
                  y_pred_0 = srai_i16<15>(slli_i16<2>(y_pred_0));
                  x_pred_1 = srai_i16<15>(slli_i16<2>(x_pred_1));
                  y_pred_1 = srai_i16<15>(slli_i16<2>(y_pred_1));

                  x_vertices_0 = (x_vertices_0 ^ vec_i16(x_pred_0)) - vec_i16(x_pred_0);
                  y_vertices_0 = (y_vertices_0 ^ vec_i16(y_pred_0)) - vec_i16(y_pred_0);
                  x_vertices_1 = (x_vertices_1 ^ vec_i16(x_pred_1)) - vec_i16(x_pred_1);
                  y_vertices_1 = (y_vertices_1 ^ vec_i16(y_pred_1)) - vec_i16(y_pred_1);

                  Vec16xI16 xy_interleaved_lo0 = interleave_lo_u16(x_vertices_0, y_vertices_0);
                  Vec16xI16 xy_interleaved_hi0 = interleave_hi_u16(x_vertices_0, y_vertices_0);
                  Vec16xI16 xy_interleaved_lo1 = interleave_lo_u16(x_vertices_1, y_vertices_1);
                  Vec16xI16 xy_interleaved_hi1 = interleave_hi_u16(x_vertices_1, y_vertices_1);

                  storea_128(decoded_vertex_array + i +  0, xy_interleaved_lo0);
                  storea_128(decoded_vertex_array + i +  4, xy_interleaved_hi0);
                  storea_128(decoded_vertex_array + i +  8, xy_interleaved_lo1);
                  storea_128(decoded_vertex_array + i + 12, xy_interleaved_hi1);

                  storea_128(decoded_vertex_array + i + 16, extract_i128<1>(xy_interleaved_lo0));
                  storea_128(decoded_vertex_array + i + 20, extract_i128<1>(xy_interleaved_hi0));
                  storea_128(decoded_vertex_array + i + 24, extract_i128<1>(xy_interleaved_lo1));
                  storea_128(decoded_vertex_array + i + 28, extract_i128<1>(xy_interleaved_hi1));

                  i += 32;
                } while (i < tt_vertex_count - 16);
              }

              // Process remaining 16 vertices.
              if (i < tt_vertex_count) {
                Vec16xU8 f_data = fix_swizzle_predicate(loada<Vec16xU8>(f_data_ptr + i), v_0x0F0F);
                Vec16xU8 x_pred = slli_i64<8>(loada<Vec16xU8>(x_pred_ptr + i));
                Vec16xU8 y_pred = slli_i64<8>(loada<Vec16xU8>(y_pred_ptr + i));

                x_pred += swizzlev_u8(v_decode_op_x_imm, f_data);
                y_pred += swizzlev_u8(v_decode_op_y_imm, f_data);

                Vec32xU8 x_pred_256 = permute_i64<1, 1, 0, 0>(vec_cast<Vec32xU8>(x_pred));
                Vec32xU8 y_pred_256 = permute_i64<1, 1, 0, 0>(vec_cast<Vec32xU8>(y_pred));

                x_pred_256 = interleave_lo_u8(x_pred_256, x_pred_256);
                y_pred_256 = interleave_lo_u8(y_pred_256, y_pred_256);

                Vec16xU8 x_vertices_initial = loadu<Vec16xU8>(g_ptr);
                Vec16xU8 y_vertices_initial = loadu<Vec16xU8>(y_ptr);

                g_ptr += x_pred_ptr[i + 7];
                y_ptr += y_pred_ptr[i + 7];

                x_pred_256 += v_lo_hi_pred_inc256;
                y_pred_256 += v_lo_hi_pred_inc256;

                Vec16xI16 x_vertices = make256_128<Vec16xI16>(loadu<Vec16xU8>(g_ptr), x_vertices_initial);
                Vec16xI16 y_vertices = make256_128<Vec16xI16>(loadu<Vec16xU8>(y_ptr), y_vertices_initial);

                // g_ptr/y_ptr is no longer needed, so the following code is not needed as well:
                //   g_ptr += x_pred_ptr[i + 15];
                //   y_ptr += y_pred_ptr[i + 15];

                x_vertices = swizzlev_u8(x_vertices, x_pred_256);
                y_vertices = swizzlev_u8(y_vertices, y_pred_256);

                x_pred_256 = srai_i16<15>(slli_i16<2>(x_pred_256));
                y_pred_256 = srai_i16<15>(slli_i16<2>(y_pred_256));

                x_vertices = (x_vertices ^ vec_i16(x_pred_256)) - vec_i16(x_pred_256);
                y_vertices = (y_vertices ^ vec_i16(y_pred_256)) - vec_i16(y_pred_256);

                Vec16xI16 xy_interleaved_lo = interleave_lo_u16(x_vertices, y_vertices);
                Vec16xI16 xy_interleaved_hi = interleave_hi_u16(x_vertices, y_vertices);

                storea_128(decoded_vertex_array + i +  0, xy_interleaved_lo);
                storea_128(decoded_vertex_array + i +  4, xy_interleaved_hi);
                storea_128(decoded_vertex_array + i +  8, extract_i128<1>(xy_interleaved_lo));
                storea_128(decoded_vertex_array + i + 12, extract_i128<1>(xy_interleaved_hi));
              }
#else
              for (size_t i = 0; i < tt_vertex_count; i += 16) {
                Vec16xU8 f_data = fix_swizzle_predicate(loada<Vec16xU8>(f_data_ptr + i), v_0x0F0F);
                Vec16xU8 x_pred = slli_i64<8>(loada<Vec16xU8>(x_pred_ptr + i));
                Vec16xU8 y_pred = slli_i64<8>(loada<Vec16xU8>(y_pred_ptr + i));

                x_pred += swizzlev_u8(v_decode_op_x_imm, f_data);
                y_pred += swizzlev_u8(v_decode_op_y_imm, f_data);

                Vec16xU8 x_pred_0 = interleave_lo_u8(x_pred, x_pred);
                Vec16xU8 x_pred_1 = interleave_hi_u8(x_pred, x_pred);
                Vec16xU8 y_pred_0 = interleave_lo_u8(y_pred, y_pred);
                Vec16xU8 y_pred_1 = interleave_hi_u8(y_pred, y_pred);

                x_pred_0 += v_lo_hi_pred_inc;
                x_pred_1 += v_lo_hi_pred_inc;
                y_pred_0 += v_lo_hi_pred_inc;
                y_pred_1 += v_lo_hi_pred_inc;

                // Process low 8 vertices.
                Vec8xI16 x_vertices_0 = vec_i16(swizzlev_u8(loadu<Vec16xU8>(g_ptr), x_pred_0));
                Vec8xI16 y_vertices_0 = vec_i16(swizzlev_u8(loadu<Vec16xU8>(y_ptr), y_pred_0));

                g_ptr += x_pred_ptr[i + 7];
                y_ptr += y_pred_ptr[i + 7];

                x_pred_0 = srai_i16<15>(slli_i16<2>(x_pred_0));
                y_pred_0 = srai_i16<15>(slli_i16<2>(y_pred_0));

                x_vertices_0 = (x_vertices_0 ^ vec_i16(x_pred_0)) - vec_i16(x_pred_0);
                y_vertices_0 = (y_vertices_0 ^ vec_i16(y_pred_0)) - vec_i16(y_pred_0);

                storea(decoded_vertex_array + i + 0, interleave_lo_u16(x_vertices_0, y_vertices_0));
                storea(decoded_vertex_array + i + 4, interleave_hi_u16(x_vertices_0, y_vertices_0));

                // Process high 8 vertices.
                Vec8xI16 x_vertices_1 = vec_i16(swizzlev_u8(loadu<Vec16xU8>(g_ptr), x_pred_1));
                Vec8xI16 y_vertices_1 = vec_i16(swizzlev_u8(loadu<Vec16xU8>(y_ptr), y_pred_1));

                g_ptr += x_pred_ptr[i + 15];
                y_ptr += y_pred_ptr[i + 15];

                x_pred_1 = srai_i16<15>(slli_i16<2>(x_pred_1));
                y_pred_1 = srai_i16<15>(slli_i16<2>(y_pred_1));

                x_vertices_1 = (x_vertices_1 ^ vec_i16(x_pred_1)) - vec_i16(x_pred_1);
                y_vertices_1 = (y_vertices_1 ^ vec_i16(y_pred_1)) - vec_i16(y_pred_1);

                storea(decoded_vertex_array + i +  8, interleave_lo_u16(x_vertices_1, y_vertices_1));
                storea(decoded_vertex_array + i + 12, interleave_hi_u16(x_vertices_1, y_vertices_1));
              }
#endif
            }
            else {
              // Restricted case - uses at most 16-byte reads below, we know that there are 16 bytes below, because:
              //   - Glyph header       [10 bytes]
              //   - NumberOfContours   [ 2 bytes]
              //   - InstructionLength  [ 2 bytes]
              //   - At least two flags [ 2 bytes] (one flag glyphs are refused as that's not enough for any contour)
              for (size_t i = 0; i < tt_vertex_count; i += 8) {
                Vec16xU8 f_data = fix_swizzle_predicate(loadu_64<Vec16xU8>(f_data_ptr + i), v_0x0F0F);
                Vec16xU8 x_pred = slli_i64<8>(loadu_64<Vec16xU8>(x_pred_ptr + i));
                Vec16xU8 y_pred = slli_i64<8>(loadu_64<Vec16xU8>(y_pred_ptr + i));

                size_t x_bytes_used = x_pred_ptr[i + 7];
                size_t y_bytes_used = y_pred_ptr[i + 7];

                g_ptr += x_bytes_used;
                y_ptr += y_bytes_used;

                x_pred += swizzlev_u8(v_decode_op_x_imm, f_data);
                y_pred += swizzlev_u8(v_decode_op_y_imm, f_data);

                x_pred += make128_u8<Vec16xU8>(uint8_t(16u - uint32_t(x_bytes_used)));
                y_pred += make128_u8<Vec16xU8>(uint8_t(16u - uint32_t(y_bytes_used)));

                x_pred = interleave_lo_u8(x_pred, x_pred);
                y_pred = interleave_lo_u8(y_pred, y_pred);

                x_pred += v_lo_hi_pred_inc;
                y_pred += v_lo_hi_pred_inc;

                Vec8xI16 x_vertices_0 = vec_i16(swizzlev_u8(loadu<Vec16xU8>(g_ptr - 16), x_pred));
                Vec8xI16 y_vertices_0 = vec_i16(swizzlev_u8(loadu<Vec16xU8>(y_ptr - 16), y_pred));

                x_pred = srai_i16<15>(slli_i16<2>(x_pred));
                y_pred = srai_i16<15>(slli_i16<2>(y_pred));

                x_vertices_0 = (x_vertices_0 ^ vec_i16(x_pred)) - vec_i16(x_pred);
                y_vertices_0 = (y_vertices_0 ^ vec_i16(y_pred)) - vec_i16(y_pred);

                storea(decoded_vertex_array + i + 0, interleave_lo_u16(x_vertices_0, y_vertices_0));
                storea(decoded_vertex_array + i + 4, interleave_hi_u16(x_vertices_0, y_vertices_0));
              }
            }
          }

          BL_OPENTYPE_PROFILE_SECTION(trace_ot_glyf.loop3_decode_vertices.end(t_loop3_start))

          // 4. Loop - Emit Path Data
          // ------------------------
          BL_OPENTYPE_PROFILE_SECTION(auto t_loop4_start = trace_ot_glyf.loop4_emit_path.start())

          // Affine transform applied to each vertex.
          //
          // NOTE: Compilers are not able to vectorize the computations efficiently, so we do it instead.
          const BLMatrix2D& glyph_transform = compound_data[compound_level].transform;
          Vec2xF64 m00_m11 = make128_f64(glyph_transform.m11, glyph_transform.m00);
          Vec2xF64 m10_m01 = make128_f64(glyph_transform.m01, glyph_transform.m10);
          Vec2xF64 current_pt = make128_f64(glyph_transform.m21, glyph_transform.m20);
          Vec2xF64 half = make128_f64(0.5);

          // Current vertex index in TT sense, advanced until `tt_vertex_count`, which must be end index of the last contour.
          size_t i = 0;

          for (size_t contour_index = 0; contour_index < contour_count; contour_index++) {
            size_t i_end = size_t(contour_array[contour_index].value()) + 1;
            if (BL_UNLIKELY(i_end <= i || i_end > tt_vertex_count)) {
              goto InvalidData;
            }

            // We do the first vertex here as we want to emit 'MoveTo' and we want to remember it for a possible off-curve
            // start. Currently this means there is some code duplicated for move-to and for other commands, unfortunately.
            uint32_t f = f_data_ptr[i];
            current_pt += transform_decoded_vertex(decoded_vertex_array + i, m00_m11, m10_m01);

            if (BL_UNLIKELY(++i >= i_end)) {
              continue;
            }

            // Contains the initial 'MoveTo'.
            Vec2xF64 initial_pt = current_pt;
            // We need to be able to handle a case in which the contour data starts off-curve.
            BLPoint* off_curve_start_vertex_ptr = nullptr;

            appender._append_simple_command(BL_PATH_CMD_MOVE);
            if (!(f & kVFOffCurve)) {
              append_vertex_1x(appender, current_pt);
            }
            else {
              off_curve_start_vertex_ptr = appender._vertex_ptr;
            }

            size_t i_end_minus_3 = axl::sub_saturate<size_t>(i_end, 3);

            while (i < i_end_minus_3) {
              Vec2xF64 d0 = transform_decoded_vertex(decoded_vertex_array + i + 0, m00_m11, m10_m01);
              Vec2xF64 d1 = transform_decoded_vertex(decoded_vertex_array + i + 1, m00_m11, m10_m01);
              Vec2xF64 d2 = transform_decoded_vertex(decoded_vertex_array + i + 2, m00_m11, m10_m01);
              Vec2xF64 d3 = transform_decoded_vertex(decoded_vertex_array + i + 3, m00_m11, m10_m01);

              f = axl::loadu_u32(f_data_ptr + i);
              uint32_t cmds = path_cmds_from_vec_flags(f);

              Vec2xF64 m0 = madd(d0, half, current_pt);
              Vec2xF64 p0 = current_pt + d0;

              appender._append_simple_command_if(uint8_t(cmds & 0xFF), (f & (kVFEmitCommand << 0)) != 0);
              append_vertex_if(appender, m0, (f & (kVFEmitMidpoint << 0)) != 0);
              append_vertex_1x(appender, p0);

              Vec2xF64 m1 = madd(d1, half, p0);
              Vec2xF64 p1 = p0 + d1;

              appender._append_simple_command_if(uint8_t((cmds >> 8) & 0xFF), (f & (kVFEmitCommand << 8)) != 0);
              append_vertex_if(appender, m1, (f & (kVFEmitMidpoint << 8)) != 0);
              append_vertex_1x(appender, p1);

              Vec2xF64 m2 = madd(d2, half, p1);
              Vec2xF64 p2 = p1 + d2;

              appender._append_simple_command_if(uint8_t((cmds >> 16) & 0xFF), (f & (kVFEmitCommand << 16)) != 0);
              append_vertex_if(appender, m2, (f & (kVFEmitMidpoint << 16)) != 0);
              append_vertex_1x(appender, p2);

              Vec2xF64 m3 = madd(d3, half, p2);
              current_pt = p2 + d3;

              appender._append_simple_command_if(uint8_t((cmds >> 24) & 0xFF), (f & (kVFEmitCommand << 24)) != 0);
              append_vertex_if(appender, m3, (f & (kVFEmitMidpoint << 24)) != 0);
              append_vertex_1x(appender, current_pt);

              i += 4;
            }

            while (i < i_end) {
              Vec2xF64 d0 = transform_decoded_vertex(decoded_vertex_array + i, m00_m11, m10_m01);
              Vec2xF64 m0 = madd(d0, half, current_pt);

              f = f_data_ptr[i];
              current_pt = current_pt + d0;

              uint32_t cmd = path_cmds_from_vec_flags(f);
              appender._append_simple_command_if(cmd, (f & kVFEmitCommand) != 0);

              append_vertex_if(appender, m0, (f & kVFEmitMidpoint) != 0);
              append_vertex_1x(appender, current_pt);

              i++;
            }

            f = f_data_ptr[i - 1];

            if (off_curve_start_vertex_ptr) {
              // Final point is the first vertex added to this contour.
              Vec2xF64 final_pt = SIMD::loadu_128<Vec2xF64>(off_curve_start_vertex_ptr);

              // The path ends off-curve in TT data, so append the missing end-point of the last QUAD command.
              if (f & kVFOffCurve) {
                append_vertex_1x(appender, (current_pt + initial_pt) * half);
                final_pt = (initial_pt + final_pt) * half;
              }

              // Append a QUAD command that goes to the beginning of the path as it started off-curve.
              append_quad(appender, initial_pt, final_pt);
            }
            else if (f & kVFOffCurve) {
              // The path ends off-curve in TT data, so append the missing end-point of the last QUAD command.
              append_vertex_1x(appender, initial_pt);
            }

            appender.close();
          }
          appender.done(out);

          BL_OPENTYPE_PROFILE_SECTION(trace_ot_glyf.loop4_emit_path.end(t_loop4_start))
        }
      }
      else if (contour_count_signed == -1) {
        BL_OPENTYPE_PROFILE_SECTION(trace_ot_glyf.glyf_compound_count.increment())

        g_ptr += sizeof(GlyfTable::GlyphData);
        remaining_size -= sizeof(GlyfTable::GlyphData);

        if (BL_UNLIKELY(++compound_level >= CompoundEntry::kMaxLevel)) {
          goto InvalidData;
        }

        goto ContinueCompound;
      }
      else {
        // Cannot be less than -1, only -1 specifies compound glyph, lesser value is invalid according to the
        // specification.
        if (BL_UNLIKELY(contour_count_signed < -1))
          goto InvalidData;

        // Otherwise the glyph has no contours.
      }
    }

    // Compound Glyph
    // --------------

    if (compound_level) {
      while (!(compound_data[compound_level].compound_flags & Compound::kMoreComponents))
        if (--compound_level == 0)
          break;

      if (compound_level) {
        g_ptr = compound_data[compound_level].g_ptr;
        remaining_size = compound_data[compound_level].remaining_size;

        // The structure that we are going to read is as follows:
        //
        //   [Header]
        //     uint16_t flags;
        //     uint16_t glyph_id;
        //
        //   [Translation]
        //     a) int8_t arg1/arg2;
        //     b) int16_t arg1/arg2;
        //
        //   [Scale/Affine]
        //     a) <None>
        //     b) int16_t scale;
        //     c) int16_t scale_x, scale_y;
        //     d) int16_t m00, m01, m10, m11;

ContinueCompound:
        {
          uint32_t flags;
          int arg1, arg2;
          axl::OverflowFlag of{};

          remaining_size = axl::sub_overflow<size_t>(remaining_size, 6, of);
          if (BL_UNLIKELY(axl::did_overflow(of))) {
            goto InvalidData;
          }

          flags = axl::loadu_u16_be(g_ptr);
          glyph_id = axl::loadu_u16_be(g_ptr + 2);
          if (BL_UNLIKELY(glyph_id >= ot_face_impl->face_info.glyph_count)) {
            goto InvalidData;
          }

          arg1 = axl::load_i8(g_ptr + 4);
          arg2 = axl::load_i8(g_ptr + 5);
          g_ptr += 6;

          if (flags & Compound::kArgsAreWords) {
            remaining_size = axl::sub_overflow<size_t>(remaining_size, 2, of);
            if (BL_UNLIKELY(axl::did_overflow(of))) {
              goto InvalidData;
            }

            arg1 = axl::shl(arg1, 8) | (arg2 & 0xFF);
            arg2 = axl::loadu_i16_be(g_ptr);
            g_ptr += 2;
          }

          if (!(flags & Compound::kArgsAreXYValues)) {
            // This makes them unsigned.
            arg1 &= 0xFFFFu;
            arg2 &= 0xFFFFu;

            // TODO: [OpenType] GLYF ArgsAreXYValues not implemented. I don't know how atm.
          }

          constexpr double kScaleF2x14 = 1.0 / 16384.0;

          BLMatrix2D& cm = compound_data[compound_level].transform;
          cm.reset(1.0, 0.0, 0.0, 1.0, double(arg1), double(arg2));

          if (flags & Compound::kAnyCompoundScale) {
            if (flags & Compound::kWeHaveScale) {
              // Simple scaling:
              //   [Sc, 0]
              //   [0, Sc]
              remaining_size = axl::sub_overflow<size_t>(remaining_size, 2, of);
              if (BL_UNLIKELY(axl::did_overflow(of))) {
                goto InvalidData;
              }

              double scale = double(axl::loadu_i16_be(g_ptr)) * kScaleF2x14;
              cm.m00 = scale;
              cm.m11 = scale;
              g_ptr += 2;
            }
            else if (flags & Compound::kWeHaveScaleXY) {
              // Simple scaling:
              //   [Sx, 0]
              //   [0, Sy]
              remaining_size = axl::sub_overflow<size_t>(remaining_size, 4, of);
              if (BL_UNLIKELY(axl::did_overflow(of))) {
                goto InvalidData;
              }

              cm.m00 = double(axl::loadu_i16_be(g_ptr + 0)) * kScaleF2x14;
              cm.m11 = double(axl::loadu_i16_be(g_ptr + 2)) * kScaleF2x14;
              g_ptr += 4;
            }
            else {
              // Affine case:
              //   [A, B]
              //   [C, D]
              remaining_size = axl::sub_overflow<size_t>(remaining_size, 8, of);
              if (BL_UNLIKELY(axl::did_overflow(of))) {
                goto InvalidData;
              }

              cm.m00 = double(axl::loadu_i16_be(g_ptr + 0)) * kScaleF2x14;
              cm.m01 = double(axl::loadu_i16_be(g_ptr + 2)) * kScaleF2x14;
              cm.m10 = double(axl::loadu_i16_be(g_ptr + 4)) * kScaleF2x14;
              cm.m11 = double(axl::loadu_i16_be(g_ptr + 6)) * kScaleF2x14;
              g_ptr += 8;
            }

            // Translation scale should only happen when `kArgsAreXYValues` is set. The default behavior according to
            // the specification is `kUnscaledComponentOffset`, which can be overridden by `kScaledComponentOffset`.
            // However, if both or neither are set then the behavior is the same as `kUnscaledComponentOffset`.
            if ((flags & (Compound::kArgsAreXYValues | Compound::kAnyCompoundOffset    )) ==
                         (Compound::kArgsAreXYValues | Compound::kScaledComponentOffset)) {
              // This is what FreeType does and what's not 100% according to the specification. However, according to
              // FreeType this would produce much better offsets so we will match FreeType instead of following the
              // specification.
              cm.m20 *= Geometry::magnitude(BLPoint(cm.m00, cm.m01));
              cm.m21 *= Geometry::magnitude(BLPoint(cm.m10, cm.m11));
            }
          }

          compound_data[compound_level].g_ptr = g_ptr;
          compound_data[compound_level].remaining_size = remaining_size;
          compound_data[compound_level].compound_flags = flags;
          TransformDetail::multiply(cm, cm, compound_data[compound_level - 1].transform);
          continue;
        }
      }
    }

    break;
  }

  *contour_count_out = contour_count_total;
  return BL_SUCCESS;

InvalidData:
  *contour_count_out = 0;
  return bl_make_error(BL_ERROR_INVALID_DATA);
}

} // {anonymous}
} // {bl::OpenType::GlyfImpl}

//! \}
//! \endcond

#endif // BLEND2D_OPENTYPE_OT_GLYF_SIMD_IMPL_P_H_INCLUDED
