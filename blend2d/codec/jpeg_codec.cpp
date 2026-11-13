// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// The JPEG codec is based on stb_image <https://github.com/nothings/stb>
// released into PUBLIC DOMAIN. Blend2D's JPEG codec can be distributed
// under Blend2D's ZLIB license or under STB's PUBLIC DOMAIN as well.

#include <blend2d/core/api_build_p.h>
#if !defined(BL_BUILD_NO_CODECS)

#include <blend2d/axl/commons.h>
#include <blend2d/axl/mem_ops.h>
#include <blend2d/axl/scopedbuffer_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/core/string_p.h>
#include <blend2d/codec/jpeg_codec_p.h>
#include <blend2d/codec/jpeg_huffman_p.h>
#include <blend2d/codec/jpeg_ops_p.h>

namespace bl::Jpeg {

// Codecs - Jpeg - Globals
// =======================

static constinit ObjectLayer::EternalVirtImpl<JpegCodecImpl, BLImageCodecVirt> jpeg_codec{};

static BLImageCodecCore jpeg_codec_object;
static BLImageDecoderVirt jpeg_decoder_virt;
/*
static BLImageEncoderVirt jpeg_encoder_virt;
*/

// Codecs - Jpeg - Decoder - DeZigZag Table
// ========================================

// Mapping table of zigzagged 8x8 data into a natural order.
static const uint8_t jpeg_de_zig_zag_table[64 + 16] = {
  0 , 1 , 8 , 16, 9 , 2 , 3 , 10,
  17, 24, 32, 25, 18, 11, 4 , 5 ,
  12, 19, 26, 33, 40, 48, 41, 34,
  27, 20, 13, 6 , 7 , 14, 21, 28,
  35, 42, 49, 56, 57, 50, 43, 36,
  29, 22, 15, 23, 30, 37, 44, 51,
  58, 59, 52, 45, 38, 31, 39, 46,
  53, 60, 61, 54, 47, 55, 62, 63,

  // These are not part of JPEG's spec, however, it's convenient as the decoder doesn't have to check whether
  // the coefficient index is out of bounds.
  63, 63, 63, 63, 63, 63, 63, 63,
  63, 63, 63, 63, 63, 63, 63, 63
};

// Codecs - Jpeg - Decoder - Process Marker
// ========================================

static BLResult jpeg_decoder_process_marker(JpegDecoderImpl* impl, uint32_t m, const uint8_t* p, size_t remain, size_t& consumed_bytes) noexcept {
  // Should be zero when passed in.
  BL_ASSERT(consumed_bytes == 0);

  BLImageInfo& image_info = impl->image_info;

#define GET_PAYLOAD_SIZE(MinSize)                     \
  size_t size;                                        \
                                                      \
  do {                                                \
    if (remain < MinSize)                             \
      return bl_make_error(BL_ERROR_DATA_TRUNCATED);  \
                                                      \
    size = axl::loadu_u16_be(p);                      \
    if (size < MinSize)                               \
      return bl_make_error(BL_ERROR_INVALID_DATA);    \
                                                      \
    if (size > remain)                                \
      return bl_make_error(BL_ERROR_DATA_TRUNCATED);  \
                                                      \
    p += 2;                                           \
    remain = size - 2;                                \
  } while (false)

  // SOF - Start of Frame
  // --------------------
  //
  //        WORD - Size (Consumed by GET_PAYLOAD_...)
  //
  //   [00] BYTE - Precision `P`
  //   [01] WORD - Height `Y`
  //   [03] WORD - Width `X`
  //   [05] BYTE - Number of components `Nf`
  //
  //   [06] Specification of each component [0..Nf] {
  //        [00] BYTE Component identifier `id`
  //        [01] BYTE Horizontal `Hi` and vertical `Vi` sampling factor
  //        [02] BYTE Quantization table destination selector `TQi`
  //   }

  if (is_marker_sof(m)) {
    uint32_t sof_marker = m;

    // Forbid multiple SOF markers in a single JPEG file.
    if (impl->sof_marker) {
      return bl_make_error(BL_ERROR_JPEG_MULTIPLE_SOF);
    }

    // Check if SOF type is supported.
    if (sof_marker != kMarkerSOF0 && sof_marker != kMarkerSOF1 && sof_marker != kMarkerSOF2) {
      return bl_make_error(BL_ERROR_JPEG_UNSUPPORTED_SOF);
    }

    // 11 bytes is a minimum size of SOF describing exactly one component.
    GET_PAYLOAD_SIZE(2 + 6 + 3);

    uint32_t bpp = p[0];
    uint32_t h = axl::loadu_u16_be(p + 1);
    uint32_t w = axl::loadu_u16_be(p + 3);
    uint32_t component_count = p[5];

    if (size != 8 + 3 * component_count)
      return bl_make_error(BL_ERROR_JPEG_INVALID_SOF);

    // Advance header.
    p += 6;

    if (w == 0) {
      return bl_make_error(BL_ERROR_INVALID_DATA);
    }

    // TODO: [JPEG] Unsupported delayed height (0).
    if (h == 0) {
      return bl_make_error(BL_ERROR_JPEG_UNSUPPORTED_FEATURE);
    }

    if (w > BL_RUNTIME_MAX_IMAGE_SIZE || h > BL_RUNTIME_MAX_IMAGE_SIZE) {
      return bl_make_error(BL_ERROR_IMAGE_TOO_LARGE);
    }

    // Check number of components and SOF size.
    if ((component_count != 1 && component_count != 3)) {
      return bl_make_error(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);
    }

    // TODO: [JPEG] 16-BPC.
    if (bpp != 8) {
      return bl_make_error(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);
    }

    // Maximum horizontal/vertical sampling factor of all components.
    uint32_t mcu_sf_w = 1;
    uint32_t mcu_sf_h = 1;

    uint32_t i, j;
    for (i = 0; i < component_count; i++, p += 3) {
      DecoderComponent* comp = &impl->comp[i];

      // Check if the ID doesn't collide with previous components.
      uint32_t comp_id = p[0];
      for (j = 0; j < i; j++) {
        if (impl->comp[j].comp_id == comp_id) {
          return bl_make_error(BL_ERROR_INVALID_DATA);
        }
      }

      // TODO: [JPEG] Is this necessary?
      // Required by JFIF.
      if (comp_id != i + 1) {
        // Some version of JpegTran outputs non-JFIF-compliant files!
        if (comp_id != i) {
          return bl_make_error(BL_ERROR_INVALID_DATA);
        }
      }

      // Horizontal/Vertical sampling factor.
      uint32_t sf = p[1];
      uint32_t sf_w = sf >> 4;
      uint32_t sf_h = sf & 15;

      if (sf_w == 0 || sf_w > 4 || sf_h == 0 || sf_h > 4) {
        return bl_make_error(BL_ERROR_INVALID_DATA);
      }

      // Bail to 1 if there is only one component as it contributes to nothing.
      if (component_count == 1) {
        sf_w = 1;
        sf_h = 1;
      }

      // Quantization ID.
      uint32_t quant_id = p[2];
      if (quant_id > 3) {
        return bl_make_error(BL_ERROR_INVALID_DATA);
      }

      // Save to DecoderComponent.
      comp->comp_id  = uint8_t(comp_id);
      comp->sf_w     = uint8_t(sf_w);
      comp->sf_h     = uint8_t(sf_h);
      comp->quant_id = uint8_t(quant_id);

      // We need to know maximum horizontal and vertical sampling factor to calculate the correct MCU size (WxH).
      mcu_sf_w = bl_max(mcu_sf_w, sf_w);
      mcu_sf_h = bl_max(mcu_sf_h, sf_h);
    }

    // Compute interleaved MCU info.
    uint32_t mcu_px_w = mcu_sf_w * kDctSize;
    uint32_t mcu_px_h = mcu_sf_h * kDctSize;

    uint32_t mcu_count_w = (w + mcu_px_w - 1) / mcu_px_w;
    uint32_t mcu_count_h = (h + mcu_px_h - 1) / mcu_px_h;
    bool is_baseline = sof_marker != kMarkerSOF2;

    for (i = 0; i < component_count; i++) {
      DecoderComponent* comp = &impl->comp[i];

      // Number of effective pixels (e.g. for non-interleaved MCU).
      comp->px_w = (w * uint32_t(comp->sf_w) + mcu_sf_w - 1) / mcu_sf_w;
      comp->px_h = (h * uint32_t(comp->sf_h) + mcu_sf_h - 1) / mcu_sf_h;

      // Allocate enough memory for all blocks even those that won't be used fully.
      comp->bl_w = mcu_count_w * uint32_t(comp->sf_w);
      comp->bl_h = mcu_count_h * uint32_t(comp->sf_h);

      comp->os_w = comp->bl_w * kDctSize;
      comp->os_h = comp->bl_h * kDctSize;

      comp->data = static_cast<uint8_t*>(impl->allocator.alloc(comp->os_w * comp->os_h));
      if (comp->data == nullptr) {
        return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
      }

      if (!is_baseline) {
        uint32_t kBlock8x8UInt16 = kDctSize2 * uint32_t(sizeof(int16_t));
        size_t coeff_size = comp->bl_w * comp->bl_h * kBlock8x8UInt16;
        int16_t* coeff_data = static_cast<int16_t*>(impl->allocator.alloc(coeff_size, 16));

        if (coeff_data == nullptr) {
          return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
        }

        comp->coeff = coeff_data;
        memset(comp->coeff, 0, coeff_size);
      }
    }

    // Everything seems ok, store the image information.
    image_info.flags = 0;
    image_info.size.reset(int(w), int(h));
    image_info.depth = uint16_t(component_count * bpp);
    image_info.plane_count = uint16_t(component_count);
    image_info.frame_count = 1;

    if (!is_baseline) {
      image_info.flags |= BL_IMAGE_INFO_FLAG_PROGRESSIVE;
    }

    impl->sof_marker = uint8_t(sof_marker);
    impl->delayed_height = (h == 0);
    impl->mcu.sf.w = uint8_t(mcu_sf_w);
    impl->mcu.sf.h = uint8_t(mcu_sf_h);
    impl->mcu.px.w = uint8_t(mcu_px_w);
    impl->mcu.px.h = uint8_t(mcu_px_h);
    impl->mcu.count.w = mcu_count_w;
    impl->mcu.count.h = mcu_count_h;

    consumed_bytes = size;
    return BL_SUCCESS;
  }

  // DHT - Define Huffman Table
  // --------------------------
  //
  //        WORD - Size (Consumed by GET_PAYLOAD_...)
  //
  //   [00] BYTE - Table class `tc` and table identifier `ti`.
  //   [01] 16xB - The count of Huffman codes of size 1..16.
  //
  //   [17] .... - The one byte symbols sorted by Huffman code. The number of symbols is the sum of the 16 code counts.

  if (m == kMarkerDHT) {
    GET_PAYLOAD_SIZE(2 + 17);

    while (remain) {
      uint32_t q = *p++;
      remain--;

      uint32_t table_class = q >> 4; // Table class.
      uint32_t table_id = q & 15; // Table id (0-3).

      // Invalid class or id.
      if (table_class >= kTableCount || table_id > 3) {
        return bl_make_error(BL_ERROR_INVALID_DATA);
      }

      size_t table_size_in_bytes;
      if (table_class == kTableDC) {
        BL_PROPAGATE(jpeg_build_huffman_dc(&impl->dc_table[table_id], p, remain, &table_size_in_bytes));
        impl->dc_table_mask = uint8_t(impl->dc_table_mask | axl::bit_mask<uint32_t>(table_id));
      }
      else {
        BL_PROPAGATE(jpeg_build_huffman_ac(&impl->ac_table[table_id], p, remain, &table_size_in_bytes));
        impl->ac_table_mask = uint8_t(impl->ac_table_mask | axl::bit_mask<uint32_t>(table_id));
      }

      p += table_size_in_bytes;
      remain -= table_size_in_bytes;
    }

    consumed_bytes = size;
    return BL_SUCCESS;
  }

  // DQT - Define Quantization Table
  // -------------------------------
  //
  //        WORD - Size (Consumed by GET_PAYLOAD_...)
  //
  //   [00] BYTE - Quantization value size `quant_sz` (0-1) and table identifier `quant_id`.
  //   [01] .... - 64 or 128 bytes depending on `qs`.

  if (m == kMarkerDQT) {
    GET_PAYLOAD_SIZE(2 + 65);

    while (remain >= 65) {
      uint32_t q = *p++;

      uint32_t q_size = q >> 4;
      uint32_t q_id = q & 15;

      if (q_size > 1 || q_id > 3) {
        return bl_make_error(BL_ERROR_INVALID_DATA);
      }

      uint16_t* q_table = impl->q_table[q_id].data;
      uint32_t required_size = 1 + 64 * (q_size + 1);

      if (required_size > remain) {
        break;
      }

      if (q_size == 0) {
        for (uint32_t k = 0; k < 64; k++, p++) {
          q_table[jpeg_de_zig_zag_table[k]] = *p;
        }
      }
      else {
        for (uint32_t k = 0; k < 64; k++, p += 2) {
          q_table[jpeg_de_zig_zag_table[k]] = uint16_t(axl::loadu_u16_be(reinterpret_cast<const uint16_t*>(p)));
        }
      }

      impl->q_table_mask = uint8_t(impl->q_table_mask | axl::bit_mask<uint32_t>(q_id));
      remain -= required_size;
    }

    if (remain != 0) {
      return bl_make_error(BL_ERROR_INVALID_DATA);
    }

    consumed_bytes = size;
    return BL_SUCCESS;
  }

  // DRI - Define Restart Interval
  // -----------------------------
  //
  //        WORD - Size (Consumed by GET_PAYLOAD_...)
  //
  //   [00] WORD - Restart interval.

  if (m == kMarkerDRI) {
    if (remain < 4) {
      return bl_make_error(BL_ERROR_DATA_TRUNCATED);
    }

    size_t size = axl::loadu_u16_be(p + 0);
    uint32_t ri = axl::loadu_u16_be(p + 2);

    // DRI payload should be 4 bytes.
    if (size != 4) {
      return bl_make_error(BL_ERROR_INVALID_DATA);
    }

    impl->restart_interval = ri;
    consumed_bytes = size;
    return BL_SUCCESS;
  }

  // SOS - Start of Scan
  // -------------------
  //
  //        WORD - Size (Consumed by GET_PAYLOAD_...)
  //
  //   [00] BYTE - Number of components in this SOS:
  //
  //   [01] Specification of each component - {
  //        [00] BYTE - Component ID
  //        [01] BYTE - DC and AC Selector
  //   }
  //
  //   [01 + NumComponents * 2]:
  //        [00] BYTE - Spectral Selection Start
  //        [01] BYTE - Spectral Selection End
  //        [02] BYTE - Successive Approximation High/Low

  if (m == kMarkerSOS) {
    GET_PAYLOAD_SIZE(2 + 6);

    uint32_t sof_marker = impl->sof_marker;
    uint32_t component_count = image_info.plane_count;

    uint32_t sc_count = *p++;
    uint32_t sc_mask = 0;

    if (size != 6 + sc_count * 2) {
      return bl_make_error(BL_ERROR_IMAGE_UNKNOWN_FILE_FORMAT);
    }

    if (sc_count < 1 || sc_count > component_count) {
      return bl_make_error(BL_ERROR_JPEG_INVALID_SOS);
    }

    uint32_t ss_start    = uint32_t(p[sc_count * 2 + 0]);
    uint32_t ss_end      = uint32_t(p[sc_count * 2 + 1]);
    uint32_t sa_low_bit  = uint32_t(p[sc_count * 2 + 2]) & 15;
    uint32_t sa_high_bit = uint32_t(p[sc_count * 2 + 2]) >> 4;

    if (sof_marker == kMarkerSOF0 || sof_marker == kMarkerSOF1) {
      if (ss_start != 0 || sa_low_bit != 0 || sa_high_bit != 0) {
        return bl_make_error(BL_ERROR_INVALID_DATA);
      }

      // The value should be 63, but it's zero sometimes.
      ss_end = 63;
    }

    if (sof_marker == kMarkerSOF2) {
      if (ss_start > 63 || ss_end > 63 || ss_start > ss_end || sa_low_bit > 13 || sa_high_bit > 13) {
        return bl_make_error(BL_ERROR_INVALID_DATA);
      }

      // AC & DC cannot be merged in a progressive JPEG.
      if (ss_start == 0 && ss_end != 0) {
        return bl_make_error(BL_ERROR_INVALID_DATA);
      }
    }

    DecoderSOS& sos = impl->sos;
    sos.sc_count    = uint8_t(sc_count);
    sos.ss_start    = uint8_t(ss_start);
    sos.ss_end      = uint8_t(ss_end);
    sos.sa_low_bit  = uint8_t(sa_low_bit);
    sos.sa_high_bit = uint8_t(sa_high_bit);

    for (uint32_t i = 0; i < sc_count; i++, p += 2) {
      uint32_t comp_id = p[0];
      uint32_t index = 0;

      while (impl->comp[index].comp_id != comp_id) {
        if (++index >= component_count) {
          return bl_make_error(BL_ERROR_JPEG_INVALID_SOS);
        }
      }

      // One huffman stream shouldn't overwrite the same component.
      if (axl::bit_test(sc_mask, index)) {
        return bl_make_error(BL_ERROR_JPEG_INVALID_SOS);
      }

      sc_mask |= axl::bit_mask<uint32_t>(index);

      uint32_t selector = p[1];
      uint32_t ac_id = selector & 15;
      uint32_t dc_id = selector >> 4;

      // Validate AC & DC selectors.
      if (ac_id > 3 || (!axl::bit_test(impl->ac_table_mask, ac_id) && ss_end  > 0)) {
        return bl_make_error(BL_ERROR_INVALID_DATA);
      }

      if (dc_id > 3 || (!axl::bit_test(impl->dc_table_mask, dc_id) && ss_end == 0)) {
        return bl_make_error(BL_ERROR_INVALID_DATA);
      }

      // Link the current component to the `index` and update AC & DC selectors.
      DecoderComponent* comp = &impl->comp[index];
      comp->dc_id = uint8_t(dc_id);
      comp->ac_id = uint8_t(ac_id);
      sos.sc_comp[i] = comp;
    }

    consumed_bytes = size;
    return BL_SUCCESS;
  }

  // APP - Application
  // -----------------

  if (is_marker_app(m)) {
    GET_PAYLOAD_SIZE(2);

    // APP0 - "JFIF\0"
    // ---------------

    if (m == kMarkerAPP0 && remain >= 5 && memcmp(p, "JFIF", 5) == 0) {
      if (bl_test_flag(impl->status_flags, DecoderStatusFlags::kDoneJFIF)) {
        return bl_make_error(BL_ERROR_INVALID_DATA);
      }

      if (remain < 14) {
        return bl_make_error(BL_ERROR_INVALID_DATA);
      }

      uint32_t jfif_major = p[5];
      uint32_t jfif_minor = p[6];

      // Check the density unit, correct it to aspect-only if it's wrong, but
      // don't fail as of one wrong value won't make any difference anyway.
      uint32_t density_unit = p[7];
      uint32_t x_density = axl::loadu_u16_be(p + 8);
      uint32_t y_density = axl::loadu_u16_be(p + 10);

      switch (density_unit) {
        case kDensityOnlyAspect:
          // TODO: [JPEG]
          break;

        case kDensityPixelsPerIN:
          image_info.density.reset(double(int(x_density)) * 39.3701, double(int(y_density)) * 39.3701);
          break;

        case kDensityPixelsPerCM:
          image_info.density.reset(double(int(x_density * 100)), double(int(y_density * 100)));
          break;

        default:
          break;
      }

      uint32_t thumb_w = p[12];
      uint32_t thumb_h = p[13];

      impl->status_flags |= DecoderStatusFlags::kDoneJFIF;
      impl->jfif_major = uint8_t(jfif_major);
      impl->jfif_minor = uint8_t(jfif_minor);

      if (thumb_w && thumb_h) {
        uint32_t thumb_size = thumb_w * thumb_h * 3;

        if (thumb_size + 14 < remain) {
          return bl_make_error(BL_ERROR_INVALID_DATA);
        }

        DecoderThumbnail& thumb = impl->thumb;
        thumb.format = kThumbnailRGB24;
        thumb.w = uint8_t(thumb_w);
        thumb.h = uint8_t(thumb_h);
        thumb.index = impl->buffer_index + 18;
        thumb.size = thumb_size;
        impl->status_flags |= DecoderStatusFlags::kHasThumb;
      }
    }

    // APP0 - "JFXX\0"
    // ---------------

    if (m == kMarkerAPP0 && remain >= 5 && memcmp(p, "JFXX", 5) == 0) {
      if (bl_test_flag(impl->status_flags, DecoderStatusFlags::kDoneJFXX)) {
        return bl_make_error(BL_ERROR_INVALID_DATA);
      }

      if (remain < 6) {
        return bl_make_error(BL_ERROR_INVALID_DATA);
      }

      uint32_t format = p[5];
      uint32_t thumb_w = 0;
      uint32_t thumb_h = 0;
      uint32_t thumb_size = 0;

      switch (format) {
        case kThumbnailJPEG:
          // Cannot overflow as the payload size is just 16-bit uint.
          thumb_size = uint32_t(remain - 6);
          break;

        case kThumbnailPAL8:
          thumb_w = p[6];
          thumb_h = p[7];
          thumb_size = 768 + thumb_w * thumb_h;
          break;

        case kThumbnailRGB24:
          thumb_w = p[6];
          thumb_h = p[7];
          thumb_size = thumb_w * thumb_h * 3;
          break;

        default:
          return bl_make_error(BL_ERROR_INVALID_DATA);
      }

      if (thumb_size + 6 > remain)
        return bl_make_error(BL_ERROR_INVALID_DATA);

      impl->thumb.format = uint8_t(format);
      impl->thumb.w = uint8_t(thumb_w);
      impl->thumb.h = uint8_t(thumb_h);
      impl->thumb.index = impl->buffer_index + 10;
      impl->thumb.size = thumb_size;

      impl->status_flags |= DecoderStatusFlags::kDoneJFXX | DecoderStatusFlags::kHasThumb;
    }

    // APP1 - "EXIF\0\0"
    // -----------------
    /*
    // TODO: [JPEG] This would require some work to make this possible.
    if (m == kMarkerAPP1 && remain >= 6 && memcmp(p, "Exif\0", 6) == 0) {
      // These should be only one EXIF marker in the whole JPEG image, not sure
      // what to do if there is more...
      if (!(impl->status_flags & BL_JPEG_DECODER_DONE_EXIF)) {
        p += 6;
        remain -= 6;

        // Need at least more 8 bytes required by TIFF header.
        if (remain < 8)
          return bl_make_error(kErrorExifInvalidHeader);

        // Check if the EXIF marker has a proper TIFF header.
        uint32_t byte_order;
        uint32_t do_byte_swap;

        if (memcmp(p, jpeg_exif_le, 4))
          byte_order = BL_BYTE_ORDER_LE;
        else if (memcmp(p, jpeg_exif_be, 4))
          byte_order = BL_BYTE_ORDER_BE;
        else
          return bl_make_error(kErrorExifInvalidHeader);

        do_byte_swap = byte_order != BL_BYTE_ORDER_NATIVE;
        impl->status_flags |= BL_JPEG_DECODER_DONE_EXIF;
      }
    }
    */

    consumed_bytes = size;
    return BL_SUCCESS;
  }

  // COM - Comment
  // -------------

  if (m == kMarkerCOM) {
    GET_PAYLOAD_SIZE(2);

    consumed_bytes = size;
    return BL_SUCCESS;
  }

  // EOI - End of Image
  // ------------------

  if (m == kMarkerEOI) {
    impl->status_flags |= DecoderStatusFlags::kDoneEOI;
    return BL_SUCCESS;
  }

  // Invalid / Unknown
  // -----------------

  return bl_make_error(BL_ERROR_INVALID_DATA);

#undef GET_PAYLOAD_SIZE
}

// Codecs - Jpeg - Decoder - Process Stream
// ========================================

struct JpegDecoderRun {
  //! Component linked with the run.
  DecoderComponent* comp;

  //! Current data pointer (advanced during decoding).
  uint8_t* data;
  //! De-quantization table pointer.
  const Block<uint16_t>* q_table;

  //! Count of 8x8 blocks required by a single MCU, calculated as `sf_w * sf_h`.
  uint32_t count;
  //! Stride.
  uint32_t stride;
  //! Horizontal/Vertical advance per MCU.
  uint32_t advance[2];

  //! Offsets of all blocks of this component that are part of a single MCU.
  intptr_t offset[16];
};

// Called after a restart marker (RES) has been reached.
static BLResult jpeg_decoder_handle_restart(JpegDecoderImpl* impl, JpegDecoderBitStream& stream, const uint8_t* pEnd) noexcept {
  if (stream.restart_counter == 0 || --stream.restart_counter != 0)
    return BL_SUCCESS;

  // I think this shouldn't be necessary to refill the code buffer/size as all bytes should have been consumed.
  // However, since the spec is so vague, I'm not sure if this is necessary, recommended, or forbidden :(
  JpegDecoderBitReader reader(stream);
  reader.refill();

  if (!reader.at_end() || (size_t)(pEnd - reader.ptr) < 2 || !is_marker_rst(reader.ptr[1]))
    return bl_make_error(BL_ERROR_DECOMPRESSION_FAILED);

  // Skip the marker and flush entropy bits.
  reader.flush();
  reader.advance(2);
  reader.done(stream);

  stream.eob_run = 0;
  stream.restart_counter = impl->restart_interval;

  // Reset DC predictions.
  DecoderComponent* comp = impl->comp;
  comp[0].dc_pred = 0;
  comp[1].dc_pred = 0;
  comp[2].dc_pred = 0;
  comp[3].dc_pred = 0;
  return BL_SUCCESS;
}

//! Decode a baseline 8x8 block.
static BLResult jpeg_decoder_read_baseline_block(JpegDecoderImpl* impl, JpegDecoderBitStream& stream, DecoderComponent* comp, int16_t* dst) noexcept {
  const JpegDecoderHuffmanTable* dc_table = &impl->dc_table[comp->dc_id];
  const JpegDecoderHuffmanTable* ac_table = &impl->ac_table[comp->ac_id];

  JpegDecoderBitReader reader(stream);
  reader.refill();

  // Decode DC - Maximum Bytes Consumed: 4 (unescaped)
  // -------------------------------------------------

  uint32_t s;
  int32_t dc_pred = comp->dc_pred;
  BL_PROPAGATE(reader.read_code(s, dc_table));

  if (s) {
    reader.refill_if_32bit();
    BL_PROPAGATE(reader.require_bits(s));

    int32_t dc_val = reader.read_signed(s);
    dc_pred += dc_val;
    comp->dc_pred = dc_pred;
  }
  dst[0] = int16_t(dc_pred);

  // Decode AC - Maximum Bytes Consumed: 4 * 63 (unescaped)
  // ------------------------------------------------------

  uint32_t k = 1;
  const int16_t* ac_accel = impl->ac_table[comp->ac_id].ac_accel;

  do {
    reader.refill();

    uint32_t c = reader.peek<uint32_t>(jpeg_huffman_accel_bits);
    int32_t ac = ac_accel[c];

    // Fast AC.
    if (ac) {
      s = (ac & 15);       // Size.
      k += (ac >> 4) & 15; // Skip.
      ac >>= 8;
      reader.drop(s);
      dst[jpeg_de_zig_zag_table[k++]] = int16_t(ac);
    }
    else {
      BL_PROPAGATE(reader.read_code(ac, ac_table));
      s = ac & 15;
      ac >>= 4;

      if (s == 0) {
        // End block.
        if (ac != 0xF)
          break;
        k += 16;
      }
      else {
        k += uint32_t(ac);

        reader.refill_if_32bit();
        BL_PROPAGATE(reader.require_bits(s));

        ac = reader.read_signed(s);
        dst[jpeg_de_zig_zag_table[k++]] = int16_t(ac);
      }
    }
  } while (k < 64);

  reader.done(stream);
  return BL_SUCCESS;
}

//! Decode a progressive 8x8 block (AC or DC coefficients, but never both).
static BLResult jpeg_decoder_read_progressive_block(JpegDecoderImpl* impl, JpegDecoderBitStream& stream, DecoderComponent* comp, int16_t* dst) noexcept {
  JpegDecoderBitReader reader(stream);
  reader.refill();

  uint32_t k     = uint32_t(impl->sos.ss_start);
  uint32_t k_end = uint32_t(impl->sos.ss_end) + 1;
  uint32_t shift = impl->sos.sa_low_bit;

  // Decode DC - Maximum Bytes Consumed: 4 (unescaped)
  // -------------------------------------------------

  if (k == 0) {
    const JpegDecoderHuffmanTable* dc_table = &impl->dc_table[comp->dc_id];
    uint32_t s;

    if (impl->sos.sa_high_bit == 0) {
      // Initial scan for DC coefficient.
      int32_t dc_pred = comp->dc_pred;
      BL_PROPAGATE(reader.read_code(s, dc_table));

      if (s) {
        reader.refill_if_32bit();
        BL_PROPAGATE(reader.require_bits(s));

        int32_t dc_val = reader.read_signed(s);
        dc_pred += dc_val;
        comp->dc_pred = dc_pred;
      }

      dst[0] = int16_t(axl::shl(dc_pred, shift));
    }
    else {
      // Refinement scan for DC coefficient.
      BL_PROPAGATE(reader.require_bits(1));

      s = reader.read_bit<uint32_t>();
      dst[0] = int16_t(int32_t(dst[0]) + int32_t(s << shift));
    }

    k++;
  }

  // Decode AC - Maximum Bytes Consumed: max(4 * 63, 8) (unescaped)
  // --------------------------------------------------------------

  if (k < k_end) {
    const JpegDecoderHuffmanTable* ac_table = &impl->ac_table[comp->ac_id];
    const int16_t* ac_accel = impl->ac_table[comp->ac_id].ac_accel;

    if (impl->sos.sa_high_bit == 0) {
      // Initial scan for AC coefficients.
      if (stream.eob_run) {
        stream.eob_run--;
        return BL_SUCCESS;
      }

      do {
        // Fast AC.
        reader.refill();
        int32_t r = ac_accel[reader.peek(jpeg_huffman_accel_bits)];

        if (r) {
          int32_t s = r & 15;
          k += (r >> 4) & 15;
          reader.drop(uint32_t(s));

          uint32_t zig = jpeg_de_zig_zag_table[k++];
          dst[zig] = int16_t(axl::shl(r >> 8, shift));
        }
        else {
          BL_PROPAGATE(reader.read_code(r, ac_table));
          reader.refill_if_32bit();

          int32_t s = r & 15;
          r >>= 4;

          if (s == 0) {
            if (r < 15) {
              uint32_t eob_run = 0;
              if (r) {
                BL_PROPAGATE(reader.require_bits(uint32_t(r)));
                eob_run = reader.read_unsigned(uint32_t(r));
              }
              stream.eob_run = eob_run + (1u << r) - 1;
              break;
            }
            k += 16;
          }
          else {
            k += uint32_t(r);
            r = reader.read_signed(uint32_t(s));

            uint32_t zig = jpeg_de_zig_zag_table[k++];
            dst[zig] = int16_t(axl::shl(r, shift));
          }
        }
      } while (k < k_end);
    }
    else {
      // Refinement scan for AC coefficients.
      int32_t bit = int32_t(1) << shift;
      if (stream.eob_run) {
        do {
          int16_t* p = &dst[jpeg_de_zig_zag_table[k++]];
          int32_t pVal = *p;

          if (pVal) {
            BL_PROPAGATE(reader.require_bits(1));
            uint32_t b = reader.read_bit<uint32_t>();

            reader.refill();
            if (b && (pVal & bit) == 0)
              *p = int16_t(pVal + (pVal > 0 ? bit : -bit));
          }
        } while (k < k_end);
        stream.eob_run--;
      }
      else {
        do {
          int32_t r, s;

          reader.refill();
          BL_PROPAGATE(reader.read_code(r, ac_table));

          reader.refill_if_32bit();
          s = r & 15;
          r >>= 4;

          if (s == 0) {
            if (r < 15) {
              uint32_t eob_run = 0;
              if (r) {
                BL_PROPAGATE(reader.require_bits(uint32_t(r)));
                eob_run = reader.read_unsigned(uint32_t(r));
              }
              stream.eob_run = eob_run + (1u << r) - 1;
              r = 64; // Force end of block.
            }
            // r=15 s=0 already does the right thing (write 16 0s).
          }
          else {
            if (BL_UNLIKELY(s != 1))
              return bl_make_error(BL_ERROR_DECOMPRESSION_FAILED);

            BL_PROPAGATE(reader.require_bits(1));
            uint32_t sign = reader.read_bit<uint32_t>();
            s = sign ? bit : -bit;
          }

          // Advance by `r`.
          while (k < k_end) {
            int16_t* p = &dst[jpeg_de_zig_zag_table[k++]];
            int32_t pVal = *p;

            if (pVal) {
              uint32_t b;

              reader.refill();
              BL_PROPAGATE(reader.require_bits(1));

              b = reader.read_bit<uint32_t>();
              if (b && (pVal & bit) == 0)
                *p = int16_t(pVal + (pVal > 0 ? bit : -bit));
            }
            else {
              if (r == 0) {
                *p = int16_t(s);
                break;
              }
              r--;
            }
          }
        } while (k < k_end);
      }
    }
  }

  reader.done(stream);
  return BL_SUCCESS;
}

static BLResult decoder_process_stream(JpegDecoderImpl* impl, const uint8_t* p, size_t remain, size_t& consumed_bytes) noexcept {
  DecoderSOS& sos = impl->sos;

  const uint8_t* start = p;
  const uint8_t* end = p + remain;

  // Initialize
  // ----------

  // Just needed to determine the logic.
  uint32_t sof_marker = impl->sof_marker;

  // Whether the stream is baseline or progressive. Progressive streams use multiple SOS markers to progressively
  // update the image being decoded.
  bool is_baseline = sof_marker != kMarkerSOF2;

  // If this is a baseline stream then the unit-size is 1 byte, because the block of coefficients is immediately
  // IDCTed to pixel values after it is decoded. However, progressive decoding cannot use this space optimization
  // as coefficients are updated progressively.
  uint32_t unit_size = is_baseline ? 1 : 2;

  // Initialize the entropy stream.
  JpegDecoderBitStream stream;
  stream.reset(p, end);
  stream.restart_counter = impl->restart_interval;

  uint32_t i;
  uint32_t sc_count = sos.sc_count;

  uint32_t mcu_x = 0;
  uint32_t mcu_y = 0;

  // TODO: [JPEG] This is not right, we must calculate MCU W/H every time.
  uint32_t mcu_w = impl->mcu.count.w;
  uint32_t mcu_h = impl->mcu.count.h;

  // A single component's decoding doesn't use interleaved MCUs.
  if (sc_count == 1) {
    DecoderComponent* comp = sos.sc_comp[0];
    mcu_w = (comp->px_w + kDctSize - 1) / kDctSize;
    mcu_h = (comp->px_h + kDctSize - 1) / kDctSize;
  }

  // Initialize decoder runs (each run specifies one component per scan).
  JpegDecoderRun runs[4];
  for (i = 0; i < sc_count; i++) {
    JpegDecoderRun* run = &runs[i];
    DecoderComponent* comp = sos.sc_comp[i];

    uint32_t sf_w = sc_count > 1 ? uint32_t(comp->sf_w) : uint32_t(1);
    uint32_t sf_h = sc_count > 1 ? uint32_t(comp->sf_h) : uint32_t(1);

    uint32_t count = 0;
    uint32_t offset = 0;

    if (is_baseline) {
      uint32_t stride = comp->os_w * unit_size;

      for (uint32_t y = 0; y < sf_h; y++) {
        for (uint32_t x = 0; x < sf_w; x++) {
          run->offset[count++] = intptr_t(offset + x * unit_size * kDctSize);
        }
        offset += stride * kDctSize;
      }

      run->comp = comp;
      run->data = comp->data;
      run->q_table = &impl->q_table[comp->quant_id];

      run->count = count;
      run->stride = stride;
      run->advance[0] = sf_w * unit_size * kDctSize;
      run->advance[1] = run->advance[0] + (sf_h * kDctSize - 1) * stride;
    }
    else {
      uint32_t block_size = unit_size * kDctSize2;
      uint32_t block_stride = comp->bl_w * block_size;

      for (uint32_t y = 0; y < sf_h; y++) {
        for (uint32_t x = 0; x < sf_w; x++) {
          run->offset[count++] = intptr_t(offset + x * block_size);
        }
        offset += block_stride;
      }

      run->comp = comp;
      run->data = reinterpret_cast<uint8_t*>(comp->coeff);
      run->q_table = nullptr;

      run->count = count;
      run->stride = 0;

      run->advance[0] = sf_w * block_size;
      run->advance[1] = sf_h * block_stride - (mcu_w - 1) * run->advance[0];
    }
  }

  // SOF0/1 - Baseline / Extended
  // ----------------------------

  if (sof_marker == kMarkerSOF0 || sof_marker == kMarkerSOF1) {
    Block<int16_t> tmp_block;

    for (;;) {
      // Increment it here so we can use `mcu_x == mcu_w` in the inner loop.
      mcu_x++;

      // Decode all blocks required by a single MCU.
      for (i = 0; i < sc_count; i++) {
        JpegDecoderRun* run = &runs[i];
        uint8_t* block_data = run->data;
        uint32_t block_count = run->count;

        for (uint32_t n = 0; n < block_count; n++) {
          tmp_block.reset();
          BL_PROPAGATE(jpeg_decoder_read_baseline_block(impl, stream, run->comp, tmp_block.data));
          jpeg_opts.idct8(block_data + run->offset[n], intptr_t(run->stride), tmp_block.data, run->q_table->data);
        }

        run->data = block_data + run->advance[mcu_x == mcu_w];
      }

      // Advance.
      if (mcu_x == mcu_w) {
        if (++mcu_y == mcu_h) {
          break;
        }
        mcu_x = 0;
      }

      // Restart.
      BL_PROPAGATE(jpeg_decoder_handle_restart(impl, stream, end));
    }
  }

  // SOF2 - Progressive
  // ------------------

  else if (sof_marker == kMarkerSOF2) {
    for (;;) {
      // Increment it here so we can use `mcu_x == mcu_w` in the inner loop.
      mcu_x++;

      // Decode all blocks required by a single MCU.
      for (i = 0; i < sc_count; i++) {
        JpegDecoderRun* run = &runs[i];

        uint8_t* block_data = run->data;
        uint32_t block_count = run->count;

        for (uint32_t n = 0; n < block_count; n++) {
          BL_PROPAGATE(jpeg_decoder_read_progressive_block(impl, stream, run->comp,
            reinterpret_cast<int16_t*>(block_data + run->offset[n])));
        }

        run->data = block_data + run->advance[mcu_x == mcu_w];
      }

      // Advance.
      if (mcu_x == mcu_w) {
        if (++mcu_y == mcu_h) {
          break;
        }
        mcu_x = 0;
      }

      // Restart.
      BL_PROPAGATE(jpeg_decoder_handle_restart(impl, stream, end));
    }
  }

  // End
  // ---

  else {
    BL_NOT_REACHED();
  }

  p = stream.ptr;

  // Skip zeros at the end of the entropy stream that was not consumed `refill()`
  while (p != end && p[0] == 0x00) {
    p++;
  }

  consumed_bytes = (size_t)(p - start);
  return BL_SUCCESS;
}

// Codecs - Jpeg - Decoder - Process MCUs
// ======================================

static BLResult jpeg_decoder_process_mcus(JpegDecoderImpl* impl) noexcept {
  if (impl->sof_marker == kMarkerSOF2) {
    uint32_t component_count = impl->image_info.plane_count;

    // Dequantize & IDCT.
    for (uint32_t n = 0; n < component_count; n++) {
      DecoderComponent& comp = impl->comp[n];

      uint32_t w = (comp.px_w + 7) >> 3;
      uint32_t h = (comp.px_h + 7) >> 3;
      const Block<uint16_t>* q_table = &impl->q_table[comp.quant_id];

      for (uint32_t j = 0; j < h; j++) {
        for (uint32_t i = 0; i < w; i++) {
          int16_t *data = comp.coeff + 64 * (i + j * comp.bl_w);
          jpeg_opts.idct8(comp.data + comp.os_w * j * 8 + i * 8, intptr_t(comp.os_w), data, q_table->data);
        }
      }
    }
  }

  return BL_SUCCESS;
}

// Codecs - Jpeg - Decoder - ConvertToRGB
// ======================================

struct JpegDecoderUpsample {
  uint8_t* line[2];

  // Expansion factor in each axis.
  uint32_t hs, vs;
  // Horizontal pixels pre-expansion.
  uint32_t w_lores;
  // How far through vertical expansion we are.
  uint32_t ystep;
  // Which pre-expansion row we're on.
  uint32_t ypos;
  // Selected upsample function.
  uint8_t* (BL_CDECL* upsample)(uint8_t* out, uint8_t* in0, uint8_t* in1, uint32_t w, uint32_t hs) noexcept;
};

static BLResult jpeg_decoder_convert_to_rgb(JpegDecoderImpl* impl, BLImageData& dst) noexcept {
  uint32_t w = uint32_t(impl->image_info.size.w);
  uint32_t h = uint32_t(impl->image_info.size.h);

  BL_ASSERT(uint32_t(dst.size.w) >= w);
  BL_ASSERT(uint32_t(dst.size.h) >= h);

  uint8_t* dst_line = static_cast<uint8_t*>(dst.pixel_data);
  intptr_t dst_stride = dst.stride;

  ScopedBufferTmp<1024 * 3 + 16> tmp_mem;

  // Allocate a line buffer that's big enough for up-sampling off the edges with up-sample factor of 4.
  uint32_t component_count = impl->image_info.plane_count;
  BL_ASSERT(component_count > 0u && component_count <= 4u);

  uint32_t line_stride = axl::align_up(w + 3, 16);
  uint8_t* line_buffer = static_cast<uint8_t*>(tmp_mem.alloc(line_stride * component_count));

  if (BL_UNLIKELY(!line_buffer))
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);

  JpegDecoderUpsample upsample[4];
  uint8_t* pPlane[4];
  uint8_t* pBuffer[4];

  for (uint32_t k = 0; k < component_count; k++) {
    DecoderComponent& comp = impl->comp[k];
    JpegDecoderUpsample* r = &upsample[k];

    pBuffer[k] = line_buffer + k * line_stride;

    r->hs      = uint32_t(impl->mcu.sf.w / comp.sf_w);
    r->vs      = uint32_t(impl->mcu.sf.h / comp.sf_h);
    r->ystep   = r->vs >> 1;
    r->w_lores = (w + r->hs - 1) / r->hs;
    r->ypos    = 0;
    r->line[0] = comp.data;
    r->line[1] = comp.data;

    if      (r->hs == 1 && r->vs == 1) r->upsample = jpeg_opts.upsample_1x1;
    else if (r->hs == 1 && r->vs == 2) r->upsample = jpeg_opts.upsample_1x2;
    else if (r->hs == 2 && r->vs == 1) r->upsample = jpeg_opts.upsample_2x1;
    else if (r->hs == 2 && r->vs == 2) r->upsample = jpeg_opts.upsample_2x2;
    else                               r->upsample = jpeg_opts.upsample_any;
  }

  // Now go ahead and resample.
  for (uint32_t y = 0; y < h; y++, dst_line += dst_stride) {
    for (uint32_t k = 0; k < component_count; k++) {
      DecoderComponent& comp = impl->comp[k];
      JpegDecoderUpsample* r = &upsample[k];

      int y_bot = r->ystep >= (r->vs >> 1);
      pPlane[k] = r->upsample(pBuffer[k], r->line[y_bot], r->line[1 - y_bot], r->w_lores, r->hs);

      if (++r->ystep >= r->vs) {
        r->ystep = 0;
        r->line[0] = r->line[1];

        if (++r->ypos < comp.px_h) {
          r->line[1] += comp.os_w;
        }
      }
    }

    uint8_t* pY = pPlane[0];
    if (component_count == 3) {
      jpeg_opts.conv_ycbcr8_to_rgb32(dst_line, pY, pPlane[1], pPlane[2], w);
    }
    else {
      for (uint32_t x = 0; x < w; x++) {
        axl::storea_u32(dst_line + x * 4, 0xFF000000u + uint32_t(pY[x]) * 0x010101u);
      }
    }
  }

  return BL_SUCCESS;
}

// Codecs - Jpeg - Decoder - Read Internal
// =======================================

static BLResult jpeg_decoder_read_info_internal(JpegDecoderImpl* impl, const uint8_t* p, size_t size) noexcept {
  // JPEG file signature is 2 bytes (0xFF, 0xD8) followed by markers, SOF
  // (start of file) marker contains 1 byte signature and at least 8 bytes of
  // data describing basic information of the image.
  if (size < 2 + 8 + 1)
    return bl_make_error(BL_ERROR_DATA_TRUNCATED);

  const uint8_t* start = p;
  const uint8_t* end = p + size;

  // Check JPEG signature (SOI marker).
  if (p[0] != 0xFF || p[1] != kMarkerSOI)
    return bl_make_error(BL_ERROR_INVALID_SIGNATURE);

  memcpy(impl->image_info.format, "JPEG", 5);
  memcpy(impl->image_info.compression, "HUFFMAN", 8);

  p += 2;
  impl->status_flags |= DecoderStatusFlags::kDoneSOI;

  // Process markers until SOF.
  for (;;) {
    impl->buffer_index = (size_t)(p - start);

    if ((size_t)(end - p) < 2)
      return bl_make_error(BL_ERROR_DATA_TRUNCATED);

    if (p[0] != 0xFF)
      return bl_make_error(BL_ERROR_INVALID_DATA);

    uint32_t m = p[1];
    p += 2;

    // Some files have an extra padding (0xFF) after their blocks, ignore it.
    if (m == kMarkerInvalid) {
      while (p != end && (m = p[0]) == kMarkerInvalid) {
        p++;
      }

      if (p == end) {
        break;
      }

      p++;
    }

    size_t consumed_bytes = 0;
    BL_PROPAGATE(jpeg_decoder_process_marker(impl, m, p, (size_t)(end - p), consumed_bytes));

    BL_ASSERT(consumed_bytes < (size_t)(end - p));
    p += consumed_bytes;

    // Terminate after SOF has been processed, the rest is handled by `decode()`.
    if (is_marker_sof(m)) {
      break;
    }
  }

  impl->buffer_index = (size_t)(p - start);
  return BL_SUCCESS;
}

static BLResult jpeg_decoder_read_frame_internal(JpegDecoderImpl* impl, BLImage* image_out, const uint8_t* p, size_t size) noexcept {
  const uint8_t* start = p;
  const uint8_t* end = p + size;

  if (size < impl->buffer_index)
    return bl_make_error(BL_ERROR_DATA_TRUNCATED);

  p += impl->buffer_index;

  // Process markers.
  //
  // We are already after SOF, which was processed by `jpeg_decoder_read_info_internal`.
  for (;;) {
    impl->buffer_index = (size_t)(p - start);
    if ((size_t)(end - p) < 2) {
      return bl_make_error(BL_ERROR_DATA_TRUNCATED);
    }

    if (p[0] != 0xFF) {
      return bl_make_error(BL_ERROR_INVALID_DATA);
    }

    uint32_t m = p[1];
    p += 2;

    // Some files have an extra padding (0xFF) after their blocks, ignore it.
    if (m == kMarkerInvalid) {
      while (p != end && (m = p[0]) == kMarkerInvalid) {
        p++;
      }

      if (p == end) {
        break;
      }
      p++;
    }

    // Process the marker.
    {
      size_t consumed_bytes = 0;
      BL_PROPAGATE(jpeg_decoder_process_marker(impl, m, p, (size_t)(end - p), consumed_bytes));

      BL_ASSERT((size_t)(end - p) >= consumed_bytes);
      p += consumed_bytes;
    }

    // EOI - terminate.
    if (m == kMarkerEOI) {
      break;
    }

    // SOS - process the entropy coded data-stream that follows SOS.
    if (m == kMarkerSOS) {
      size_t consumed_bytes = 0;
      BL_PROPAGATE(decoder_process_stream(impl, p, (size_t)(end - p), consumed_bytes));

      BL_ASSERT((size_t)(end - p) >= consumed_bytes);
      p += consumed_bytes;
      impl->status_flags |= DecoderStatusFlags::kDoneSOS;
    }
  }

  // Process MCUs.
  BL_PROPAGATE(jpeg_decoder_process_mcus(impl));

  // Create the final image and convert YCbCr -> RGB.
  uint32_t w = uint32_t(impl->image_info.size.w);
  uint32_t h = uint32_t(impl->image_info.size.h);
  BLFormat format = BL_FORMAT_XRGB32;
  BLImageData image_data;

  BL_PROPAGATE(image_out->create(int(w), int(h), format));
  BL_PROPAGATE(image_out->make_mutable(&image_data));
  BL_PROPAGATE(jpeg_decoder_convert_to_rgb(impl, image_data));

  impl->buffer_index = (size_t)(p - start);
  impl->frame_index++;

  return BL_SUCCESS;
}

// Codecs - Jpeg - Decoder - Interface
// ===================================

static BLResult BL_CDECL jpeg_decoder_restart(BLImageDecoderCore* self) noexcept {
  JpegDecoderImpl* impl = self->_impl<JpegDecoderImpl>();

  impl->last_result = BL_SUCCESS;
  impl->frame_index = 0;
  impl->buffer_index = 0;

  impl->allocator.reset();
  impl->image_info.reset();
  impl->status_flags = DecoderStatusFlags::kNone;
  impl->restart_interval = 0;
  impl->sof_marker = 0;
  impl->colorspace = 0;
  impl->delayed_height = 0;
  impl->jfif_major = 0;
  impl->jfif_minor = 0;
  impl->dc_table_mask = 0;
  impl->ac_table_mask = 0;
  impl->q_table_mask = 0;
  impl->mcu.reset();
  impl->sos.reset();
  impl->thumb.reset();
  memset(impl->comp, 0, sizeof(impl->comp));

  return BL_SUCCESS;
}

static BLResult BL_CDECL jpeg_decoder_read_info(BLImageDecoderCore* self, BLImageInfo* info_out, const uint8_t* p, size_t size) noexcept {
  JpegDecoderImpl* impl = self->_impl<JpegDecoderImpl>();
  BLResult result = impl->last_result;

  if (impl->buffer_index == 0 && result == BL_SUCCESS) {
    result = jpeg_decoder_read_info_internal(impl, p, size);
    if (result != BL_SUCCESS)
      impl->last_result = result;
  }

  if (info_out)
    memcpy(info_out, &impl->image_info, sizeof(BLImageInfo));

  return result;
}

static BLResult BL_CDECL jpeg_decoder_read_frame(BLImageDecoderCore* self, BLImageCore* image_out, const uint8_t* p, size_t size) noexcept {
  BL_PROPAGATE(jpeg_decoder_read_info(self, nullptr, p, size));

  JpegDecoderImpl* impl = self->_impl<JpegDecoderImpl>();
  if (impl->frame_index) {
    return bl_make_error(BL_ERROR_NO_MORE_DATA);
  }

  BLResult result = jpeg_decoder_read_frame_internal(impl, static_cast<BLImage*>(image_out), p, size);
  if (result != BL_SUCCESS) {
    impl->last_result = result;
  }
  return result;
}

static BLResult BL_CDECL jpeg_decoder_create(BLImageDecoderCore* self) noexcept {
  BL_PROPAGATE(ObjectLayer::alloc_virt_impl_t<JpegDecoderImpl>(self, &jpeg_decoder_virt));
  JpegDecoderImpl* impl = self->_impl<JpegDecoderImpl>();
  impl->ctor(&jpeg_codec_object);
  bl_call_ctor(impl->allocator);
  return jpeg_decoder_restart(self);
}

static BLResult BL_CDECL jpeg_decoder_destroy(BLObjectCore* self) noexcept {
  JpegDecoderImpl* impl = self->_impl<JpegDecoderImpl>();
  impl->allocator.reset();
  impl->dtor();
  return ObjectLayer::dealloc_impl(impl);
}

// Codecs - Jpeg - Interface
// ===========================

static BLResult BL_CDECL jpeg_codec_destroy(BLObjectCore* self) noexcept {
  // Built-in codecs are never destroyed.
  axl::maybe_unused(self);
  return BL_SUCCESS;
}

static uint32_t BL_CDECL jpeg_codec_inspect_data(const BLImageCodecCore* self, const uint8_t* data, size_t size) noexcept {
  axl::maybe_unused(self);

  // JPEG minimum size and signature (SOI).
  if (size < 2 || data[0] != 0xFF || data[1] != kMarkerSOI) {
    return 0;
  }

  // JPEG signature has to be followed by a marker that starts with 0xFF.
  if (size > 2 && data[2] != 0xFF) {
    return 0;
  }

  return 100;
}

static BLResult BL_CDECL jpeg_codec_create_decoder(const BLImageCodecCore* self, BLImageDecoderCore* dst) noexcept {
  axl::maybe_unused(self);

  BLImageDecoderCore tmp;
  BL_PROPAGATE(jpeg_decoder_create(&tmp));
  return bl_image_decoder_assign_move(dst, &tmp);
}

static BLResult BL_CDECL jpeg_codec_create_encoder(const BLImageCodecCore* self, BLImageEncoderCore* dst) noexcept {
  axl::maybe_unused(self, dst);
  return bl_make_error(BL_ERROR_IMAGE_ENCODER_NOT_PROVIDED);

  // TODO: [JPEG] Encoder
  /*
  BLImageEncoderCore tmp;
  BL_PROPAGATE(jpeg_encoder_create(tmp._d));
  return bl_image_encoder_assign_move(dst, &tmp);
  */
}

// Codecs - Jpeg - Runtime Registration
// ====================================

void jpeg_codec_on_init([[maybe_unused]] BLRuntimeContext* rt, BLArray<BLImageCodec>* codecs) noexcept {
  // Initialize JPEG jpeg_opts.
  jpeg_opts.idct8 = jpeg_idct8;
  jpeg_opts.conv_ycbcr8_to_rgb32 = jpeg_rgb32_from_ycbcr8;

#ifdef BL_BUILD_OPT_SSE2
  jpeg_opts.idct8 = jpeg_idct8_sse2;
  jpeg_opts.conv_ycbcr8_to_rgb32 = jpeg_rgb32_from_ycbcr8_sse2;
#endif

  jpeg_opts.upsample_1x1 = jpeg_upsample_1x1;
  jpeg_opts.upsample_1x2 = jpeg_upsample_1x2;
  jpeg_opts.upsample_2x1 = jpeg_upsample_2x1;
  jpeg_opts.upsample_2x2 = jpeg_upsample_2x2;
  jpeg_opts.upsample_any = jpeg_upsample_generic;

  // Initialize JPEG codec.
  jpeg_codec.virt.base.destroy = jpeg_codec_destroy;
  jpeg_codec.virt.base.get_property = bl_object_impl_get_property;
  jpeg_codec.virt.base.set_property = bl_object_impl_set_property;
  jpeg_codec.virt.inspect_data = jpeg_codec_inspect_data;
  jpeg_codec.virt.create_decoder = jpeg_codec_create_decoder;
  jpeg_codec.virt.create_encoder = jpeg_codec_create_encoder;

  jpeg_codec.impl->ctor();
  jpeg_codec.impl->features =
    BL_IMAGE_CODEC_FEATURE_READ  |
    BL_IMAGE_CODEC_FEATURE_WRITE |
    BL_IMAGE_CODEC_FEATURE_LOSSY ;
  jpeg_codec.impl->name.dcast().assign("JPEG");
  jpeg_codec.impl->vendor.dcast().assign("Blend2D");
  jpeg_codec.impl->mime_type.dcast().assign("image/jpeg");
  jpeg_codec.impl->extensions.dcast().assign("jpg|jpeg|jif|jfi|jfif");
  ObjectLayer::init_virt(&jpeg_codec_object, BL_OBJECT_TYPE_IMAGE_CODEC, 0u, &jpeg_codec.impl, &jpeg_codec.virt);

  // Initialize JPEG decoder virtual functions.
  jpeg_decoder_virt.base.destroy = jpeg_decoder_destroy;
  jpeg_decoder_virt.base.get_property = bl_object_impl_get_property;
  jpeg_decoder_virt.base.set_property = bl_object_impl_set_property;
  jpeg_decoder_virt.restart = jpeg_decoder_restart;
  jpeg_decoder_virt.read_info = jpeg_decoder_read_info;
  jpeg_decoder_virt.read_frame = jpeg_decoder_read_frame;

  // Initialize JPEG encoder virtual functions.
  // TODO: [JPEG] Encoder

  codecs->append(jpeg_codec_object.dcast());
}

} // {bl::Jpeg}

#endif // !BL_BUILD_NO_CODECS
