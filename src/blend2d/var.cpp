
// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "bitset_p.h"
#include "font_p.h"
#include "fontfeaturesettings_p.h"
#include "fontvariationsettings_p.h"
#include "gradient_p.h"
#include "image_p.h"
#include "object_p.h"
#include "path_p.h"
#include "pattern_p.h"
#include "math_p.h"
#include "rgba_p.h"
#include "string_p.h"
#include "var_p.h"

// BLVar - API - Init & Destroy
// ============================

BL_API_IMPL BLResult blVarInitType(BLUnknown* self, BLObjectType type) noexcept {
  BLResult result = BL_SUCCESS;

  if (BL_UNLIKELY(uint32_t(type) > BL_OBJECT_TYPE_MAX_VALUE)) {
    type = BL_OBJECT_TYPE_NULL;
    result = blTraceError(BL_ERROR_INVALID_VALUE);
  }

  blAsObject(self)->_d = blObjectDefaults[type]._d;
  return result;
}

BL_API_IMPL BLResult blVarInitNull(BLUnknown* self) noexcept {
  blAsObject(self)->_d.initNull();
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blVarInitBool(BLUnknown* self, bool value) noexcept {
  blAsObject(self)->_d.initBool(value);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blVarInitInt32(BLUnknown* self, int32_t value) noexcept {
  blAsObject(self)->_d.initInt64(value);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blVarInitInt64(BLUnknown* self, int64_t value) noexcept {
  blAsObject(self)->_d.initInt64(value);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blVarInitUInt32(BLUnknown* self, uint32_t value) noexcept {
  blAsObject(self)->_d.initUInt64(value);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blVarInitUInt64(BLUnknown* self, uint64_t value) noexcept {
  blAsObject(self)->_d.initUInt64(value);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blVarInitDouble(BLUnknown* self, double value) noexcept {
  blAsObject(self)->_d.initDouble(value);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blVarInitRgba(BLUnknown* self, const BLRgba* rgba) noexcept {
  return BLVarPrivate::initRgba(blAsObject(self), rgba);
}

BL_API_IMPL BLResult blVarInitRgba32(BLUnknown* self, uint32_t rgba32) noexcept {
  return BLVarPrivate::initRgba32(blAsObject(self), BLRgba32(rgba32));
}

BL_API_IMPL BLResult blVarInitRgba64(BLUnknown* self, uint64_t rgba64) noexcept {
  return BLVarPrivate::initRgba64(blAsObject(self), BLRgba64(rgba64));
}

BL_API_IMPL BLResult blVarInitMove(BLUnknown* self, BLUnknown* other) noexcept {
  BL_ASSERT(self != other);

  return blObjectPrivateInitMoveUnknown(blAsObject(self), blAsObject(other));
}

BL_API_IMPL BLResult blVarInitWeak(BLUnknown* self, const BLUnknown* other) noexcept {
  BL_ASSERT(static_cast<void*>(self) != other);

  return blObjectPrivateInitWeakUnknown(blAsObject(self), blAsObject(other));
}

BL_API_IMPL BLResult blVarDestroy(BLUnknown* self) noexcept {
  return blObjectPrivateReleaseUnknown(blAsObject(self));
}

// BLVar - API - Reset
// ===================

BL_API_IMPL BLResult blVarReset(BLUnknown* self) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  blAsObject(self)->_d.initNull();
  return blObjectPrivateReleaseUnknown(&tmp);
}

// BLVar - API - Assign
// ====================

BL_API_IMPL BLResult blVarAssignNull(BLUnknown* self) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  blAsObject(self)->_d.initNull();
  return blObjectPrivateReleaseUnknown(&tmp);
}

BL_API_IMPL BLResult blVarAssignBool(BLUnknown* self, bool value) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  blAsObject(self)->_d.initBool(value);
  return blObjectPrivateReleaseUnknown(&tmp);
}

BL_API_IMPL BLResult blVarAssignInt32(BLUnknown* self, int32_t value) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  blAsObject(self)->_d.initInt64(value);
  return blObjectPrivateReleaseUnknown(&tmp);
}

BL_API_IMPL BLResult blVarAssignInt64(BLUnknown* self, int64_t value) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  blAsObject(self)->_d.initInt64(value);
  return blObjectPrivateReleaseUnknown(&tmp);
}

BL_API_IMPL BLResult blVarAssignUInt32(BLUnknown* self, uint32_t value) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  blAsObject(self)->_d.initUInt64(value);
  return blObjectPrivateReleaseUnknown(&tmp);
}

BL_API_IMPL BLResult blVarAssignUInt64(BLUnknown* self, uint64_t value) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  blAsObject(self)->_d.initUInt64(value);
  return blObjectPrivateReleaseUnknown(&tmp);
}

BL_API_IMPL BLResult blVarAssignDouble(BLUnknown* self, double value) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  blAsObject(self)->_d.initDouble(value);
  return blObjectPrivateReleaseUnknown(&tmp);
}

BL_API_IMPL BLResult blVarAssignRgba(BLUnknown* self, const BLRgba* rgba) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  BLVarPrivate::initRgba(blAsObject(self), rgba);
  return blObjectPrivateReleaseUnknown(&tmp);
}

BL_API_IMPL BLResult blVarAssignRgba32(BLUnknown* self, uint32_t rgba32) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  BLVarPrivate::initRgba32(blAsObject(self), BLRgba32(rgba32));
  return blObjectPrivateReleaseUnknown(&tmp);
}

BL_API_IMPL BLResult blVarAssignRgba64(BLUnknown* self, uint64_t rgba64) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  BLVarPrivate::initRgba64(blAsObject(self), BLRgba64(rgba64));
  return blObjectPrivateReleaseUnknown(&tmp);
}

BL_API_IMPL BLResult blVarAssignMove(BLUnknown* self, BLUnknown* other) noexcept {
  BLObjectType otherType = blAsObject(other)->_d.getType();
  BLObjectCore tmp = *blAsObject(other);

  blAsObject(other)->_d = blObjectDefaults[otherType]._d;
  blObjectPrivateReleaseUnknown(blAsObject(self));

  blAsObject(self)->_d = tmp._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blVarAssignWeak(BLUnknown* self, const BLUnknown* other) noexcept {
  return blObjectPrivateAssignWeakUnknown(blAsObject(self), blAsObject(other));
}

// BLVar - API - Get Type & Value
// ==============================

BL_API_IMPL BLObjectType blVarGetType(const BLUnknown* self) noexcept {
  return blAsObject(self)->_d.getType();
}

BL_API_IMPL BLResult blVarToBool(const BLUnknown* self, bool* out) noexcept {
  const BLObjectDetail& d = blAsObject(self)->_d;

  switch (d.getType()) {
    case BL_OBJECT_TYPE_NULL: {
      *out = false;
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_BOOL:
    case BL_OBJECT_TYPE_INT64:
    case BL_OBJECT_TYPE_UINT64: {
      *out = (d.u64_data[0] != 0);
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_DOUBLE: {
      *out = (d.f64_data[0] != 0.0 && !blIsNaN(d.f64_data[0]));
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_STRING: {
      return !static_cast<const BLString*>(static_cast<const BLObjectCore*>(self))->empty();
    }

    default: {
      *out = false;
      return blTraceError(BL_ERROR_INVALID_CONVERSION);
    }
  }
}

BL_API_IMPL BLResult blVarToInt32(const BLUnknown* self, int32_t* out) noexcept {
  const BLObjectDetail& d = blAsObject(self)->_d;

  switch (d.getType()) {
    case BL_OBJECT_TYPE_NULL: {
      *out = 0;
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_BOOL: {
      *out = int32_t(d.i64_data[0] & 0xFFFFFFFFu);
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_INT64: {
      int64_t v = d.i64_data[0];

      if (v < int64_t(INT32_MIN)) {
        *out = INT32_MIN;
        return blTraceError(BL_ERROR_OVERFLOW);
      }

      if (v > int64_t(INT32_MAX)) {
        *out = INT32_MAX;
        return blTraceError(BL_ERROR_OVERFLOW);
      }

      *out = int32_t(v);
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_UINT64: {
      if (d.u64_data[0] > uint64_t(INT32_MAX)) {
        *out = INT32_MAX;
        return blTraceError(BL_ERROR_OVERFLOW);
      }

      *out = int32_t(d.i64_data[0]);
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_DOUBLE: {
      double f = d.f64_data[0];

      if (blIsNaN(f)) {
        *out = 0;
        return blTraceError(BL_ERROR_INVALID_CONVERSION);
      }

      if (f < double(INT32_MIN)) {
        *out = INT32_MIN;
        return blTraceError(BL_ERROR_OVERFLOW);
      }

      if (f > double(INT32_MAX)) {
        *out = INT32_MAX;
        return blTraceError(BL_ERROR_OVERFLOW);
      }

      int32_t v = blTruncToInt(f);

      *out = v;
      if (double(v) != f)
        return blTraceError(BL_ERROR_OVERFLOW);
      else
        return BL_SUCCESS;
    }

    default: {
      *out = 0;
      return blTraceError(BL_ERROR_INVALID_CONVERSION);
    }
  }
}

BL_API_IMPL BLResult blVarToInt64(const BLUnknown* self, int64_t* out) noexcept {
  const BLObjectDetail& d = blAsObject(self)->_d;

  switch (d.getType()) {
    case BL_OBJECT_TYPE_NULL: {
      *out = 0;
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_BOOL:
    case BL_OBJECT_TYPE_INT64: {
      *out = d.i64_data[0];
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_UINT64: {
      if (d.u64_data[0] > uint64_t(INT64_MAX)) {
        *out = INT64_MAX;
        return blTraceError(BL_ERROR_OVERFLOW);
      }

      *out = d.i64_data[0];
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_DOUBLE: {
      double f = d.f64_data[0];
      if (blIsNaN(f)) {
        *out = 0;
        return blTraceError(BL_ERROR_INVALID_CONVERSION);
      }

      if (f < double(INT64_MIN)) {
        *out = INT64_MIN;
        return blTraceError(BL_ERROR_OVERFLOW);
      }

      if (f > double(INT64_MAX)) {
        *out = INT64_MAX;
        return blTraceError(BL_ERROR_OVERFLOW);
      }

      int64_t v = blTruncToInt64(f);

      *out = v;
      if (double(v) != f)
        return blTraceError(BL_ERROR_OVERFLOW);
      else
        return BL_SUCCESS;
    }

    default: {
      *out = 0;
      return blTraceError(BL_ERROR_INVALID_CONVERSION);
    }
  }
}

BL_API_IMPL BLResult blVarToUInt32(const BLUnknown* self, uint32_t* out) noexcept {
  const BLObjectDetail& d = blAsObject(self)->_d;

  switch (d.getType()) {
    case BL_OBJECT_TYPE_NULL: {
      *out = 0;
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_BOOL: {
      *out = uint32_t(d.u64_data[0] & 0xFFFFFFFFu);
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_UINT64: {
      uint64_t v = d.u64_data[0];

      if (v > uint64_t(UINT32_MAX)) {
        *out = UINT32_MAX;
        return blTraceError(BL_ERROR_OVERFLOW);
      }

      *out = uint32_t(v);
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_INT64: {
      int64_t v = d.i64_data[0];

      if (v < 0) {
        *out = 0;
        return blTraceError(BL_ERROR_OVERFLOW);
      }

      if (v > int64_t(UINT32_MAX)) {
        *out = UINT32_MAX;
        return blTraceError(BL_ERROR_OVERFLOW);
      }

      *out = uint32_t(v);
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_DOUBLE: {
      double f = d.f64_data[0];
      if (blIsNaN(f)) {
        *out = 0u;
        return blTraceError(BL_ERROR_INVALID_CONVERSION);
      }

      if (f < 0) {
        *out = 0u;
        return blTraceError(BL_ERROR_OVERFLOW);
      }

      if (f > double(UINT32_MAX)) {
        *out = UINT32_MAX;
        return blTraceError(BL_ERROR_OVERFLOW);
      }

      uint32_t v = uint32_t(f);

      *out = v;
      if (double(v) != f)
        return blTraceError(BL_ERROR_OVERFLOW);

      return BL_SUCCESS;
    }

    default: {
      *out = 0u;
      return blTraceError(BL_ERROR_INVALID_CONVERSION);
    }
  }
}

BL_API_IMPL BLResult blVarToUInt64(const BLUnknown* self, uint64_t* out) noexcept {
  const BLObjectDetail& d = blAsObject(self)->_d;

  switch (d.getType()) {
    case BL_OBJECT_TYPE_NULL: {
      *out = 0;
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_BOOL:
    case BL_OBJECT_TYPE_UINT64: {
      *out = d.u64_data[0];
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_INT64: {
      int64_t v = d.i64_data[0];

      if (v < 0) {
        *out = 0;
        return blTraceError(BL_ERROR_OVERFLOW);
      }

      *out = uint64_t(v);
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_DOUBLE: {
      double f = d.f64_data[0];

      if (blIsNaN(f)) {
        *out = 0u;
        return blTraceError(BL_ERROR_INVALID_CONVERSION);
      }

      if (f < 0) {
        *out = 0u;
        return blTraceError(BL_ERROR_OVERFLOW);
      }

      if (f > double(UINT64_MAX)) {
        *out = UINT64_MAX;
        return blTraceError(BL_ERROR_OVERFLOW);
      }

      uint64_t v = uint64_t(f);

      *out = v;
      if (double(v) != f)
        return blTraceError(BL_ERROR_OVERFLOW);

      return BL_SUCCESS;
    }

    default: {
      *out = 0u;
      return blTraceError(BL_ERROR_INVALID_CONVERSION);
    }
  }
}

BL_API_IMPL BLResult blVarToDouble(const BLUnknown* self, double* out) noexcept {
  const BLObjectDetail& d = blAsObject(self)->_d;

  switch (d.getType()) {
    case BL_OBJECT_TYPE_NULL: {
      *out = 0.0;
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_BOOL:
      *out = d.f64_data[0] ? 1.0 : 0.0;
      return BL_SUCCESS;

    case BL_OBJECT_TYPE_INT64: {
      double v = double(d.i64_data[0]);

      *out = v;
      if (int64_t(v) != d.i64_data[0])
        return blTraceError(BL_ERROR_OVERFLOW);

      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_UINT64: {
      double v = double(d.u64_data[0]);

      *out = v;
      if (uint64_t(v) != d.u64_data[0])
        return blTraceError(BL_ERROR_OVERFLOW);

      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_DOUBLE: {
      *out = d.f64_data[0];
      return BL_SUCCESS;
    }

    default: {
      *out = 0.0;
      return blTraceError(BL_ERROR_INVALID_STATE);
    }
  }
}

BL_API_IMPL BLResult blVarToRgba(const BLUnknown* self, BLRgba* out) noexcept {
  const BLObjectDetail& d = blAsObject(self)->_d;

  if (!d.hasObjectSignature()) {
    out->reset(d.f32_data[0], d.f32_data[1], d.f32_data[2], d.f32_data[3]);
    return BL_SUCCESS;
  }

  return blTraceError(BL_ERROR_INVALID_STATE);
}

BL_API_IMPL BLResult blVarToRgba32(const BLUnknown* self, uint32_t* out) noexcept {
  const BLObjectDetail& d = blAsObject(self)->_d;

  if (!d.hasObjectSignature()) {
    *out = reinterpret_cast<const BLRgba*>(self)->toRgba32().value;
    return BL_SUCCESS;
  }

  return blTraceError(BL_ERROR_INVALID_STATE);
}

BL_API_IMPL BLResult blVarToRgba64(const BLUnknown* self, uint64_t* out) noexcept {
  const BLObjectDetail& d = blAsObject(self)->_d;

  if (!d.hasObjectSignature()) {
    *out = reinterpret_cast<const BLRgba*>(self)->toRgba64().value;
    return BL_SUCCESS;
  }

  return blTraceError(BL_ERROR_INVALID_STATE);
}

// BLVar - API - Equality & Comparison
// ===================================

BL_API_IMPL bool blVarEquals(const BLUnknown* a, const BLUnknown* b) noexcept {
  if (blObjectPrivateBinaryEquals(blAsObject(a), blAsObject(b)))
    return true;

  const BLObjectDetail& aD = blAsObject(a)->_d;
  const BLObjectDetail& bD = blAsObject(b)->_d;

  BLObjectType aType = aD.getType();
  BLObjectType bType = bD.getType();

  if (aType != bType) {
    if (bType == BL_OBJECT_TYPE_BOOL)
      return blVarEqualsBool(a, bD.u64_data[0] != 0);

    if (bType == BL_OBJECT_TYPE_INT64)
      return blVarEqualsInt64(a, bD.i64_data[0]);

    if (bType == BL_OBJECT_TYPE_UINT64)
      return blVarEqualsUInt64(a, bD.u64_data[0]);

    if (bType == BL_OBJECT_TYPE_DOUBLE)
      return blVarEqualsDouble(a, bD.f64_data[0]);

    return false;
  }

  switch (aType) {
    case BL_OBJECT_TYPE_NULL:
      // Suspicious: NULL objects should be binary equal - this should never happen.
      return true;

    case BL_OBJECT_TYPE_RGBA:
      // BLRgba must be binary equal.
      return false;

    case BL_OBJECT_TYPE_PATTERN:
      return blPatternEquals(static_cast<const BLPatternCore*>(a), static_cast<const BLPatternCore*>(b));

    case BL_OBJECT_TYPE_GRADIENT:
      return blGradientEquals(static_cast<const BLGradientCore*>(a), static_cast<const BLGradientCore*>(b));

    case BL_OBJECT_TYPE_IMAGE:
      return blImageEquals(static_cast<const BLImageCore*>(a), static_cast<const BLImageCore*>(b));

    case BL_OBJECT_TYPE_PATH:
      return blPathEquals(static_cast<const BLPathCore*>(a), static_cast<const BLPathCore*>(b));

    case BL_OBJECT_TYPE_FONT:
      return blFontEquals(static_cast<const BLFontCore*>(a), static_cast<const BLFontCore*>(b));

    case BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS:
      return blFontFeatureSettingsEquals(static_cast<const BLFontFeatureSettingsCore*>(a), static_cast<const BLFontFeatureSettingsCore*>(b));

    case BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS:
      return blFontVariationSettingsEquals(static_cast<const BLFontVariationSettingsCore*>(a), static_cast<const BLFontVariationSettingsCore*>(b));

    case BL_OBJECT_TYPE_BIT_SET:
      return blBitSetEquals(static_cast<const BLBitSetCore*>(a), static_cast<const BLBitSetCore*>(b));

    case BL_OBJECT_TYPE_BOOL:
    case BL_OBJECT_TYPE_INT64:
    case BL_OBJECT_TYPE_UINT64:
      // These must be binary equal.
      return false;

    case BL_OBJECT_TYPE_DOUBLE:
      return aD.f64_data[0] == aD.f64_data[0];

    case BL_OBJECT_TYPE_STRING:
      return blStringEquals(static_cast<const BLStringCore*>(a), static_cast<const BLStringCore*>(b));

    case BL_OBJECT_TYPE_ARRAY_OBJECT:
    case BL_OBJECT_TYPE_ARRAY_INT8:
    case BL_OBJECT_TYPE_ARRAY_UINT8:
    case BL_OBJECT_TYPE_ARRAY_INT16:
    case BL_OBJECT_TYPE_ARRAY_UINT16:
    case BL_OBJECT_TYPE_ARRAY_INT32:
    case BL_OBJECT_TYPE_ARRAY_UINT32:
    case BL_OBJECT_TYPE_ARRAY_INT64:
    case BL_OBJECT_TYPE_ARRAY_UINT64:
    case BL_OBJECT_TYPE_ARRAY_FLOAT32:
    case BL_OBJECT_TYPE_ARRAY_FLOAT64:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_1:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_2:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_3:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_4:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_6:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_8:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_10:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_12:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_16:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_20:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_24:
    case BL_OBJECT_TYPE_ARRAY_STRUCT_32:
      return blArrayEquals(static_cast<const BLArrayCore*>(a), static_cast<const BLArrayCore*>(b));

    default:
      return false;
  }
}

BL_API_IMPL bool blVarEqualsNull(const BLUnknown* self) noexcept {
  const BLObjectDetail& d = blAsObject(self)->_d;

  return d.getType() == BL_OBJECT_TYPE_NULL;
}

BL_API_IMPL bool blVarEqualsBool(const BLUnknown* self, bool value) noexcept {
  const BLObjectDetail& d = blAsObject(self)->_d;

  switch (d.getType()) {
    case BL_OBJECT_TYPE_NULL:
      return false;

    case BL_OBJECT_TYPE_BOOL:
    case BL_OBJECT_TYPE_INT64:
    case BL_OBJECT_TYPE_UINT64:
      return d.u64_data[0] == uint64_t(value);

    case BL_OBJECT_TYPE_DOUBLE:
      return d.f64_data[0] == double(value);

    default:
      return false;
  }
}

BL_API_IMPL bool blVarEqualsInt64(const BLUnknown* self, int64_t value) noexcept {
  const BLObjectDetail& d = blAsObject(self)->_d;

  switch (d.getType()) {
    case BL_OBJECT_TYPE_NULL:
      return false;

    case BL_OBJECT_TYPE_BOOL:
    case BL_OBJECT_TYPE_INT64:
      return d.i64_data[0] == value;

    case BL_OBJECT_TYPE_UINT64:
      return d.i64_data[0] == value && value >= 0;

    case BL_OBJECT_TYPE_DOUBLE:
      return d.f64_data[0] == double(value) && int64_t(double(value)) == value;

    default:
      return false;
  }

}

BL_API_IMPL bool blVarEqualsUInt64(const BLUnknown* self, uint64_t value) noexcept {
  const BLObjectDetail& d = blAsObject(self)->_d;

  switch (d.getType()) {
    case BL_OBJECT_TYPE_NULL:
      return false;

    case BL_OBJECT_TYPE_BOOL:
    case BL_OBJECT_TYPE_UINT64:
      return d.u64_data[0] == value;

    case BL_OBJECT_TYPE_INT64:
      return d.u64_data[0] == value && d.i64_data[0] >= 0;

    case BL_OBJECT_TYPE_DOUBLE:
      return d.f64_data[0] == double(value) && uint64_t(double(value)) == value;

    default:
      return false;
  }
}

BL_API_IMPL bool blVarEqualsDouble(const BLUnknown* self, double value) noexcept {
  const BLObjectDetail& d = blAsObject(self)->_d;

  switch (d.getType()) {
    case BL_OBJECT_TYPE_NULL:
      return false;

    case BL_OBJECT_TYPE_BOOL:
      return double(d.u64_data[0]) == value;

    case BL_OBJECT_TYPE_INT64:
      return double(d.i64_data[0]) == value && int64_t(double(d.i64_data[0])) == d.i64_data[0];

    case BL_OBJECT_TYPE_UINT64:
      return double(d.u64_data[0]) == value && uint64_t(double(d.u64_data[0])) == d.u64_data[0];

    case BL_OBJECT_TYPE_DOUBLE:
      return d.f64_data[0] == value || (blIsNaN(d.f64_data[0]) && blIsNaN(value));

    default:
      return false;
  }
}

BL_API_IMPL bool blVarStrictEquals(const BLUnknown* a, const BLUnknown* b) noexcept {
  return blObjectPrivateBinaryEquals(blAsObject(a), blAsObject(b));
}

// BLVar - Tests
// =============

#ifdef BL_TEST
UNIT(var) {
  INFO("Verifying null value functionality");
  {
    EXPECT_EQ(BLVar().type(), BL_OBJECT_TYPE_NULL);
    EXPECT_EQ(BLVar(), BLVar());

    // Null can be used as a style to draw nothing.
    EXPECT_TRUE(BLVar().isStyle());
  }

  INFO("Verifying bool value functionality");
  {
    EXPECT_EQ(BLVar(true).type(), BL_OBJECT_TYPE_BOOL);
    EXPECT_EQ(BLVar(true), true);
    EXPECT_EQ(BLVar(true), 1);
    EXPECT_EQ(BLVar(true), 1u);
    EXPECT_EQ(BLVar(true), 1.0);
    EXPECT_EQ(BLVar(true), BLVar(true));
    EXPECT_EQ(BLVar(true), BLVar(1));
    EXPECT_EQ(BLVar(true), BLVar(1u));
    EXPECT_EQ(BLVar(true), BLVar(1.0));

    EXPECT_EQ(BLVar(false).type(), BL_OBJECT_TYPE_BOOL);
    EXPECT_EQ(BLVar(false), false);
    EXPECT_EQ(BLVar(false), 0);
    EXPECT_EQ(BLVar(false), 0u);
    EXPECT_EQ(BLVar(false), 0.0);
    EXPECT_EQ(BLVar(false), BLVar(false));
    EXPECT_EQ(BLVar(false), BLVar(0));
    EXPECT_EQ(BLVar(false), BLVar(0u));
    EXPECT_EQ(BLVar(false), BLVar(0.0));
  }

  INFO("Verifying int/uint value functionality");
  {
    EXPECT_EQ(BLVar(0).type(), BL_OBJECT_TYPE_INT64);
    EXPECT_EQ(BLVar(1).type(), BL_OBJECT_TYPE_INT64);
    EXPECT_EQ(BLVar(0u).type(), BL_OBJECT_TYPE_UINT64);
    EXPECT_EQ(BLVar(1u).type(), BL_OBJECT_TYPE_UINT64);
    EXPECT_EQ(BLVar(INT64_MAX).type(), BL_OBJECT_TYPE_INT64);
    EXPECT_EQ(BLVar(INT64_MIN).type(), BL_OBJECT_TYPE_INT64);
    EXPECT_EQ(BLVar(UINT64_MAX).type(), BL_OBJECT_TYPE_UINT64);

    EXPECT_EQ(BLVar(0), 0);
    EXPECT_EQ(BLVar(0u), 0u);
    EXPECT_EQ(BLVar(0), false);
    EXPECT_EQ(BLVar(1), 1);
    EXPECT_EQ(BLVar(1u), 1u);
    EXPECT_EQ(BLVar(1), true);
    EXPECT_EQ(BLVar(-1), -1);
    EXPECT_EQ(BLVar(char(1)), 1);
    EXPECT_EQ(BLVar(int64_t(0)), int64_t(0));
    EXPECT_EQ(BLVar(int64_t(1)), int64_t(1));
    EXPECT_EQ(BLVar(int64_t(-1)), int64_t(-1));
    EXPECT_EQ(BLVar(INT64_MIN), INT64_MIN);
    EXPECT_EQ(BLVar(INT64_MAX), INT64_MAX);
    EXPECT_EQ(BLVar(UINT64_MAX), UINT64_MAX);
    EXPECT_EQ(BLVar(uint64_t(0)), uint64_t(0));
    EXPECT_EQ(BLVar(uint64_t(1)), uint64_t(1));

    EXPECT_EQ(BLVar(int64_t(1)), BLVar(uint64_t(1)));
    EXPECT_EQ(BLVar(uint64_t(1)), BLVar(int64_t(1)));
    EXPECT_EQ(BLVar(double(1)), BLVar(int64_t(1)));
    EXPECT_EQ(BLVar(double(1)), BLVar(uint64_t(1)));
    EXPECT_EQ(BLVar(int64_t(1)), BLVar(double(1)));
    EXPECT_EQ(BLVar(uint64_t(1)), BLVar(double(1)));
  }

  INFO("Verifying float/double value functionality");
  {
    EXPECT_EQ(BLVar(0.0f).type(), BL_OBJECT_TYPE_DOUBLE);
    EXPECT_EQ(BLVar(0.0).type(), BL_OBJECT_TYPE_DOUBLE);
    EXPECT_EQ(BLVar(blNaN<float>()).type(), BL_OBJECT_TYPE_DOUBLE);
    EXPECT_EQ(BLVar(blNaN<double>()).type(), BL_OBJECT_TYPE_DOUBLE);

    EXPECT_EQ(BLVar(float(0)), float(0));
    EXPECT_EQ(BLVar(float(0.5)), float(0.5));
    EXPECT_EQ(BLVar(double(0)), double(0));
    EXPECT_EQ(BLVar(double(0.5)), double(0.5));
  }

  INFO("Verifying BLRgba value functionality");
  {
    EXPECT_EQ(BLVar(BLRgba(0.1f, 0.2f, 0.3f, 0.5f)).type(), BL_OBJECT_TYPE_RGBA);
    EXPECT_EQ(BLVar(BLRgba(0.1f, 0.2f, 0.3f, 0.5f)), BLVar(BLRgba(0.1f, 0.2f, 0.3f, 0.5f)));
    EXPECT_TRUE(BLVar(BLRgba(0.1f, 0.2f, 0.3f, 0.5f)).isRgba());
    EXPECT_TRUE(BLVar(BLRgba(0.1f, 0.2f, 0.3f, 0.5f)).isStyle());

    // Wrapped BLRgba is an exception - it doesn't form a valid BLObject signature.
    EXPECT_FALSE(BLVar(BLRgba(0.1f, 0.2f, 0.3f, 0.5f))._d.hasObjectSignature());
  }

  INFO("Checking BLGradient value functionality");
  {
    EXPECT_TRUE(BLVar(BLGradient()).isStyle());

    BLGradient g;
    g.addStop(0.0, BLRgba32(0x00000000u));
    g.addStop(1.0, BLRgba32(0xFFFFFFFFu));

    BLVar var(std::move(g));

    // The object should have been moved, so `g` should be default constructed now.
    EXPECT_EQ(g._d.getType(), BL_OBJECT_TYPE_GRADIENT);
    EXPECT_EQ(g, BLGradient());
    EXPECT_FALSE(var.equals(g));

    g = var.as<BLGradient>();
    EXPECT_TRUE(var.equals(g));
  }

  INFO("Checking BLPattern value functionality");
  {
    EXPECT_TRUE(BLVar(BLPattern()).isStyle());

    BLPattern p(BLImage(16, 16, BL_FORMAT_PRGB32));
    BLVar var(std::move(p));

    // The object should have been moved, so `p` should be default constructed now.
    EXPECT_EQ(p._d.getType(), BL_OBJECT_TYPE_PATTERN);
    EXPECT_EQ(p, BLPattern());
    EXPECT_FALSE(var.equals(p));

    p = var.as<BLPattern>();
    EXPECT_TRUE(var.equals(p));
  }

  INFO("Checking bool/int/uint/double value conversions");
  {
    bool b;
    EXPECT_TRUE(BLVar(0).toBool(&b) == BL_SUCCESS && b == false);
    EXPECT_TRUE(BLVar(1).toBool(&b) == BL_SUCCESS && b == true);
    EXPECT_TRUE(BLVar(0.0).toBool(&b) == BL_SUCCESS && b == false);
    EXPECT_TRUE(BLVar(blNaN<double>()).toBool(&b) == BL_SUCCESS && b == false);
    EXPECT_TRUE(BLVar(1.0).toBool(&b) == BL_SUCCESS && b == true);
    EXPECT_TRUE(BLVar(123456.0).toBool(&b) == BL_SUCCESS && b == true);
    EXPECT_TRUE(BLVar(-123456.0).toBool(&b) == BL_SUCCESS && b == true);

    int64_t i64;
    EXPECT_TRUE(BLVar(0).toInt64(&i64) == BL_SUCCESS && i64 == 0);
    EXPECT_TRUE(BLVar(1).toInt64(&i64) == BL_SUCCESS && i64 == 1);
    EXPECT_TRUE(BLVar(INT64_MIN).toInt64(&i64) == BL_SUCCESS && i64 == INT64_MIN);
    EXPECT_TRUE(BLVar(INT64_MAX).toInt64(&i64) == BL_SUCCESS && i64 == INT64_MAX);
    EXPECT_TRUE(BLVar(0.0).toInt64(&i64) == BL_SUCCESS && i64 == 0);
    EXPECT_TRUE(BLVar(blNaN<double>()).toInt64(&i64) == BL_ERROR_INVALID_CONVERSION && i64 == 0);
    EXPECT_TRUE(BLVar(1.0).toInt64(&i64) == BL_SUCCESS && i64 == 1);
    EXPECT_TRUE(BLVar(123456.0).toInt64(&i64) == BL_SUCCESS && i64 == 123456);
    EXPECT_TRUE(BLVar(-123456.0).toInt64(&i64) == BL_SUCCESS && i64 == -123456);
    EXPECT_TRUE(BLVar(123456.3).toInt64(&i64) == BL_ERROR_OVERFLOW && i64 == 123456);
    EXPECT_TRUE(BLVar(123456.9).toInt64(&i64) == BL_ERROR_OVERFLOW && i64 == 123456);

    uint64_t u64;
    EXPECT_TRUE(BLVar(0).toUInt64(&u64) == BL_SUCCESS && u64 == 0);
    EXPECT_TRUE(BLVar(1).toUInt64(&u64) == BL_SUCCESS && u64 == 1);
    EXPECT_TRUE(BLVar(INT64_MIN).toUInt64(&u64) == BL_ERROR_OVERFLOW && u64 == 0);
    EXPECT_TRUE(BLVar(INT64_MAX).toUInt64(&u64) == BL_SUCCESS && u64 == uint64_t(INT64_MAX));
    EXPECT_TRUE(BLVar(0.0).toUInt64(&u64) == BL_SUCCESS && u64 == 0);
    EXPECT_TRUE(BLVar(blNaN<double>()).toUInt64(&u64) == BL_ERROR_INVALID_CONVERSION && u64 == 0);
    EXPECT_TRUE(BLVar(1.0).toUInt64(&u64) == BL_SUCCESS && u64 == 1);
    EXPECT_TRUE(BLVar(123456.0).toUInt64(&u64) == BL_SUCCESS && u64 == 123456);
    EXPECT_TRUE(BLVar(-123456.0).toUInt64(&u64) == BL_ERROR_OVERFLOW && u64 == 0);
    EXPECT_TRUE(BLVar(123456.3).toUInt64(&u64) == BL_ERROR_OVERFLOW && u64 == 123456);
    EXPECT_TRUE(BLVar(123456.9).toUInt64(&u64) == BL_ERROR_OVERFLOW && u64 == 123456);

    double f64;
    EXPECT_TRUE(BLVar(true).toDouble(&f64) == BL_SUCCESS && f64 == 1.0);
    EXPECT_TRUE(BLVar(false).toDouble(&f64) == BL_SUCCESS && f64 == 0.0);
    EXPECT_TRUE(BLVar(0).toDouble(&f64) == BL_SUCCESS && f64 == 0.0);
    EXPECT_TRUE(BLVar(1).toDouble(&f64) == BL_SUCCESS && f64 == 1.0);
    EXPECT_TRUE(BLVar(0u).toDouble(&f64) == BL_SUCCESS && f64 == 0.0);
    EXPECT_TRUE(BLVar(1u).toDouble(&f64) == BL_SUCCESS && f64 == 1.0);
    EXPECT_TRUE(BLVar(0.0).toDouble(&f64) == BL_SUCCESS && f64 == 0.0);
    EXPECT_TRUE(BLVar(1.0).toDouble(&f64) == BL_SUCCESS && f64 == 1.0);
    EXPECT_TRUE(BLVar(blNaN<double>()).toDouble(&f64) == BL_SUCCESS && blIsNaN(f64));
  }
}
#endif
