// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "format_p.h"
#include "tables_p.h"

// BLFormatInfo - Globals
// ======================

const BLFormatInfo blFormatInfo[] = {
  #define U 0 // Used only to distinguish between zero and unused.
  // Public Formats:
  { 0 , blFormatFlagsStatic(BLInternalFormat(0)), {{ { U, U, U, U }, { U , U , U , U  } }} }, // <kNONE>
  { 32, blFormatFlagsStatic(BLInternalFormat(1)), {{ { 8, 8, 8, 8 }, { 16, 8 , 0 , 24 } }} }, // <kPRGB32>
  { 32, blFormatFlagsStatic(BLInternalFormat(2)), {{ { 8, 8, 8, U }, { 16, 8 , 0 , U  } }} }, // <kXRGB32>
  { 8 , blFormatFlagsStatic(BLInternalFormat(3)), {{ { U, U, U, 8 }, { U , U , U , 0  } }} }, // <kA8>
  // Internal Formats:
  { 32, blFormatFlagsStatic(BLInternalFormat(4)), {{ { 8, 8, 8, U }, { 16, 8 , 0 , U  } }} }, // <kFRGB32>
  { 32, blFormatFlagsStatic(BLInternalFormat(5)), {{ { 8, 8, 8, 8 }, { 16, 8 , 0 , 24 } }} }, // <kZERO32>
  #undef U
};

static_assert(uint32_t(BLInternalFormat::kMaxValue) == 5,
              "New formats must be added to 'blFormatInfo' table");

// BLFormatInfo - Tables
// =====================

// Indexes of components based on format flags that describe components. Each bit in the mask describes RGBA components
// (in order). Thus 0x1 descrines red component, 0x2 green 0x4 blue, and 0x8 alpha. Components can be combined so 0x7
// describes RGB and 0xF RGBA.
struct BLPixelConverterComponentIndexesGen {
  static constexpr uint8_t value(size_t i) noexcept {
    return i == BL_FORMAT_FLAG_RGB   ? uint8_t(0x7) :
           i == BL_FORMAT_FLAG_ALPHA ? uint8_t(0x8) :
           i == BL_FORMAT_FLAG_RGBA  ? uint8_t(0xF) :
           i == BL_FORMAT_FLAG_LUM   ? uint8_t(0x7) :
           i == BL_FORMAT_FLAG_LUMA  ? uint8_t(0xF) : uint8_t(0);
  }
};

static constexpr const auto blPixelConverterComponentIndexesTable =
  blMakeLookupTable<uint8_t, 16, BLPixelConverterComponentIndexesGen>();

// BLFormatInfo - Query
// ====================

BLResult blFormatInfoQuery(BLFormatInfo* self, BLFormat format) noexcept {
  if (BL_UNLIKELY(format == BL_FORMAT_NONE || format > BL_FORMAT_MAX_VALUE)) {
    self->reset();
    return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  *self = blFormatInfo[format];
  return BL_SUCCESS;
}

// BLFormatInfo - Sanitize
// =======================

static BL_INLINE bool blFormatInfoIsDepthValid(uint32_t depth) noexcept {
  switch (depth) {
    case 1:
    case 2:
    case 4:
    case 8:
    case 16:
    case 24:
    case 32:
      return true;

    default:
      return false;
  }
}

BL_API_IMPL BLResult blFormatInfoSanitize(BLFormatInfo* self) noexcept {
  BLFormatInfo& f = *self;

  // Filter out all flags that will be computed.
  f.flags &= BL_FORMAT_ALL_FLAGS;

  uint32_t i;
  bool masksOverlap = false;
  bool notByteAligned = false;
  bool crossesByteBoundary = false;
  bool hasUndefinedBits = false;

  // Check depth.
  if (!blFormatInfoIsDepthValid(f.depth))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (f.flags & BL_FORMAT_FLAG_INDEXED) {
    // In 32-bit mode shifts are not overlapping with `palette` so zero them.
    if (sizeof(void*) == 4)
      memset(f.shifts, 0, sizeof(f.shifts));

    // Indexed formats are up to 8 bits-per-pixel and must have palette.
    if (f.depth > 8 || !f.palette)
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }
  else {
    // Check whether RGB|A components are correct.
    uint64_t masksCombined = 0;

    // Check whether pixel components are specified correctly.
    uint32_t componentIndexes = blPixelConverterComponentIndexesTable[f.flags & 0xF];
    if (!componentIndexes)
      return blTraceError(BL_ERROR_INVALID_VALUE);

    for (i = 0; i < 4; i++) {
      uint32_t size = f.sizes[i];
      uint32_t shift = f.shifts[i];

      if (size == 0) {
        // Fail if this component must be provided.
        if (componentIndexes & (1u << i))
          return blTraceError(BL_ERROR_INVALID_VALUE);

        // Undefined size (0) must have zero shift as well. As it's not used it doesn't make sense to assign
        // it a value.
        if (shift != 0)
          return blTraceError(BL_ERROR_INVALID_VALUE);
      }
      else {
        // Fail if this component must not be provided.
        if (!(componentIndexes & (1u << i)))
          return blTraceError(BL_ERROR_INVALID_VALUE);

        // Fail if the size is too large.
        if (size > 16)
          return blTraceError(BL_ERROR_INVALID_VALUE);

        // Shifted mask overflows the pixel depth?
        if (shift + size > f.depth)
          return blTraceError(BL_ERROR_INVALID_VALUE);

        // Byte aligned means that shifts are [0, 8, 16, 24] and mask is 0xFF.
        if (size != 8 || (shift & 0x7u) != 0)
          notByteAligned = true;

        // Does the mask cross a byte-boundary?
        if ((shift / 8u) != ((shift + size - 1) / 8u))
          crossesByteBoundary = true;

        // Does the mask overlap with others?
        uint64_t maskAsU64 = uint64_t(BLIntOps::nonZeroLsbMask<uint32_t>(size)) << shift;
        if (masksCombined & maskAsU64) {
          masksOverlap = true;
          // Alpha channels cannot overlap.
          if (i == 3)
            return blTraceError(BL_ERROR_INVALID_VALUE);
        }

        masksCombined |= maskAsU64;
      }
    }

    if (BLIntOps::nonZeroLsbMask<uint64_t>(f.depth) ^ masksCombined)
      hasUndefinedBits = true;

    // Unset `BL_FORMAT_FLAG_PREMULTIPLIED` if the format doesn't have alpha.
    if (!(f.flags & (BL_FORMAT_FLAG_ALPHA)))
      f.flags &= ~BL_FORMAT_FLAG_PREMULTIPLIED;

    // It's allowed that masks overlap only when the pixel format describes a grayscale (LUM).
    bool isLUM = (f.flags & BL_FORMAT_FLAG_LUM) != 0;
    if (isLUM != masksOverlap)
      return blTraceError(BL_ERROR_INVALID_VALUE);

    // RGB components must match in grayscale (LUM) mode.
    if (isLUM && (f.rSize != f.gSize || f.rShift != f.gShift ||
                  f.gSize != f.bSize || f.gShift != f.bShift))
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  // Switch to a native byte-order if possible.
  if (f.flags & BL_FORMAT_FLAG_BYTE_SWAP) {
    if (f.depth <= 8) {
      // Switch to native byte-order if the depth <= 8.
      f.flags &= ~BL_FORMAT_FLAG_BYTE_SWAP;
    }
    else if (!crossesByteBoundary) {
      // Switch to native byte-order if no mask crosses byte boundaries.
      for (i = 0; i < 4; i++) {
        uint32_t size = f.sizes[i];
        if (!size)
          continue;
        f.shifts[i] = uint8_t(f.depth - f.shifts[i] - size);
      }

      f.flags &= ~BL_FORMAT_FLAG_BYTE_SWAP;
    }
  }

  // Add computed flags.
  if (!notByteAligned)
    f.flags |= BL_FORMAT_FLAG_BYTE_ALIGNED;

  if (hasUndefinedBits)
    f.flags |= BL_FORMAT_FLAG_UNDEFINED_BITS;

  return BL_SUCCESS;
}
