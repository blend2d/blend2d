// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/math_p.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/bit_array_p.h>
#include <blend2d/core/coverage_set_p.h>
#include <blend2d/core/font_p.h>
#include <blend2d/core/font_feature_settings_p.h>
#include <blend2d/core/font_variation_settings_p.h>
#include <blend2d/core/gradient_p.h>
#include <blend2d/core/image_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/path_p.h>
#include <blend2d/core/pattern_p.h>
#include <blend2d/core/rgba_p.h>
#include <blend2d/core/string_p.h>
#include <blend2d/core/var_p.h>

// bl::Var - API - Init & Destroy
// ==============================

BL_API_IMPL BLResult bl_var_init_type(BLObjectCore* self, BLObjectType type) noexcept {
  BLResult result = BL_SUCCESS;

  if (BL_UNLIKELY(uint32_t(type) > BL_OBJECT_TYPE_MAX_VALUE)) {
    type = BL_OBJECT_TYPE_NULL;
    result = bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  self->_d = bl_object_defaults[type]._d;
  return result;
}

BL_API_IMPL BLResult bl_var_init_null(BLObjectCore* self) noexcept {
  using namespace bl;
  return ObjectLayer::init_null(self);
}

BL_API_IMPL BLResult bl_var_init_bool(BLObjectCore* self, bool value) noexcept {
  using namespace bl;
  return ObjectLayer::init_bool(self, value);
}

BL_API_IMPL BLResult bl_var_init_int32(BLObjectCore* self, int32_t value) noexcept {
  using namespace bl;
  return ObjectLayer::init_int64(self, int64_t(value));
}

BL_API_IMPL BLResult bl_var_init_int64(BLObjectCore* self, int64_t value) noexcept {
  using namespace bl;
  return ObjectLayer::init_int64(self, value);
}

BL_API_IMPL BLResult bl_var_init_uint32(BLObjectCore* self, uint32_t value) noexcept {
  using namespace bl;
  return ObjectLayer::init_uint64(self, uint64_t(value));
}

BL_API_IMPL BLResult bl_var_init_uint64(BLObjectCore* self, uint64_t value) noexcept {
  using namespace bl;
  return ObjectLayer::init_uint64(self, value);
}

BL_API_IMPL BLResult bl_var_init_double(BLObjectCore* self, double value) noexcept {
  using namespace bl;
  return ObjectLayer::init_double(self, value);
}

BL_API_IMPL BLResult bl_var_init_rgba(BLObjectCore* self, const BLRgba* rgba) noexcept {
  using namespace bl;
  return ObjectLayer::init_sso_with_value(self, BL_OBJECT_TYPE_RGBA, *rgba);
}

BL_API_IMPL BLResult bl_var_init_rgba32(BLObjectCore* self, uint32_t rgba32) noexcept {
  using namespace bl;
  return ObjectLayer::init_sso_with_value(self, BL_OBJECT_TYPE_RGBA32, rgba32);
}

BL_API_IMPL BLResult bl_var_init_rgba64(BLObjectCore* self, uint64_t rgba64) noexcept {
  using namespace bl;
  return ObjectLayer::init_sso_with_value(self, BL_OBJECT_TYPE_RGBA64, rgba64);
}

BL_API_IMPL BLResult bl_var_init_move(BLObjectCore* self, BLObjectCore* other) noexcept {
  using namespace bl;
  BL_ASSERT(self != other);

  return ObjectLayer::move_object(self, other);
}

BL_API_IMPL BLResult bl_var_init_weak(BLObjectCore* self, const BLObjectCore* other) noexcept {
  using namespace bl;
  BL_ASSERT(static_cast<void*>(self) != other);

  return ObjectLayer::copy_object(self, other);
}

BL_API_IMPL BLResult bl_var_destroy(BLObjectCore* self) noexcept {
  using namespace bl;
  return ObjectLayer::release_object(self);
}

// bl::Var - API - Reset
// =====================

BL_API_IMPL BLResult bl_var_reset(BLObjectCore* self) noexcept {
  using namespace bl;

  ObjectLayer::release_object(self);
  return ObjectLayer::init_null(self);
}

// bl::Var - API - Assign
// ======================

BL_API_IMPL BLResult bl_var_assign_null(BLObjectCore* self) noexcept {
  using namespace bl;

  ObjectLayer::release_object(self);
  return ObjectLayer::init_null(self);
}

BL_API_IMPL BLResult bl_var_assign_bool(BLObjectCore* self, bool value) noexcept {
  using namespace bl;

  ObjectLayer::release_object(self);
  return ObjectLayer::init_bool(self, value);
}

BL_API_IMPL BLResult bl_var_assign_int32(BLObjectCore* self, int32_t value) noexcept {
  using namespace bl;

  ObjectLayer::release_object(self);
  return ObjectLayer::init_int64(self, int64_t(value));
}

BL_API_IMPL BLResult bl_var_assign_int64(BLObjectCore* self, int64_t value) noexcept {
  using namespace bl;

  ObjectLayer::release_object(self);
  return ObjectLayer::init_int64(self, value);
}

BL_API_IMPL BLResult bl_var_assign_uint32(BLObjectCore* self, uint32_t value) noexcept {
  using namespace bl;

  ObjectLayer::release_object(self);
  return ObjectLayer::init_uint64(self, uint64_t(value));
}

BL_API_IMPL BLResult bl_var_assign_uint64(BLObjectCore* self, uint64_t value) noexcept {
  using namespace bl;

  ObjectLayer::release_object(self);
  return ObjectLayer::init_uint64(self, value);
}

BL_API_IMPL BLResult bl_var_assign_double(BLObjectCore* self, double value) noexcept {
  using namespace bl;

  ObjectLayer::release_object(self);
  return ObjectLayer::init_double(self, value);
}

BL_API_IMPL BLResult bl_var_assign_rgba(BLObjectCore* self, const BLRgba* rgba) noexcept {
  using namespace bl;

  BLRgba rgba_copy = *rgba;
  ObjectLayer::release_object(self);
  return ObjectLayer::init_sso_with_value(self, BL_OBJECT_TYPE_RGBA, rgba_copy);
}

BL_API_IMPL BLResult bl_var_assign_rgba32(BLObjectCore* self, uint32_t rgba32) noexcept {
  using namespace bl;

  ObjectLayer::release_object(self);
  return ObjectLayer::init_sso_with_value(self, BL_OBJECT_TYPE_RGBA32, rgba32);
}

BL_API_IMPL BLResult bl_var_assign_rgba64(BLObjectCore* self, uint64_t rgba64) noexcept {
  using namespace bl;

  ObjectLayer::release_object(self);
  return ObjectLayer::init_sso_with_value(self, BL_OBJECT_TYPE_RGBA64, rgba64);
}

BL_API_IMPL BLResult bl_var_assign_move(BLObjectCore* self, BLObjectCore* other) noexcept {
  using namespace bl;
  return ObjectLayer::move_object(self, other);
}

BL_API_IMPL BLResult bl_var_assign_weak(BLObjectCore* self, const BLObjectCore* other) noexcept {
  using namespace bl;
  return ObjectLayer::assign_object_weak(self, other);
}

// bl::Var - API - Get Type & Value
// ================================

BL_API_IMPL BLObjectType bl_var_get_type(const BLObjectCore* self) noexcept {
  return self->_d.type();
}

BL_API_IMPL BLResult bl_var_to_bool(const BLObjectCore* self, bool* out) noexcept {
  using namespace bl;

  switch (self->_d.type()) {
    case BL_OBJECT_TYPE_NULL: {
      *out = false;
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_BOOL:
    case BL_OBJECT_TYPE_INT64:
    case BL_OBJECT_TYPE_UINT64: {
      uint64_t value = ObjectLayer::sso_value<uint64_t>(self);
      *out = value != 0u;
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_FLOAT64: {
      double value = ObjectLayer::sso_value<double>(self);
      *out = axl::bool_and(value != 0.0, !Math::is_nan(value));
      return BL_SUCCESS;
    }

    case BL_OBJECT_TYPE_STRING: {
      *out = !static_cast<const BLString*>(self)->is_empty();
      return BL_SUCCESS;
    }

    default: {
      *out = false;
      return bl_make_error(BL_ERROR_INVALID_CONVERSION);
    }
  }
}

BL_API_IMPL BLResult bl_var_to_int32(const BLObjectCore* self, int32_t* out) noexcept {
  using namespace bl;
  BLResult result = BL_SUCCESS;

  switch (self->_d.type()) {
    case BL_OBJECT_TYPE_NULL: {
      *out = 0;
      return result;
    }

    case BL_OBJECT_TYPE_BOOL: {
      *out = int32_t(ObjectLayer::sso_value<uint64_t>(self) & 0xFFFFFFFFu);
      return result;
    }

    case BL_OBJECT_TYPE_INT64: {
      int64_t v = ObjectLayer::sso_value<int64_t>(self);
      if (v < int64_t(INT32_MIN)) {
        v = INT32_MIN;
        result = bl_make_error(BL_ERROR_OVERFLOW);
      }
      else if (v > int64_t(INT32_MAX)) {
        v = INT32_MAX;
        result = bl_make_error(BL_ERROR_OVERFLOW);
      }

      *out = int32_t(v);
      return result;
    }

    case BL_OBJECT_TYPE_UINT64: {
      uint64_t v = ObjectLayer::sso_value<uint64_t>(self);
      if (v > uint64_t(INT32_MAX)) {
        v = INT32_MAX;
        result = bl_make_error(BL_ERROR_OVERFLOW);
      }

      *out = int32_t(v);
      return result;
    }

    case BL_OBJECT_TYPE_FLOAT64: {
      double f = ObjectLayer::sso_value<double>(self);

      if (bl::Math::is_nan(f)) {
        *out = 0;
        return bl_make_error(BL_ERROR_INVALID_CONVERSION);
      }

      if (f < double(INT32_MIN)) {
        *out = INT32_MIN;
        return bl_make_error(BL_ERROR_OVERFLOW);
      }

      if (f > double(INT32_MAX)) {
        *out = INT32_MAX;
        return bl_make_error(BL_ERROR_OVERFLOW);
      }

      int32_t v = bl::Math::trunc_to_int(f);
      *out = v;

      if (double(v) != f)
        return bl_make_error(BL_ERROR_OVERFLOW);
      else
        return BL_SUCCESS;
    }

    default: {
      *out = 0;
      return bl_make_error(BL_ERROR_INVALID_CONVERSION);
    }
  }
}

BL_API_IMPL BLResult bl_var_to_int64(const BLObjectCore* self, int64_t* out) noexcept {
  using namespace bl;
  BLResult result = BL_SUCCESS;

  switch (self->_d.type()) {
    case BL_OBJECT_TYPE_NULL: {
      *out = 0;
      return result;
    }

    case BL_OBJECT_TYPE_BOOL:
    case BL_OBJECT_TYPE_INT64: {
      *out = ObjectLayer::sso_value<int64_t>(self);
      return result;
    }

    case BL_OBJECT_TYPE_UINT64: {
      uint64_t v = ObjectLayer::sso_value<uint64_t>(self);
      if (v > uint64_t(INT64_MAX)) {
        v = INT64_MAX;
        result = bl_make_error(BL_ERROR_OVERFLOW);
      }

      *out = int64_t(v);
      return result;
    }

    case BL_OBJECT_TYPE_FLOAT64: {
      double f = ObjectLayer::sso_value<double>(self);

      if (bl::Math::is_nan(f)) {
        *out = 0;
        return bl_make_error(BL_ERROR_INVALID_CONVERSION);
      }

      if (f < double(INT64_MIN)) {
        *out = INT64_MIN;
        return bl_make_error(BL_ERROR_OVERFLOW);
      }

      if (f > double(INT64_MAX)) {
        *out = INT64_MAX;
        return bl_make_error(BL_ERROR_OVERFLOW);
      }

      int64_t v = bl::Math::trunc_to_int64(f);

      *out = v;
      if (double(v) != f)
        return bl_make_error(BL_ERROR_OVERFLOW);
      else
        return BL_SUCCESS;
    }

    default: {
      *out = 0;
      return bl_make_error(BL_ERROR_INVALID_CONVERSION);
    }
  }
}

BL_API_IMPL BLResult bl_var_to_uint32(const BLObjectCore* self, uint32_t* out) noexcept {
  using namespace bl;
  BLResult result = BL_SUCCESS;

  switch (self->_d.type()) {
    case BL_OBJECT_TYPE_NULL: {
      *out = 0;
      return result;
    }

    case BL_OBJECT_TYPE_BOOL: {
      *out = uint32_t(ObjectLayer::sso_value<uint64_t>(self) & 0xFFFFFFFFu);
      return result;
    }

    case BL_OBJECT_TYPE_UINT64: {
      uint64_t v = ObjectLayer::sso_value<uint64_t>(self);
      if (v > uint64_t(UINT32_MAX)) {
        v = UINT32_MAX;
        result = bl_make_error(BL_ERROR_OVERFLOW);
      }

      *out = uint32_t(v);
      return result;
    }

    case BL_OBJECT_TYPE_INT64: {
      int64_t v = ObjectLayer::sso_value<int64_t>(self);

      if (v < 0) {
        v = int64_t(0);
        result = bl_make_error(BL_ERROR_OVERFLOW);
      }
      else if (v > int64_t(UINT32_MAX)) {
        v = int64_t(UINT32_MAX);
        result = bl_make_error(BL_ERROR_OVERFLOW);
      }

      *out = uint32_t(v);
      return result;
    }

    case BL_OBJECT_TYPE_FLOAT64: {
      double f = ObjectLayer::sso_value<double>(self);

      if (bl::Math::is_nan(f)) {
        *out = 0u;
        return bl_make_error(BL_ERROR_INVALID_CONVERSION);
      }

      if (f < 0) {
        *out = 0u;
        return bl_make_error(BL_ERROR_OVERFLOW);
      }

      if (f > double(UINT32_MAX)) {
        *out = UINT32_MAX;
        return bl_make_error(BL_ERROR_OVERFLOW);
      }

      uint32_t v = uint32_t(f);

      *out = v;
      if (double(v) != f)
        return bl_make_error(BL_ERROR_OVERFLOW);

      return BL_SUCCESS;
    }

    default: {
      *out = 0u;
      return bl_make_error(BL_ERROR_INVALID_CONVERSION);
    }
  }
}

BL_API_IMPL BLResult bl_var_to_uint64(const BLObjectCore* self, uint64_t* out) noexcept {
  using namespace bl;
  BLResult result = BL_SUCCESS;

  switch (self->_d.type()) {
    case BL_OBJECT_TYPE_NULL: {
      *out = 0;
      return result;
    }

    case BL_OBJECT_TYPE_BOOL:
    case BL_OBJECT_TYPE_UINT64: {
      *out = ObjectLayer::sso_value<uint64_t>(self);
      return result;
    }

    case BL_OBJECT_TYPE_INT64: {
      int64_t v = ObjectLayer::sso_value<int64_t>(self);
      if (v < 0) {
        v = 0;
        result = bl_make_error(BL_ERROR_OVERFLOW);
      }

      *out = uint64_t(v);
      return result;
    }

    case BL_OBJECT_TYPE_FLOAT64: {
      double f = ObjectLayer::sso_value<double>(self);

      if (bl::Math::is_nan(f)) {
        *out = 0u;
        return bl_make_error(BL_ERROR_INVALID_CONVERSION);
      }

      if (f < 0) {
        *out = 0u;
        return bl_make_error(BL_ERROR_OVERFLOW);
      }

      if (f > double(UINT64_MAX)) {
        *out = UINT64_MAX;
        return bl_make_error(BL_ERROR_OVERFLOW);
      }

      uint64_t v = uint64_t(f);

      *out = v;
      if (double(v) != f)
        return bl_make_error(BL_ERROR_OVERFLOW);

      return BL_SUCCESS;
    }

    default: {
      *out = 0u;
      return bl_make_error(BL_ERROR_INVALID_CONVERSION);
    }
  }
}

BL_API_IMPL BLResult bl_var_to_double(const BLObjectCore* self, double* out) noexcept {
  using namespace bl;
  BLResult result = BL_SUCCESS;

  switch (self->_d.type()) {
    case BL_OBJECT_TYPE_NULL: {
      *out = 0.0;
      return result;
    }

    case BL_OBJECT_TYPE_BOOL: {
      *out = ObjectLayer::sso_value<uint64_t>(self) ? 1.0 : 0.0;
      return result;
    }

    case BL_OBJECT_TYPE_INT64: {
      int64_t v = ObjectLayer::sso_value<int64_t>(self);
      double f = double(v);

      *out = f;
      if (int64_t(f) != v) {
        result = bl_make_error(BL_ERROR_OVERFLOW);
      }

      return result;
    }

    case BL_OBJECT_TYPE_UINT64: {
      uint64_t v = ObjectLayer::sso_value<uint64_t>(self);
      double f = double(v);

      *out = f;
      if (uint64_t(f) != v) {
        result = bl_make_error(BL_ERROR_OVERFLOW);
      }

      return result;
    }

    case BL_OBJECT_TYPE_FLOAT64: {
      *out = ObjectLayer::sso_value<double>(self);
      return BL_SUCCESS;
    }

    default: {
      *out = 0.0;
      return bl_make_error(BL_ERROR_INVALID_STATE);
    }
  }
}

BL_API_IMPL BLResult bl_var_to_rgba(const BLObjectCore* self, BLRgba* out) noexcept {
  using namespace bl;

  if (self->_d.is_rgba()) {
    *out = ObjectLayer::sso_value<BLRgba>(self);
    return BL_SUCCESS;
  }

  if (self->_d.is_rgba32()) {
    *out = BLRgba(ObjectLayer::sso_value<BLRgba32>(self));
    return BL_SUCCESS;
  }

  if (self->_d.is_rgba64()) {
    *out = BLRgba(ObjectLayer::sso_value<BLRgba64>(self));
    return BL_SUCCESS;
  }

  return bl_make_error(BL_ERROR_INVALID_STATE);
}

BL_API_IMPL BLResult bl_var_to_rgba32(const BLObjectCore* self, uint32_t* out) noexcept {
  using namespace bl;

  if (self->_d.is_rgba32()) {
    *out = ObjectLayer::sso_value<BLRgba32>(self).value;
    return BL_SUCCESS;
  }

  if (self->_d.is_rgba64()) {
    *out = BLRgba32(ObjectLayer::sso_value<BLRgba64>(self)).value;
    return BL_SUCCESS;
  }

  if (self->_d.is_rgba()) {
    *out = ObjectLayer::sso_value<BLRgba>(self).to_rgba32().value;
    return BL_SUCCESS;
  }

  return bl_make_error(BL_ERROR_INVALID_STATE);
}

BL_API_IMPL BLResult bl_var_to_rgba64(const BLObjectCore* self, uint64_t* out) noexcept {
  using namespace bl;

  if (self->_d.is_rgba64()) {
    *out = ObjectLayer::sso_value<BLRgba64>(self).value;
    return BL_SUCCESS;
  }

  if (self->_d.is_rgba32()) {
    *out = BLRgba64(ObjectLayer::sso_value<BLRgba32>(self)).value;
    return BL_SUCCESS;
  }

  if (self->_d.is_rgba()) {
    *out = ObjectLayer::sso_value<BLRgba>(self).to_rgba64().value;
    return BL_SUCCESS;
  }

  return bl_make_error(BL_ERROR_INVALID_STATE);
}

// bl::Var - API - Equality & Comparison
// =====================================

BL_API_IMPL bool bl_var_equals(const BLObjectCore* a, const BLObjectCore* b) noexcept {
  using namespace bl;

  // Binary equality works for all primitive values and objects in SSO mode.
  if (a->_d == b->_d) {
    return true;
  }

  BLObjectType a_type = a->_d.type();
  BLObjectType b_type = b->_d.type();

  if (a_type != b_type) {
    if (b_type == BL_OBJECT_TYPE_BOOL) {
      return bl_var_equals_bool(a, ObjectLayer::sso_value<uint64_t>(b) != 0u);
    }
    else if (b_type == BL_OBJECT_TYPE_INT64) {
      return bl_var_equals_int64(a, ObjectLayer::sso_value<int64_t>(b));
    }
    else if (b_type == BL_OBJECT_TYPE_UINT64) {
      return bl_var_equals_uint64(a, ObjectLayer::sso_value<uint64_t>(b));
    }
    else if (b_type == BL_OBJECT_TYPE_FLOAT64) {
      return bl_var_equals_double(a, ObjectLayer::sso_value<double>(b));
    }
    else {
      return false;
    }
  }

  switch (a_type) {
    case BL_OBJECT_TYPE_NULL:
      // Null objects should be binary equal - this should never happen.
      return true;

    case BL_OBJECT_TYPE_RGBA:
      // BLRgba must be binary equal.
      return false;

    case BL_OBJECT_TYPE_PATTERN:
      return bl_pattern_equals(static_cast<const BLPatternCore*>(a), static_cast<const BLPatternCore*>(b));

    case BL_OBJECT_TYPE_GRADIENT:
      return bl_gradient_equals(static_cast<const BLGradientCore*>(a), static_cast<const BLGradientCore*>(b));

    case BL_OBJECT_TYPE_IMAGE:
      return bl_image_equals(static_cast<const BLImageCore*>(a), static_cast<const BLImageCore*>(b));

    case BL_OBJECT_TYPE_PATH:
      return bl_path_equals(static_cast<const BLPathCore*>(a), static_cast<const BLPathCore*>(b));

    case BL_OBJECT_TYPE_FONT:
      return bl_font_equals(static_cast<const BLFontCore*>(a), static_cast<const BLFontCore*>(b));

    case BL_OBJECT_TYPE_FONT_FEATURE_SETTINGS:
      return bl_font_feature_settings_equals(static_cast<const BLFontFeatureSettingsCore*>(a), static_cast<const BLFontFeatureSettingsCore*>(b));

    case BL_OBJECT_TYPE_FONT_VARIATION_SETTINGS:
      return bl_font_variation_settings_equals(static_cast<const BLFontVariationSettingsCore*>(a), static_cast<const BLFontVariationSettingsCore*>(b));

    case BL_OBJECT_TYPE_COVERAGE_SET:
      return bl_coverage_set_equals(static_cast<const BLCoverageSetCore*>(a), static_cast<const BLCoverageSetCore*>(b));

    case BL_OBJECT_TYPE_BIT_ARRAY:
      return bl_bit_array_equals(static_cast<const BLBitArrayCore*>(a), static_cast<const BLBitArrayCore*>(b));

    case BL_OBJECT_TYPE_BOOL:
    case BL_OBJECT_TYPE_INT64:
    case BL_OBJECT_TYPE_UINT64:
      // These must be binary equal.
      return false;

    case BL_OBJECT_TYPE_FLOAT64:
      // Only true if comparing 0.0 and -0.0 - these compare equally, but have a different representation.
      return ObjectLayer::sso_value<double>(a) == ObjectLayer::sso_value<double>(b);

    case BL_OBJECT_TYPE_STRING:
      return bl_string_equals(static_cast<const BLStringCore*>(a), static_cast<const BLStringCore*>(b));

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
      return bl_array_equals(static_cast<const BLArrayCore*>(a), static_cast<const BLArrayCore*>(b));

    default:
      return false;
  }
}

BL_API_IMPL bool bl_var_equals_null(const BLObjectCore* self) noexcept {
  return self->_d.type() == BL_OBJECT_TYPE_NULL;
}

BL_API_IMPL bool bl_var_equals_bool(const BLObjectCore* self, bool value) noexcept {
  using namespace bl;

  switch (self->_d.type()) {
    case BL_OBJECT_TYPE_NULL: {
      return false;
    }

    case BL_OBJECT_TYPE_BOOL:
    case BL_OBJECT_TYPE_INT64:
    case BL_OBJECT_TYPE_UINT64: {
      return ObjectLayer::sso_value<uint64_t>(self) == uint64_t(value);
    }

    case BL_OBJECT_TYPE_FLOAT64: {
      return ObjectLayer::sso_value<double>(self) == double(value);
    }

    default: {
      return false;
    }
  }
}

BL_API_IMPL bool bl_var_equals_int64(const BLObjectCore* self, int64_t value) noexcept {
  using namespace bl;

  switch (self->_d.type()) {
    case BL_OBJECT_TYPE_NULL: {
      return false;
    }

    case BL_OBJECT_TYPE_BOOL:
    case BL_OBJECT_TYPE_INT64: {
      return ObjectLayer::sso_value<int64_t>(self) == value;
    }

    case BL_OBJECT_TYPE_UINT64: {
      return ObjectLayer::sso_value<int64_t>(self) == value && value >= 0;
    }

    case BL_OBJECT_TYPE_FLOAT64: {
      return ObjectLayer::sso_value<double>(self) == double(value) && int64_t(double(value)) == value;
    }

    default: {
      return false;
    }
  }

}

BL_API_IMPL bool bl_var_equals_uint64(const BLObjectCore* self, uint64_t value) noexcept {
  using namespace bl;

  switch (self->_d.type()) {
    case BL_OBJECT_TYPE_NULL: {
      return false;
    }

    case BL_OBJECT_TYPE_BOOL:
    case BL_OBJECT_TYPE_UINT64: {
      return ObjectLayer::sso_value<uint64_t>(self) == value;
    }

    case BL_OBJECT_TYPE_INT64: {
      int64_t v = ObjectLayer::sso_value<int64_t>(self);
      return uint64_t(v) == value && v >= 0;
    }

    case BL_OBJECT_TYPE_FLOAT64: {
      double f = ObjectLayer::sso_value<double>(self);
      return f == double(value) && uint64_t(double(value)) == value;
    }

    default: {
      return false;
    }
  }
}

BL_API_IMPL bool bl_var_equals_double(const BLObjectCore* self, double value) noexcept {
  using namespace bl;

  switch (self->_d.type()) {
    case BL_OBJECT_TYPE_NULL: {
      return false;
    }

    case BL_OBJECT_TYPE_BOOL: {
      return double(int(ObjectLayer::sso_value<uint64_t>(self) & 0x1)) == value;
    }

    case BL_OBJECT_TYPE_INT64: {
      int64_t v = ObjectLayer::sso_value<int64_t>(self);
      return double(v) == value && int64_t(double(v)) == v;
    }

    case BL_OBJECT_TYPE_UINT64: {
      uint64_t v = ObjectLayer::sso_value<uint64_t>(self);
      return double(v) == value && uint64_t(double(v)) == v;
    }

    case BL_OBJECT_TYPE_FLOAT64: {
      double f = ObjectLayer::sso_value<double>(self);
      return std::bit_cast<uint64_t>(f) == std::bit_cast<uint64_t>(value) || f == value;
    }

    default: {
      return false;
    }
  }
}

BL_API_IMPL bool BL_CDECL bl_var_equals_rgba(const BLObjectCore* self, const BLRgba* rgba) noexcept {
  using namespace bl;

  BLRgba content;
  if (self->_d.is_rgba()) {
    content = ObjectLayer::sso_value<BLRgba>(self);
  }
  else if (self->_d.is_rgba32()) {
    content = BLRgba(ObjectLayer::sso_value<BLRgba32>(self));
  }
  else if (self->_d.is_rgba64()) {
    content = BLRgba(ObjectLayer::sso_value<BLRgba64>(self));
  }
  else {
    return false;
  }

  return memcmp(&content, rgba, sizeof(BLRgba)) == 0;
}

BL_API_IMPL bool BL_CDECL bl_var_equals_rgba32(const BLObjectCore* self, uint32_t rgba32) noexcept {
  using namespace bl;

  if (self->_d.is_rgba32()) {
    return ObjectLayer::sso_value<BLRgba32>(self) == BLRgba32(rgba32);
  }
  else if (self->_d.is_rgba64()) {
    return BLRgba32(ObjectLayer::sso_value<BLRgba64>(self)) == BLRgba32(rgba32);
  }
  else if (self->_d.is_rgba()) {
    return ObjectLayer::sso_value<BLRgba>(self).to_rgba32() == BLRgba32(rgba32);
  }
  else {
    return false;
  }
}

BL_API_IMPL bool BL_CDECL bl_var_equals_rgba64(const BLObjectCore* self, uint64_t rgba64) noexcept {
  using namespace bl;

  if (self->_d.is_rgba64()) {
    return ObjectLayer::sso_value<BLRgba64>(self) == BLRgba64(rgba64);
  }
  else if (self->_d.is_rgba32()) {
    return BLRgba64(ObjectLayer::sso_value<BLRgba32>(self)) == BLRgba64(rgba64);
  }
  else if (self->_d.is_rgba()) {
    return ObjectLayer::sso_value<BLRgba>(self).to_rgba64() == BLRgba64(rgba64);
  }
  else {
    return false;
  }
}

BL_API_IMPL bool bl_var_strict_equals(const BLObjectCore* a, const BLObjectCore* b) noexcept {
  return a->_d == b->_d;
}
