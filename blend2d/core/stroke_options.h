// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_STROKE_OPTIONS_H_INCLUDED
#define BLEND2D_CORE_STROKE_OPTIONS_H_INCLUDED

#include <blend2d/core/array.h>
#include <blend2d/core/object_layer.h>
#include <blend2d/core/span.h>

//! \addtogroup bl_geometry
//! \{

//! \name Path Stroking
//! \{

//! Stroke join type.
BL_DEFINE_ENUM(BLStrokeJoin) {
  //! Miter-join possibly clipped at `miter_limit` [default].
  BL_STROKE_JOIN_MITER_CLIP = 0,
  //! Miter-join or bevel-join depending on miter_limit condition.
  BL_STROKE_JOIN_MITER_BEVEL = 1,
  //! Miter-join or round-join depending on miter_limit condition.
  BL_STROKE_JOIN_MITER_ROUND = 2,
  //! Bevel-join.
  BL_STROKE_JOIN_BEVEL = 3,
  //! Round-join.
  BL_STROKE_JOIN_ROUND = 4,

  //! Maximum value of `BLStrokeJoin`.
  BL_STROKE_JOIN_MAX_VALUE = 4

  BL_FORCE_ENUM_UINT32(BL_STROKE_JOIN)
};

//! Position of a stroke-cap.
BL_DEFINE_ENUM(BLStrokeCapPosition) {
  //! Start of the path.
  BL_STROKE_CAP_POSITION_START = 0,
  //! End of the path.
  BL_STROKE_CAP_POSITION_END = 1,

  //! Maximum value of `BLStrokeCapPosition`.
  BL_STROKE_CAP_POSITION_MAX_VALUE = 1

  BL_FORCE_ENUM_UINT32(BL_STROKE_CAP_POSITION)
};

//! A presentation attribute defining the shape to be used at the end of open sub-paths.
BL_DEFINE_ENUM(BLStrokeCap) {
  //! Butt cap [default].
  BL_STROKE_CAP_BUTT = 0,
  //! Square cap.
  BL_STROKE_CAP_SQUARE = 1,
  //! Round cap.
  BL_STROKE_CAP_ROUND = 2,
  //! Round cap reversed.
  BL_STROKE_CAP_ROUND_REV = 3,
  //! Triangle cap.
  BL_STROKE_CAP_TRIANGLE = 4,
  //! Triangle cap reversed.
  BL_STROKE_CAP_TRIANGLE_REV = 5,

  //! Maximum value of `BLStrokeCap`.
  BL_STROKE_CAP_MAX_VALUE = 5

  BL_FORCE_ENUM_UINT32(BL_STROKE_CAP)
};

//! Stroke transform order.
BL_DEFINE_ENUM(BLStrokeTransformOrder) {
  //! Transform after stroke  => `Transform(Stroke(Input))` [default].
  BL_STROKE_TRANSFORM_ORDER_AFTER = 0,
  //! Transform before stroke => `Stroke(Transform(Input))`.
  BL_STROKE_TRANSFORM_ORDER_BEFORE = 1,

  //! Maximum value of `BLStrokeTransformOrder`.
  BL_STROKE_TRANSFORM_ORDER_MAX_VALUE = 1

  BL_FORCE_ENUM_UINT32(BL_STROKE_TRANSFORM_ORDER)
};

//! Mode that specifies how to construct offset curves.
BL_DEFINE_ENUM(BLOffsetMode) {
  //! Use default mode (decided by Blend2D).
  BL_OFFSET_MODE_DEFAULT = 0,
  //! Iterative offset construction.
  BL_OFFSET_MODE_ITERATIVE = 1,

  //! Maximum value of `BLOffsetMode`.
  BL_OFFSET_MODE_MAX_VALUE = 1

  BL_FORCE_ENUM_UINT32(BL_OFFSET_MODE)
};

//! \}
//! \}

//! \addtogroup bl_c_api
//! \{

//! \name Stroke Options - C API
//! \{

//! \cond INTERNAL

#if defined(__cplusplus)
//! Stroke options [Impl].
struct BLStrokeOptionsImpl {};
#endif // __cplusplus

//! \endcond

//! Stroke options [C API].
struct BLStrokeOptionsCore BL_INHERITS_FROM(BLObjectCore) {
  BL_DEFINE_OBJECT(BLStrokeOptions, bl::ObjectLayer::ContainerInfo<
    BL_OBJECT_TYPE_STROKE_OPTIONS,
    bl::ObjectLayer::ObjectMode::kAny,
    bl::ObjectLayer::ObjectLayout::kCompact32,
    4u, 8u, BLObjectDetail::kMetaByteIndex>
  )
};

BL_EXTERN_C_BEGIN

BL_API BLResult BL_CDECL bl_stroke_options_init(BLStrokeOptionsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_stroke_options_init_move(BLStrokeOptionsCore* self, BLStrokeOptionsCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_stroke_options_init_weak(BLStrokeOptionsCore* self, const BLStrokeOptionsCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_stroke_options_destroy(BLStrokeOptionsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_stroke_options_reset(BLStrokeOptionsCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_stroke_options_assign_move(BLStrokeOptionsCore* self, BLStrokeOptionsCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_stroke_options_assign_weak(BLStrokeOptionsCore* self, const BLStrokeOptionsCore* other) BL_NOEXCEPT_C;

BL_API BLStrokeCap BL_CDECL bl_stroke_options_get_start_cap(const BLStrokeOptionsCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API BLStrokeCap BL_CDECL bl_stroke_options_get_end_cap(const BLStrokeOptionsCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API BLStrokeJoin BL_CDECL bl_stroke_options_get_join(const BLStrokeOptionsCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API BLStrokeTransformOrder BL_CDECL bl_stroke_options_get_transform_order(const BLStrokeOptionsCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API float BL_CDECL bl_stroke_options_get_width(const BLStrokeOptionsCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API float BL_CDECL bl_stroke_options_get_miter_limit(const BLStrokeOptionsCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API float BL_CDECL bl_stroke_options_get_dash_offset(const BLStrokeOptionsCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API const float* BL_CDECL bl_stroke_options_get_dash_array(const BLStrokeOptionsCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API size_t BL_CDECL bl_stroke_options_get_dash_count(const BLStrokeOptionsCore* self) BL_NOEXCEPT_C BL_PURE;

BL_API BLResult BL_CDECL bl_stroke_options_set_start_cap(BLStrokeOptionsCore* self, BLStrokeCap start_cap) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_stroke_options_set_end_cap(BLStrokeOptionsCore* self, BLStrokeCap end_cap) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_stroke_options_set_join(BLStrokeOptionsCore* self, BLStrokeJoin join) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_stroke_options_set_transform_order(BLStrokeOptionsCore* self, BLStrokeTransformOrder order) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_stroke_options_set_width(BLStrokeOptionsCore* self, float width) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_stroke_options_set_miter_limit(BLStrokeOptionsCore* self, float miter_limit) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_stroke_options_set_dash_offset(BLStrokeOptionsCore* self, float dash_offset) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL bl_stroke_options_set_dash_array_object(BLStrokeOptionsCore* self, const BLArrayCore* array) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_stroke_options_set_dash_array_data(BLStrokeOptionsCore* self, const float* data, size_t size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_stroke_options_reset_dash_array(BLStrokeOptionsCore* self) BL_NOEXCEPT_C;

BL_API bool BL_CDECL bl_stroke_options_equals(const BLStrokeOptionsCore* a, const BLStrokeOptionsCore* b) BL_NOEXCEPT_C;

BL_EXTERN_C_END

//! \}
//! \}

//! \addtogroup bl_geometry
//! \{

//! \name BLPath - C++ API
//! \{

#ifdef __cplusplus
//! Stroke options [C++ API].
//!
//! You should use this as a structure and use members of `BLStrokeOptionsCore` directly.
class BLStrokeOptions final : public BLStrokeOptionsCore {
public:
  //! \cond INTERNAL
  //! Stroke options layout (internal).
  struct Layout {
    float width;       // 00-03
    float miter_limit; // 04-07
    union DashArray {
      struct Dyn {
        BLObjectDetail::Ptr64<float> data; // aliases BLStrokeOptionsImpl because it has no content.
        uint32_t size;
        uint32_t capacity;
      } dyn;
      struct SSO {
        float data[4];
      } sso;
    } dash_array;
    float dash_offset; // 24-27
    uint16_t payload;  // 28-29 - 2 bytes to hold caps, join, and transform order.
    uint8_t type;      // 30
    uint8_t meta;      // 31    - dash-segment-count in SSO mode.
  };

  static_assert(sizeof(Layout) == sizeof(BLObjectDetail));

  static constexpr uint32_t _stroke_cap_bit_size = 3;

  using StartCapField  = bl::ObjectLayer::PayloadField<BLStrokeCap           , 0x0007u>; // 3 bits.
  using EndCapField    = bl::ObjectLayer::PayloadField<BLStrokeCap           , 0x0038u>; // 3 bits.
  using JoinField      = bl::ObjectLayer::PayloadField<BLStrokeJoin          , 0x7000u>; // 3 bits.
  using TransformField = bl::ObjectLayer::PayloadField<BLStrokeTransformOrder, 0x8000u>; // 1 bit.
  using BothCapsField  = bl::ObjectLayer::PayloadField<uint32_t              , 0x003Fu>; // 6 bits.

  //! \endcond

  static constexpr float default_width = 1.0f;
  static constexpr float default_miter_limit = 4.0f;
  static constexpr float default_dash_offset = 0.0f;
  static constexpr BLStrokeCap default_caps = BL_STROKE_CAP_BUTT;
  static constexpr BLStrokeJoin default_join = BL_STROKE_JOIN_MITER_CLIP;
  static constexpr BLStrokeTransformOrder default_transform_order = BL_STROKE_TRANSFORM_ORDER_AFTER;

  //! \cond INTERNAL
  static BL_INLINE_NODEBUG void _init_default(BLObjectDetail& d) noexcept {
    d.zero_all();
    d.set_type_byte(uint8_t(BL_OBJECT_TYPE_STROKE_OPTIONS));
    d.layout<Layout>().width = default_width;
    d.layout<Layout>().miter_limit = default_miter_limit;
  }
  //! \endcond

  BL_INLINE BLStrokeOptions() noexcept {
    _init_default(_d);
  }

  BL_INLINE BLStrokeOptions(BLStrokeOptions&& other) noexcept {
    this->_d = other._d;
    _init_default(other._d);
  }

  BL_INLINE BLStrokeOptions(const BLStrokeOptions& other) noexcept {
    bl_stroke_options_init_weak(this, &other);
  }

  BL_INLINE ~BLStrokeOptions() noexcept {
    if (bl::ObjectLayer::requires_destroy(this)) {
      bl_stroke_options_destroy(this);
    }
  }

  BL_INLINE_NODEBUG BLStrokeOptions& operator=(BLStrokeOptions&& other) noexcept {
    bl_stroke_options_assign_move(this, &other);
    return *this;
  }

  BL_INLINE_NODEBUG BLStrokeOptions& operator=(const BLStrokeOptions& other) noexcept {
    bl_stroke_options_assign_weak(this, &other);
    return *this;
  }

  [[nodiscard]] BL_INLINE_NODEBUG bool operator==(const BLStrokeOptions& other) const noexcept { return equals(other); }
  [[nodiscard]] BL_INLINE_NODEBUG bool operator!=(const BLStrokeOptions& other) const noexcept { return !equals(other); }

  BL_INLINE_NODEBUG BLResult reset() noexcept {
    return bl_stroke_options_reset(this);
  }

  [[nodiscard]] BL_INLINE_NODEBUG bool equals(const BLStrokeOptions& other) const noexcept {
    return bl_stroke_options_equals(this, &other);
  }

  BL_INLINE_NODEBUG BLResult assign(BLStrokeOptions&& other) noexcept {
    return bl_stroke_options_assign_move(this, &other);
  }

  BL_INLINE_NODEBUG BLResult assign(const BLStrokeOptions& other) noexcept {
    return bl_stroke_options_assign_weak(this, &other);
  }

  [[nodiscard]] BL_INLINE_NODEBUG BLStrokeCap start_cap() const noexcept { return bl::ObjectLayer::get_payload_field<StartCapField>(this); }
  [[nodiscard]] BL_INLINE_NODEBUG BLStrokeCap end_cap() const noexcept { return bl::ObjectLayer::get_payload_field<EndCapField>(this); }

  [[nodiscard]] BL_INLINE BLStrokeCap cap_by_position(BLStrokeCapPosition pos) const noexcept {
    constexpr uint32_t cap_bit_mask = (1u << _stroke_cap_bit_size) - 1;

    uint32_t payload = bl::ObjectLayer::get_payload_u16(this);
    uint32_t shift = uint32_t(pos) * _stroke_cap_bit_size;
    return BLStrokeCap((payload >> shift) & cap_bit_mask);
  }

  [[nodiscard]] BL_INLINE_NODEBUG BLStrokeJoin join() const noexcept { return bl::ObjectLayer::get_payload_field<JoinField>(this); }
  [[nodiscard]] BL_INLINE_NODEBUG BLStrokeTransformOrder transform_order() const noexcept { return bl::ObjectLayer::get_payload_field<TransformField>(this); }
  [[nodiscard]] BL_INLINE_NODEBUG float width() const noexcept { return _d.layout<Layout>().width; }
  [[nodiscard]] BL_INLINE_NODEBUG float miter_limit() const noexcept { return _d.layout<Layout>().miter_limit; }
  [[nodiscard]] BL_INLINE_NODEBUG float dash_offset() const noexcept { return _d.layout<Layout>().dash_offset; }

  [[nodiscard]] BL_INLINE_NODEBUG bool has_dash_array() const noexcept { return bl::ObjectLayer::get_size(this) == 0u; }
  [[nodiscard]] BL_INLINE_NODEBUG const float* dash_array_data() const noexcept { return bl::ObjectLayer::get_data<float>(this); }
  [[nodiscard]] BL_INLINE_NODEBUG size_t dash_array_size() const noexcept { return bl::ObjectLayer::get_size(this); }
  [[nodiscard]] BL_INLINE_NODEBUG size_t dash_array_capacity() const noexcept { return bl::ObjectLayer::get_capacity(this); }
  [[nodiscard]] BL_INLINE_NODEBUG BLSpan<const float> dash_array() const noexcept { return {dash_array_data(), dash_array_size()}; }

  BL_INLINE_NODEBUG BLResult set_start_cap(BLStrokeCap start_cap) noexcept {
    return bl::ObjectLayer::set_payload_field<StartCapField>(this, start_cap);
  }

  BL_INLINE_NODEBUG BLResult set_end_cap(BLStrokeCap end_cap) noexcept {
    return bl::ObjectLayer::set_payload_field<EndCapField>(this, end_cap);
  }

  BL_INLINE_NODEBUG BLResult set_caps(BLStrokeCap caps) noexcept {
    uint32_t combined = uint32_t(caps) * (1u | (1u << 3));
    return bl::ObjectLayer::set_payload_field<BothCapsField>(this, combined);
  }

  BL_INLINE_NODEBUG BLResult set_caps(BLStrokeCap start_cap, BLStrokeCap end_cap) noexcept {
    uint32_t combined = uint32_t(start_cap) | (uint32_t(end_cap) << 3);
    return bl::ObjectLayer::set_payload_field<BothCapsField>(this, combined);
  }

  BL_INLINE BLResult set_cap_by_position(BLStrokeCapPosition pos, BLStrokeCap cap) noexcept {
    constexpr uint32_t cap_bit_mask = (1u << _stroke_cap_bit_size) - 1;

    uint32_t payload = bl::ObjectLayer::get_payload_u16(this);
    uint32_t shift = uint32_t(pos) * _stroke_cap_bit_size;
    uint32_t mask = cap_bit_mask << shift;

    return bl::ObjectLayer::set_payload_u16(this, (payload & ~mask) | (uint32_t(cap) << shift));
  }

  BL_INLINE_NODEBUG BLResult set_join(BLStrokeJoin join) noexcept {
    return bl::ObjectLayer::set_payload_field<JoinField>(this, join);
  }

  BL_INLINE_NODEBUG BLResult set_transform_order(BLStrokeTransformOrder order) noexcept {
    return bl::ObjectLayer::set_payload_field<TransformField>(this, order);
  }

  BL_INLINE_NODEBUG BLResult set_width(float value) noexcept {
    _d.layout<Layout>().width = value;
    return BL_SUCCESS;
  }

  BL_INLINE_NODEBUG BLResult set_miter_limit(float value) noexcept {
    _d.layout<Layout>().miter_limit = value;
    return BL_SUCCESS;
  }

  BL_INLINE_NODEBUG BLResult set_dash_offset(float value) noexcept {
    _d.layout<Layout>().dash_offset = value;
    return BL_SUCCESS;
  }

  BL_INLINE_NODEBUG BLResult set_dash_array(const float* data, size_t size) noexcept {
    return bl_stroke_options_set_dash_array_data(this, data, size);
  }

  BL_INLINE_NODEBUG BLResult set_dash_array(const BLArray<float>& array) noexcept {
    return bl_stroke_options_set_dash_array_object(this, &array);
  }

  BL_INLINE_NODEBUG BLResult set_dash_array(BLSpanConvertible<const float> auto&& data) noexcept
    requires (!std::same_as<std::remove_cvref_t<decltype(data)>, BLArray<float>>)
  {
    return bl_stroke_options_set_dash_array_data(this, data.data(), data.size());
  }

  BL_INLINE_NODEBUG BLResult reset_dash_array() noexcept {
    return bl_stroke_options_reset_dash_array(this);
  }

  BL_INLINE_NODEBUG BLResult reset_caps() noexcept { return set_caps(BL_STROKE_CAP_BUTT); }
  BL_INLINE_NODEBUG BLResult reset_join() noexcept { return set_join(BL_STROKE_JOIN_MITER_CLIP); }
  BL_INLINE_NODEBUG BLResult reset_transform_order() noexcept { return set_transform_order(BL_STROKE_TRANSFORM_ORDER_AFTER); }
  BL_INLINE_NODEBUG BLResult reset_width() noexcept { return set_width(default_width); }
  BL_INLINE_NODEBUG BLResult reset_miter_limit() noexcept { return set_miter_limit(default_miter_limit); }
  BL_INLINE_NODEBUG BLResult reset_dash_offset() noexcept { return set_dash_offset(default_dash_offset); }
};
#endif

//! \}
//! \}

#endif // BLEND2D_CORE_STROKE_OPTIONS_H_INCLUDED
