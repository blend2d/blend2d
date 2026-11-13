// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if !defined(BL_BUILD_NO_CODECS) && defined(BL_TEST)

#include <blend2d/axl/commons.h>
#include <blend2d/axl/mem_alloc.h>
#include <blend2d/axl/mem_ops.h>
#include <blend2d/core/random.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/codec/png_codec_p.h>
#include <blend2d/codec/png_ops_p.h>

// Codecs - Png - Tests
// ====================

namespace bl::Png::Tests {

static const uint32_t png_bpp_data[] = { 1, 2, 3, 4, 6, 8 };

static const char* png_filter_names[] = { "None", "Sub", "Up", "Avg", "Paeth", "Random" };

static const uint8_t buffer_overrun_guard[16] = {
  0xFE, 0xAF, 0x10, 0x00, 0xFF, 0x01, 0x02, 0x03,
  0x04, 0xAA, 0xFA, 0xBB, 0xAA, 0x99, 0x88, 0x77
};

static void fill_random_image(uint8_t* p, uint32_t w, uint32_t h, uint32_t bpp, uint32_t filter, BLRandom rnd) noexcept {
  w *= bpp;

  for (uint32_t y = 0; y < h; y++) {
    if (filter < kFilterTypeCount) {
      *p++ = static_cast<uint8_t>(filter);
    }
    else {
      *p++ = uint8_t(rnd.next_uint32() % kFilterTypeCount);
    }

    for (uint32_t x = 0; x < w; x++) {
      *p++ = uint8_t(rnd.next_uint32() >> 24u);
    }
  }
}

// Use PNG's original Paeth definition so the oracle is independent of the
// optimized scalar and vector predictors used by the implementation.
static uint32_t reference_paeth(uint32_t a, uint32_t b, uint32_t c) noexcept {
  int32_t p = int32_t(a) + int32_t(b) - int32_t(c);
  uint32_t pa = uint32_t(bl_abs(p - int32_t(a)));
  uint32_t pb = uint32_t(bl_abs(p - int32_t(b)));
  uint32_t pc = uint32_t(bl_abs(p - int32_t(c)));
  return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

static void reference_filter(uint8_t* dst, const uint8_t* src, uint32_t bpp, uint32_t bpl, uint32_t h, uint32_t filter) noexcept {
  if (filter == kFilterTypeDefault)
    filter = kFilterTypePaeth;

  for (uint32_t y = 0; y < h; y++) {
    const uint8_t* row = src + size_t(y) * bpl + 1u;
    const uint8_t* prev = y ? row - bpl : nullptr;
    *dst++ = uint8_t(filter);

    for (uint32_t x = 0; x < bpl - 1u; x++) {
      uint32_t a = x >= bpp ? row[x - bpp] : 0u;
      uint32_t b = prev ? prev[x] : 0u;
      uint32_t c = prev && x >= bpp ? prev[x - bpp] : 0u;
      uint32_t predicted = 0u;

      switch (filter) {
        case kFilterTypeSub: predicted = a; break;
        case kFilterTypeUp: predicted = b; break;
        case kFilterTypeAvg: predicted = (a + b) / 2u; break;
        case kFilterTypePaeth: predicted = reference_paeth(a, b, c); break;
      }

      *dst++ = uint8_t(uint32_t(row[x]) - predicted);
    }
  }
}

static void test_forward_filter(const Ops::FunctionTable& ft) noexcept {
  static const uint32_t widths[] = {
    1, 2, 3, 4, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65,
    127, 128, 129, 191, 192, 193
  };

  static const uint32_t heights[] = { 1, 2, 5 };
  static const uint32_t misalignments[] = { 0, 1, 2, 3, 7, 15, 16, 17, 31, 32, 33, 63 };
  constexpr uint32_t kMaxSize = (193 * 8 + 1) * 5;
  constexpr uint32_t kMaxMaskWords = ((193 * 8 + 63) / 64) * 5;
  constexpr uint64_t kMaskGuard = 0xA719F48D32BE650Cu;

  uint8_t input[kMaxSize];
  uint8_t expected[kMaxSize];
  uint8_t output_base[kMaxSize + 256];
  uint64_t expected_masks[kMaxMaskWords];
  uint64_t mask_storage[kMaxMaskWords + 2u];

  BLRandom rnd(0x0F17E25A12345678u);

  Ops::FunctionTable reference;
  Ops::init_func_table_ref(reference);

  for (uint32_t w : widths) {
    for (uint32_t bpp : png_bpp_data) {
      for (uint32_t h : heights) {
        uint32_t bpl = w * bpp + 1u;
        uint32_t size = bpl * h;

        size_t mask_stride = (size_t(bpl - 1u) + 63u) / 64u;
        size_t mask_count = mask_stride * h;

        uint64_t* masks = mask_storage + 1u;
        for (uint32_t pattern = 0; pattern < 4; pattern++) {
          for (uint32_t y = 0; y < h; y++) {
            uint8_t* row = input + y * bpl;
            row[0] = 0xA5u;
            for (uint32_t x = 1; x < bpl; x++) {
              row[x] = pattern == 0 ? uint8_t(rnd.next_uint32() >> 24u) :
                       pattern == 1 ? 0u :
                       pattern == 2 ? uint8_t((x + y) & 1u ? 255u : 0u) : uint8_t((x - 1u) / (y + 1u));
            }
          }

          for (uint32_t filter = 0; filter <= kFilterTypeDefault; filter++) {
            reference_filter(expected, input, bpp, bpl, h, filter);
            memset(expected_masks, 0, mask_count * sizeof(uint64_t));
            for (uint32_t y = 0; y < h; y++) {
              const uint8_t* row = expected + y * bpl + 1u;
              for (uint32_t x = 0; x + 1u < bpl - 1u; x++) {
                if (row[x] == row[x + 1u]) {
                  expected_masks[y * mask_stride + x / 64u] |= uint64_t(1) << (x % 64u);
                }
              }
            }

            for (uint32_t misalignment : misalignments) {
              uint8_t* output = axl::align_up(output_base + 64, 64) + misalignment;
              memcpy(output, input, size);
              memcpy(output - sizeof(buffer_overrun_guard), buffer_overrun_guard, sizeof(buffer_overrun_guard));
              memcpy(output + size, buffer_overrun_guard, sizeof(buffer_overrun_guard));

              EXPECT_OK(ft.filter[bpp](output, bpp, bpl, h, filter));
              EXPECT_EQ(memcmp(output, expected, size), 0).message(
                "Forward filter mismatch: W=%u H=%u Bpp=%u Filter=%u Pattern=%u Misalignment=%u",
                w, h, bpp, filter, pattern, misalignment);
              EXPECT_EQ(memcmp(output - sizeof(buffer_overrun_guard), buffer_overrun_guard, sizeof(buffer_overrun_guard)), 0).message(
                "Forward filter underrun: W=%u H=%u Bpp=%u Filter=%u Misalignment=%u", w, h, bpp, filter, misalignment);
              EXPECT_EQ(memcmp(output + size, buffer_overrun_guard, sizeof(buffer_overrun_guard)), 0).message(
                "Forward filter overrun: W=%u H=%u Bpp=%u Filter=%u Misalignment=%u", w, h, bpp, filter, misalignment);

              EXPECT_OK(reference.inverse_filter[bpp](output, bpp, bpl, h));
              // Inverse filtering leaves the row markers in place.
              for (uint32_t y = 0; y < h; y++)
                output[y * bpl] = input[y * bpl];
              EXPECT_EQ(memcmp(output, input, size), 0).message(
                "Forward/inverse roundtrip mismatch: W=%u H=%u Bpp=%u Filter=%u Misalignment=%u", w, h, bpp, filter, misalignment);

              memcpy(output, input, size);
              // A producer must replace every mask word, including the final
              // equality bit and padding, without touching adjacent rows.
              memset(masks, 0xFF, mask_count * sizeof(uint64_t));
              mask_storage[0] = kMaskGuard;
              masks[mask_count] = kMaskGuard;

              EXPECT_OK(ft.filter_with_masks[bpp](output, bpp, bpl, h, filter, masks));
              EXPECT_EQ(memcmp(output, expected, size), 0).message(
                "Masked forward filter mismatch: W=%u H=%u Bpp=%u Filter=%u Pattern=%u Misalignment=%u",
                w, h, bpp, filter, pattern, misalignment);
              EXPECT_EQ(memcmp(masks, expected_masks, mask_count * sizeof(uint64_t)), 0).message(
                "Forward filter mask mismatch: W=%u H=%u Bpp=%u Filter=%u Pattern=%u Misalignment=%u",
                w, h, bpp, filter, pattern, misalignment);
              EXPECT_EQ(mask_storage[0], kMaskGuard);
              EXPECT_EQ(masks[mask_count], kMaskGuard);
              EXPECT_EQ(memcmp(output - sizeof(buffer_overrun_guard), buffer_overrun_guard, sizeof(buffer_overrun_guard)), 0);
              EXPECT_EQ(memcmp(output + size, buffer_overrun_guard, sizeof(buffer_overrun_guard)), 0);
            }
          }
        }
      }
    }
  }

  static const uint32_t invalid_filters[] = { kFilterTypeDefault + 1u, 0xFFFFFFFFu };
  for (uint32_t bpp : png_bpp_data) {
    uint32_t bpl = 3u * bpp + 1u;
    uint32_t size = bpl * 2u;

    for (uint32_t i = 0; i < size; i++) {
      input[i] = uint8_t(rnd.next_uint32() >> 24u);
    }

    memcpy(expected, input, size);
    for (uint32_t filter : invalid_filters) {
      EXPECT_EQ(ft.filter[bpp](input, bpp, bpl, 2u, filter), BL_ERROR_INVALID_VALUE);
      EXPECT_EQ(memcmp(input, expected, size), 0).message("Invalid filter modified the image: Bpp=%u Filter=%u", bpp, filter);

      memset(mask_storage, 0xA7, sizeof(mask_storage));
      EXPECT_EQ(ft.filter_with_masks[bpp](input, bpp, bpl, 2u, filter, mask_storage + 1u), BL_ERROR_INVALID_VALUE);
      EXPECT_EQ(memcmp(input, expected, size), 0).message("Invalid masked filter modified the image: Bpp=%u Filter=%u", bpp, filter);

      for (uint64_t mask : mask_storage) {
        EXPECT_EQ(mask, uint64_t(0xA7A7A7A7A7A7A7A7u)).message("Invalid filter modified masks: Bpp=%u Filter=%u", bpp, filter);
      }
    }
  }
}

#ifdef BL_BUILD_OPT_AVX512_V2
static void test_forward_filter_mask_tails(const Ops::FunctionTable& ft) noexcept {
  constexpr uint32_t kMaxRowSize = 193;
  constexpr uint32_t kHeight = 3;
  constexpr uint32_t kMaxSize = (kMaxRowSize + 1u) * kHeight;
  constexpr uint32_t kMaxMaskWords = ((kMaxRowSize + 63u) / 64u) * kHeight;
  constexpr uint64_t kMaskGuard = 0xA719F48D32BE650Cu;
  uint8_t input[kMaxSize];
  uint8_t output_base[kMaxSize + 256u];
  uint64_t expected_masks[kMaxMaskWords];
  uint64_t mask_storage[kMaxMaskWords + 2u];
  BLRandom rnd(0xB319A6572841u);

  // Cover every 64-byte tail and input alignment, including whole words whose
  // final comparison must exclude the next row's identical filter marker.
  for (uint32_t row_size = 1; row_size <= kMaxRowSize; row_size++) {
    uint32_t bpl = row_size + 1u;
    uint32_t size = bpl * kHeight;
    size_t stride = (size_t(row_size) + 63u) / 64u;
    size_t mask_count = stride * kHeight;
    uint64_t* masks = mask_storage + 1u;

    for (uint32_t pattern = 0; pattern < 4; pattern++) {
      memset(expected_masks, 0, mask_count * sizeof(uint64_t));

      for (uint32_t y = 0; y < kHeight; y++) {
        uint8_t* row = input + y * bpl + 1u;
        row[-1] = uint8_t(kFilterTypeNone);
        for (uint32_t x = 0; x < row_size; x++) {
          row[x] = pattern == 0 ? 0u :
                   pattern == 1 ? uint8_t(x & 1u) :
                   pattern == 2 ? uint8_t((x + y) / 3u) : uint8_t(rnd.next_uint32() >> 24u);
        }

        for (uint32_t x = 0; x + 1u < row_size; x++) {
          if (row[x] == row[x + 1u])
            expected_masks[y * stride + x / 64u] |= uint64_t(1) << (x % 64u);
        }
      }

      for (uint32_t misalignment = 0; misalignment < 64u; misalignment++) {
        uint8_t* output = axl::align_up(output_base + 64u, 64u) + misalignment;

        memcpy(output, input, size);
        memcpy(output - sizeof(buffer_overrun_guard), buffer_overrun_guard, sizeof(buffer_overrun_guard));
        memcpy(output + size, buffer_overrun_guard, sizeof(buffer_overrun_guard));

        memset(masks, 0xFF, mask_count * sizeof(uint64_t));
        mask_storage[0] = kMaskGuard;
        masks[mask_count] = kMaskGuard;

        EXPECT_OK(ft.filter_with_masks[1](output, 1u, bpl, kHeight, kFilterTypeNone, masks));
        EXPECT_EQ(memcmp(output, input, size), 0).message(
          "Mask tail filter mismatch: RowSize=%u Pattern=%u Misalignment=%u", row_size, pattern, misalignment);
        EXPECT_EQ(memcmp(masks, expected_masks, mask_count * sizeof(uint64_t)), 0).message(
          "Mask tail mismatch: RowSize=%u Pattern=%u Misalignment=%u", row_size, pattern, misalignment);
        EXPECT_EQ(mask_storage[0], kMaskGuard);
        EXPECT_EQ(masks[mask_count], kMaskGuard);
        EXPECT_EQ(memcmp(output - sizeof(buffer_overrun_guard), buffer_overrun_guard, sizeof(buffer_overrun_guard)), 0);
        EXPECT_EQ(memcmp(output + size, buffer_overrun_guard, sizeof(buffer_overrun_guard)), 0);
      }
    }
  }
}
#endif
static void decode_png_scanlines(BLArray<uint8_t>& scanlines, const BLArray<uint8_t>& png) noexcept {
  BLArray<uint8_t> idat;
  for (size_t offset = 8; offset + 12u <= png.size();) {
    uint32_t size = axl::loadu_u32_be(png.data() + offset);
    EXPECT_LE(size_t(size), png.size() - offset - 12u);
    if (!memcmp(png.data() + offset + 4u, "IDAT", 4u)) {
      EXPECT_OK(idat.append_data(png.data() + offset + 8u, size));
    }
    offset += size_t(size) + 12u;
  }

  Compression::Deflate::Decoder decoder;
  EXPECT_OK(decoder.init(Compression::Deflate::FormatType::kZlib));
  EXPECT_OK(decoder.decode(scanlines, { idat.data(), idat.size() }));
}

static void test_diagonal_runs(Ops::FunctionTable& reference, Ops::FunctionTable& optimized) noexcept {
  static const uint32_t widths[] = { 1, 7, 8, 15, 31, 32, 33, 64, 65, 129 };
  static const uint32_t misalignments[] = { 0, 1, 3, 15, 31, 63 };
  constexpr uint32_t kMaxHeight = 17;
  constexpr uint32_t kMaxSize = (129 * 4 + 1) * kMaxHeight;
  uint8_t input[kMaxSize];
  uint8_t ref_output[kMaxSize];
  uint8_t opt_output_base[kMaxSize + 256];
  BLRandom rnd(0x12345678u);

  for (uint32_t bpp = 3; bpp <= 4; bpp++) {
    for (uint32_t w : widths) {
      uint32_t bpl = w * bpp + 1u;
      for (uint32_t h = 1; h <= kMaxHeight; h++) {
        uint32_t png_size = bpl * h;
        for (uint32_t pattern = 0; pattern < 5; pattern++) {
          fill_random_image(input, w, h, bpp, kFilterTypeNone, rnd);
          for (uint32_t y = 0; y < h; y++) {
            uint32_t filter;
            switch (pattern) {
              case 0: filter = kFilterTypePaeth; break;
              case 1: filter = kFilterTypeAvg; break;
              case 2: filter = y % 5u < 4u ? kFilterTypePaeth : kFilterTypeUp; break;
              case 3: filter = y % 9u < 8u ? kFilterTypePaeth : 255u; break;
              default: filter = y % 8u < 4u ? kFilterTypeAvg : kFilterTypePaeth; break;
            }
            input[y * bpl] = uint8_t(filter);
          }

          memcpy(ref_output, input, png_size);
          reference.inverse_filter[bpp](ref_output, bpp, bpl, h);

          for (uint32_t misalignment : misalignments) {
            uint8_t* opt_output = axl::align_up(opt_output_base, 64) + misalignment;
            memcpy(opt_output, input, png_size);
            memcpy(opt_output + png_size, buffer_overrun_guard, sizeof(buffer_overrun_guard));
            optimized.inverse_filter[bpp](opt_output, bpp, bpl, h);

            EXPECT_EQ(memcmp(ref_output, opt_output, png_size), 0).message(
              "Diagonal filter mismatch: W=%u H=%u Bpp=%u Pattern=%u Misalignment=%u", w, h, bpp, pattern, misalignment);

            EXPECT_EQ(memcmp(opt_output + png_size, buffer_overrun_guard, sizeof(buffer_overrun_guard)), 0).message(
              "Diagonal filter overrun: W=%u H=%u Bpp=%u Pattern=%u Misalignment=%u", w, h, bpp, pattern, misalignment);
          }
        }
      }
    }
  }
}

static void test_avg_low_bpp(Ops::FunctionTable& reference, Ops::FunctionTable& optimized) noexcept {
  // Cover eight-byte blocks, scalar tails, and carry propagation between blocks and rows.
  static const uint32_t widths[] = { 1, 3, 4, 5, 7, 8, 9, 15, 16, 17, 18, 127, 128, 129 };
  static const uint32_t heights[] = { 2, 5 };
  static const uint32_t misalignments[] = { 0, 1, 7, 15, 31, 63 };
  static const uint32_t first_filters[] = { kFilterTypeNone, kFilterTypeAvg };
  static const uint8_t values[] = { 0, 1, 126, 127, 128, 254, 255 };
  constexpr uint32_t kMaxSize = (129 * 2 + 1) * 5;
  uint8_t input[kMaxSize];
  uint8_t ref_output[kMaxSize];
  uint8_t opt_output_base[kMaxSize + 256];

  for (uint32_t bpp = 1; bpp <= 2; bpp++) {
    for (uint32_t w : widths) {
      uint32_t bpl = w * bpp + 1u;
      for (uint32_t h : heights) {
        uint32_t png_size = bpl * h;
        for (uint32_t first_filter : first_filters) {
          for (uint32_t pattern = 0; pattern < 4; pattern++) {
            for (uint32_t y = 0; y < h; y++) {
              uint8_t* row = input + y * bpl;
              row[0] = uint8_t(y ? kFilterTypeAvg : first_filter);
              for (uint32_t x = 0; x < w * bpp; x++) {
                uint32_t pixel = x / bpp;
                uint32_t channel = x % bpp;
                // Different channel periods expose carry or masking between the packed lanes.
                row[x + 1u] = pattern == 0 ? 0u :
                              pattern == 1 ? 255u :
                              pattern == 2 ? uint8_t((pixel + y + channel) & 1u ? 255u : 0u) :
                              values[(pixel * (channel * 2u + 1u) + y * (channel * 2u + 3u)) % BL_ARRAY_SIZE(values)];
              }
            }

            memcpy(ref_output, input, png_size);
            EXPECT_OK(reference.inverse_filter[bpp](ref_output, bpp, bpl, h));

            for (uint32_t misalignment : misalignments) {
              uint8_t* opt_output = axl::align_up(opt_output_base + 64, 64) + misalignment;
              memcpy(opt_output, input, png_size);
              memcpy(opt_output - sizeof(buffer_overrun_guard), buffer_overrun_guard, sizeof(buffer_overrun_guard));
              memcpy(opt_output + png_size, buffer_overrun_guard, sizeof(buffer_overrun_guard));
              EXPECT_OK(optimized.inverse_filter[bpp](opt_output, bpp, bpl, h));

              EXPECT_EQ(memcmp(opt_output, ref_output, png_size), 0).message(
                "AVG inverse filter mismatch: W=%u H=%u Bpp=%u FirstFilter=%u Pattern=%u Misalignment=%u",
                w, h, bpp, first_filter, pattern, misalignment);
              EXPECT_EQ(memcmp(opt_output - sizeof(buffer_overrun_guard), buffer_overrun_guard, sizeof(buffer_overrun_guard)), 0).message(
                "AVG inverse filter underrun: W=%u H=%u Bpp=%u Misalignment=%u", w, h, bpp, misalignment);
              EXPECT_EQ(memcmp(opt_output + png_size, buffer_overrun_guard, sizeof(buffer_overrun_guard)), 0).message(
                "AVG inverse filter overrun: W=%u H=%u Bpp=%u Misalignment=%u", w, h, bpp, misalignment);
            }
          }
        }
      }
    }
  }
}

static void test_simd_impl(
  Ops::FunctionTable& reference,
  Ops::FunctionTable& optimized
) noexcept {
  test_diagonal_runs(reference, optimized);
  test_avg_low_bpp(reference, optimized);

  // First test individual filters with each supported BPP.
  BLRandom rnd(0x12345678u);

  uint32_t min_width = 1;
  uint32_t max_width = 111;
  uint32_t h = 24;

  uint32_t buffer_overrun_guard_size = BL_ARRAY_SIZE(buffer_overrun_guard);

  for (uint32_t w = min_width; w <= max_width; w++) {
    for (uint32_t bpp_index = 0; bpp_index < BL_ARRAY_SIZE(png_bpp_data); bpp_index++) {
      uint32_t bpp = png_bpp_data[bpp_index];
      uint32_t bpl = w * bpp + 1u;
      uint32_t png_size = bpl * h;

      uint8_t* ref_image = axl::mem_alloc<uint8_t>(png_size);
      uint8_t* ref_output = axl::mem_alloc<uint8_t>(png_size);
      uint8_t* opt_output_base = axl::mem_alloc<uint8_t>(png_size + 256);

      EXPECT_NOT_NULL(ref_image);
      EXPECT_NOT_NULL(ref_output);
      EXPECT_NOT_NULL(opt_output_base);

      for (uint32_t filter = 0; filter <= kFilterTypeCount; filter++) {
        fill_random_image(ref_image, w, h, bpp, filter, rnd);

        memcpy(ref_output, ref_image, png_size);
        reference.inverse_filter[bpp](ref_output, bpp, bpl, h);

        for (uint32_t misalignment = 0; misalignment < 64; misalignment++) {
          uint8_t* opt_output = axl::align_up(opt_output_base, 64) + misalignment;

          memcpy(opt_output, ref_image, png_size);
          memcpy(opt_output + png_size, buffer_overrun_guard, buffer_overrun_guard_size);
          optimized.inverse_filter[bpp](opt_output, bpp, bpl, h);

          bool output_match = memcmp(ref_output, opt_output, png_size) == 0;
          EXPECT_TRUE(output_match).message(
            "Invalid Output: W=%u H=%u Bpp=%u Misalignment=%u Filter=%s",
            w,
            h,
            bpp,
            misalignment,
            png_filter_names[filter]
          );

          bool guard_match = memcmp(opt_output + png_size, buffer_overrun_guard, buffer_overrun_guard_size) == 0;
          EXPECT_TRUE(guard_match).message(
            "BUFFER OVERRUN: W=%u H=%u Bpp=%u Misalignment=%u Filter=%s",
            w,
            h,
            bpp,
            misalignment,
            png_filter_names[filter]
          );
        }
      }

      axl::mem_free(opt_output_base);
      axl::mem_free(ref_output);
      axl::mem_free(ref_image);
    }
  }
}

TEST_CASE(codec_png_filter) {
  Ops::FunctionTable reference;
  Ops::init_func_table_ref(reference);

  if (test_runner.test_variation("reference")) {
    test_forward_filter(reference);
  }

#ifdef BL_BUILD_OPT_SSE2
  if (bl_runtime_has_sse2(&bl_runtime_context) && test_runner.test_variation("sse2")) {
    Ops::FunctionTable optimized;
    Ops::init_func_table_sse2(optimized);
    test_forward_filter(optimized);
  }
#endif // BL_BUILD_OPT_SSE2

#ifdef BL_BUILD_OPT_AVX
  if (bl_runtime_has_avx(&bl_runtime_context) && test_runner.test_variation("avx")) {
    Ops::FunctionTable optimized;
    Ops::init_func_table_avx(optimized);
    test_forward_filter(optimized);
  }
#endif // BL_BUILD_OPT_AVX

#ifdef BL_BUILD_OPT_AVX512_V2
  if (bl_runtime_has_avx512_v2(&bl_runtime_context) && test_runner.test_variation("avx512_v2")) {
    Ops::FunctionTable optimized;
    Ops::init_func_table_ref(optimized);
    Ops::init_fast_encoder_avx512_v2(optimized);
    test_forward_filter(optimized);
    test_forward_filter_mask_tails(optimized);
  }
#endif // BL_BUILD_OPT_AVX512_V2

#ifdef BL_BUILD_OPT_ASIMD
  if (bl_runtime_has_asimd(&bl_runtime_context) && test_runner.test_variation("neon")) {
    Ops::FunctionTable optimized;
    Ops::init_func_table_asimd(optimized);
    test_forward_filter(optimized);
  }
#endif // BL_BUILD_OPT_ASIMD
}

TEST_CASE(codec_png_inverse_filter_simd) {
  Ops::FunctionTable reference;
  Ops::init_func_table_ref(reference);

#ifdef BL_BUILD_OPT_SSE2
  if (bl_runtime_has_sse2(&bl_runtime_context) && test_runner.test_variation("sse2")) {
    Ops::FunctionTable optimized;
    init_func_table_sse2(optimized);
    test_simd_impl(reference, optimized);
  }
#endif // BL_BUILD_OPT_SSE2

#ifdef BL_BUILD_OPT_AVX
  if (bl_runtime_has_avx(&bl_runtime_context) && test_runner.test_variation("avx")) {
    Ops::FunctionTable optimized;
    init_func_table_avx(optimized);
    test_simd_impl(reference, optimized);
  }
#endif // BL_BUILD_OPT_AVX

#ifdef BL_BUILD_OPT_ASIMD
  if (bl_runtime_has_asimd(&bl_runtime_context) && test_runner.test_variation("neon")) {
    Ops::FunctionTable optimized;
    init_func_table_asimd(optimized);
    test_simd_impl(reference, optimized);
  }
#endif // BL_BUILD_OPT_ASIMD
}

TEST_CASE(codec_png_encoder_filter) {
  BLImageCodec codec;
  EXPECT_OK(codec.find_by_name("PNG"));
  BLImageEncoder encoder;
  EXPECT_OK(codec.create_encoder(&encoder));

  uint64_t property = 0;
  EXPECT_OK(bl_object_get_property_uint64(&encoder, "filter", SIZE_MAX, &property));
  EXPECT_EQ(property, uint64_t(kFilterTypeDefault));

  for (uint32_t filter = 0; filter <= kFilterTypeDefault; filter++) {
    EXPECT_OK(bl_object_set_property_uint32(&encoder, "filter", SIZE_MAX, filter));
    EXPECT_OK(bl_object_get_property_uint64(&encoder, "filter", SIZE_MAX, &property));
    EXPECT_EQ(property, uint64_t(filter));
  }

  EXPECT_EQ(bl_object_set_property_uint32(&encoder, "filter", SIZE_MAX, kFilterTypeDefault + 1u), BL_ERROR_INVALID_VALUE);
  EXPECT_NE(bl_object_set_property_int64(&encoder, "filter", SIZE_MAX, -1), BL_SUCCESS);
  EXPECT_OK(bl_object_get_property_uint64(&encoder, "filter", SIZE_MAX, &property));
  EXPECT_EQ(property, uint64_t(kFilterTypeDefault));
  EXPECT_OK(bl_object_set_property_uint32(&encoder, "filter", SIZE_MAX, kFilterTypeUp));
  EXPECT_OK(encoder.restart());
  EXPECT_OK(bl_object_get_property_uint64(&encoder, "filter", SIZE_MAX, &property));
  EXPECT_EQ(property, uint64_t(kFilterTypeDefault));

  static const BLFormat formats[] = { BL_FORMAT_XRGB32, BL_FORMAT_PRGB32, BL_FORMAT_A8 };
  constexpr uint32_t kWidth = 33;
  constexpr uint32_t kHeight = 5;

  for (BLFormat format : formats) {
    BLImage image;
    EXPECT_OK(image.create(int(kWidth), int(kHeight), format));
    BLImageData data;
    EXPECT_OK(image.make_mutable(&data));

    for (uint32_t y = 0; y < kHeight; y++) {
      uint8_t* row = static_cast<uint8_t*>(data.pixel_data) + intptr_t(y) * data.stride;
      for (uint32_t x = 0; x < kWidth; x++) {
        if (format == BL_FORMAT_A8) {
          row[x] = uint8_t(x * 7u + y * 17u);
        }
        else {
          uint32_t a = format == BL_FORMAT_PRGB32 ? ((x + y) % 4u) * 85u : 255u;
          uint32_t r = ((x * 7u + y * 13u) & 255u) * a / 255u;
          uint32_t g = ((x * 5u + y * 19u) & 255u) * a / 255u;
          uint32_t b = ((x * 3u + y * 23u) & 255u) * a / 255u;
          axl::storeu_u32(row + x * 4u, (a << 24u) | (r << 16u) | (g << 8u) | b);
        }
      }
    }

    uint32_t bpp = format == BL_FORMAT_A8 ? 1u : format == BL_FORMAT_XRGB32 ? 3u : 4u;
    uint32_t bpl = kWidth * bpp + 1u;
    BLArray<uint8_t> paeth_png;

    for (uint32_t filter = 0; filter <= kFilterTypeDefault; filter++) {
      EXPECT_OK(encoder.restart());
      EXPECT_OK(bl_object_set_property_uint32(&encoder, "compression", SIZE_MAX, 1u));
      if (filter != kFilterTypeDefault)
        EXPECT_OK(bl_object_set_property_uint32(&encoder, "filter", SIZE_MAX, filter));

      BLArray<uint8_t> png;
      EXPECT_OK(encoder.write_frame(png, image));
      BLArray<uint8_t> scanlines;
      decode_png_scanlines(scanlines, png);
      EXPECT_EQ(scanlines.size(), size_t(bpl) * kHeight);
      for (uint32_t y = 0; y < kHeight; y++)
        EXPECT_EQ(scanlines[y * bpl], uint8_t(filter == kFilterTypeDefault ? kFilterTypePaeth : filter));

      if (filter == kFilterTypePaeth) {
        paeth_png = png;
      }

      if (filter == kFilterTypeDefault) {
        EXPECT_EQ(png.size(), paeth_png.size());
        EXPECT_EQ(memcmp(png.data(), paeth_png.data(), png.size()), 0);
      }

      BLImage decoded;
      EXPECT_OK(decoded.read_from_data(png));
      EXPECT_EQ(decoded.width(), int(kWidth));
      EXPECT_EQ(decoded.height(), int(kHeight));

      BLImageData decoded_data;
      EXPECT_OK(decoded.get_data(&decoded_data));

      for (uint32_t y = 0; y < kHeight; y++) {
        const uint8_t* src = static_cast<const uint8_t*>(data.pixel_data) + intptr_t(y) * data.stride;
        const uint8_t* dst = static_cast<const uint8_t*>(decoded_data.pixel_data) + intptr_t(y) * decoded_data.stride;
        if (format == BL_FORMAT_A8) {
          for (uint32_t x = 0; x < kWidth; x++) {
            EXPECT_EQ(axl::loadu_u32(dst + x * 4u), 0xFF000000u | uint32_t(src[x]) * 0x010101u);
          }
        }
        else {
          EXPECT_EQ(memcmp(src, dst, kWidth * 4u), 0).message("Encoder filter roundtrip mismatch: Format=%u Filter=%u Row=%u", uint32_t(format), filter, y);
        }
      }
    }
  }
}

} // {bl::Png::Tests}

#endif // !BL_BUILD_NO_CODECS && BL_TEST
