
// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "bitarray_p.h"
#include "bitset_p.h"
#include "font_p.h"
#include "fontfeaturesettings_p.h"
#include "fontvariationsettings_p.h"
#include "gradient_p.h"
#include "image_p.h"
#include "object_p.h"
#include "path_p.h"
#include "pattern_p.h"
#include "rgba_p.h"
#include "string_p.h"
#include "var_p.h"
#include "support/math_p.h"

// bl::Var - API - Init & Destroy
// ==============================

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
  return bl::VarInternal::initRgba(blAsObject(self), rgba);
}

BL_API_IMPL BLResult blVarInitRgba32(BLUnknown* self, uint32_t rgba32) noexcept {
  blAsObject(self)->_d.initRgba32(rgba32);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blVarInitRgba64(BLUnknown* self, uint64_t rgba64) noexcept {
  blAsObject(self)->_d.initRgba64(rgba64);
  return BL_SUCCESS;
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
  return bl::ObjectInternal::releaseUnknownInstance(blAsObject(self));
}

// bl::Var - API - Reset
// =====================

BL_API_IMPL BLResult blVarReset(BLUnknown* self) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  blAsObject(self)->_d.initNull();
  return bl::ObjectInternal::releaseUnknownInstance(&tmp);
}

// bl::Var - API - Assign
// ======================

BL_API_IMPL BLResult blVarAssignNull(BLUnknown* self) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  blAsObject(self)->_d.initNull();
  return bl::ObjectInternal::releaseUnknownInstance(&tmp);
}

BL_API_IMPL BLResult blVarAssignBool(BLUnknown* self, bool value) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  blAsObject(self)->_d.initBool(value);
  return bl::ObjectInternal::releaseUnknownInstance(&tmp);
}

BL_API_IMPL BLResult blVarAssignInt32(BLUnknown* self, int32_t value) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  blAsObject(self)->_d.initInt64(value);
  return bl::ObjectInternal::releaseUnknownInstance(&tmp);
}

BL_API_IMPL BLResult blVarAssignInt64(BLUnknown* self, int64_t value) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  blAsObject(self)->_d.initInt64(value);
  return bl::ObjectInternal::releaseUnknownInstance(&tmp);
}

BL_API_IMPL BLResult blVarAssignUInt32(BLUnknown* self, uint32_t value) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  blAsObject(self)->_d.initUInt64(value);
  return bl::ObjectInternal::releaseUnknownInstance(&tmp);
}

BL_API_IMPL BLResult blVarAssignUInt64(BLUnknown* self, uint64_t value) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  blAsObject(self)->_d.initUInt64(value);
  return bl::ObjectInternal::releaseUnknownInstance(&tmp);
}

BL_API_IMPL BLResult blVarAssignDouble(BLUnknown* self, double value) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  blAsObject(self)->_d.initDouble(value);
  return bl::ObjectInternal::releaseUnknownInstance(&tmp);
}

BL_API_IMPL BLResult blVarAssignRgba(BLUnknown* self, const BLRgba* rgba) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  bl::VarInternal::initRgba(blAsObject(self), rgba);
  return bl::ObjectInternal::releaseUnknownInstance(&tmp);
}

BL_API_IMPL BLResult blVarAssignRgba32(BLUnknown* self, uint32_t rgba32) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  blAsObject(self)->_d.initRgba32(rgba32);
  return bl::ObjectInternal::releaseUnknownInstance(&tmp);
}

BL_API_IMPL BLResult blVarAssignRgba64(BLUnknown* self, uint64_t rgba64) noexcept {
  BLObjectCore tmp = *blAsObject(self);
  blAsObject(self)->_d.initRgba64(rgba64);
  return bl::ObjectInternal::releaseUnknownInstance(&tmp);
}

BL_API_IMPL BLResult blVarAssignMove(BLUnknown* self, BLUnknown* other) noexcept {
  BLObjectType otherType = blAsObject(other)->_d.getType();
  BLObjectCore tmp = *blAsObject(other);

  blAsObject(other)->_d = blObjectDefaults[otherType]._d;
  bl::ObjectInternal::releaseUnknownInstance(blAsObject(self));

  blAsObject(self)->_d = tmp._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blVarAssignWeak(BLUnknown* self, const BLUnknown* other) noexcept {
  return blObjectPrivateAssignWeakUnknown(blAsObject(self), blAsObject(other));
}

// bl::Var - API - Get Type & Value
// ================================

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
      *out = (d.f64_data[0] != 0.0 && !bl::Math::isNaN(d.f64_data[0]));
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

      if (bl::Math::isNaN(f)) {
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

      int32_t v = bl::Math::truncToInt(f);

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

      if (bl::Math::isNaN(f)) {
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

      int64_t v = bl::Math::truncToInt64(f);

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

      if (bl::Math::isNaN(f)) {
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

      if (bl::Math::isNaN(f)) {
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

  if (d.isRgba32()) {
    *out = BLRgba(BLRgba32(d.u32_data[0]));
    return BL_SUCCESS;
  }

  if (d.isRgba64()) {
    *out = BLRgba(BLRgba64(d.u64_data[0]));
    return BL_SUCCESS;
  }

  return blTraceError(BL_ERROR_INVALID_STATE);
}

BL_API_IMPL BLResult blVarToRgba32(const BLUnknown* self, uint32_t* out) noexcept {
  const BLObjectDetail& d = blAsObject(self)->_d;

  if (d.isRgba32()) {
    *out = d.u32_data[0];
    return BL_SUCCESS;
  }

  if (d.isRgba64()) {
    *out = BLRgba32(BLRgba64(d.u64_data[0])).value;
    return BL_SUCCESS;
  }

  if (!d.hasObjectSignature()) {
    *out = reinterpret_cast<const BLRgba*>(self)->toRgba32().value;
    return BL_SUCCESS;
  }

  return blTraceError(BL_ERROR_INVALID_STATE);
}

BL_API_IMPL BLResult blVarToRgba64(const BLUnknown* self, uint64_t* out) noexcept {
  const BLObjectDetail& d = blAsObject(self)->_d;

  if (d.isRgba64()) {
    *out = d.u64_data[0];
    return BL_SUCCESS;
  }

  if (d.isRgba32()) {
    *out = BLRgba64(BLRgba32(d.u32_data[0])).value;
    return BL_SUCCESS;
  }

  if (!d.hasObjectSignature()) {
    *out = reinterpret_cast<const BLRgba*>(self)->toRgba64().value;
    return BL_SUCCESS;
  }

  return blTraceError(BL_ERROR_INVALID_STATE);
}

// bl::Var - API - Equality & Comparison
// =====================================

BL_API_IMPL bool blVarEquals(const BLUnknown* a, const BLUnknown* b) noexcept {
  const BLObjectDetail& aD = blAsObject(a)->_d;
  const BLObjectDetail& bD = blAsObject(b)->_d;

  if (aD == bD)
    return true;

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

    case BL_OBJECT_TYPE_BIT_ARRAY:
      return blBitArrayEquals(static_cast<const BLBitArrayCore*>(a), static_cast<const BLBitArrayCore*>(b));

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
      return d.f64_data[0] == value || (bl::Math::isNaN(d.f64_data[0]) && bl::Math::isNaN(value));

    default:
      return false;
  }
}

BL_API_IMPL bool BL_CDECL blVarEqualsRgba(const BLUnknown* self, const BLRgba* rgba) noexcept {
  const BLObjectDetail& d = blAsObject(self)->_d;

  if (!d.hasObjectSignature())
    return memcmp(d.f32_data, rgba, 4 * sizeof(float)) == 0;

  if (d.isRgba32()) {
    BLRgba converted = BLRgba(BLRgba32(d.u32_data[0]));
    return memcmp(&converted, rgba, 4 * sizeof(float)) == 0;
  }

  if (d.isRgba64()) {
    BLRgba converted = BLRgba(BLRgba64(d.u64_data[0]));
    return memcmp(&converted, rgba, 4 * sizeof(float)) == 0;
  }

  return false;
}

BL_API_IMPL bool BL_CDECL blVarEqualsRgba32(const BLUnknown* self, uint32_t rgba32) noexcept {
  const BLObjectDetail& d = blAsObject(self)->_d;

  if (d.isRgba32()) {
    return d.u32_data[0] == rgba32;
  }

  if (d.isRgba64()) {
    BLRgba64 converted = BLRgba64(BLRgba32(rgba32));
    return d.u64_data[0] == converted.value;
  }

  if (!d.hasObjectSignature()) {
    BLRgba converted = BLRgba(BLRgba32(rgba32));
    return memcmp(d.f32_data, &converted, 4 * sizeof(float)) == 0;
  }

  return false;
}

BL_API_IMPL bool BL_CDECL blVarEqualsRgba64(const BLUnknown* self, uint64_t rgba64) noexcept {
  const BLObjectDetail& d = blAsObject(self)->_d;

  if (d.isRgba32()) {
    BLRgba64 converted = BLRgba64(BLRgba32(d.u32_data[0]));
    return converted.value == rgba64;
  }

  if (d.isRgba64()) {
    return d.u64_data[0] == rgba64;
  }

  if (!d.hasObjectSignature()) {
    BLRgba rgba = BLRgba(BLRgba64(rgba64));
    return memcmp(d.f32_data, &rgba, 4 * sizeof(float)) == 0;
  }

  return false;
}

BL_API_IMPL bool blVarStrictEquals(const BLUnknown* a, const BLUnknown* b) noexcept {
  const BLObjectDetail& aD = blAsObject(a)->_d;
  const BLObjectDetail& bD = blAsObject(b)->_d;

  return aD == bD;
}
