// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>
#if !defined(BL_BUILD_NO_CODECS)

#include <blend2d/core/runtime_p.h>
#include <blend2d/codec/png_ops_p.h>

namespace bl::Png::Ops {

FunctionTable func_table;

// Codecs - Png - Ops - Filter
// =========================

static BLResult BL_CDECL filter_impl(uint8_t* p, uint32_t bpp, uint32_t bpl, uint32_t h, uint32_t filter_type) noexcept {
  BL_ASSERT(bpp > 0u);
  BL_ASSERT(bpl > bpp);
  BL_ASSERT(h > 0u);

  if (filter_type > kFilterTypeDefault) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  if (filter_type == kFilterTypeDefault) {
    filter_type = kFilterTypePaeth;
  }

  uint32_t row_bytes = bpl - 1u;
  uint32_t y = h - 1u;
  p += size_t(y) * bpl;

  // Work backwards so the left pixel and the row above still contain original
  // samples. This avoids allocating another image or preserving a previous row.
  for (;;) {
    p[0] = uint8_t(filter_type);
    uint8_t* d = p + 1;
    const uint8_t* u = y ? d - bpl : nullptr;
    uint32_t row_filter = y ? filter_type : simplify_filter_of_first_row(filter_type);

    switch (row_filter) {
      case kFilterTypeSub:
        for (uint32_t i = row_bytes; i > bpp;) {
          --i;
          d[i] = uint8_t(uint32_t(d[i]) - d[i - bpp]);
        }
        break;

      case kFilterTypeUp:
        for (uint32_t i = 0; i < row_bytes; i++) {
          d[i] = uint8_t(uint32_t(d[i]) - u[i]);
        }
        break;

      case kFilterTypeAvg:
        for (uint32_t i = row_bytes; i > bpp;) {
          --i;
          d[i] = uint8_t(uint32_t(d[i]) - apply_avg_filter(d[i - bpp], u[i]));
        }
        for (uint32_t i = 0; i < bpp; i++) {
          d[i] = uint8_t(uint32_t(d[i]) - (u[i] >> 1));
        }
        break;

      case kFilterTypePaeth:
        for (uint32_t i = row_bytes; i > bpp;) {
          --i;
          d[i] = uint8_t(uint32_t(d[i]) - apply_paeth_filter(d[i - bpp], u[i], u[i - bpp]));
        }
        for (uint32_t i = 0; i < bpp; i++) {
          d[i] = uint8_t(uint32_t(d[i]) - u[i]);
        }
        break;

      case kFilterTypeAvg0:
        for (uint32_t i = row_bytes; i > bpp;) {
          --i;
          d[i] = uint8_t(uint32_t(d[i]) - (d[i - bpp] >> 1));
        }
        break;

      case kFilterTypeNone:
        break;
    }

    if (y == 0u) {
      break;
    }

    y--;
    p -= bpl;
  }

  return BL_SUCCESS;
}

static BLResult BL_CDECL filter_with_masks_impl(uint8_t* p, uint32_t bpp, uint32_t bpl, uint32_t h, uint32_t filter_type, uint64_t* masks) noexcept {
  BL_PROPAGATE(filter_impl(p, bpp, bpl, h, filter_type));
  FastEncoder::build_repeat_masks(p, bpl, h, masks);
  return BL_SUCCESS;
}

// Codecs - Png - Ops - Inverse Filter
// ===================================

static BLResult BL_CDECL inverse_filter_impl(uint8_t* p, uint32_t bpp, uint32_t bpl, uint32_t h) noexcept {
  BL_ASSERT(bpp > 0u);
  BL_ASSERT(bpl > 1u);
  BL_ASSERT(h   > 0u);

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
    uint32_t i;

    switch (filter_type) {
      case kFilterTypeSub: {
        for (i = bpl - bpp; i != 0; i--, p++)
          p[bpp] = apply_sum_filter(p[bpp], p[0]);

        p += bpp;
        break;
      }

      case kFilterTypeUp: {
        BL_ASSERT(u != nullptr);
        for (i = bpl; i != 0; i--, p++, u++)
          p[0] = apply_sum_filter(p[0], u[0]);
        break;
      }

      case kFilterTypeAvg: {
        BL_ASSERT(u != nullptr);
        for (i = 0; i < bpp; i++)
          p[i] = apply_sum_filter(p[i], u[i] >> 1);

        u += bpp;
        for (i = bpl - bpp; i != 0; i--, p++, u++)
          p[bpp] = apply_sum_filter(p[bpp], apply_avg_filter(p[0], u[0]));

        p += bpp;
        break;
      }

      case kFilterTypePaeth: {
        BL_ASSERT(u != nullptr);
        for (i = 0; i < bpp; i++)
          p[i] = apply_sum_filter(p[i], u[i]);

        for (i = bpl - bpp; i != 0; i--, p++, u++)
          p[bpp] = apply_sum_filter(p[bpp], apply_paeth_filter(p[0], u[bpp], u[0]));

        p += bpp;
        break;
      }

      case kFilterTypeAvg0: {
        for (i = bpl - bpp; i != 0; i--, p++)
          p[bpp] = apply_sum_filter(p[bpp], p[0] >> 1);

        p += bpp;
        break;
      }

      case kFilterTypeNone:
      default:
        p += bpl;
        break;
    }

    if (--y == 0)
      break;

    u = p - bpl;
    filter_type = *p++;

    if (filter_type >= kFilterTypeCount)
      filter_type = kFilterTypeNone;
  }

  return BL_SUCCESS;
}

void init_func_table_ref(FunctionTable& ft) noexcept {
  ft.encode_rows_fast = FastEncoder::encode_rows_ref;
  ft.write_header_fast = FastEncoder::write_header_ref;
  ft.filter[1] = filter_impl;
  ft.filter[2] = filter_impl;
  ft.filter[3] = filter_impl;
  ft.filter[4] = filter_impl;
  ft.filter[6] = filter_impl;
  ft.filter[8] = filter_impl;

  ft.filter_with_masks[1] = filter_with_masks_impl;
  ft.filter_with_masks[2] = filter_with_masks_impl;
  ft.filter_with_masks[3] = filter_with_masks_impl;
  ft.filter_with_masks[4] = filter_with_masks_impl;
  ft.filter_with_masks[6] = filter_with_masks_impl;
  ft.filter_with_masks[8] = filter_with_masks_impl;

  ft.inverse_filter[1] = inverse_filter_impl;
  ft.inverse_filter[2] = inverse_filter_impl;
  ft.inverse_filter[3] = inverse_filter_impl;
  ft.inverse_filter[4] = inverse_filter_impl;
  ft.inverse_filter[6] = inverse_filter_impl;
  ft.inverse_filter[8] = inverse_filter_impl;
}

void init_func_table([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  // Initialize optimized PNG functions.
  FunctionTable& ft = func_table;

#if !defined(BL_BUILD_OPT_SSE2) && !defined(BL_BUILD_OPT_ASIMD)
  init_func_table_ref(ft);
#endif

#if defined(BL_BUILD_OPT_SSE2)
  if (bl_runtime_has_sse2(rt)) {
    init_func_table_sse2(ft);
  }
  else {
    init_func_table_ref(ft);
  }
#endif // BL_BUILD_OPT_SSE2

#if defined(BL_BUILD_OPT_SSSE3)
  if (bl_runtime_has_ssse3(rt)) {
    init_fast_encoder_ssse3(ft);
  }
#endif // BL_BUILD_OPT_SSSE3

#if defined(BL_BUILD_OPT_AVX)
  if (bl_runtime_has_avx(rt)) {
    init_func_table_avx(ft);
  }
#endif // BL_BUILD_OPT_AVX

#if defined(BL_BUILD_OPT_AVX512_V2)
  if (bl_runtime_has_avx512_v2(rt)) {
    init_fast_encoder_avx512_v2(ft);
  }
#endif // BL_BUILD_OPT_AVX512_V2

#if defined(BL_BUILD_OPT_ASIMD)
  if (bl_runtime_has_asimd(rt)) {
    init_func_table_asimd(ft);
  }
  else {
    init_func_table_ref(ft);
  }
#endif // BL_BUILD_OPT_ASIMD
}

} // {bl::Png::Ops}

#endif // !BL_BUILD_NO_CODECS
