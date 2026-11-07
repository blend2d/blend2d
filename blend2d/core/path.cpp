// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/commons.h>
#include <blend2d/axl/lookuptable_p.h>
#include <blend2d/axl/math_p.h>
#include <blend2d/core/array_p.h>
#include <blend2d/core/matrix_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/path_p.h>
#include <blend2d/core/runtime_p.h>
#include <blend2d/geometry/bezier_p.h>
#include <blend2d/geometry/stroke_p.h>

const BLApproximationOptions bl_default_approximation_options = bl::PathDetail::make_default_approximation_options();

// Path - Globals
// ==============

namespace bl::PathDetail {

struct PathNullImpl : public PathPrivateImpl {
  static constexpr size_t capacity = 64;
  uint8_t data[capacity];
};

static consteval PathNullImpl make_eternal_path_impl() noexcept {
  PathNullImpl impl{};
  impl.flags = BL_PATH_FLAG_EMPTY;
  return impl;
}

static constexpr ObjectLayer::EternalImpl<PathNullImpl> null_path {
  .padding{},
  .header{},
  .impl = make_eternal_path_impl()
};

static BL_INLINE BLResult alloc_impl(BLPathCore* self, ObjectImplSize impl_size) noexcept {
  size_t byte_capacity = capacity_from_impl_size(impl_size);
  BL_ASSERT(byte_capacity <= kMaxDataSize);

  BL_PROPAGATE(ObjectLayer::alloc_impl_t<PathPrivateImpl>(self, BL_OBJECT_TYPE_PATH, impl_size));

  PathPrivateImpl* impl = ObjectLayer::get_impl(self);
  self->_d.dyn.data_or_virt.ptr = axl::offset_ptr<void>(impl, sizeof(PathPrivateImpl));
  self->_d.dyn.size_u32 = 0;
  self->_d.dyn.capacity_u32 = uint32_t(byte_capacity);
  self->_d.dyn.payload_u32 = 0;
  impl->flags = BL_PATH_FLAG_DIRTY;

  return BL_SUCCESS;
}

// Plain realloc - allocates a new path, copies its data into it, and replaces the
// impl in `self`. Flags and cached information are cleared.
static BL_NOINLINE BLResult realloc_impl(BLPathCore* self, ObjectImplSize impl_size) noexcept {
  BLPathCore tmp;
  BL_PROPAGATE(alloc_impl(&tmp, impl_size));

  size_t vertex_data_size = get_vertex_data_size(self);
  size_t control_data_size = get_control_data_size(self);
  set_data_sizes(&tmp, vertex_data_size, control_data_size);

  uint8_t* dst = ObjectLayer::get_data<uint8_t>(&tmp);
  const uint8_t* src = ObjectLayer::get_data<uint8_t>(self);

  memcpy(dst, src, vertex_data_size);
  memcpy(dst + ObjectLayer::get_capacity(&tmp) - control_data_size,
         src + ObjectLayer::get_capacity(self) - control_data_size,
         control_data_size);

  return ObjectLayer::replace_object(self, &tmp);
}

// Plain realloc for append operation - allocates a new path, copies its data into
// it, and replaces the impl in `self`. Flags and cached information are cleared.
static BL_NOINLINE BLResult realloc_for_append(BLPathCore* self, size_t append_byte_size) noexcept {
  size_t byte_size = ObjectLayer::get_size(self);
  size_t result_size = axl::add_saturate(byte_size, append_byte_size);

  if (BL_UNLIKELY(result_size > kMaxDataSize)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  ObjectImplSize impl_size = ObjectLayer::expand_impl_size_32(impl_size_from_capacity(result_size));
  return realloc_impl(self, impl_size);
}

struct AppendTarget {
  BLPath::Vertex* vertex_data;
  uint8_t* control_rev;
  size_t vertex_data_size;
  size_t control_data_size;
};

static BL_INLINE BLResultOr<AppendTarget> prepare_append(BLPathCore* self, size_t append_byte_size) noexcept {
  size_t vertex_data_size = get_vertex_data_size(self);
  size_t control_data_size = get_control_data_size(self);
  size_t byte_size = vertex_data_size + control_data_size;

  if (BL_UNLIKELY(!ObjectLayer::check_dynamic_capacity_and_mutability(self, append_byte_size, byte_size))) {
    BL_PROPAGATE(realloc_for_append(self, append_byte_size));
  }

  void* path_data = ObjectLayer::get_data<void>(self);
  clear_flags(self);

  return AppendTarget{
    axl::offset_ptr<BLPoint>(path_data, vertex_data_size),
    axl::offset_ptr<uint8_t>(path_data, ObjectLayer::get_capacity(self) - control_data_size),
    vertex_data_size,
    control_data_size
  };
}

template<typename T>
static BL_INLINE BLResultOr<AppendTarget> prepare_append_possibly_aliased(BLPathCore* self, size_t append_byte_size, const T ** ptr) noexcept {
  size_t vertex_data_size = get_vertex_data_size(self);
  size_t control_data_size = get_control_data_size(self);
  size_t byte_size = vertex_data_size + control_data_size;

  if (BL_UNLIKELY(!ObjectLayer::check_dynamic_capacity_and_mutability(self, append_byte_size, byte_size))) {
    // In our case calculating offset is enough to determine whether the pointer is aliased - `*ptr` can only possibly
    // alias vertices as aliasing control data makes no sense (there is no use-case for that, and reinterpreting control
    // data as vertices is a nonsense).
    size_t offset = reinterpret_cast<uintptr_t>(*ptr) - reinterpret_cast<uintptr_t>(ObjectLayer::get_data<void>(self));

    BL_PROPAGATE(realloc_for_append(self, append_byte_size));

    if (offset < vertex_data_size) {
      *ptr = axl::offset_ptr<const T>(ObjectLayer::get_data<void>(self), offset);
    }
  }

  clear_flags(self);

  void* path_data = ObjectLayer::get_data<void>(self);
  return AppendTarget{
    axl::offset_ptr<BLPoint>(path_data, vertex_data_size),
    axl::offset_ptr<uint8_t>(path_data, ObjectLayer::get_capacity(self) - control_data_size),
    vertex_data_size,
    control_data_size
  };
}

static BL_INLINE BLResult make_mutable(BLPathCore* self) noexcept {
  if (BL_UNLIKELY(!ObjectLayer::is_object_mutable(self))) {
    ObjectImplSize impl_size = impl_size_from_capacity(ObjectLayer::get_size(self));
    BL_PROPAGATE(realloc_impl(self, impl_size));
  }

  return clear_flags(self);
}

} // {bl::PathDetail}

// Path - API - Init & Destroy
// ===========================

BL_API_IMPL BLResult bl_path_init(BLPathCore* self) noexcept {
  BL_USING_DETAIL(bl::PathDetail);

  return ObjectLayer::init_object(self);
}

BL_API_IMPL BLResult bl_path_init_move(BLPathCore* self, BLPathCore* other) noexcept {
  BL_USING_DETAIL(bl::PathDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::move_object(self, other);
}

BL_API_IMPL BLResult bl_path_init_weak(BLPathCore* self, const BLPathCore* other) noexcept {
  BL_USING_DETAIL(bl::PathDetail);

  BL_ASSERT(self != other);
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::copy_object<ObjectLayer::ObjectMode::kDyn>(self, other);
}

BL_API_IMPL BLResult bl_path_destroy(BLPathCore* self) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::release_object(self);
}

// Path - API - Reset
// ==================

BL_API_IMPL BLResult bl_path_reset(BLPathCore* self) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::reset_object(self);
}

// Path - API - Accessors
// ======================

BL_API_IMPL size_t bl_path_get_byte_size(const BLPathCore* self) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_size(self);
}

BL_API_IMPL size_t bl_path_get_byte_capacity(const BLPathCore* self) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return ObjectLayer::get_capacity(self);
}

BL_API_IMPL size_t bl_path_get_vertex_count(const BLPathCore* self) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return get_vertex_count(self);
}

BL_API_IMPL size_t bl_path_get_vertex_data_size(const BLPathCore* self) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return get_vertex_data_size(self);
}

BL_API_IMPL size_t bl_path_get_control_data_size(const BLPathCore* self) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return get_control_data_size(self);
}

BL_API_IMPL const BLPoint* bl_path_get_vertex_data(const BLPathCore* self) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return get_vertex_data(self);
}

BL_API_IMPL const uint8_t* bl_path_get_control_data_rev(const BLPathCore* self) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return get_control_data_rev(self);
}

// Path - API - Clear
// ==================

BL_API_IMPL BLResult bl_path_clear(BLPathCore* self) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (self->_d.is_ref_counted()) {
    if (ObjectLayer::is_object_mutable<ObjectLayer::ObjectMode::kRC>(self)) {
      set_data_sizes(self, 0u, 0u);
      return clear_flags(self);
    }

    ObjectLayer::release_object<ObjectLayer::ObjectMode::kRC>(self);
  }

  return ObjectLayer::init_object(self);
}

// Path - API - Storage Size & Capacity
// ====================================

BL_API_IMPL BLResult bl_path_shrink(BLPathCore* self) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  // Don't shrink SSO or non-ref counted objects - these are possibly eternal.
  if (!self->_d.is_ref_counted()) {
    return BL_SUCCESS;
  }

  ObjectImplSize fitting_impl_size = impl_size_from_capacity(ObjectLayer::get_size(self));
  ObjectImplSize current_impl_size = impl_size_from_capacity(ObjectLayer::get_capacity(self));

  if (fitting_impl_size != current_impl_size) {
    BL_PROPAGATE(realloc_impl(self, fitting_impl_size));
  }

  // Update also path info when shrinking as shrinking is most of the time used by paths with long life-time.
  uint32_t dummy_flags;
  return bl_path_get_info_flags(self, &dummy_flags);
}

BL_API_IMPL BLResult bl_path_reserve_byte_size(BLPathCore* self, size_t byte_size) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (ObjectLayer::check_dynamic_capacity_and_mutability(self, byte_size)) {
    return BL_SUCCESS;
  }

  byte_size = axl::max(byte_size, ObjectLayer::get_size(self));
  if (BL_UNLIKELY(byte_size > kMaxDataSize)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  ObjectImplSize impl_size = impl_size_from_capacity(byte_size);
  return realloc_impl(self, impl_size);
}

BL_API_IMPL BLResult bl_path_reserve_byte_size_for_append(BLPathCore* self, size_t append_byte_size) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  // The path stores vertex-data size and control-data size in two separate fields (`get_size()` returns only the
  // former). Both must be accounted for here, otherwise appending to a path that already has content under-reserves
  // by the existing control-data size and the appended data overruns the existing vertices.
  size_t path_byte_size = get_vertex_data_size(self) + get_control_data_size(self);
  if (ObjectLayer::check_dynamic_capacity_and_mutability(self, append_byte_size, path_byte_size)) {
    return BL_SUCCESS;
  }

  path_byte_size = axl::add_saturate(path_byte_size, append_byte_size);
  if (BL_UNLIKELY(path_byte_size > kMaxDataSize)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  ObjectImplSize impl_size = ObjectLayer::expand_impl_size_32(impl_size_from_capacity(path_byte_size));
  return realloc_impl(self, impl_size);
}

BL_API_IMPL BLResult bl_path_add_uninitialized(BLPathCore* self, size_t append_vertex_count, size_t append_control_data_size, BLPoint** vertex_data_out, uint8_t** control_data_rev_out) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  // Calculated as `(SIZE_MAX - append_control_data_size) / sizeof(BLPath::Vertex)`
  size_t max_vertex_count_before_overflow = ~append_control_data_size / sizeof(BLPath::Vertex);

  size_t path_vertex_data_size = get_vertex_data_size(self);
  size_t path_control_data_size = get_control_data_size(self);
  size_t path_byte_size = path_vertex_data_size + path_control_data_size;

  if (BL_UNLIKELY(append_vertex_count >= max_vertex_count_before_overflow)) {
    return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  size_t append_byte_size = append_vertex_count * sizeof(BLPath::Vertex) + append_control_data_size;

  if (BL_UNLIKELY(!ObjectLayer::check_dynamic_capacity_and_mutability(self, append_byte_size, path_byte_size))) {
    size_t result_byte_size = axl::add_saturate(path_byte_size, append_byte_size);
    if (BL_UNLIKELY(result_byte_size > kMaxDataSize)) {
      return bl_make_error(BL_ERROR_OUT_OF_MEMORY);
    }

    ObjectImplSize impl_size = ObjectLayer::expand_impl_size_32(impl_size_from_capacity(result_byte_size));
    BL_PROPAGATE(realloc_impl(self, impl_size));
  }

  void* path_data = ObjectLayer::get_data<void>(self);
  *vertex_data_out = axl::offset_ptr<BLPoint>(path_data, path_vertex_data_size);
  *control_data_rev_out = axl::offset_ptr<uint8_t>(path_data, ObjectLayer::get_capacity(self) - path_control_data_size);

  set_data_sizes(self,
    path_vertex_data_size + append_vertex_count * sizeof(BLPoint),
    path_control_data_size + append_control_data_size);

  return clear_flags(self);
}

// Path - API - Assign
// ===================

BL_API_IMPL BLResult bl_path_assign_move(BLPathCore* self, BLPathCore* other) noexcept {
  BL_USING_DETAIL(bl::PathDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_move(self, other);
}

BL_API_IMPL BLResult bl_path_assign_weak(BLPathCore* self, const BLPathCore* other) noexcept {
  BL_USING_DETAIL(bl::PathDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  return ObjectLayer::assign_object_weak(self, other);
}

BL_API_IMPL BLResult bl_path_assign_deep(BLPathCore* self, const BLPathCore* other) noexcept {
  BL_USING_DETAIL(bl::PathDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  if (self == other) {
    return make_mutable(self);
  }

  size_t vertex_data_size = get_vertex_data_size(other);
  size_t control_data_size = get_control_data_size(other);
  size_t byte_size = vertex_data_size + control_data_size;

  if (!ObjectLayer::check_dynamic_capacity_and_mutability(self, byte_size)) {
    BLPathCore tmp;
    BL_PROPAGATE(alloc_impl(&tmp, impl_size_from_capacity(byte_size)));
    ObjectLayer::replace_object(self, &tmp);
  }

  set_data_sizes(self, vertex_data_size, control_data_size);

  BLPoint* dst_vertex_data = get_vertex_data(self);
  uint8_t* dst_control_data = get_control_data_rev(self) - control_data_size;

  memcpy(dst_vertex_data, get_vertex_data(other), vertex_data_size);
  memcpy(dst_control_data, get_control_data_rev(other) - control_data_size, control_data_size);

  return clear_flags(self);
}

// Path - Arcs Helpers
// ===================

namespace bl::PathDetail {

static const double path_arc90_deg_steps_table[] = {
  Math::kPI_DIV_2,
  Math::kPI,
  Math::kPI_MUL_1p5,
  Math::kPI_MUL_2
};

static void path_arc_to_conic_spline(PathAppender& dst, BLPoint c, BLPoint r, double start_angle, double sweep_angle, BLPathCmd initial_cmd, bool maybe_redundant_line_to = false) noexcept {
  double start_sin = Math::sin(start_angle);
  double start_cos = Math::cos(start_angle);

  BLMatrix2D transform = BLMatrix2D::make_sin_cos(start_sin, start_cos);
  transform.post_scale(r);
  transform.post_translate(c);

  if (sweep_angle < 0) {
    transform.scale(1.0, -1.0);
    sweep_angle = -sweep_angle;
  }

  BLPoint v1(1.0, 0.0);
  BLPoint vc(1.0, 1.0);
  BLPoint v2;

  if (sweep_angle >= Math::kPI_MUL_2 - Math::epsilon<double>()) {
    sweep_angle = Math::kPI_MUL_2;
    v2 = v1;
  }
  else {
    if (Math::is_nan(sweep_angle)) {
      return;
    }

    double sweep_sin = Math::sin(sweep_angle);
    double sweep_cos = Math::cos(sweep_angle);
    v2 = BLPoint(sweep_cos, sweep_sin);
  }

  BLPoint p0 = transform.map_point(v1);
  dst.vertex_to(initial_cmd, p0);

  if (maybe_redundant_line_to && is_single_vertex_cmd(dst.prev_prev_command())) {
    BL_ASSERT(initial_cmd == BL_PATH_CMD_LINE);

    double diff = bl_max(bl_abs(p0.x - dst.prev_prev_vertex().x), bl_abs(p0.y - dst.prev_prev_vertex().y));
    if (diff < Math::epsilon<double>()) {
      dst._revert(1u, 1u);
    }
  }

  size_t i = 0;

  while (sweep_angle > path_arc90_deg_steps_table[i]) {
    v1 = Geometry::normal(v1);
    BLPoint p1 = transform.map_point(vc);
    BLPoint p2 = transform.map_point(v1);
    dst.arc_quadrant_to(p1, p2);

    // Full circle.
    if (++i == 4) {
      return;
    }

    vc = Geometry::normal(vc);
    p0 = p2;
  }

  // Calculate the remaining control point.
  vc = v1 + v2;
  vc = 2.0 * vc / Geometry::dot(vc, vc);

  // This is actually half of the remaining cos. It is required that v1 dot v2 > -1 holds
  // but we can safely assume it does (only critical for angles close to 180 degrees).
  double w = Math::sqrt(0.5 * Geometry::dot(v1, v2) + 0.5);
  dst.conic_to(transform.map_point(vc), transform.map_point(v2), w);
}

// Path - Info Updater
// ===================

class PathInfoUpdater {
public:
  uint32_t move_to_count;
  uint32_t flags;
  BLBox vertex_bounding_box;
  BLBox ink_bounding_box;

  BL_INLINE PathInfoUpdater() noexcept
    : move_to_count(0),
      flags(0),
      vertex_bounding_box(axl::max_value<double>(), axl::max_value<double>(), axl::min_value<double>(), axl::min_value<double>()),
      ink_bounding_box(axl::max_value<double>(), axl::max_value<double>(), axl::min_value<double>(), axl::min_value<double>()) {}

  BLResult update(BLPathView view, uint32_t has_prev_vertex = 0) noexcept {
    PathIterator it(view);

    while (it.has_command()) {
      if (it.is_move()) {
        move_to_count++;
        has_prev_vertex = true;
        Geometry::bound(ink_bounding_box, it.vertex(0));
        it.advance_cmd<BL_PATH_CMD_MOVE>();
      }
      else if (it.is_line()) {
        if (!has_prev_vertex) {
          return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
        }

        Geometry::bound(ink_bounding_box, it.vertex(0));
        it.advance_cmd<BL_PATH_CMD_LINE>();
      }
      else if (it.is_quad()) {
        if (!has_prev_vertex) {
          return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
        }

        BLPoint ctrl = it.vertex(0);
        Geometry::bound(ink_bounding_box, it.vertex(1));

        // Calculate tight bounding-box only when control points are outside the current one.
        if (!axl::bool_and(ctrl.x >= ink_bounding_box.x0,
                           ctrl.y >= ink_bounding_box.y0,
                           ctrl.x <= ink_bounding_box.x1,
                           ctrl.y <= ink_bounding_box.y1)) {
          BLPoint extrema = Geometry::quad_extrema_point(Geometry::quad_ref(it.vertex_ptr() - 1));
          Geometry::bound(ink_bounding_box, extrema);
          Geometry::bound(vertex_bounding_box, ctrl);
        }

        flags |= BL_PATH_FLAG_QUADS;
        it.advance_cmd<BL_PATH_CMD_QUAD>();
      }
      else if (it.is_conic()) {
        if (!has_prev_vertex) {
          return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
        }

        BLPoint ctrl = it.vertex(0);
        Geometry::bound(ink_bounding_box, it.vertex(1));

        if (!axl::bool_and(ctrl.x >= ink_bounding_box.x0,
                           ctrl.y >= ink_bounding_box.y0,
                           ctrl.x <= ink_bounding_box.x1,
                           ctrl.y <= ink_bounding_box.y1)) {
          BLPoint extrema = Geometry::conic_extrema_point(it.vertex_ptr() - 1, it.weight());
          Geometry::bound(ink_bounding_box, extrema);
          Geometry::bound(vertex_bounding_box, ctrl);
        }

        flags |= BL_PATH_FLAG_CONICS;
        it.advance_cmd<BL_PATH_CMD_CONIC>();
      }
      else if (it.is_cubic()) {
        if (!has_prev_vertex) {
          return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
        }

        BLPoint ctrl1 = it.vertex(0);
        BLPoint ctrl2 = it.vertex(1);
        Geometry::bound(ink_bounding_box, it.vertex(2));

        // Calculate tight bounding-box only when control points are outside the current one.
        if (!axl::bool_and(axl::min(ctrl1.x, ctrl2.x) >= ink_bounding_box.x0,
                           axl::min(ctrl1.y, ctrl2.y) >= ink_bounding_box.y0,
                           axl::max(ctrl1.x, ctrl2.x) <= ink_bounding_box.x1,
                           axl::max(ctrl1.y, ctrl2.y) <= ink_bounding_box.y1)) {
          BLPoint extrema[2];
          Geometry::cubic_extrema_points(Geometry::cubic_ref(it.vertex_ptr() - 1), extrema);
          Geometry::bound(ink_bounding_box, extrema[0]);
          Geometry::bound(ink_bounding_box, extrema[1]);
          Geometry::bound(vertex_bounding_box, ctrl1);
          Geometry::bound(vertex_bounding_box, ctrl2);
        }

        flags |= BL_PATH_FLAG_CUBICS;
        it.advance_cmd<BL_PATH_CMD_CUBIC>();
      }
      else if (it.is_close()) {
        has_prev_vertex = false;
        it.advance_cmd<BL_PATH_CMD_CLOSE>();
      }
      else {
        return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
      }
    }

    vertex_bounding_box.x0 = bl_min(vertex_bounding_box.x0, ink_bounding_box.x0);
    vertex_bounding_box.y0 = bl_min(vertex_bounding_box.y0, ink_bounding_box.y0);
    vertex_bounding_box.x1 = bl_max(vertex_bounding_box.x1, ink_bounding_box.x1);
    vertex_bounding_box.y1 = bl_max(vertex_bounding_box.y1, ink_bounding_box.y1);

    if (move_to_count > 1) {
      flags |= BL_PATH_FLAG_MULTIPLE;
    }

    if (!Math::is_finite(vertex_bounding_box, ink_bounding_box)) {
      return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
    }

    return BL_SUCCESS;
  }
};

// Path - API - Path Construction
// ==============================

struct PathByteSizeFromGeometryTypeGenerator {
  static consteval uint8_t value(size_t i) noexcept {
    return uint8_t(i == BL_GEOMETRY_TYPE_BOX_I        ? BLPath::Close::kByteSize + 4 * BLPath::LineTo::kByteSize :
                   i == BL_GEOMETRY_TYPE_BOX_D        ? BLPath::Close::kByteSize + 4 * BLPath::LineTo::kByteSize :
                   i == BL_GEOMETRY_TYPE_RECT_I       ? BLPath::Close::kByteSize + 4 * BLPath::LineTo::kByteSize :
                   i == BL_GEOMETRY_TYPE_RECT_D       ? BLPath::Close::kByteSize + 4 * BLPath::LineTo::kByteSize :
                   i == BL_GEOMETRY_TYPE_CIRCLE_D     ? BLPath::Close::kByteSize + 1 * BLPath::LineTo::kByteSize + 4 * BLPath::ConicTo::kByteSize :
                   i == BL_GEOMETRY_TYPE_ELLIPSE_D    ? BLPath::Close::kByteSize + 1 * BLPath::LineTo::kByteSize + 4 * BLPath::ConicTo::kByteSize :
                   i == BL_GEOMETRY_TYPE_ROUND_RECT_D ? BLPath::Close::kByteSize + 4 * BLPath::LineTo::kByteSize + 4 * BLPath::ConicTo::kByteSize :
                   i == BL_GEOMETRY_TYPE_ARC_D        ? BLPath::Close::kByteSize + 1 * BLPath::LineTo::kByteSize + 4 * BLPath::ConicTo::kByteSize :
                   i == BL_GEOMETRY_TYPE_CHORD_D      ? BLPath::Close::kByteSize + 4 * BLPath::LineTo::kByteSize + 4 * BLPath::ConicTo::kByteSize :
                   i == BL_GEOMETRY_TYPE_PIE_D        ? BLPath::Close::kByteSize + 4 * BLPath::LineTo::kByteSize + 4 * BLPath::ConicTo::kByteSize :
                   i == BL_GEOMETRY_TYPE_LINE_D       ? BLPath::Close::kByteSize + 2 * BLPath::LineTo::kByteSize :
                   i == BL_GEOMETRY_TYPE_TRIANGLE_D   ? BLPath::Close::kByteSize + 3 * BLPath::LineTo::kByteSize : 0);
  }
};

static constexpr const auto path_byte_size_from_geometry_type =
  make_lookup_table<uint8_t, BL_GEOMETRY_TYPE_MAX_VALUE + 1, PathByteSizeFromGeometryTypeGenerator>();

static BL_INLINE BLResult append_box_internal(BLPathCore* self, double x0, double y0, double x1, double y1, BLGeometryDirection dir) noexcept {
  BLResultOr<AppendTarget> target = prepare_append(self, BLPath::LineTo::kByteSize * 4u + BLPath::Close::kByteSize);
  BL_PROPAGATE(target.result());

  BLPoint* vertex_ptr = target().vertex_data;
  uint8_t* control_ptr = target().control_rev;

  set_data_sizes(self, target().vertex_data_size + 4u * sizeof(BLPath::Vertex),
                       target().control_data_size + 5u);

  vertex_ptr[0] = BLPoint(x0, y0);
  vertex_ptr[1] = BLPoint(x1, y0);
  vertex_ptr[2] = BLPoint(x1, y1);
  vertex_ptr[3] = BLPoint(x0, y1);
  control_ptr[-1] = BL_PATH_CMD_MOVE;
  control_ptr[-2] = BL_PATH_CMD_LINE;
  control_ptr[-3] = BL_PATH_CMD_LINE;
  control_ptr[-4] = BL_PATH_CMD_LINE;
  control_ptr[-5] = BL_PATH_CMD_CLOSE;

  if (dir == BL_GEOMETRY_DIRECTION_CW) {
    return BL_SUCCESS;
  }

  vertex_ptr[1] = BLPoint(x0, y1);
  vertex_ptr[3] = BLPoint(x1, y0);

  return BL_SUCCESS;
}

static BLResult append_transformed_path_with_type(BLPathCore* self, const BLPathCore* other, const BLMatrix2D* transform, uint32_t transform_type) noexcept {
  BL_USING_DETAIL(bl::PathDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  size_t src_vertex_data_size = get_vertex_data_size(other);
  size_t src_control_data_size = get_control_data_size(other);

  if (src_control_data_size == 0u) {
    return BL_SUCCESS;
  }

  BL_PROPAGATE(bl_path_reserve_byte_size_for_append(self, src_vertex_data_size + src_control_data_size));

  size_t dst_vertex_data_size = get_vertex_data_size(self);
  size_t dst_control_data_size = get_control_data_size(self);

  uint8_t* dst_data = ObjectLayer::get_data<uint8_t>(self);
  BLPoint* dst_vertex_ptr = axl::offset_ptr<BLPoint>(dst_data, dst_vertex_data_size);
  uint8_t* dst_control_ptr = axl::offset_ptr(dst_data, ObjectLayer::get_capacity(self) - dst_control_data_size);

  const BLPoint* src_vertex_ptr = ObjectLayer::get_data<BLPoint>(other);
  const uint8_t* src_control_ptr = axl::offset_ptr<uint8_t>(src_vertex_ptr, ObjectLayer::get_capacity(other));

  set_data_sizes(self, dst_vertex_data_size + src_vertex_data_size,
                       dst_control_data_size + src_control_data_size);
  clear_flags(self);

  memcpy(dst_control_ptr - src_control_data_size, src_control_ptr - src_control_data_size, src_control_data_size);
  return TransformDetail::map_pointd_array_funcs[transform_type](transform, dst_vertex_ptr, src_vertex_ptr, src_vertex_data_size / sizeof(BLPath::Vertex));
}

} // {bl::PathDetail}

BL_API_IMPL BLResult bl_path_set_vertex_at(BLPathCore* self, size_t index, double x, double y) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  size_t vertex_count = get_vertex_count(self);
  if (BL_LIKELY(axl::bool_and(ObjectLayer::is_object_mutable(self), index < vertex_count))) {
    ObjectLayer::get_data<BLPath::Vertex>(self)[index] = BLPath::Vertex(x, y);
  }
  else {
    if (BL_UNLIKELY(index >= vertex_count)) {
      return bl_make_error(BL_ERROR_INVALID_VALUE);
    }

    ObjectImplSize impl_size = impl_size_from_capacity(ObjectLayer::get_size(self));
    BL_PROPAGATE(realloc_impl(self, impl_size));

    ObjectLayer::get_data<BLPath::Vertex>(self)[index] = BLPath::Vertex(x, y);
  }

  return clear_flags(self);
}

BL_API_IMPL BLResult bl_path_set_weight_at(BLPathCore* self, size_t control_index, double w) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  size_t control_data_size = get_control_data_size(self);
  uint8_t* control_data_rev = get_control_data_rev(self);

  if (!axl::bool_and(control_index < control_data_size, control_index + 10 <= control_data_size)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  control_data_rev -= control_index;

  if (!axl::bool_and(control_data_rev[-1] == BL_PATH_CMD_CONIC,
                     control_data_rev[-10] == BL_PATH_CMD_CONIC)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  if (BL_UNLIKELY(!ObjectLayer::is_object_mutable(self))) {
    ObjectImplSize impl_size = impl_size_from_capacity(ObjectLayer::get_size(self));
    BL_PROPAGATE(realloc_impl(self, impl_size));
    control_data_rev = get_control_data_rev(self) - control_index;
  }

  store_weight(control_data_rev - 9, w);
  return clear_flags(self);
}

BL_API_IMPL BLResult bl_path_move_to(BLPathCore* self, double x0, double y0) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLResultOr<AppendTarget> target = prepare_append(self, BLPath::MoveTo::kByteSize);
  BL_PROPAGATE(target.result());

  set_data_sizes(self, target().vertex_data_size + 1u * sizeof(BLPath::Vertex),
                       target().control_data_size + 1u);

  target().vertex_data[0] = {x0, y0};
  target().control_rev[-1] = BL_PATH_CMD_MOVE;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_path_line_to(BLPathCore* self, double x1, double y1) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLResultOr<AppendTarget> target = prepare_append(self, BLPath::LineTo::kByteSize);
  BL_PROPAGATE(target.result());

  set_data_sizes(self, target().vertex_data_size + 1u * sizeof(BLPath::Vertex),
                       target().control_data_size + 1u);

  target().vertex_data[0] = {x1, y1};
  target().control_rev[-1] = BL_PATH_CMD_LINE;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_path_poly_to(BLPathCore* self, const BLPoint* poly, size_t count) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  constexpr size_t kMaxCount = kMaxDataSize / (sizeof(BLPath::Vertex) + 1);

  // Avoid empty `poly` and inputs that are beyond internal limits.
  if (BL_UNLIKELY(count - 1u >= kMaxCount)) {
    return count == 0u ? BL_SUCCESS : bl_make_error(BL_ERROR_OUT_OF_MEMORY);
  }

  BLResultOr<AppendTarget> target = prepare_append_possibly_aliased(self, count * BLPath::LineTo::kByteSize, &poly);
  BL_PROPAGATE(target.result());

  set_data_sizes(self, target().vertex_data_size + count * sizeof(BLPath::Vertex),
                       target().control_data_size + count);

  BLPath::Vertex* vertex_data = target().vertex_data;
  uint8_t* control_data = target().control_rev - count;

  for (size_t i = 0; i < count; i++) {
    vertex_data[i] = poly[i];
    control_data[i] = BL_PATH_CMD_LINE;
  }

  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_path_quad_to(BLPathCore* self, double x1, double y1, double x2, double y2) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLResultOr<AppendTarget> target = prepare_append(self, BLPath::QuadTo::kByteSize);
  BL_PROPAGATE(target.result());

  set_data_sizes(self, target().vertex_data_size + 2u * sizeof(BLPath::Vertex),
                       target().control_data_size + 1u);

  target().vertex_data[0] = {x1, y1};
  target().vertex_data[1] = {x2, y2};
  target().control_rev[-1] = BL_PATH_CMD_QUAD;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_path_conic_to(BLPathCore* self, double x1, double y1, double x2, double y2, double w) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLResultOr<AppendTarget> target = prepare_append(self, BLPath::ConicTo::kByteSize);
  BL_PROPAGATE(target.result());

  set_data_sizes(self, target().vertex_data_size + 2u * sizeof(BLPath::Vertex),
                       target().control_data_size + BLPath::ConicTo::kControlSize);

  BLPath::Vertex* vertex_data = target().vertex_data;
  uint8_t* control_rev = target().control_rev;

  using W = double;

  vertex_data[0] = {x1, y1};
  vertex_data[1] = {x2, y2};
  control_rev[-1] = BL_PATH_CMD_CONIC;
  axl::storeu(control_rev - 1u - sizeof(W), w);
  control_rev[-int(sizeof(W) + 2u)] = BL_PATH_CMD_CONIC;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_path_cubic_to(BLPathCore* self, double x1, double y1, double x2, double y2, double x3, double y3) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLResultOr<AppendTarget> target = prepare_append(self, BLPath::CubicTo::kByteSize);
  BL_PROPAGATE(target.result());

  set_data_sizes(self, target().vertex_data_size + 3u * sizeof(BLPath::Vertex),
                       target().control_data_size + 1u);

  target().vertex_data[0] = {x1, y1};
  target().vertex_data[1] = {x2, y2};
  target().vertex_data[2] = {x3, y3};
  target().control_rev[-1] = BL_PATH_CMD_CUBIC;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_path_smooth_quad_to(BLPathCore* self, double x2, double y2) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  size_t control_data_size = get_control_data_size(self);
  const uint8_t* control_data_ptr = get_control_data_rev(self) - control_data_size;

  if (BL_UNLIKELY(control_data_size == 0u)) {
    return bl_make_error(BL_ERROR_NO_MATCHING_VERTEX);
  }

  BLPathCmd last_cmd = BLPathCmd(control_data_ptr[0]);
  if (BL_UNLIKELY(vertex_count_of_cmd(last_cmd) == 0u)) {
    return bl_make_error(BL_ERROR_NO_MATCHING_VERTEX);
  }

  BLResultOr<AppendTarget> target = prepare_append(self, BLPath::QuadTo::kByteSize);
  BL_PROPAGATE(target.result());

  set_data_sizes(self, target().vertex_data_size + 2u * sizeof(BLPath::Vertex),
                       target().control_data_size + 1u);

  // Cannot happen if path has commands that have vertices (checked on the top).
  BL_ASSERT(get_vertex_count(self) > 0u);

  BLPoint* vertex_data = target().vertex_data;
  uint8_t* control_rev = target().control_rev;

  double x1 = vertex_data[-1].x;
  double y1 = vertex_data[-1].y;

  if (last_cmd == BL_PATH_CMD_QUAD) {
    // Cannot happen if the last command is QUAD.
    BL_ASSERT(get_vertex_count(self) > 1u);

    x1 += x1 - vertex_data[-2].x;
    y1 += y1 - vertex_data[-2].y;
  }

  vertex_data[0] = {x1, y1};
  vertex_data[1] = {x2, y2};
  control_rev[-1] = BL_PATH_CMD_QUAD;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_path_smooth_cubic_to(BLPathCore* self, double x2, double y2, double x3, double y3) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  size_t control_data_size = get_control_data_size(self);
  const uint8_t* control_data_ptr = get_control_data_rev(self) - control_data_size;

  if (BL_UNLIKELY(control_data_size == 0u)) {
    return bl_make_error(BL_ERROR_NO_MATCHING_VERTEX);
  }

  BLPathCmd last_cmd = BLPathCmd(control_data_ptr[0]);
  if (BL_UNLIKELY(vertex_count_of_cmd(last_cmd) == 0u)) {
    return bl_make_error(BL_ERROR_NO_MATCHING_VERTEX);
  }

  BLResultOr<AppendTarget> target = prepare_append(self, BLPath::CubicTo::kByteSize);
  BL_PROPAGATE(target.result());

  set_data_sizes(self, target().vertex_data_size + 3u * sizeof(BLPath::Vertex),
                       target().control_data_size + 1u);

  [[maybe_unused]] size_t vertex_count = get_vertex_count(self);
  // Cannot happen if path has commands that have vertices (checked on the top).
  BL_ASSERT(vertex_count != 0u);

  BLPoint* vertex_data = target().vertex_data;
  uint8_t* control_rev = target().control_rev;

  double x1 = vertex_data[-1].x;
  double y1 = vertex_data[-1].y;

  if (last_cmd == BL_PATH_CMD_CUBIC) {
    // Cannot happen if the last command is CUBIC.
    BL_ASSERT(vertex_count >= 3u);

    x1 += x1 - vertex_data[-2].x;
    y1 += y1 - vertex_data[-2].y;
  }

  vertex_data[0] = {x1, y1};
  vertex_data[1] = {x2, y2};
  vertex_data[2] = {x3, y3};
  control_rev[-1] = BL_PATH_CMD_CUBIC;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_path_arc_to(BLPathCore* self, double x, double y, double rx, double ry, double start, double sweep, bool force_move_to) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  bool maybe_redundant_line_to = false;
  BLPathCmd initial_cmd = BL_PATH_CMD_MOVE;

  if (!force_move_to) {
    size_t control_data_size = get_control_data_size(self);
    const uint8_t* control_data_ptr = get_control_data_rev(self) - control_data_size;

    if (control_data_size != 0u) {
      BLPathCmd last_cmd = BLPathCmd(control_data_ptr[0]);
      maybe_redundant_line_to = vertex_count_of_cmd(last_cmd) != 0u;
      if (maybe_redundant_line_to) {
        initial_cmd = BL_PATH_CMD_LINE;
      }
    }
  }

  PathAppender dst;
  BL_PROPAGATE(dst.begin_append(self, BLPath::LineTo::kByteSize + BLPath::ConicTo::kByteSize * 4u));
  path_arc_to_conic_spline(dst, BLPoint(x, y), BLPoint(rx, ry), start, sweep, initial_cmd, maybe_redundant_line_to);

  dst.done(self);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_path_arc_quadrant_to(BLPathCore* self, double x1, double y1, double x2, double y2) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLResultOr<AppendTarget> target = prepare_append(self, BLPath::ConicTo::kByteSize);
  BL_PROPAGATE(target.result());

  set_data_sizes(self, target().vertex_data_size + 2u * sizeof(BLPath::Vertex),
                       target().control_data_size + BLPath::ConicTo::kControlSize);

  target().vertex_data[0] = {x1, y1};
  target().vertex_data[1] = {x2, y2};
  store_conic_command(target().control_rev - BLPath::ConicTo::kControlSize, Math::kSQRT_0p5);

  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_path_elliptic_arc_to(BLPathCore* self, double rx, double ry, double x_axis_rotation, bool large_arc_flag, bool sweep_flag, double x1, double y1) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  size_t control_data_size = get_control_data_size(self);
  const uint8_t* control_data_ptr = get_control_data_rev(self) - control_data_size;

  if (BL_UNLIKELY(control_data_size == 0u)) {
    return bl_make_error(BL_ERROR_NO_MATCHING_VERTEX);
  }

  BLPathCmd last_cmd = BLPathCmd(control_data_ptr[0]);
  if (BL_UNLIKELY(vertex_count_of_cmd(last_cmd) == 0u)) {
    return bl_make_error(BL_ERROR_NO_MATCHING_VERTEX);
  }

  BLPoint p0 = get_vertex_data_end(self)[-1]; // Start point.
  BLPoint p1(x1, y1);                                  // End point.

  // Special case - out of range radii.
  //   - See https://www.w3.org/TR/SVG/implnote.html#ArcCorrectionOutOfRangeRadii
  rx = bl_abs(rx);
  ry = bl_abs(ry);

  // Special case - out of range parameters:
  //   - See https://www.w3.org/TR/SVG/paths.html#ArcOutOfRangeParameters
  if (p0 == p1) {
    return BL_SUCCESS;
  }

  if (axl::bool_or(!(rx > Math::epsilon<double>()), !(ry > Math::epsilon<double>()))) {
    return bl_path_line_to(self, p1.x, p1.y);
  }

  // Calculate sin/cos for reuse.
  double rot_sin = Math::sin(x_axis_rotation);
  double rot_cos = Math::cos(x_axis_rotation);

  // Inverse rotation to align the ellipse.
  BLMatrix2D transform = BLMatrix2D::make_sin_cos(-rot_sin, rot_cos);

  // Vector from center (transformed midpoint).
  BLPoint v = transform.map_point((p0 - p1) * 0.5);

  // If scale > 1 the ellipse will need to be rescaled.
  double scale = Math::square(v.x) / Math::square(rx) +
                 Math::square(v.y) / Math::square(ry) ;
  if (scale > 1.0) {
    scale = Math::sqrt(scale);
    rx *= scale;
    ry *= scale;
  }

  // Prepend scale.
  transform.post_scale(1.0 / rx, 1.0 / ry);

  // Calculate unit coordinates.
  BLPoint pp0 = transform.map_point(p0);
  BLPoint pp1 = transform.map_point(p1);

  // New vector from center (unit midpoint).
  v = (pp1 - pp0) * 0.5;
  BLPoint pc = pp0 + v;

  // If length^2 >= 1 the point is already the center.
  double len2 = Geometry::magnitude_squared(v);
  if (len2 < 1.0) {
    v = Math::sqrt(1.0 / len2 - 1.0) * Geometry::normal(v);

    if (large_arc_flag != sweep_flag)
      pc += v;
    else
      pc -= v;
  }

  // Both vectors are unit vectors.
  BLPoint v1 = pp0 - pc;
  BLPoint v2 = pp1 - pc;

  // Set up the final transformation matrix.
  transform.reset_to_sin_cos(v1.y, v1.x);
  transform.post_translate(pc);
  transform.post_scale(rx, ry);
  TransformDetail::multiply(transform, transform, BLMatrix2D::make_sin_cos(rot_sin, rot_cos));

  // We have sin = v1.Cross(v2) / (v1.Length * v2.Length)
  // with length of 'v1' and 'v2' both 1 (unit vectors).
  rot_sin = Geometry::cross(v1, v2);

  // Accordingly cos = v1.Dot(v2) / (v1.Length * v2.Length)
  // to get the angle between 'v1' and 'v2'.
  rot_cos = Geometry::dot(v1, v2);

  // So the sweep angle is Atan2(y, x) = Atan2(sin, cos)
  // https://stackoverflow.com/a/16544330
  double sweep_angle = Math::atan2(rot_sin, rot_cos);
  if (sweep_flag) {
    // Correct the angle if necessary.
    if (sweep_angle < 0) {
      sweep_angle += Math::kPI_MUL_2;
    }

    // |  v1.X  v1.Y  0 |   | v2.X |   | v1.X * v2.X + v1.Y * v2.Y |
    // | -v1.Y  v1.X  0 | * | v2.Y | = | v1.X * v2.Y - v1.Y * v2.X |
    // |  0     0     1 |   | 1    |   | 1                         |
    v2.reset(rot_cos, rot_sin);
  }
  else {
    if (sweep_angle > 0) {
      sweep_angle -= Math::kPI_MUL_2;
    }

    // Flip Y.
    transform.scale(1.0, -1.0);

    v2.reset(rot_cos, -rot_sin);
    sweep_angle = bl_abs(sweep_angle);
  }

  // First quadrant (start and control point).
  v1.reset(1, 0);
  v.reset(1, 1);

  // The the number of 90deg segments we are gonna need. If `i == 1` it means
  // we need one 90deg segment and one smaller segment handled after the loop.
  size_t i = 3;
  if (sweep_angle < Math::kPI_MUL_1p5 + Math::kANGLE_EPSILON) i = 2;
  if (sweep_angle < Math::kPI         + Math::kANGLE_EPSILON) i = 1;
  if (sweep_angle < Math::kPI_DIV_2   + Math::kANGLE_EPSILON) i = 0;

  PathAppender appender;
  BL_PROPAGATE(appender.begin_append(self, BLPath::LineTo::kByteSize + 4 * BLPath::ConicTo::kByteSize));

  // Process 90 degree segments.
  while (i) {
    v1 = Geometry::normal(v1);

    // Transformed points of the arc segment.
    pp0 = transform.map_point(v);
    pp1 = transform.map_point(v1);
    appender.arc_quadrant_to(pp0, pp1);

    v = Geometry::normal(v);
    i--;
  }

  // Calculate the remaining control point.
  v = v1 + v2;
  v = 2.0 * v / Geometry::dot(v, v);

  // Final arc segment.
  pp0 = transform.map_point(v);
  pp1 = p1;

  // This is actually half of the remaining cos. It is required that v1 dot v2 > -1 holds
  // but we can safely assume it (only critical for angles close to 180 degrees).
  rot_cos = Math::sqrt(0.5 * (1.0 + Geometry::dot(v1, v2)));
  appender.conic_to(pp0, pp1, rot_cos);
  appender.done(self);

  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_path_close(BLPathCore* self) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  size_t capacity = ObjectLayer::dyn_capacity(self);
  size_t control_data_size = get_control_data_size(self);

  // Prevent closing an empty path.
  if (BL_UNLIKELY(control_data_size == 0u)) {
    return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
  }

  // Prevent closing an already closed figure.
  uint8_t* control_data_rev = ObjectLayer::get_data<uint8_t>(self) + capacity - control_data_size;
  if (BL_UNLIKELY(control_data_rev[0] == BL_PATH_CMD_CLOSE)) {
    return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
  }

  size_t vertex_data_size = get_vertex_data_size(self);
  size_t byte_size = vertex_data_size + control_data_size;

  if (BL_UNLIKELY(!ObjectLayer::check_dynamic_capacity_and_mutability(self, BLPath::Close::kByteSize, byte_size))) {
    BL_PROPAGATE(realloc_for_append(self, BLPath::Close::kByteSize));
    capacity = ObjectLayer::dyn_capacity(self);
    control_data_rev = ObjectLayer::get_data<uint8_t>(self) + capacity - control_data_size;
  }

  self->_d.dyn.payload_u32 = uint32_t(control_data_size + 1u);
  control_data_rev[-1] = BL_PATH_CMD_CLOSE;
  return clear_flags(self);
}

BL_API_IMPL BLResult bl_path_add_box_i(BLPathCore* self, const BLBoxI* box, BLGeometryDirection dir) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return append_box_internal(self, double(box->x0), double(box->y0), double(box->x1), double(box->y1), dir);
}

BL_API_IMPL BLResult bl_path_add_box_d(BLPathCore* self, const BLBox* box, BLGeometryDirection dir) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  return append_box_internal(self, box->x0, box->y0, box->x1, box->y1, dir);
}

BL_API_IMPL BLResult bl_path_add_rect_i(BLPathCore* self, const BLRectI* rect, BLGeometryDirection dir) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLBox box;
  box.x0 = double(rect->x);
  box.y0 = double(rect->y);
  box.x1 = double(rect->w) + double(box.x0);
  box.y1 = double(rect->h) + double(box.y0);
  return bl_path_add_box_d(self, &box, dir);
}

BL_API_IMPL BLResult bl_path_add_rect_d(BLPathCore* self, const BLRect* rect, BLGeometryDirection dir) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLBox box;
  box.x0 = rect->x;
  box.y0 = rect->y;
  box.x1 = rect->w + box.x0;
  box.y1 = rect->h + box.y0;
  return bl_path_add_box_d(self, &box, dir);
}

BL_API_IMPL BLResult bl_path_add_geometry(BLPathCore* self, BLGeometryType geometry_type, const void* geometry_data, const BLMatrix2D* m, BLGeometryDirection dir) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (BL_UNLIKELY(uint32_t(geometry_type) > BL_GEOMETRY_TYPE_MAX_VALUE)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  size_t byte_size = path_byte_size_from_geometry_type[geometry_type];
  size_t object_count = 0;
  size_t initial_vertex_data_size = get_vertex_data_size(self);

  const void* geometry_src = geometry_data;

  if (byte_size == 0) {
    switch (geometry_type) {
      // We don't expect this often so that's why we pessimistically check it here...
      case BL_GEOMETRY_TYPE_NONE: {
        return BL_SUCCESS;
      }

      case BL_GEOMETRY_TYPE_POLYLINE_D:
      case BL_GEOMETRY_TYPE_POLYLINE_I: {
        const BLSpan<const uint8_t>* span = static_cast<const BLSpan<const uint8_t>*>(geometry_data);

        object_count = span->size();
        geometry_src = span->data();

        if (!object_count) {
          return BL_SUCCESS;
        }

        byte_size = axl::mul_saturate(object_count, BLPath::LineTo::kByteSize);
        break;
      }

      case BL_GEOMETRY_TYPE_POLYGON_D:
      case BL_GEOMETRY_TYPE_POLYGON_I: {
        const BLSpan<const uint8_t>* span = static_cast<const BLSpan<const uint8_t>*>(geometry_data);

        object_count = span->size();
        geometry_src = span->data();

        if (!object_count) {
          return BL_SUCCESS;
        }

        byte_size = axl::mul_saturate(object_count, BLPath::LineTo::kByteSize + BLPath::Close::kByteSize);
        break;
      }

      case BL_GEOMETRY_TYPE_SPAN_BOX_D:
      case BL_GEOMETRY_TYPE_SPAN_BOX_I:
      case BL_GEOMETRY_TYPE_SPAN_RECT_D:
      case BL_GEOMETRY_TYPE_SPAN_RECT_I: {
        const BLSpan<const uint8_t>* span = static_cast<const BLSpan<const uint8_t>*>(geometry_data);

        object_count = span->size();
        geometry_src = span->data();

        if (!object_count) {
          return BL_SUCCESS;
        }

        byte_size = axl::mul_saturate<size_t>(object_count, BLPath::LineTo::kByteSize * 5u);
        break;
      }

      case BL_GEOMETRY_TYPE_PATH_D: {
        const BLPath* other = static_cast<const BLPath*>(geometry_data);
        BL_ASSERT(ObjectLayer::is_object_valid(other));

        if (other->is_empty()) {
          return BL_SUCCESS;
        }

        if (dir == BL_GEOMETRY_DIRECTION_CW) {
          if (m)
            return bl_path_add_transformed_path(self, other, m);
          else
            return bl_path_add_path(self, other);
        }
        else {
          if (m) {
            return bl_path_add_reversed_transformed_path(self, other, m);
          }
          else {
            return bl_path_add_reversed_path(self, other);
          }
        }
        break;
      }

      // Should never be reached as we filtered all border cases already...
      default: {
        return bl_make_error(BL_ERROR_INVALID_VALUE);
      }
    }
  }

  // Should never be zero if we went here.
  BL_ASSERT(byte_size != 0);
  BLResultOr<AppendTarget> target = prepare_append_possibly_aliased(self, byte_size, &geometry_src);
  BL_PROPAGATE(target.result());

  PathAppender appender;
  appender.begin_append_to_data(target().vertex_data, target().control_rev);

  double x0, y0; // For adding 'BLBox', 'BLBoxI', 'BLRect', 'BLRectI', and `BLRoundRect`.
  double x1, y1;

  switch (geometry_type) {
    case BL_GEOMETRY_TYPE_BOX_I:
      x0 = double(static_cast<const BLBoxI*>(geometry_src)->x0);
      y0 = double(static_cast<const BLBoxI*>(geometry_src)->y0);
      x1 = double(static_cast<const BLBoxI*>(geometry_src)->x1);
      y1 = double(static_cast<const BLBoxI*>(geometry_src)->y1);
      goto AddBoxD;

    case BL_GEOMETRY_TYPE_BOX_D:
      x0 = static_cast<const BLBox*>(geometry_src)->x0;
      y0 = static_cast<const BLBox*>(geometry_src)->y0;
      x1 = static_cast<const BLBox*>(geometry_src)->x1;
      y1 = static_cast<const BLBox*>(geometry_src)->y1;
      goto AddBoxD;

    case BL_GEOMETRY_TYPE_RECT_I:
      x0 = double(static_cast<const BLRectI*>(geometry_src)->x);
      y0 = double(static_cast<const BLRectI*>(geometry_src)->y);
      x1 = double(static_cast<const BLRectI*>(geometry_src)->w) + x0;
      y1 = double(static_cast<const BLRectI*>(geometry_src)->h) + y0;
      goto AddBoxD;

    case BL_GEOMETRY_TYPE_RECT_D:
      x0 = static_cast<const BLRect*>(geometry_src)->x;
      y0 = static_cast<const BLRect*>(geometry_src)->y;
      x1 = static_cast<const BLRect*>(geometry_src)->w + x0;
      y1 = static_cast<const BLRect*>(geometry_src)->h + y0;

AddBoxD:
      appender.add_box(x0, y0, x1, y1, dir);
      break;

    case BL_GEOMETRY_TYPE_CIRCLE_D:
    case BL_GEOMETRY_TYPE_ELLIPSE_D: {
      double cx, cy;
      double rx, ry;

      if (geometry_type == BL_GEOMETRY_TYPE_CIRCLE_D) {
        const BLCircle* circle = static_cast<const BLCircle*>(geometry_src);
        cx = circle->cx;
        cy = circle->cy;
        rx = circle->r;
        ry = bl_abs(rx);
      }
      else {
        const BLEllipse* ellipse = static_cast<const BLEllipse*>(geometry_src);
        cx = ellipse->cx;
        cy = ellipse->cy;
        rx = ellipse->rx;
        ry = ellipse->ry;
      }

      if (dir != BL_GEOMETRY_DIRECTION_CW) {
        ry = -ry;
      }

      x0 = cx - rx;
      y0 = cy - ry;
      x1 = cx + rx;
      y1 = cy + ry;

      appender.move_to(x1, cy);
      appender.arc_quadrant_to(x1, y1, cx, y1);
      appender.arc_quadrant_to(x0, y1, x0, cy);
      appender.arc_quadrant_to(x0, y0, cx, y0);
      appender.arc_quadrant_to(x1, y0, x1, cy);
      appender.close();
      break;
    }

    case BL_GEOMETRY_TYPE_ROUND_RECT_D: {
      const BLRoundRect* round = static_cast<const BLRoundRect*>(geometry_src);

      x0 = round->x;
      y0 = round->y;
      x1 = round->x + round->w;
      y1 = round->y + round->h;

      double w_half = round->w * 0.5;
      double h_half = round->h * 0.5;

      double rx = bl_min(bl_abs(round->rx), w_half);
      double ry = bl_min(bl_abs(round->ry), h_half);

      // Degrade to box if rx/ry are degenerate.
      if (BL_UNLIKELY(!(rx > Math::epsilon<double>() && ry > Math::epsilon<double>()))) {
        goto AddBoxD;
      }

      if (dir == BL_GEOMETRY_DIRECTION_CW) {
        appender.move_to(x0 + rx, y0);
        appender.line_to(x1 - rx, y0);
        appender.arc_quadrant_to(x1, y0, x1, y0 + ry);
        appender.line_to(x1, y1 - ry);
        appender.arc_quadrant_to(x1, y1, x1 - rx, y1);
        appender.line_to(x0 + rx, y1);
        appender.arc_quadrant_to(x0, y1, x0, y1 - ry);
        appender.line_to(x0, y0 + ry);
        appender.arc_quadrant_to(x0, y0, x0 + rx, y0);
        appender.close();
      }
      else {
        appender.move_to(x0 + rx, y0);
        appender.arc_quadrant_to(x0, y0, x0, y0 + ry);
        appender.line_to(x0, y1 - ry);
        appender.arc_quadrant_to(x0, y1, x0 + rx, y1);
        appender.line_to(x1 - rx, y1);
        appender.arc_quadrant_to(x1, y1, x1, y1 - ry);
        appender.line_to(x1, y0 + ry);
        appender.arc_quadrant_to(x1, y0, x1 - rx, y0);
        appender.close();
      }
      break;
    }

    case BL_GEOMETRY_TYPE_LINE_D: {
      const BLPoint* src = static_cast<const BLPoint*>(geometry_src);
      size_t first = size_t(dir != BL_GEOMETRY_DIRECTION_CW);

      appender.move_to(src[first]);
      appender.line_to(src[first ^ 1]);
      break;
    }

    case BL_GEOMETRY_TYPE_ARC_D: {
      const BLArc* arc = static_cast<const BLArc*>(geometry_src);

      BLPoint c(arc->cx, arc->cy);
      BLPoint r(arc->rx, arc->ry);
      double start = arc->start;
      double sweep = arc->sweep;

      if (dir != BL_GEOMETRY_DIRECTION_CW) {
        sweep = -sweep;
      }

      path_arc_to_conic_spline(appender, c, r, start, sweep, BL_PATH_CMD_MOVE);
      break;
    }

    case BL_GEOMETRY_TYPE_CHORD_D:
    case BL_GEOMETRY_TYPE_PIE_D: {
      const BLArc* arc = static_cast<const BLArc*>(geometry_src);

      BLPoint c(arc->cx, arc->cy);
      BLPoint r(arc->rx, arc->ry);
      double start = arc->start;
      double sweep = arc->sweep;

      if (dir != BL_GEOMETRY_DIRECTION_CW) {
        sweep = -sweep;
      }

      BLPathCmd arc_initial_cmd = BL_PATH_CMD_MOVE;
      if (geometry_type == BL_GEOMETRY_TYPE_PIE_D) {
        appender.move_to(c);
        arc_initial_cmd = BL_PATH_CMD_LINE;
      }

      path_arc_to_conic_spline(appender, c, r, start, sweep, arc_initial_cmd);
      appender.close();
      break;
    }

    case BL_GEOMETRY_TYPE_TRIANGLE_D: {
      const BLPoint* src = static_cast<const BLPoint*>(geometry_src);
      size_t cw = dir == BL_GEOMETRY_DIRECTION_CW ? 0 : 2;

      appender.move_to(src[0 + cw]);
      appender.line_to(src[1]);
      appender.line_to(src[2 - cw]);
      appender.close();
      break;
    }

    case BL_GEOMETRY_TYPE_POLYLINE_I: {
      const BLPointI* src = static_cast<const BLPointI*>(geometry_src);
      PathAppender::Cmd* initial = appender._control_ptr;

      if (dir == BL_GEOMETRY_DIRECTION_CW) {
        for (size_t i = object_count; i; i--) {
          appender.line_to(*src++);
        }
      }
      else {
        src += object_count - 1;
        for (size_t i = object_count; i; i--) {
          appender.line_to(*src--);
        }
      }

      initial[-1].value = uint8_t(BL_PATH_CMD_MOVE);
      break;
    }

    case BL_GEOMETRY_TYPE_POLYLINE_D: {
      const BLPoint* src = static_cast<const BLPoint*>(geometry_src);
      PathAppender::Cmd* initial = appender._control_ptr;

      if (dir == BL_GEOMETRY_DIRECTION_CW) {
        for (size_t i = object_count; i; i--) {
          appender.line_to(*src++);
        }
      }
      else {
        src += object_count - 1;
        for (size_t i = object_count; i; i--) {
          appender.line_to(*src--);
        }
      }

      initial[-1].value = uint8_t(BL_PATH_CMD_MOVE);
      break;
    }

    case BL_GEOMETRY_TYPE_POLYGON_I: {
      const BLPointI* src = static_cast<const BLPointI*>(geometry_src);
      PathAppender::Cmd* initial = appender._control_ptr;

      if (dir == BL_GEOMETRY_DIRECTION_CW) {
        for (size_t i = object_count; i; i--) {
          appender.line_to(*src++);
        }
      }
      else {
        src += object_count - 1;
        for (size_t i = object_count; i; i--) {
          appender.line_to(*src--);
        }
      }

      initial[-1].value = uint8_t(BL_PATH_CMD_MOVE);
      appender.close();
      break;
    }

    case BL_GEOMETRY_TYPE_POLYGON_D: {
      const BLPoint* src = static_cast<const BLPoint*>(geometry_src);
      PathAppender::Cmd* initial = appender._control_ptr;

      if (dir == BL_GEOMETRY_DIRECTION_CW) {
        for (size_t i = object_count; i; i--) {
          appender.line_to(*src++);
        }
      }
      else {
        src += object_count - 1;
        for (size_t i = object_count; i; i--) {
          appender.line_to(*src--);
        }
      }

      initial[-1].value = uint8_t(BL_PATH_CMD_MOVE);
      appender.close();
      break;
    }

    case BL_GEOMETRY_TYPE_SPAN_BOX_I: {
      const BLBoxI* src = static_cast<const BLBoxI*>(geometry_src);

      if (dir == BL_GEOMETRY_DIRECTION_CW) {
        for (size_t i = object_count; i != 0; i--, src++) {
          if (!Geometry::is_valid(*src)) {
            continue;
          }
          appender.add_box_cw(src->x0, src->y0, src->x1, src->y1);
        }
      }
      else {
        src += object_count - 1;
        for (size_t i = object_count; i != 0; i--, src--) {
          if (!Geometry::is_valid(*src)) {
            continue;
          }
          appender.add_box_ccw(src->x0, src->y0, src->x1, src->y1);
        }
      }
      break;
    }

    case BL_GEOMETRY_TYPE_SPAN_BOX_D: {
      const BLBox* src = static_cast<const BLBox*>(geometry_src);

      if (dir == BL_GEOMETRY_DIRECTION_CW) {
        for (size_t i = object_count; i != 0; i--, src++) {
          if (!Geometry::is_valid(*src)) {
            continue;
          }
          appender.add_box_cw(src->x0, src->y0, src->x1, src->y1);
        }
      }
      else {
        src += object_count - 1;
        for (size_t i = object_count; i != 0; i--, src--) {
          if (!Geometry::is_valid(*src)) {
            continue;
          }
          appender.add_box_ccw(src->x0, src->y0, src->x1, src->y1);
        }
      }
      break;
    }

    case BL_GEOMETRY_TYPE_SPAN_RECT_I: {
      const BLRectI* src = static_cast<const BLRectI*>(geometry_src);

      if (dir == BL_GEOMETRY_DIRECTION_CW) {
        for (size_t i = object_count; i != 0; i--, src++) {
          if (!Geometry::is_valid(*src)) {
            continue;
          }
          appender.add_rect_cw(double(src->x), double(src->y), double(src->w), double(src->h));
        }
      }
      else {
        src += object_count - 1;
        for (size_t i = object_count; i != 0; i--, src--) {
          if (!Geometry::is_valid(*src)) {
            continue;
          }
          appender.add_rect_ccw(double(src->x), double(src->y), double(src->w), double(src->h));
        }
      }
      break;
    }

    case BL_GEOMETRY_TYPE_SPAN_RECT_D: {
      const BLRect* src = static_cast<const BLRect*>(geometry_src);

      if (dir == BL_GEOMETRY_DIRECTION_CW) {
        for (size_t i = object_count; i != 0; i--, src++) {
          if (!Geometry::is_valid(*src)) {
            continue;
          }
          appender.add_rect_cw(src->x, src->y, src->w, src->h);
        }
      }
      else {
        src += object_count - 1;
        for (size_t i = object_count; i != 0; i--, src--) {
          if (!Geometry::is_valid(*src)) {
            continue;
          }
          appender.add_rect_ccw(src->x, src->y, src->w, src->h);
        }
      }
      break;
    }

    default: {
      // This is not possible considering even bad input as we have filtered this already.
      BL_NOT_REACHED();
    }
  }

  appender.done(self);
  if (!m) {
    return BL_SUCCESS;
  }

  BLPoint* vertex_data = axl::offset_ptr(get_vertex_data(self), initial_vertex_data_size);
  return bl_matrix2d_map_point_d_array(m,
    vertex_data,
    vertex_data,
    (get_vertex_data_size(self) - initial_vertex_data_size) / sizeof(BLPath::Vertex));
}

BL_API_IMPL BLResult bl_path_add_path(BLPathCore* self, const BLPathCore* other) noexcept {
  BL_USING_DETAIL(bl::PathDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  size_t src_vertex_data_size = get_vertex_data_size(other);
  size_t src_control_data_size = get_control_data_size(other);

  if (src_control_data_size == 0u) {
    return BL_SUCCESS;
  }

  BL_PROPAGATE(bl_path_reserve_byte_size_for_append(self, src_vertex_data_size + src_control_data_size));

  size_t dst_vertex_data_size = get_vertex_data_size(self);
  size_t dst_control_data_size = get_control_data_size(self);

  set_data_sizes(self, dst_vertex_data_size + src_vertex_data_size, dst_control_data_size + src_control_data_size);
  clear_flags(self);

  uint8_t* dst_ptr = get_path_data(self);
  const uint8_t* src_ptr = get_path_data(other);
  memcpy(dst_ptr + dst_vertex_data_size, src_ptr, src_vertex_data_size);

  uint8_t* dst_control_ptr = dst_ptr + ObjectLayer::get_capacity(self) - dst_control_data_size;
  const uint8_t* src_control_ptr = src_ptr + ObjectLayer::get_capacity(other);

  memcpy(dst_control_ptr - src_control_data_size, src_control_ptr - src_control_data_size, src_control_data_size);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_path_add_translated_path(BLPathCore* self, const BLPathCore* other, const BLPoint* p) noexcept {
  BL_USING_DETAIL(bl::PathDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  BLMatrix2D transform = BLMatrix2D::make_translation(*p);
  return append_transformed_path_with_type(self, other, &transform, BL_TRANSFORM_TYPE_TRANSLATE);
}

BL_API_IMPL BLResult bl_path_add_transformed_path(BLPathCore* self, const BLPathCore* other, const BLMatrix2D* transform) noexcept {
  BL_USING_DETAIL(bl::PathDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  size_t vertex_count = get_vertex_count(other);
  if (vertex_count == 0u) {
    return BL_SUCCESS;
  }

  BLTransformType transform_type = (vertex_count >= BL_MATRIX_TYPE_MINIMUM_SIZE) ? transform->type() : BL_TRANSFORM_TYPE_AFFINE;
  return append_transformed_path_with_type(self, other, transform, transform_type);
}

BL_API_IMPL BLResult bl_path_add_reversed_path(BLPathCore* self, const BLPathCore* other) noexcept {
  BL_USING_DETAIL(bl::PathDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  PathAppender dst;
  BL_PROPAGATE(dst.begin_append(self, ObjectLayer::get_size(other)));

  BLResult result = dst.append_reversed_path(other->dcast().view());
  dst.done(self);
  return result;
}

BL_API_IMPL BLResult  bl_path_add_reversed_transformed_path(BLPathCore* self, const BLPathCore* other, const BLMatrix2D* transform) noexcept {
  BL_USING_DETAIL(bl::PathDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  if (BL_UNLIKELY(is_empty(other))) {
    return BL_SUCCESS;
  }

  size_t initial_vertex_count = get_vertex_count(self);
  BL_PROPAGATE(bl_path_add_reversed_path(self, other));

  size_t final_vertex_count = get_vertex_count(self);
  size_t added_vertex_count = final_vertex_count - initial_vertex_count;

  // If the `other` path wasn't empty, it should have added at least one vertex, which we should see here.
  BL_ASSERT(added_vertex_count > 0u);

  BLPoint* vertex_ptr = ObjectLayer::get_data<BLPoint>(self) + initial_vertex_count;
  BLTransformType transform_type = (added_vertex_count >= BL_MATRIX_TYPE_MINIMUM_SIZE) ? transform->type() : BL_TRANSFORM_TYPE_AFFINE;
  return TransformDetail::map_pointd_array_funcs[transform_type](transform, vertex_ptr, vertex_ptr, added_vertex_count);
}

// Path - API - Stroke
// ===================

namespace bl::PathDetail {

static BLResult append_stroked_path_sink(BLPathCore* a, BLPathCore* b, BLPathCore* c, const BLPathRegion* input_region, void* user_data) noexcept {
  BL_ASSERT(ObjectLayer::is_object_valid(a));
  BL_ASSERT(ObjectLayer::is_object_valid(b));
  BL_ASSERT(ObjectLayer::is_object_valid(c));

  axl::maybe_unused(input_region, user_data);

  size_t required_byte_size = b->dcast().byte_size() + c->dcast().byte_size() + BLPath::MoveTo::kByteSize;

  PathAppender appender;
  BL_PROPAGATE(appender.begin_append(a, required_byte_size));

  BLResult result = appender.join_reversed_path(b->dcast().view());
  appender.join_path(c->dcast().view());
  appender.close_if_open();
  appender.done(a);
  return result;
}

} // {bl::PathDetail}

BL_API_IMPL BLResult bl_path_add_stroked_path(BLPathCore* self, const BLPathCore* other, const BLStrokeOptionsCore* options, const BLApproximationOptions* approx) noexcept {
  BL_USING_DETAIL(bl::PathDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(other));

  if (is_empty(other)) {
    return BL_SUCCESS;
  }

  if (!approx) {
    approx = &bl_default_approximation_options;
  }

  BLPath b_path;
  BLPath c_path;

  if (self == other) {
    // Border case, we don't want anything to happen to the `other` path during processing.
    // And since stroking may need to reallocate the output path it would be unsafe.
    BLPath tmp(other->dcast());
    return Geometry::stroke_path(tmp.view(), options->dcast(), *approx, self->dcast(), b_path, c_path, append_stroked_path_sink, nullptr);
  }
  else {
    return Geometry::stroke_path(other->dcast().view(), options->dcast(), *approx, self->dcast(), b_path, c_path, append_stroked_path_sink, nullptr);
  }
}

BL_API_IMPL BLResult BL_CDECL bl_path_stroke_to_sink(
    const BLPathCore* self,
    const BLStrokeOptionsCore* stroke_options,
    const BLApproximationOptions* approximation_options,
    BLPathCore* a,
    BLPathCore* b,
    BLPathCore* c,
    BLPathStrokeSinkFunc sink, void* user_data) noexcept {
  BL_USING_DETAIL(bl::PathDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(self));
  BL_ASSERT(ObjectLayer::is_object_valid(a));
  BL_ASSERT(ObjectLayer::is_object_valid(b));
  BL_ASSERT(ObjectLayer::is_object_valid(c));

  if (is_empty(self)) {
    return BL_SUCCESS;
  }

  if (!approximation_options) {
    approximation_options = &bl_default_approximation_options;
  }

  BLPathView input = self->dcast().view();

  if (a == self || b == self || c == self) {
    BLPath tmp(self->dcast());
    return Geometry::stroke_path(input, stroke_options->dcast(), *approximation_options, a->dcast(), b->dcast(), c->dcast(), sink, user_data);
  }
  else {
    return Geometry::stroke_path(input, stroke_options->dcast(), *approximation_options, a->dcast(), b->dcast(), c->dcast(), sink, user_data);
  }
}

BL_API_IMPL BLResult bl_path_add_vertex_outline_of_path(BLPathCore* self, const BLPathCore* other) noexcept {
  BL_USING_DETAIL(bl::PathDetail);

  // Approximate the output size to be the number of vertices + close for each vertex in the worst case.
  size_t required_size = other->dcast().vertex_count() * (1u + 1u + sizeof(BLPoint));
  if (required_size == 0) {
    return BL_SUCCESS;
  }

  PathAppender appender;
  BL_PROPAGATE(appender.begin_append(self, required_size));

  PathIterator it(other->dcast().view());
  while (it.has_command()) {
    if (it.command() != BL_PATH_CMD_MOVE) {
      return bl_make_error(BL_ERROR_CORRUPTED_PATH_DATA);
    }

    appender.move_to(it.vertex(0));
    it.advance_cmd<BL_PATH_CMD_MOVE>();

    while (it.has_command()) {
      if (it.is_line()) {
        appender.line_to(it.vertex(0));
        it.advance_cmd<BL_PATH_CMD_LINE>();
      }
      else if (it.is_quad()) {
        appender.line_to(it.vertex(0));
        appender.line_to(it.vertex(1));
        it.advance_cmd<BL_PATH_CMD_QUAD>();
      }
      else if (it.is_conic()) {
        appender.line_to(it.vertex(0));
        appender.line_to(it.vertex(1));
        it.advance_cmd<BL_PATH_CMD_CONIC>();
      }
      else if (it.is_cubic()) {
        appender.line_to(it.vertex(0));
        appender.line_to(it.vertex(1));
        appender.line_to(it.vertex(2));
        it.advance_cmd<BL_PATH_CMD_CUBIC>();
      }
      else if (it.is_close()) {
        appender.close();
        it.advance_cmd<BL_PATH_CMD_CLOSE>();
        break;
      }
      else if (it.is_move()) {
        break;
      }
      else {
        return bl_make_error(BL_ERROR_CORRUPTED_PATH_DATA);
      }
    }
  }

  appender.done(self);
  return BL_SUCCESS;
}

// Path - API - Path Transformations
// =================================

namespace bl::PathDetail {

static BLResult transform_with_type(BLPathCore* self, const BLMatrix2D* transform, uint32_t transform_type) noexcept {
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  size_t vertex_count = get_vertex_count(self);
  if (!vertex_count) {
    return BL_SUCCESS;
  }

  BL_PROPAGATE(make_mutable(self));
  BLPoint* vertex_data = ObjectLayer::get_data<BLPoint>(self);
  return TransformDetail::map_pointd_array_funcs[transform_type](transform, vertex_data, vertex_data, vertex_count);
}

} // {bl::PathDetail}

BL_API_IMPL BLResult bl_path_translate(BLPathCore* self, const BLPoint* p) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLMatrix2D transform = BLMatrix2D::make_translation(*p);
  return transform_with_type(self, &transform, BL_TRANSFORM_TYPE_TRANSLATE);
}

BL_API_IMPL BLResult bl_path_transform(BLPathCore* self, const BLMatrix2D* m) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  // Only check the transform type if we reach the limit as the check costs cycles too.
  size_t vertex_count = get_vertex_count(self);
  BLTransformType transform_type = (vertex_count >= BL_MATRIX_TYPE_MINIMUM_SIZE) ? m->type() : BL_TRANSFORM_TYPE_AFFINE;

  return transform_with_type(self, m, transform_type);
}

// Maps a horizontal/vertical alignment sub-field of `fit_flags` to a factor in [0.0, 1.0] that distributes
// the free space left in the rectangle (0.0 = min/leading edge, 0.5 = center, 1.0 = max/trailing edge).
static BL_INLINE double path_fit_align_factor(uint32_t align) noexcept {
  // BL_PATH_FIT_ALIGN_?_MID (0) -> 0.5, _MIN -> 0.0, _MAX -> 1.0. Center is the default for an unset sub-field.
  return align == 0u ? 0.5 : (align == 1u ? 0.0 : 1.0);
}

BL_API_IMPL BLResult bl_path_fit_to(BLPathCore* self, const BLRect* rect, BLPathFitFlags fit_flags) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  if (!axl::bool_and(Math::is_finite(*rect), rect->w > 0.0, rect->h > 0.0)) {
    return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  if (is_empty(self)) {
    return BL_SUCCESS;
  }

  PathInfoUpdater updater;
  BL_PROPAGATE(updater.update(self->dcast().view()));

  const BLBox& ink_bb = updater.ink_bounding_box;
  double bx = ink_bb.x0;
  double by = ink_bb.y0;
  double bw = ink_bb.x1 - ink_bb.x0;
  double bh = ink_bb.y1 - ink_bb.y0;

  // A degenerate bounding box (zero width or height) cannot be fit by scaling (would yield infinite scales).
  if (!axl::bool_and(bw > 0.0, bh > 0.0)) {
    return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
  }

  double sx = rect->w / bw;
  double sy = rect->h / bh;

  // Resolve the scaling policy. `BL_PATH_FIT_SCALE_FILL` keeps `sx/sy` independent (aspect ratio not
  // preserved), `CONTAIN/COVER` collapse them into a single uniform scale (aspect ratio preserved).
  switch (fit_flags & BL_PATH_FIT_SCALE_MASK) {
    case BL_PATH_FIT_SCALE_FILL:
      break;
    case BL_PATH_FIT_SCALE_CONTAIN:
      sx = sy = bl_min(sx, sy);
      break;
    case BL_PATH_FIT_SCALE_COVER:
      sx = sy = bl_max(sx, sy);
      break;
    default:
      return bl_make_error(BL_ERROR_INVALID_VALUE);
  }

  // Never scale up if requested - clamp every scale factor to a maximum of 1.0.
  if (fit_flags & BL_PATH_FIT_SHRINK_ONLY) {
    sx = bl_min(sx, 1.0);
    sy = bl_min(sy, 1.0);
  }

  // Distribute the free space left in the rectangle according to the requested alignment. For `FILL`
  // the scaled box matches the rectangle exactly so `dx`/`dy` are zero and alignment has no effect.
  double dx = rect->w - bw * sx;
  double dy = rect->h - bh * sy;

  double ax = path_fit_align_factor((fit_flags & BL_PATH_FIT_ALIGN_X_MASK) >> axl::ctz(BL_PATH_FIT_ALIGN_X_MASK));
  double ay = path_fit_align_factor((fit_flags & BL_PATH_FIT_ALIGN_Y_MASK) >> axl::ctz(BL_PATH_FIT_ALIGN_Y_MASK));

  double tx = rect->x - bx * sx + dx * ax;
  double ty = rect->y - by * sy + dy * ay;

  BLMatrix2D transform(sx, 0.0, 0.0, sy, tx, ty);
  return transform_with_type(self, &transform, BL_TRANSFORM_TYPE_SCALE);
}

// Path - API Equals
// =================

BL_API_IMPL bool bl_path_equals(const BLPathCore* a, const BLPathCore* b) noexcept {
  BL_USING_DETAIL(bl::PathDetail);

  BL_ASSERT(ObjectLayer::is_object_valid(a));
  BL_ASSERT(ObjectLayer::is_object_valid(b));

  size_t vertex_data_size = get_vertex_data_size(a);
  size_t control_data_size = get_control_data_size(a);

  if (axl::bool_or(vertex_data_size != get_vertex_data_size(b),
                   control_data_size != get_control_data_size(b))) {
    return false;
  }

  const uint8_t* a_data = ObjectLayer::get_data<uint8_t>(a);
  const uint8_t* b_data = ObjectLayer::get_data<uint8_t>(b);

  if (axl::bool_and(a_data == b_data, ObjectLayer::get_impl(a) == ObjectLayer::get_impl(b))) {
    return true;
  }

  size_t a_control_start = ObjectLayer::get_capacity(a) - control_data_size;
  size_t b_control_start = ObjectLayer::get_capacity(b) - control_data_size;

  return memcmp(a_data, b_data, vertex_data_size) == 0 &&
         memcmp(a_data + a_control_start, b_data + b_control_start, control_data_size) == 0;
}

// Path - API Path Info
// ====================

namespace bl::PathDetail {

static BL_NOINLINE BLResult update_info(const BLPathCore* self, PathPrivateImpl* impl) noexcept {
  // Special-case. The path info is valid, but the path is invalid. We handle it here to simplify `ensure_info()`
  // and to make it a bit shorter.
  if (impl->flags & BL_PATH_FLAG_INVALID) {
    return bl_make_error(BL_ERROR_INVALID_GEOMETRY);
  }

  PathInfoUpdater updater;
  BLResult result = updater.update(self->dcast().view());

  // Path is invalid.
  if (result != BL_SUCCESS) {
    impl->flags = updater.flags | BL_PATH_FLAG_INVALID;
    impl->vertex_bounding_box.reset();
    impl->ink_bounding_box.reset();
    return result;
  }

  // Path is empty.
  if (!axl::bool_and(updater.vertex_bounding_box.x0 <= updater.vertex_bounding_box.x1,
                     updater.vertex_bounding_box.y0 <= updater.vertex_bounding_box.y1)) {
    impl->flags = updater.flags | BL_PATH_FLAG_EMPTY;
    impl->vertex_bounding_box.reset();
    impl->ink_bounding_box.reset();
    return BL_SUCCESS;
  }

  // Path is valid.
  impl->flags = updater.flags;
  impl->vertex_bounding_box = updater.vertex_bounding_box;
  impl->ink_bounding_box = updater.ink_bounding_box;
  return BL_SUCCESS;
}

static BL_INLINE BLResult ensure_info(const BLPathCore* self) noexcept {
  PathPrivateImpl* impl = ObjectLayer::get_impl(self);
  if (impl->flags & (BL_PATH_FLAG_INVALID | BL_PATH_FLAG_DIRTY)) {
    return update_info(self, impl);
  }

  return BL_SUCCESS;
}

} // {bl::PathDetail}

BL_API_IMPL BLResult bl_path_get_info_flags(const BLPathCore* self, uint32_t* flags_out) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLResult result = ensure_info(self);
  *flags_out = ObjectLayer::get_impl(self)->flags;
  return result;
}

// Path - API - Bounding Box
// =========================

BL_API_IMPL BLResult bl_path_get_vertex_bounding_box(const BLPathCore* self, BLBox* box_out) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLResult result = ensure_info(self);
  *box_out = ObjectLayer::get_impl(self)->vertex_bounding_box;
  return result;
}

BL_API_IMPL BLResult bl_path_get_ink_bounding_box(const BLPathCore* self, BLBox* box_out) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  BLResult result = ensure_info(self);
  *box_out = ObjectLayer::get_impl(self)->ink_bounding_box;
  return result;
}

// Path - API - Vertex Queries
// ===========================

BL_API_IMPL BLResult bl_path_get_last_vertex(const BLPathCore* self, BLPoint* vtx_out) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  PathReverseIterator it(self->dcast().view());

  vtx_out->reset();
  if (!it.has_command() || !get_vertex_count(self)) {
    return bl_make_error(BL_ERROR_NO_MATCHING_VERTEX);
  }

  // If the last vertex is not part of a close command, we can safely return it.
  if (it.is_close()) {
    it.advance_cmd<BL_PATH_CMD_CLOSE>();
    for (;;) {
      if (BL_UNLIKELY(!it.has_command())) {
        return bl_make_error(BL_ERROR_NO_MATCHING_VERTEX);
      }

      BLPathCmd command = it.command();
      if (is_begin_or_conic_cmd(command)) {
        if (command == BL_PATH_CMD_CONIC) {
          it.advance_cmd<BL_PATH_CMD_CONIC>();
        }
        else if (command == BL_PATH_CMD_MOVE) {
          break;
        }
        else {
          return bl_make_error(BL_ERROR_NO_MATCHING_VERTEX);
        }
      }
      else {
        it.advance_non_conic(command);
      }
    }
  }

  *vtx_out = it.vertex(0);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult bl_path_get_closest_vertex(const BLPathCore* self, const BLPoint* p, double max_distance, size_t* index_out, double* distance_out) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  size_t vertex_count = get_vertex_count(self);
  const BLPoint* vertex_data = get_vertex_data(self);

  *index_out = SIZE_MAX;
  *distance_out = Math::nan<double>();

  if (BL_UNLIKELY(!vertex_count)) {
    return bl_make_error(BL_ERROR_NO_MATCHING_VERTEX);
  }

  size_t best_index = SIZE_MAX;
  double best_distance = Math::inf<double>();
  double best_distance_sq = Math::inf<double>();

  BLPoint pt(*p);

  bool has_max_distance = axl::bool_and(max_distance > 0.0, max_distance < Math::inf<double>());
  if (has_max_distance) {
    best_distance = max_distance;
    best_distance_sq = Math::square(best_distance);

    // This code-path can be used to skip the whole path if the given point is too far.
    // We need 'max_distance' to be specified and also bounding-box to be available.
    if (ensure_info(self) == BL_SUCCESS) {
      // If the given point is outside of the path bounding-box extended by
      // `max_distance` then there is no matching vertex to possibly return.
      const BLBox& b_box = ObjectLayer::get_impl(self)->vertex_bounding_box;
      if (!axl::bool_and(pt.x >= b_box.x0 - best_distance,
                         pt.y >= b_box.y0 - best_distance,
                         pt.x <= b_box.x1 + best_distance,
                         pt.y <= b_box.y1 + best_distance)) {
        return bl_make_error(BL_ERROR_NO_MATCHING_VERTEX);
      }
    }
  }

  for (size_t i = 0; i < vertex_count; i++) {
    double d = Math::square(vertex_data[i].x - pt.x) + Math::square(vertex_data[i].y - pt.y);
    if (d < best_distance_sq) {
      best_index = i;
      best_distance_sq = d;
    }
  }

  if (best_index == SIZE_MAX) {
    best_distance = Math::nan<double>();
  }
  else {
    best_distance = Math::sqrt(best_distance_sq);
  }

  *index_out = best_index;
  *distance_out = best_distance;

  return BL_SUCCESS;
}

// Path - API - Hit Test
// =====================

BL_API_IMPL BLHitTest bl_path_hit_test(const BLPathCore* self, const BLPoint* p_, BLFillRule fill_rule) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  BL_ASSERT(ObjectLayer::is_object_valid(self));

  PathIterator it(self->dcast().view());

  if (!it.has_command()) {
    return BL_HIT_TEST_OUT;
  }

  bool has_move_to = false;
  BLPoint start {};
  BLPoint pt(*p_);

  BLPoint p0;
  BLPoint p1;

  intptr_t winding_number = 0;

  // 10 points - maximum for cubic spline having 3 cubics (1 + 3 + 3 + 3).
  BLPoint spline_data[10];

  do {
    switch (it.command()) {
      case BL_PATH_CMD_MOVE: {
        if (has_move_to) {
          // Implicitly close the previous (open) sub-path by connecting its last vertex to its start.
          p0 = it.vertex(-1);
          p1 = start;
          has_move_to = false;
          goto OnLine;
        }

        start = it.vertex(0);
        has_move_to = true;
        it.advance_cmd<BL_PATH_CMD_MOVE>();
        break;
      }

      case BL_PATH_CMD_LINE: {
        if (BL_UNLIKELY(!has_move_to)) {
          return BL_HIT_TEST_INVALID;
        }

        p0 = it.vertex(-1);
        p1 = it.vertex(0);
        it.advance_cmd<BL_PATH_CMD_LINE>();

OnLine:
        {
          BLPoint d = p1 - p0;

          if (d.y > 0.0) {
            if (pt.y >= p0.y && pt.y < p1.y) {
              double ix = p0.x + (pt.y - p0.y) * d.x / d.y;
              winding_number += (pt.x >= ix);
            }
          }
          else if (d.y < 0.0) {
            if (pt.y >= p1.y && pt.y < p0.y) {
              double ix = p0.x + (pt.y - p0.y) * d.x / d.y;
              winding_number -= (pt.x >= ix);
            }
          }
        }
        break;
      }

      case BL_PATH_CMD_QUAD: {
        if (BL_UNLIKELY(!has_move_to)) {
          return BL_HIT_TEST_INVALID;
        }

        const BLPoint* p = it.vertex_ptr() - 1u;
        it.advance_cmd<BL_PATH_CMD_QUAD>();

        double min_y = bl_min(p[0].y, p[1].y, p[2].y);
        double max_y = bl_max(p[0].y, p[1].y, p[2].y);

        if (pt.y >= min_y && pt.y <= max_y) {
          if (axl::bool_and(Math::is_near(p[0].y, p[1].y), Math::is_near(p[1].y, p[2].y))) {
            p0 = p[0];
            p1 = p[2];
            goto OnLine;
          }

          // Split to a quad-spline at Y-extrema, if found.
          const BLPoint* spline_ptr = p;
          const BLPoint* spline_end = p + 2u;

          const BLPoint* split_end = Geometry::quad_split_with_options<Geometry::QuadSplitOptions::kExtremaY>(Geometry::quad_ref(p), spline_data);
          if (split_end != spline_data) {
            spline_ptr = spline_data;
            spline_end = split_end;
          }

          do {
            min_y = bl_min(spline_ptr[0].y, spline_ptr[2].y);
            max_y = bl_max(spline_ptr[0].y, spline_ptr[2].y);

            if (pt.y >= min_y && pt.y < max_y) {
              int dir = 0;

              if (spline_ptr[0].y < spline_ptr[2].y) {
                dir = 1;
              }
              else if (spline_ptr[0].y > spline_ptr[2].y) {
                dir = -1;
              }

              // It should be only possible to have zero or one solution.
              double ti[2];
              double ix;

              Geometry::QuadCoefficients qc = Geometry::quad_coefficients(Geometry::quad_ref(spline_ptr));

              // { At^2 + Bt + C } -> { (At + B)t + C }
              if (Math::quad_roots(ti, qc.a.y, qc.b.y, qc.c.y - pt.y, 0.0, 1.0) >= 1u) {
                ix = Math::madd(Math::madd(qc.a.x, ti[0], qc.b.x), ti[0], qc.c.x);
              }
              else if (pt.y - min_y < max_y - pt.y) {
                ix = p[0].x;
              }
              else {
                ix = p[2].x;
              }

              if (pt.x >= ix) {
                winding_number += dir;
              }
            }
          } while ((spline_ptr += 2) != spline_end);
        }
        break;
      }

      case BL_PATH_CMD_CONIC: {
        if (BL_UNLIKELY(!has_move_to)) {
          return BL_HIT_TEST_INVALID;
        }

        const BLPoint* p = it.vertex_ptr() - 1u;
        double w = it.weight();
        it.advance_cmd<BL_PATH_CMD_CONIC>();

        double min_y = bl_min(p[0].y, p[1].y, p[2].y);
        double max_y = bl_max(p[0].y, p[1].y, p[2].y);

        // A rational quadratic curve with a positive weight is contained in the convex hull of its control
        // points, so the control points provide exact  Y bounds, which can be used to reject the segment early.
        if (pt.y >= min_y && pt.y <= max_y) {
          if (axl::bool_and(Math::is_near(p[0].y, p[1].y), Math::is_near(p[1].y, p[2].y))) {
            p0 = p[0];
            p1 = p[2];
            goto OnLine;
          }

          // Conic { C(t) = N(t) / D(t) } where the numerator and denominator are:
          //   N(t) = (NA*t + NB)*t + NC
          //   D(t) = (DA*t + DB)*t + 1
          BLPoint na = Math::madd(-(2.0 * w), p[1], p[0] + p[2]);
          BLPoint nb = 2.0 * Math::msub(w, p[1], p[0]);
          BLPoint nc = p[0];

          double da = 2.0 * (1.0 - w);
          double db = 2.0 * (w - 1.0);

          // Split the curve into Y-monotonic spans at its Y-extrema (at most two extrema, producing up to three
          // monotonic spans).
          Geometry::ConicDerivativeCoefficients dc = Geometry::conic_derivative_coefficients(p, w);

          double ts[4];
          size_t ts_count = 1;

          ts[0] = 0.0;
          ts_count += Math::quad_roots(ts + ts_count, dc.a.y, dc.b.y, dc.c.y, Math::kAfter0, Math::kBefore1);
          ts[ts_count++] = 1.0;

          double ta = 0.0;
          double xa = p[0].x;
          double ya = p[0].y;

          for (size_t i = 1; i < ts_count; i++) {
            double tb = ts[i];
            double xb, yb;

            if (i + 1u == ts_count) {
              // The last point must be exact.
              xb = p[2].x;
              yb = p[2].y;
            }
            else {
              double d_inv = 1.0 / Math::madd(Math::madd(da, tb, db), tb, 1.0);
              BLPoint num = Math::madd(Math::madd(na, tb, nb), tb, nc);
              xb = num.x * d_inv;
              yb = num.y * d_inv;
            }

            double span_min_y = bl_min(ya, yb);
            double span_max_y = bl_max(ya, yb);

            if (pt.y >= span_min_y && pt.y < span_max_y) {
              int dir = yb > ya ? 1 : -1;

              // It should be only possible to have zero or one solution within a Y-monotonic span.
              double ti[2];
              double ix;

              // { N(t) - pt.y * D(t) = 0 } is quadratic as the (positive) denominator
              // is folded in: { (NA - pt.y*DA)t^2 + (NB - pt.y*DB)t + (NC - pt.y) }.
              if (Math::quad_roots(ti, Math::madd(-pt.y, da, na.y),
                                       Math::madd(-pt.y, db, nb.y),
                                       nc.y - pt.y, ta, tb) >= 1) {
                ix = Math::madd(Math::madd(na.x, ti[0], nb.x), ti[0], nc.x) / Math::madd(Math::madd(da, ti[0], db), ti[0], 1.0);
              }
              else if (pt.y - span_min_y < span_max_y - pt.y) {
                ix = xa;
              }
              else {
                ix = xb;
              }

              if (pt.x >= ix)
                winding_number += dir;
            }

            ta = tb;
            xa = xb;
            ya = yb;
          }
        }
        break;
      }

      case BL_PATH_CMD_CUBIC: {
        if (BL_UNLIKELY(!has_move_to)) {
          return BL_HIT_TEST_INVALID;
        }

        const BLPoint* p = it.vertex_ptr() - 1u;
        it.advance_cmd<BL_PATH_CMD_CUBIC>();

        double min_y = bl_min(p[0].y, p[1].y, p[2].y, p[3].y);
        double max_y = bl_max(p[0].y, p[1].y, p[2].y, p[3].y);

        if (pt.y >= min_y && pt.y <= max_y) {
          if (axl::bool_and(Math::is_near(p[0].y, p[1].y),
                            Math::is_near(p[1].y, p[2].y),
                            Math::is_near(p[2].y, p[3].y))) {
            p0 = p[0];
            p1 = p[3];
            goto OnLine;
          }

          // Subdivide to a cubic spline at Y-extrema.
          const BLPoint* spline_ptr = p;
          const BLPoint* spline_end = p + 3u;

          const BLPoint* split_end = Geometry::cubic_split_with_options<Geometry::CubicSplitOptions::kExtremaY>(Geometry::cubic_ref(p), spline_data);
          if (split_end != spline_data) {
            spline_ptr = spline_data;
            spline_end = split_end;
          }

          do {
            min_y = bl_min(spline_ptr[0].y, spline_ptr[3].y);
            max_y = bl_max(spline_ptr[0].y, spline_ptr[3].y);

            if (pt.y >= min_y && pt.y < max_y) {
              int dir = 0;
              if (spline_ptr[0].y < spline_ptr[3].y)
                dir = 1;
              else if (spline_ptr[0].y > spline_ptr[3].y)
                dir = -1;

              // It should be only possible to have zero or one solution.
              double ti[3];
              double ix;

              Geometry::CubicCoefficients cc = Geometry::cubic_coefficients(Geometry::cubic_ref(spline_ptr));

              // { At^3 + Bt^2 + Ct + D } -> { ((At + B)t + C)t + D }
              if (Math::cubic_roots(ti, cc.a.y, cc.b.y, cc.c.y, cc.d.y - pt.y, Math::kAfter0, Math::kBefore1) >= 1) {
                ix = Math::madd(Math::madd(Math::madd(cc.a.x, ti[0], cc.b.x), ti[0], cc.c.x), ti[0], cc.d.x);
              }
              else if (pt.y - min_y < max_y - pt.y) {
                ix = spline_ptr[0].x;
              }
              else {
                ix = spline_ptr[3].x;
              }

              if (pt.x >= ix) {
                winding_number += dir;
              }
            }
          } while ((spline_ptr += 3) != spline_end);
        }
        break;
      }

      case BL_PATH_CMD_CLOSE: {
        if (has_move_to) {
          p0 = it.vertex(-1);
          p1 = start;
          has_move_to = false;
          goto OnLine;
        }

        it.advance_cmd<BL_PATH_CMD_CLOSE>();
        break;
      }

      default: {
        return BL_HIT_TEST_INVALID;
      }
    }
  } while (it.has_command());

  // Close the path.
  if (has_move_to) {
    p0 = it.vertex(-1);
    p1 = start;
    has_move_to = false;
    goto OnLine;
  }

  if (fill_rule == BL_FILL_RULE_EVEN_ODD) {
    winding_number &= 1;
  }

  return winding_number != 0 ? BL_HIT_TEST_IN : BL_HIT_TEST_OUT;
}

// Path - Runtime Registration
// ===========================

void bl_path_rt_init([[maybe_unused]] BLRuntimeContext* rt) noexcept {
  BL_USING_DETAIL(bl::PathDetail);
  static constexpr BLObjectType object_type = BLPath::Info::kType;

  // NOTE: const_cast is necessary as the path data is const, but Impl needs
  // non-const pointers. Since the data is eternal it will never be touched.
  BLPathCore& null_object = static_cast<BLPathCore&>(bl_object_defaults[object_type]);
  ObjectLayer::init_dyn(&null_object, object_type, 0u,
    const_cast<PathNullImpl*>(&null_path.impl),
    const_cast<uint8_t*>(null_path.impl.data)
  );
  null_object._d.dyn.capacity_u32 = uint32_t(PathNullImpl::capacity);
}
