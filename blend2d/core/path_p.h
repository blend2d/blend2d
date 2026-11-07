// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_PATH_P_H_INCLUDED
#define BLEND2D_CORE_PATH_P_H_INCLUDED

#include <blend2d/axl/math_p.h>
#include <blend2d/axl/mem_ops.h>
#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/core/path.h>
#include <blend2d/geometry/bezier_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

//! Private implementation that extends \ref BLPathImpl.
struct PathPrivateImpl : public BLPathImpl {
  //! Path flags related to caching.
  uint32_t flags;
  //! Reserved for future use (keeps a proper alignment atm).
  uint8_t reserved[12];

  //! Vertex bounding-box represents a bounding box of all vertices.
  BLBox vertex_bounding_box;
  //! Ink bounding-box represents a tight bounding box of a filled path.
  BLBox ink_bounding_box;
};

template<>
struct ObjectDispatchInfo<BLPathCore> {
  using ImplType = PathPrivateImpl;

  static constexpr bool kHasInitSSO = false;
  static constexpr size_t kImplDataOffset = sizeof(PathPrivateImpl);

  static BL_INLINE BLResult free_object(BLPathCore* self, ImplType* impl, ObjectLayer::MetaByte meta_byte) noexcept {
    axl::maybe_unused(self);
    return ObjectLayer::dealloc_impl(impl, meta_byte);
  }
};

} // {bl}

namespace bl::PathDetail {

static constexpr size_t kMaxDataSize = ObjectLayer::kMaxImplSize32 - sizeof(PathPrivateImpl);

static BL_INLINE_CONSTEXPR size_t capacity_from_impl_size(ObjectImplSize impl_size) noexcept {
  return size_t(impl_size) - sizeof(PathPrivateImpl);
}

static BL_INLINE_CONSTEXPR ObjectImplSize impl_size_from_capacity(size_t capacity) noexcept {
  return ObjectImplSize(axl::align_up(sizeof(PathPrivateImpl) + capacity, ObjectLayer::kMinImplAlignment));
}

static BL_INLINE_NODEBUG uint32_t vertex_count_of_cmd(BLPathCmd cmd) noexcept { return (uint32_t(cmd) >> 2); }
static BL_INLINE_NODEBUG bool is_single_vertex_cmd(BLPathCmd cmd) noexcept { return vertex_count_of_cmd(cmd) == 1; }
static BL_INLINE_NODEBUG bool is_double_vertex_cmd(BLPathCmd cmd) noexcept { return vertex_count_of_cmd(cmd) == 2; }
static BL_INLINE_NODEBUG bool is_begin_or_conic_cmd(BLPathCmd cmd) noexcept { return (cmd & 0x3) != 0; }
static BL_INLINE_NODEBUG bool is_move_or_close_cmd(BLPathCmd cmd) noexcept { return (cmd & 0x2) != 0; }

static BL_INLINE_NODEBUG bool is_empty(const BLPathCore* self) noexcept { return self->_d.dyn.payload_u32 == 0u; }

static BL_INLINE_NODEBUG size_t get_vertex_data_size(const BLPathCore* self) noexcept { return self->_d.dyn.size_u32; }
static BL_INLINE_NODEBUG size_t get_vertex_count(const BLPathCore* self) noexcept { return self->_d.dyn.size_u32 / sizeof(BLPath::Vertex); }

static BL_INLINE_NODEBUG size_t get_control_data_size(const BLPathCore* self) noexcept { return self->_d.dyn.payload_u32; }
static BL_INLINE_NODEBUG size_t get_control_data_rev_end(const BLPathCore* self) noexcept { return size_t(self->_d.dyn.capacity_u32 - self->_d.dyn.payload_u32); }

static BL_INLINE_NODEBUG uint8_t* get_path_data(BLPathCore* self) noexcept { return ObjectLayer::get_data<uint8_t>(self); }
static BL_INLINE_NODEBUG const uint8_t* get_path_data(const BLPathCore* self) noexcept { return ObjectLayer::get_data<uint8_t>(self); }

static BL_INLINE_NODEBUG BLPoint* get_vertex_data(BLPathCore* self) noexcept { return ObjectLayer::get_data<BLPoint>(self); }
static BL_INLINE_NODEBUG const BLPoint* get_vertex_data(const BLPathCore* self) noexcept { return ObjectLayer::get_data<BLPoint>(self); }

static BL_INLINE_NODEBUG BLPoint* get_vertex_data_end(BLPathCore* self) noexcept { return axl::offset_ptr(get_vertex_data(self), get_vertex_data_size(self)); }
static BL_INLINE_NODEBUG const BLPoint* get_vertex_data_end(const BLPathCore* self) noexcept { return axl::offset_ptr(get_vertex_data(self), get_vertex_data_size(self)); }

static BL_INLINE_NODEBUG uint8_t* get_control_data_rev(BLPathCore* self) noexcept { return ObjectLayer::get_data<uint8_t>(self) + ObjectLayer::get_capacity(self); }
static BL_INLINE_NODEBUG const uint8_t* get_control_data_rev(const BLPathCore* self) noexcept { return ObjectLayer::get_data<uint8_t>(self) + ObjectLayer::get_capacity(self); }

static BL_INLINE void set_data_sizes(BLPathCore* self, size_t vertex_data_size, size_t control_data_size) noexcept {
  BL_ASSERT(ObjectLayer::size_fits_32_bits(vertex_data_size));
  BL_ASSERT(ObjectLayer::size_fits_32_bits(control_data_size));

  self->_d.dyn.size_u32 = uint32_t(vertex_data_size);
  self->_d.dyn.payload_u32 = uint32_t(control_data_size);
}

static BL_INLINE BLResult clear_flags(BLPathCore* self) noexcept {
  PathPrivateImpl* impl = ObjectLayer::get_impl(self);
  impl->flags = BL_PATH_FLAG_DIRTY;
  return BL_SUCCESS;
}

static BL_INLINE void store_weight(uint8_t* control_ptr, double weight) noexcept {
  axl::storeu(control_ptr, weight);
}

static BL_INLINE void store_conic_command(uint8_t* control_ptr, double weight) noexcept {
  control_ptr[0] = uint8_t(BL_PATH_CMD_CONIC);
  store_weight(control_ptr + 1, weight);
  control_ptr[9] = uint8_t(BL_PATH_CMD_CONIC);
}

static BL_INLINE consteval BLApproximationOptions make_default_approximation_options() noexcept {
  return BLApproximationOptions {
    BL_FLATTEN_MODE_DEFAULT, // Default flattening mode.
    BL_OFFSET_MODE_DEFAULT,  // Default offset mode.
    { 0, 0 },                // Reserved.
    0.20f,                   // Default curve flattening tolerance.
    0.05f,                   // Default curve simplification tolerance.
    0.414213562f             // Default offset parameter.
  };
}

static BL_INLINE void copy_vertex_data(BLPoint* dst, const BLPoint* src, size_t count) noexcept {
  size_t i = count;

  BL_NOUNROLL
  while (i >= 4) {
    BLPoint v0 = src[0];
    BLPoint v1 = src[1];
    BLPoint v2 = src[2];
    BLPoint v3 = src[3];

    dst[0] = v0;
    dst[1] = v1;
    dst[2] = v2;
    dst[3] = v3;

    dst += 4;
    src += 4;
    i -= 4;
  }

  BL_NOUNROLL
  while (i) {
    *dst++ = *src++;
    i--;
  }
}
static BL_INLINE void copy_vertex_data_reversed(BLPoint* dst, const BLPoint* src, size_t count) noexcept {
  size_t i = count;
  src += count;

  BL_NOUNROLL
  while (i >= 4) {
    BLPoint v0 = src[-1];
    BLPoint v1 = src[-2];
    BLPoint v2 = src[-3];
    BLPoint v3 = src[-4];

    dst[0] = v0;
    dst[1] = v1;
    dst[2] = v2;
    dst[3] = v3;

    dst += 4;
    src -= 4;
    i -= 4;
  }

  BL_NOUNROLL
  while (i) {
    *dst++ = *--src;
    i--;
  }
}

} // {bl::PathDetail}

namespace bl {

template<BLPathCmd Cmd>
struct PathAdvance {
  static constexpr size_t kVertexCount = Cmd == BL_PATH_CMD_MOVE  ? 1u :
                                         Cmd == BL_PATH_CMD_LINE  ? 1u :
                                         Cmd == BL_PATH_CMD_QUAD  ? 2u :
                                         Cmd == BL_PATH_CMD_CONIC ? 2u :
                                         Cmd == BL_PATH_CMD_CUBIC ? 3u : 0u;
  static constexpr size_t kControlSize = Cmd == BL_PATH_CMD_CONIC ? 1u + 1u + sizeof(double) : 1u;
  static constexpr size_t kByteSize = kVertexCount * sizeof(BLPath::Vertex) + kControlSize;
};

//! \name BLPath Iterator
//! \{

//! Path iterator that can iterate over path data.
struct PathIterator {
  const BLPoint* _vertex_ptr {};
  const uint8_t* _control_rev_ptr {};
  const uint8_t* _control_rev_terminate {};

  struct Location {
    const BLPath::Vertex* vertex_data;
    const uint8_t* control_rev;
  };

  BL_INLINE_NODEBUG PathIterator() noexcept = default;
  BL_INLINE_NODEBUG explicit PathIterator(BLPathView view) noexcept { reset(view); }

  BL_INLINE_NODEBUG void reset() noexcept {
    _vertex_ptr = nullptr;
    _control_rev_ptr = nullptr;
    _control_rev_terminate = nullptr;
  }

  BL_INLINE_NODEBUG void reset(BLPathView view) noexcept {
    _vertex_ptr = view.vertex_data;
    _control_rev_ptr = view.control_data_rev - 1u;
    _control_rev_terminate = _control_rev_ptr - view.control_data_size;
  }

  BL_INLINE_NODEBUG size_t remaining_control_size() const noexcept {
    return (size_t)((_control_rev_ptr + 1u) - _control_rev_terminate);
  }

  BL_INLINE_NODEBUG Location location() const noexcept {
    return Location{_vertex_ptr, _control_rev_ptr};
  }

  BL_INLINE_NODEBUG BLPathPosition position_from(const BLPath& path) noexcept {
    return BLPathPosition{
      .vertex_offset = (size_t)(_vertex_ptr - path.vertex_data()),
      .control_offset = (size_t)((path.control_data_rev() - 1u) - _control_rev_ptr)
    };
  }

  BL_INLINE_NODEBUG BLPathPosition position_from(const Location& from) noexcept {
    return BLPathPosition{
      .vertex_offset = (size_t)(_vertex_ptr - from.vertex_data),
      .control_offset = (size_t)(from.control_rev - _control_rev_ptr)
    };
  }

  BL_INLINE_NODEBUG bool has_command() const noexcept { return _control_rev_ptr > _control_rev_terminate; }

  BL_INLINE_NODEBUG void _advance_control_ptr(size_t n) noexcept { _control_rev_ptr -= n; }
  BL_INLINE_NODEBUG void _advance_vertex_ptr(size_t n) noexcept { _vertex_ptr += n; }

  BL_INLINE_NODEBUG void advance(size_t vertex_count, size_t control_count) noexcept {
    _advance_vertex_ptr(vertex_count);
    _advance_control_ptr(control_count);
  }

  BL_INLINE_NODEBUG void advance_non_conic(BLPathCmd cmd) noexcept {
    BL_ASSERT(cmd != BL_PATH_CMD_CONIC);
    advance(PathDetail::vertex_count_of_cmd(cmd), 1u);
  }

  template<BLPathCmd Cmd>
  BL_INLINE_NODEBUG void advance_cmd() noexcept {
    advance(PathAdvance<Cmd>::kVertexCount, PathAdvance<Cmd>::kControlSize);
  }

  BL_INLINE BLPathCmd command() const noexcept {
    BL_ASSERT(has_command());
    return BLPathCmd(_control_rev_ptr[0]);
  }

  BL_INLINE bool is_move() const noexcept { return command() == BL_PATH_CMD_MOVE; }
  BL_INLINE bool is_line() const noexcept { return command() == BL_PATH_CMD_LINE; }
  BL_INLINE bool is_quad() const noexcept { return command() == BL_PATH_CMD_QUAD; }
  BL_INLINE bool is_conic() const noexcept { return command() == BL_PATH_CMD_CONIC; }
  BL_INLINE bool is_cubic() const noexcept { return command() == BL_PATH_CMD_CUBIC; }
  BL_INLINE bool is_close() const noexcept { return command() == BL_PATH_CMD_CLOSE; }

  BL_INLINE_NODEBUG double weight() const noexcept {
    BL_ASSERT(remaining_control_size() >= sizeof(double));
    return axl::loadu<double>(_control_rev_ptr - 8);
  }

  BL_INLINE_NODEBUG BLPoint vertex(auto index) const noexcept { return _vertex_ptr[index]; }
  BL_INLINE_NODEBUG const BLPoint* vertex_ptr() const noexcept { return _vertex_ptr; }

  BL_INLINE Geometry::QuadRef quad_ref() const noexcept {
    BL_ASSERT(is_quad());
    return Geometry::QuadRef(_vertex_ptr - 1);
  }

  BL_INLINE Geometry::ConicRef conic_ref() const noexcept {
    BL_ASSERT(is_conic());
    return Geometry::ConicRef(_vertex_ptr - 1, weight());
  }

  BL_INLINE Geometry::CubicRef cubic_ref() const noexcept {
    BL_ASSERT(is_cubic());
    return Geometry::CubicRef(_vertex_ptr - 1);
  }
};

//! Path iterator that can iterate over path data in a reverse direction.
struct PathReverseIterator {
  const BLPoint* _vertex_ptr {};
  const uint8_t* _control_ptr {};
  const uint8_t* _control_terminate {};

  BL_INLINE_NODEBUG PathReverseIterator() noexcept = default;
  BL_INLINE_NODEBUG explicit PathReverseIterator(BLPathView view) noexcept { reset(view); }

  BL_INLINE_NODEBUG void reset(BLPathView view) noexcept {
    _vertex_ptr = view.vertex_data + view.vertex_count - 1u;
    _control_ptr = view.control_data_rev - view.control_data_size;
    _control_terminate = view.control_data_rev;
  }

  BL_INLINE_NODEBUG size_t remaining_control_size() const noexcept {
    return (size_t)(_control_terminate - _control_ptr);
  }

  BL_INLINE_NODEBUG bool has_command() const noexcept { return _control_ptr < _control_terminate; }

  BL_INLINE_NODEBUG void _advance_control_ptr(size_t n) noexcept { _control_ptr += n; }
  BL_INLINE_NODEBUG void _advance_vertex_ptr(size_t n) noexcept { _vertex_ptr -= n; }

  BL_INLINE_NODEBUG void advance(size_t vertex_count, size_t control_count) noexcept {
    _advance_vertex_ptr(vertex_count);
    _advance_control_ptr(control_count);
  }

  BL_INLINE_NODEBUG void advance_non_conic(BLPathCmd cmd) noexcept {
    BL_ASSERT(cmd != BL_PATH_CMD_CONIC);
    advance(PathDetail::vertex_count_of_cmd(cmd), 1u);
  }

  template<BLPathCmd Cmd>
  BL_INLINE_NODEBUG void advance_cmd() noexcept {
    advance(PathAdvance<Cmd>::kVertexCount, PathAdvance<Cmd>::kControlSize);
  }

  BL_INLINE_NODEBUG BLPathCmd command() const noexcept {
    BL_ASSERT(has_command());
    return BLPathCmd(_control_ptr[0]);
  }

  BL_INLINE bool is_move() const noexcept { return command() == BL_PATH_CMD_MOVE; }
  BL_INLINE bool is_line() const noexcept { return command() == BL_PATH_CMD_LINE; }
  BL_INLINE bool is_quad() const noexcept { return command() == BL_PATH_CMD_QUAD; }
  BL_INLINE bool is_conic() const noexcept { return command() == BL_PATH_CMD_CONIC; }
  BL_INLINE bool is_cubic() const noexcept { return command() == BL_PATH_CMD_CUBIC; }
  BL_INLINE bool is_close() const noexcept { return command() == BL_PATH_CMD_CLOSE; }

  BL_INLINE_NODEBUG double weight() const noexcept {
    BL_ASSERT(remaining_control_size() >= 9);
    return axl::loadu<double>(_control_ptr + 1);
  }

  BL_INLINE_NODEBUG BLPoint vertex(auto index) const noexcept { return *(_vertex_ptr - index); }
  BL_INLINE_NODEBUG const BLPoint* vertex_ptr() const noexcept { return _vertex_ptr; }
};

//! \}

//! \name BLPath Appender
//! \{

//! Low-level interface that can be used to append vertices & commands to an existing path fast. The interface is
//! designed in a way that the user using it must reserve enough space and then call `append...()` functions that
//! can only be called when there is enough storage left for that command. The storage requirements are specified
//! by `begin()` or by `ensure()`. The latter is mostly used to reallocate the array in case more vertices are needed
//! than initially passed to `begin()`.
//!
//! When designing a loop the appender can be used in the following way, where `SourceT` is an iterable object that
//! can provide us some path vertices and commands:
//!
//! ```
//! template<typename Iterator>
//! BLResult append_to_path(BLPath& dst, const Iterator& iter) noexcept {
//!   bl::PathAppender appender;
//!   BL_PROPAGATE(appender.begin_assign(dst, 32));
//!
//!   while (!iter.end()) {
//!     // Maximum number of vertices required by move_to/line_to/quad_to/cubic_to.
//!     BL_PROPAGATE(appender.ensure(dst, 3));
//!
//!     switch (iter.command()) {
//!       case BL_PATH_CMD_MOVE:
//!         appender.move_to(iter[0]);
//!         iter.advance<BL_PATH_CMD_MOVE>();
//!         break;
//!       case BL_PATH_CMD_LINE:
//!         appender.line_to(iter[0]);
//!         iter.advance<BL_PATH_CMD_LINE>();
//!         break;
//!       case BL_PATH_CMD_QUAD:
//!         appender.quad_to(iter[0], iter[1]);
//!         iter.advance<BL_PATH_CMD_QUAD>();
//!         break;
//!       case BL_PATH_CMD_CONIC:
//!         appender.conic_to(iter[0], iter[1], iter.weight());
//!         iter.advance<BL_PATH_CMD_CONIC>();
//!         break;
//!       case BL_PATH_CMD_CUBIC:
//!         appender.cubic_to(iter[0], iter[1], iter[2]);
//!         iter.advance<BL_PATH_CMD_CUBIC>();
//!         break;
//!       case BL_PATH_CMD_CLOSE:
//!         appender.close();
//!         iter.advance<BL_PATH_CMD_CLOSE>();
//!         break;
//!       default:
//!         <error>;
//!     }
//!   }
//!
//!   appender.done(dst);
//!   return BL_SUCCESS;
//! }
//! ```
class PathAppender {
public:
  // Internal struct that represents a single command and prevents compile thinking that it may alias.
  struct Cmd { uint8_t value; };

  BLPoint* _vertex_ptr {};
  Cmd* _control_ptr {};

  BL_INLINE_NODEBUG PathAppender() noexcept = default;

  BL_INLINE_NODEBUG void reset() noexcept {
    _vertex_ptr = nullptr;
    _control_ptr = nullptr;
  }

  BL_INLINE_NODEBUG bool is_empty() const noexcept {
    return _vertex_ptr == nullptr;
  }

  BL_INLINE_NODEBUG size_t remaining_byte_size() const noexcept {
    return uintptr_t(_control_ptr) - uintptr_t(_vertex_ptr);
  }

  BL_INLINE void begin_append_to_data(BLPoint* vertex_ptr, uint8_t* control_ptr) noexcept {
    _vertex_ptr = vertex_ptr;
    _control_ptr = reinterpret_cast<Cmd*>(control_ptr);
  }

  BL_INLINE void begin_append_to_mutable_path(BLPathCore* dst) noexcept {
    BL_ASSERT(ObjectLayer::is_object_mutable(dst));

    uint8_t* data_ptr = ObjectLayer::get_data<uint8_t>(dst);
    _vertex_ptr = axl::offset_ptr<BLPoint>(data_ptr, PathDetail::get_vertex_data_size(dst));
    _control_ptr = reinterpret_cast<Cmd*>(data_ptr + ObjectLayer::get_capacity(dst) - PathDetail::get_control_data_size(dst));
    PathDetail::clear_flags(dst);
  }

  BL_INLINE BLResult begin_append(BLPathCore* dst, size_t append_byte_size) noexcept {
    BL_PROPAGATE(bl_path_reserve_byte_size_for_append(dst, append_byte_size));
    begin_append_to_mutable_path(dst);

    BL_ASSERT(remaining_byte_size() >= append_byte_size);
    return BL_SUCCESS;
  }

  BL_INLINE void sync(BLPathCore* dst) noexcept {
    uint8_t* data = ObjectLayer::get_data<uint8_t>(dst);
    size_t vertex_data_size = axl::ptr_byte_diff(data, _vertex_ptr);
    size_t control_data_size = axl::ptr_byte_diff(_control_ptr, data + ObjectLayer::get_capacity(dst));

    PathDetail::set_data_sizes(dst, vertex_data_size, control_data_size);
  }

  BL_INLINE BLResult ensure(BLPathCore* dst, size_t byte_size) noexcept {
    if (BL_LIKELY(remaining_byte_size() >= byte_size)) {
      return BL_SUCCESS;
    }
    else {
      sync(dst);
      return begin_append(dst, byte_size);
    }
  }

  BL_INLINE void done(BLPathCore* dst) noexcept {
    sync(dst);
    reset();
  }

  BL_INLINE_NODEBUG BLPathCmd prev_command() const noexcept { return BLPathCmd(_control_ptr[0].value); }
  BL_INLINE_NODEBUG BLPoint& prev_vertex() const noexcept { return _vertex_ptr[-1]; }

  BL_INLINE_NODEBUG BLPathCmd prev_prev_command() const noexcept { return BLPathCmd(_control_ptr[1].value); }
  BL_INLINE_NODEBUG BLPoint& prev_prev_vertex() const noexcept { return _vertex_ptr[-2]; }

  BL_INLINE_NODEBUG void set_prev_vertex(const BLPoint& pt) const noexcept { _vertex_ptr[-1] = pt; }

  BL_INLINE_NODEBUG void _advance_control_ptr(size_t n) noexcept { _control_ptr -= n; }
  BL_INLINE_NODEBUG void _advance_vertex_ptr(size_t n) noexcept { _vertex_ptr += n; }

  BL_INLINE void _advance(size_t vertex_count, size_t control_size) noexcept {
    BL_ASSERT(remaining_byte_size() >= vertex_count * sizeof(BLPoint) + control_size);

    _advance_vertex_ptr(vertex_count);
    _advance_control_ptr(control_size);
  }

  BL_INLINE void _revert(size_t vertex_count, size_t control_size) noexcept {
    _vertex_ptr -= vertex_count;
    _control_ptr += control_size;
  }

  BL_INLINE void _revert_if_line() noexcept {
    size_t is_line = prev_command() == BL_PATH_CMD_LINE;
    _revert(is_line, is_line);
  }

  BL_INLINE void _append_simple_command(BLPathCmd cmd) noexcept {
    _control_ptr--;
    _control_ptr[0].value = uint8_t(cmd);
  }

  template<typename Command, typename Condition>
  BL_INLINE void _append_simple_command_if(Command cmd, Condition condition) noexcept {
    _control_ptr[-1].value = uint8_t(cmd);
    _control_ptr -= size_t(condition);
  }

  template<typename T>
  BL_INLINE void _append_multiple_commands(T commands) noexcept {
    _control_ptr -= sizeof(T);
    axl::storeu<T>(_control_ptr, commands);
  }

  BL_INLINE void _append_conic_weight_and_trailing_command(double weight) noexcept {
    _control_ptr -= 9;
    axl::storeu(_control_ptr, weight);
    _control_ptr[8].value = uint8_t(BL_PATH_CMD_CONIC);
  }

  BL_INLINE void _append_conic_command(double weight) noexcept {
    _control_ptr -= 10;
    PathDetail::store_conic_command(reinterpret_cast<uint8_t*>(_control_ptr), weight);
  }

  BL_INLINE void _append_vertex_1x(double x0, double y0) noexcept {
    _vertex_ptr[0] = BLPoint(x0, y0);
    _vertex_ptr++;
  }

  BL_INLINE void _append_vertex_2x(double x0, double y0, double x1, double y1) noexcept {
    _vertex_ptr[0] = BLPoint(x0, y0);
    _vertex_ptr[1] = BLPoint(x1, y1);
    _vertex_ptr += 2;
  }

  BL_INLINE void _append_vertex_3x(double x0, double y0, double x1, double y1, double x2, double y2) noexcept {
    _vertex_ptr[0] = BLPoint(x0, y0);
    _vertex_ptr[1] = BLPoint(x1, y1);
    _vertex_ptr[2] = BLPoint(x2, y2);
    _vertex_ptr += 3;
  }

  BL_INLINE void _append_vertex_4x(double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3) noexcept {
    _vertex_ptr[0] = BLPoint(x0, y0);
    _vertex_ptr[1] = BLPoint(x1, y1);
    _vertex_ptr[2] = BLPoint(x2, y2);
    _vertex_ptr[3] = BLPoint(x3, y3);
    _vertex_ptr += 4;
  }

  BL_INLINE void _append_vertex_1x(BLPoint p0) noexcept {
    _vertex_ptr[0] = p0;
    _vertex_ptr++;
  }

  BL_INLINE void _append_vertex_2x(BLPoint p0, BLPoint p1) noexcept {
    _vertex_ptr[0] = p0;
    _vertex_ptr[1] = p1;
    _vertex_ptr += 2;
  }

  BL_INLINE void _append_vertex_3x(BLPoint p0, BLPoint p1, BLPoint p2) noexcept {
    _vertex_ptr[0] = p0;
    _vertex_ptr[1] = p1;
    _vertex_ptr[2] = p2;
    _vertex_ptr += 3;
  }

  BL_INLINE void _append_vertex_4x(BLPoint p0, BLPoint p1, BLPoint p2, BLPoint p3) noexcept {
    _vertex_ptr[0] = p0;
    _vertex_ptr[1] = p1;
    _vertex_ptr[2] = p2;
    _vertex_ptr[3] = p3;
    _vertex_ptr += 4;
  }

  BL_INLINE void move_to(BLPoint p0) noexcept { move_to(p0.x, p0.y); }
  BL_INLINE void move_to(BLPointI p0) noexcept { move_to(p0.x, p0.y); }

  BL_INLINE void move_to(double x0, double y0) noexcept {
    BL_ASSERT(remaining_byte_size() >= BLPath::MoveTo::kByteSize);

    _append_simple_command(BL_PATH_CMD_MOVE);
    _append_vertex_1x(x0, y0);
  }

  BL_INLINE void line_to(BLPoint p0) noexcept { line_to(p0.x, p0.y); }
  BL_INLINE void line_to(BLPointI p0) noexcept { line_to(p0.x, p0.y); }

  BL_INLINE void line_to(double x0, double y0) noexcept {
    BL_ASSERT(remaining_byte_size() >= BLPath::LineTo::kByteSize);

    _append_simple_command(BL_PATH_CMD_LINE);
    _append_vertex_1x(x0, y0);
  }


  BL_INLINE void poly_to(const BLPoint* pts, size_t count) noexcept {
    BL_ASSERT(remaining_byte_size() >= count * BLPath::LineTo::kByteSize);

    size_t i = count;
    while (i >= 4) {
      _append_vertex_4x(pts[0], pts[1], pts[2], pts[3]);
      _append_multiple_commands(axl::repeat_byte<uint32_t>(BL_PATH_CMD_LINE));

      pts += 4;
      i -= 4;
    }

    while (i) {
      _append_vertex_1x(pts[0]);
      _append_simple_command(BL_PATH_CMD_LINE);

      pts++;
      i--;
    }
  }

  BL_INLINE void vertex_to(BLPathCmd cmd, double x0, double y0) noexcept {
    BL_ASSERT(remaining_byte_size() >= BLPath::LineTo::kByteSize);

    _append_simple_command(cmd);
    _append_vertex_1x(x0, y0);
  }

  BL_INLINE void vertex_to(BLPathCmd cmd, BLPoint p1) noexcept { vertex_to(cmd, p1.x, p1.y); }
  BL_INLINE void vertex_to(BLPathCmd cmd, BLPointI p1) noexcept { vertex_to(cmd, p1.x, p1.y); }

  BL_INLINE void quad_to(double x0, double y0, double x1, double y1) noexcept {
    BL_ASSERT(remaining_byte_size() >= BLPath::QuadTo::kByteSize);

    _append_simple_command(BL_PATH_CMD_QUAD);
    _append_vertex_2x(x0, y0, x1, y1);
  }

  BL_INLINE void quad_to(BLPoint p1, BLPoint p2) noexcept {
    quad_to(p1.x, p1.y, p2.x, p2.y);
  }

  BL_INLINE void conic_to(double x0, double y0, double x1, double y1, double w) noexcept {
    BL_ASSERT(remaining_byte_size() >= BLPath::ConicTo::kByteSize);

    _append_conic_command(w);
    _append_vertex_2x(x0, y0, x1, y1);
  }

  BL_INLINE void conic_to(BLPoint p1, BLPoint p2, double w) noexcept {
    conic_to(p1.x, p1.y, p2.x, p2.y, w);
  }

  BL_INLINE void cubic_to(double x0, double y0, double x1, double y1, double x2, double y2) noexcept {
    BL_ASSERT(remaining_byte_size() >= BLPath::CubicTo::kByteSize);

    _append_simple_command(BL_PATH_CMD_CUBIC);
    _append_vertex_3x(x0, y0, x1, y1, x2, y2);
  }

 BL_INLINE void cubic_to(BLPoint p1, BLPoint p2, BLPoint p3) noexcept {
    return cubic_to(p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
  }

  BL_INLINE void arc_quadrant_to(double x1, double y1, double x2, double y2) noexcept {
    conic_to(x1, y1, x2, y2, Math::kSQRT_0p5);
  }

  BL_INLINE void arc_quadrant_to(BLPoint p1, BLPoint p2) noexcept {
    conic_to(p1.x, p1.y, p2.x, p2.y, Math::kSQRT_0p5);
  }

  BL_INLINE void close() noexcept {
    BL_ASSERT(remaining_byte_size() >= BLPath::Close::kByteSize);

    _control_ptr--;
    _control_ptr[0].value = uint8_t(BL_PATH_CMD_CLOSE);
  }

  BL_INLINE void close_if_open() noexcept {
    if (prev_command() != BL_PATH_CMD_CLOSE) {
      close();
    }
  }

  BL_INLINE void add_box(double x0, double y0, double x1, double y1, BLGeometryDirection dir) noexcept {
    BL_ASSERT(remaining_byte_size() >= BLPath::LineTo::kByteSize * 4u + BLPath::Close::kByteSize);

    _control_ptr -= 5;
    _vertex_ptr[0] = BLPoint(x0, y0);
    _vertex_ptr[1] = BLPoint(x1, y0);
    _vertex_ptr[2] = BLPoint(x1, y1);
    _vertex_ptr[3] = BLPoint(x0, y1);

    if (dir != BL_GEOMETRY_DIRECTION_CW) {
      _vertex_ptr[1] = BLPoint(x0, y1);
      _vertex_ptr[3] = BLPoint(x1, y0);
    }

    _control_ptr[4].value = uint8_t(BL_PATH_CMD_MOVE);
    _control_ptr[3].value = uint8_t(BL_PATH_CMD_LINE);
    _control_ptr[2].value = uint8_t(BL_PATH_CMD_LINE);
    _control_ptr[1].value = uint8_t(BL_PATH_CMD_LINE);
    _control_ptr[0].value = uint8_t(BL_PATH_CMD_CLOSE);
    _vertex_ptr += 4;
  }

  BL_INLINE void add_box_cw(double x0, double y0, double x1, double y1) noexcept {
    add_box(x0, y0, x1, y1, BL_GEOMETRY_DIRECTION_CW);
  }

  BL_INLINE void add_box_ccw(double x0, double y0, double x1, double y1) noexcept {
    add_box(x0, y0, x1, y1, BL_GEOMETRY_DIRECTION_CCW);
  }

  BL_INLINE void add_rect(double x, double y, double w, double h, BLGeometryDirection dir) noexcept {
    add_box(x, y, x + w, y + h, dir);
  }

  BL_INLINE void add_rect_cw(double x, double y, double w, double h) noexcept {
    add_box(x, y, x + w, y + h, BL_GEOMETRY_DIRECTION_CW);
  }

  BL_INLINE void add_rect_ccw(double x, double y, double w, double h) noexcept {
    add_box(x, y, x + w, y + h, BL_GEOMETRY_DIRECTION_CCW);
  }

  BL_INLINE void append_path(BLPathView view) noexcept {
    size_t vertex_count = view.vertex_count;
    size_t control_size = view.control_data_size;
    BL_ASSERT(remaining_byte_size() >= vertex_count * sizeof(BLPoint) + control_size);

    PathDetail::copy_vertex_data(_vertex_ptr, view.vertex_data, vertex_count);
    _vertex_ptr += vertex_count;
    _control_ptr -= control_size;
    memcpy(_control_ptr, view.control_data_rev - control_size, control_size);
  }

  BL_INLINE void join_path(BLPathView src) noexcept {
    if (!src.control_data_size) {
      return;
    }

    // Initial vertex (either MOVE or LINE). If the initial vertex matches the
    // the last vertex in `dst` it won't be emitted it as it would be redundant.
    bool is_closed = prev_command() == BL_PATH_CMD_CLOSE;
    if (axl::bool_or(is_closed, prev_vertex() != src.vertex_data[0])) {
      _append_vertex_1x(src.vertex_data[0]);
      _append_simple_command(is_closed ? BL_PATH_CMD_MOVE : BL_PATH_CMD_LINE);
    }

    src.control_data_rev--;
    src.control_data_size--;
    src.vertex_data++;
    src.vertex_count--;

    append_path(src);
  }

  BL_INLINE BLResult append_reversed_path(BLPathView src) noexcept {
    BL_ASSERT(remaining_byte_size() >= src.vertex_count * sizeof(BLPoint) + src.control_data_size);

    const uint8_t* src_control_ptr = src.control_data_rev - src.control_data_size;
    const uint8_t* src_control_terminate = src.control_data_rev;
    const uint8_t* src_control_terminate_opt = src.control_data_rev - sizeof(uintptr_t);

    Cmd* dst_control_ptr = _control_ptr;

    while (src_control_ptr < src_control_terminate) {
      bool figure_is_closed = (BLPathCmd(src_control_ptr[0]) == BL_PATH_CMD_CLOSE);
      src_control_ptr += size_t(figure_is_closed);
      *--dst_control_ptr = Cmd{BL_PATH_CMD_MOVE};

      for (;;) {
        if (src_control_ptr >= src_control_terminate) {
          // PATH CORRUPTION: End of data, but expected to find MOVE_TO first.
          return bl_make_error(BL_ERROR_CORRUPTED_PATH_DATA);
        }

        BLPathCmd command = BLPathCmd(src_control_ptr[0]);
        if (BL_UNLIKELY(PathDetail::is_begin_or_conic_cmd(command))) {
          if (command == BL_PATH_CMD_CONIC) {
            dst_control_ptr -= BLPath::ConicTo::kControlSize;
            PathDetail::store_conic_command(reinterpret_cast<uint8_t*>(dst_control_ptr), axl::loadu<double>(src_control_ptr + 1));
            src_control_ptr += BLPath::ConicTo::kControlSize;
          }
          else if (command == BL_PATH_CMD_MOVE) {
            // End of the current figure - advance past the MOVE so the outer loop can either terminate or continue
            // with the next (preceding) figure. Without this advance the outer `while` would re-read the same MOVE
            // command indefinitely and overrun the destination buffer.
            src_control_ptr++;
            break;
          }
          else {
            // PATH CORRUPTION: Unexpected command found during iteration.
            return bl_make_error(BL_ERROR_CORRUPTED_PATH_DATA);
          }
        }
        else {
          *--dst_control_ptr = Cmd{uint8_t(command)};
          src_control_ptr++;

          // OPTIMIZATION: Do multiple commands at once if they are not special.
          if constexpr (axl::kHasUnalignedMemoryAccess) {
            while (src_control_ptr <= src_control_terminate_opt) {
              uintptr_t commands = axl::loadu<uintptr_t>(src_control_ptr);
              constexpr uintptr_t predicate = axl::repeat_byte<uintptr_t>(0x3);
              if (commands & predicate) {
                break;
              }

              dst_control_ptr -= sizeof(uintptr_t);
              src_control_ptr += sizeof(uintptr_t);
              axl::storeu(dst_control_ptr, axl::byteswap(commands));
            }
          }
        }
      }

      if (figure_is_closed) {
        *--dst_control_ptr = Cmd{BL_PATH_CMD_CLOSE};
      }
    }

    _control_ptr = dst_control_ptr;
    PathDetail::copy_vertex_data_reversed(_vertex_ptr, src.vertex_data, src.vertex_count);

    _vertex_ptr += src.vertex_count;
    return BL_SUCCESS;
  }

  //! Appends a content of path `src` in reverse order, joining it with the current path's data (i.e. continuing
  //! where the appender ends instead of inserting an initial move). The reversed content begins at `src`'s last
  //! vertex:
  //!
  //!   - If the last sub-path is closed the reversed sub-path starts a new one with MOVE (regular append operation).
  //!   - Otherwise it continues the sub-path. The starting vertex is emitted as a LINE, unless it coincides with
  //!     the last point, in which case it is omitted (the following segment continues from the existing point).
  //!   - A trailing CLOSE in `src` produces a CLOSE as well.
  //!
  //! The caller must have reserved enough storage (at least the byte size of `src` plus one additional MOVE).
  BL_INLINE BLResult join_reversed_path(BLPathView src) noexcept {
    BL_ASSERT(remaining_byte_size() >= src.vertex_count * sizeof(BLPoint) + src.control_data_size + BLPath::MoveTo::kByteSize);

    if (!src.control_data_size) {
      return BL_SUCCESS;
    }

    using Cmd = PathAppender::Cmd;

    // The reversed figure's first point is `src`'s last vertex.
    bool dst_is_closed = (prev_command() == BL_PATH_CMD_CLOSE);
    BLPoint first_point = src.vertex_data[src.vertex_count - 1u];

    // Omit the redundant first vertex if we are continuing the contour and it coincides with `dst`'s end point.
    bool omit_first = !dst_is_closed && prev_vertex() == first_point;

    const uint8_t* src_control_ptr = src.control_data_rev - src.control_data_size;  // Last forward command (lowest address).
    const uint8_t* src_control_terminate = src.control_data_rev;                    // One past the first forward command.

    // A trailing CLOSE is the last forward command (lowest address) - skip it now and re-emit it at the end.
    bool has_close = (BLPathCmd(src_control_ptr[0]) == BL_PATH_CMD_CLOSE);
    src_control_ptr += size_t(has_close);

    Cmd* dst_control_ptr = _control_ptr;

    // Initial command that continues the contour (or opens a new sub-figure), unless the first vertex is omitted.
    if (!omit_first) {
      *--dst_control_ptr = Cmd{uint8_t(dst_is_closed ? BL_PATH_CMD_MOVE : BL_PATH_CMD_LINE)};
    }

    // Reverse the figure's segment commands (terminates on the figure's MOVE).
    for (;;) {
      if (src_control_ptr >= src_control_terminate) {
        // PATH CORRUPTION: End of data, but expected to find a MOVE first.
        return bl_make_error(BL_ERROR_CORRUPTED_PATH_DATA);
      }

      BLPathCmd command = BLPathCmd(src_control_ptr[0]);
      if (BL_UNLIKELY(PathDetail::is_begin_or_conic_cmd(command))) {
        if (command == BL_PATH_CMD_CONIC) {
          dst_control_ptr -= BLPath::ConicTo::kControlSize;
          PathDetail::store_conic_command(reinterpret_cast<uint8_t*>(dst_control_ptr), axl::loadu<double>(src_control_ptr + 1));
          src_control_ptr += BLPath::ConicTo::kControlSize;
        }
        else if (command == BL_PATH_CMD_MOVE) {
          // Beginning of the figure reached - the reversal of this figure is complete.
          break;
        }
        else {
          // PATH CORRUPTION: Unexpected command found during iteration.
          return bl_make_error(BL_ERROR_CORRUPTED_PATH_DATA);
        }
      }
      else {
        *--dst_control_ptr = Cmd{uint8_t(command)};
        src_control_ptr++;
      }
    }

    // Re-emit CLOSE if the figure was closed.
    if (has_close) {
      *--dst_control_ptr = Cmd{uint8_t(BL_PATH_CMD_CLOSE)};
    }

    _control_ptr = dst_control_ptr;

    // Copy the vertices reversed, optionally skipping the omitted first vertex.
    size_t vertex_count = src.vertex_count - size_t(omit_first);
    PathDetail::copy_vertex_data_reversed(_vertex_ptr, src.vertex_data, vertex_count);
    _vertex_ptr += vertex_count;

    return BL_SUCCESS;
  }
};

//! \}

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_CORE_PATH_P_H_INCLUDED
