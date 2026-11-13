// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_PATH_H_INCLUDED
#define BLEND2D_CORE_PATH_H_INCLUDED

#include <blend2d/core/array.h>
#include <blend2d/core/geometry.h>
#include <blend2d/core/object_layer.h>
#include <blend2d/core/span.h>
#include <blend2d/core/stroke_options.h>

//! \addtogroup bl_geometry
//! \{

//! \name BLPath - Constants
//! \{

//! Path command/segment types stored in path control data.
//!
//! The convention:
//!
//! ```
//!         BLPathCmd
//! Close [0000|00|10] (10 - sub-path end)
//! Move  [0000|01|10] (10 - sub-path end)
//! Line  [0000|01|00]
//! Quad  [0000|10|00]
//! Conic [0000|10|01] (01 - conic)
//! Cubic [0000|11|00]
//! ```
//!
//! When path command is shifted two bits right the remaining value describes the number of vertices.
BL_DEFINE_ENUM(BLPathCmd) {
  //! Close command (closes a figure).
  BL_PATH_CMD_CLOSE = 0x02,
  //! Move-to command (starts a new figure and closes the previous figure if not preceeded with BL_PATH_CMD_CLOSE).
  BL_PATH_CMD_MOVE = 0x06,
  //! Line-to command.
  BL_PATH_CMD_LINE = 0x04,
  //! Quad-to command.
  BL_PATH_CMD_QUAD = 0x08,
  //! Conic-to command.
  BL_PATH_CMD_CONIC = 0x09,
  //! Cubic-to command.
  BL_PATH_CMD_CUBIC = 0x0C,

  //! Maximum value of `BLPathCmd`.
  BL_PATH_CMD_MAX_VALUE = BL_PATH_CMD_CUBIC

  BL_FORCE_ENUM_UINT32(BL_PATH_CMD)
};

//! Path flags.
BL_DEFINE_ENUM(BLPathFlags) {
  //! No flags.
  BL_PATH_NO_FLAGS = 0u,
  //! Path is empty (no commands or close commands only).
  BL_PATH_FLAG_EMPTY = 0x00000001u,
  //! Path contains multiple figures.
  BL_PATH_FLAG_MULTIPLE = 0x00000002u,
  //! Path contains one or more quad curves.
  BL_PATH_FLAG_QUADS = 0x00000004u,
  //! Path contains one or more conic curves.
  BL_PATH_FLAG_CONICS = 0x00000008u,
  //! Path contains one or more cubic curves.
  BL_PATH_FLAG_CUBICS = 0x00000010u,
  //! Path is invalid.
  BL_PATH_FLAG_INVALID = 0x40000000u,
  //! Flags are dirty (not reflecting the current status).
  BL_PATH_FLAG_DIRTY = 0x80000000u

  BL_FORCE_ENUM_UINT32(BL_PATH_FLAG)
};

//! Mode that specifies how curves are approximated to line segments.
BL_DEFINE_ENUM(BLFlattenMode) {
  //! Use default mode (decided by Blend2D).
  BL_FLATTEN_MODE_DEFAULT = 0,
  //! Recursive subdivision flattening.
  BL_FLATTEN_MODE_RECURSIVE = 1,

  //! Maximum value of `BLFlattenMode`.
  BL_FLATTEN_MODE_MAX_VALUE = 1

  BL_FORCE_ENUM_UINT32(BL_FLATTEN_MODE)
};

//! Flags used by \ref BLPath::fit_to() that control how a path is fitted into a rectangle.
//!
//! The flags combine three independent groups plus optional modifiers:
//!   - a scaling policy (\ref BL_PATH_FIT_SCALE_FILL, \ref BL_PATH_FIT_SCALE_CONTAIN, \ref BL_PATH_FIT_SCALE_COVER),
//!   - a horizontal alignment (\ref BL_PATH_FIT_ALIGN_X_MID, \ref BL_PATH_FIT_ALIGN_X_MIN, \ref BL_PATH_FIT_ALIGN_X_MAX),
//!   - a vertical alignment (\ref BL_PATH_FIT_ALIGN_Y_MID, \ref BL_PATH_FIT_ALIGN_Y_MIN, \ref BL_PATH_FIT_ALIGN_Y_MAX).
//!
//! Alignment only has an effect when the aspect ratio is preserved (\ref BL_PATH_FIT_SCALE_CONTAIN or
//! \ref BL_PATH_FIT_SCALE_COVER); \ref BL_PATH_FIT_SCALE_FILL always fills the whole rectangle and leaves
//! no free space to align within.
BL_DEFINE_ENUM(BLPathFitFlags) {
  //! No flags - same as \ref BL_PATH_FIT_SCALE_FILL with centered alignment (default, keeps the legacy behavior).
  BL_PATH_FIT_NO_FLAGS = 0u,

  //! Scale each axis independently so the path exactly fills the rectangle (default, aspect ratio is NOT preserved).
  BL_PATH_FIT_SCALE_FILL = 0x00000000u,
  //! Scale uniformly so the whole path fits inside the rectangle (aspect ratio preserved, may leave free margins).
  BL_PATH_FIT_SCALE_CONTAIN = 0x00000001u,
  //! Scale uniformly so the path covers the whole rectangle (aspect ratio preserved, path may exceed the rectangle).
  BL_PATH_FIT_SCALE_COVER = 0x00000002u,
  //! Mask of scaling policy bits.
  BL_PATH_FIT_SCALE_MASK = 0x00000003u,

  //! Align the path to the center along X (default).
  BL_PATH_FIT_ALIGN_X_MID = 0x00000000u,
  //! Align the path to the minimum X coordinate of the rectangle.
  BL_PATH_FIT_ALIGN_X_MIN = 0x00000004u,
  //! Align the path to the maximum X coordinate of the rectangle.
  BL_PATH_FIT_ALIGN_X_MAX = 0x00000008u,
  //! Mask of horizontal alignment bits.
  BL_PATH_FIT_ALIGN_X_MASK = 0x0000000Cu,

  //! Align the path to the center along Y (default).
  BL_PATH_FIT_ALIGN_Y_MID = 0x00000000u,
  //! Align the path to the minimum Y coordinate of the rectangle.
  BL_PATH_FIT_ALIGN_Y_MIN = 0x00000010u,
  //! Align the path to the maximum Y coordinate of the rectangle.
  BL_PATH_FIT_ALIGN_Y_MAX = 0x00000020u,
  //! Mask of vertical alignment bits.
  BL_PATH_FIT_ALIGN_Y_MASK = 0x00000030u,

  //! Never scale up - clamp every scale factor to a maximum of 1.0 (keeps a small path at its original size).
  //!
  //! Pairs naturally with \ref BL_PATH_FIT_SCALE_CONTAIN (equivalent to CSS `object-fit: scale-down`). When combined
  //! with \ref BL_PATH_FIT_SCALE_COVER the rectangle may end up not fully covered, as covering a smaller path would
  //! otherwise require scaling it up.
  BL_PATH_FIT_SHRINK_ONLY = 0x00000040u

  BL_FORCE_ENUM_UINT32(BL_PATH_FIT)
};

//! \}

//! \name BLPath - Structs
//! \{

//! Path view provides pointers to vertex and command data along with their size.
struct BLPathView {
  const BLPoint* vertex_data;
  const uint8_t* control_data_rev;

  size_t vertex_count;
  size_t control_data_size;
};

//! A position in a path that takes into account both control and vertex offsets.
struct BLPathPosition {
  size_t vertex_offset;
  size_t control_offset;
};

//! A region in a path that usually describes a sub-path, but can describe any part of a path.
struct BLPathRegion {
  //! Start position in region (inclusive).
  BLPathPosition start;
  //! End position in region (exclusive).
  BLPathPosition end;
};

//! \}

//! \name BLPath - Sinks
//! \{

//! Path sink - an optional callback that can be used to consume a processed path.
typedef BLResult (BL_CDECL* BLPathSinkFunc)(BLPathCore* path, const void* info, void* user_data) BL_NOEXCEPT_C;

//! Dashed-path sink - an optional callback that can be used to consume RAW data that is generated by dashing context.
//!
//! Dasher uses sink internally to push each closed figure to the consumer via `path`, which is the same as the path
//! passed to the dasher via arguments. User then can process the path and clear it in case that the path has been
//! processed. If the path is not cleared, the dasher appends another figure into it.
typedef BLResult (BL_CDECL* BLPathDashSinkFunc)(BLPathCore* path, void* user_data) BL_NOEXCEPT_C;

//! Stroked-path sink - sink that is used by path stroker. This sink consumes both `a` and `b` offsets of the path.
//! The sink will be called for each figure and is responsible for joining these paths. If the paths are not closed
//! then the sink must insert start cap, then join `b`, and then insert end cap.
//!
//! The sink must also clean up the paths as this is not done by the offsetter. The reason is that in case the `a` path
//! is the output path you can just keep it and insert `b` path into it (clearing only `b` path after each call).
typedef BLResult (BL_CDECL* BLPathStrokeSinkFunc)(BLPathCore* a, BLPathCore* b, BLPathCore* c, const BLPathRegion* input_region, void* user_data) BL_NOEXCEPT_C;

//! \}

//! \name BLPath - Globals
//!
//! 2D path functionality is provided by \ref BLPathCore in C API and wrapped by \ref BLPath in C++ API.
//!
//! \{
BL_EXTERN_C_BEGIN

//! Default approximation options used by Blend2D.
extern BL_API const BLApproximationOptions bl_default_approximation_options;

BL_EXTERN_C_END

//! \}
//! \}

//! \addtogroup bl_c_api
//! \{

//! \name BLPath - C API
//! \{

//! \cond INTERNAL
//! \name BLPath - Internals
//! \{

#if defined(__cplusplus)
//! 2D vector path [Impl].
struct BLPathImpl {};
#endif // __cplusplus

//! \}
//! \endcond

//! 2D vector path [C API].
struct BLPathCore BL_INHERITS_FROM(BLObjectCore) {
  BL_DEFINE_OBJECT(BLPath, bl::ObjectLayer::ObjectInfo<
    BL_OBJECT_TYPE_PATH,
    bl::ObjectLayer::ObjectMode::kDyn,
    bl::ObjectLayer::ObjectLayout::kPath>
  )

#ifdef __cplusplus
  using Vertex = BLPoint;
#endif
};

BL_EXTERN_C_BEGIN

BL_API BLResult BL_CDECL bl_path_init(BLPathCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_init_move(BLPathCore* self, BLPathCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_init_weak(BLPathCore* self, const BLPathCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_init_uninitialized(BLPathCore* self, size_t vertex_count, size_t control_data_size, BLPoint** vertex_data_out, uint8_t** control_data_rev_out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_destroy(BLPathCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_reset(BLPathCore* self) BL_NOEXCEPT_C;
BL_API size_t BL_CDECL bl_path_get_byte_size(const BLPathCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API size_t BL_CDECL bl_path_get_byte_capacity(const BLPathCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API size_t BL_CDECL bl_path_get_vertex_count(const BLPathCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API size_t BL_CDECL bl_path_get_vertex_data_size(const BLPathCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API size_t BL_CDECL bl_path_get_control_data_size(const BLPathCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API const BLPoint* BL_CDECL bl_path_get_vertex_data(const BLPathCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API const uint8_t* BL_CDECL bl_path_get_control_data_rev(const BLPathCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API BLResult BL_CDECL bl_path_clear(BLPathCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_shrink(BLPathCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_reserve_byte_size(BLPathCore* self, size_t byte_size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_reserve_byte_size_for_append(BLPathCore* self, size_t append_byte_size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_assign_move(BLPathCore* self, BLPathCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_assign_weak(BLPathCore* self, const BLPathCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_assign_deep(BLPathCore* self, const BLPathCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_set_vertex_at(BLPathCore* self, size_t vertex_index, double x, double y) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_set_weight_at(BLPathCore* self, size_t control_index, double w) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_move_to(BLPathCore* self, double x0, double y0) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_line_to(BLPathCore* self, double x1, double y1) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_poly_to(BLPathCore* self, const BLPoint* poly, size_t count) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_quad_to(BLPathCore* self, double x1, double y1, double x2, double y2) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_conic_to(BLPathCore* self, double x1, double y1, double x2, double y2, double w) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_cubic_to(BLPathCore* self, double x1, double y1, double x2, double y2, double x3, double y3) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_smooth_quad_to(BLPathCore* self, double x2, double y2) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_smooth_cubic_to(BLPathCore* self, double x2, double y2, double x3, double y3) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_arc_to(BLPathCore* self, double x, double y, double rx, double ry, double start, double sweep, bool force_move_to) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_arc_quadrant_to(BLPathCore* self, double x1, double y1, double x2, double y2) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_elliptic_arc_to(BLPathCore* self, double rx, double ry, double x_axis_rotation, bool large_arc_flag, bool sweep_flag, double x1, double y1) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_close(BLPathCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_add_uninitialized(BLPathCore* self, size_t append_vertex_count, size_t append_control_data_size, BLPoint** vertex_data_out, uint8_t** control_data_rev_out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_add_geometry(BLPathCore* self, BLGeometryType geometry_type, const void* geometry_data, const BLMatrix2D* m, BLGeometryDirection dir) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_add_box_i(BLPathCore* self, const BLBoxI* box, BLGeometryDirection dir) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_add_box_d(BLPathCore* self, const BLBox* box, BLGeometryDirection dir) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_add_rect_i(BLPathCore* self, const BLRectI* rect, BLGeometryDirection dir) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_add_rect_d(BLPathCore* self, const BLRect* rect, BLGeometryDirection dir) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_add_path(BLPathCore* self, const BLPathCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_add_translated_path(BLPathCore* self, const BLPathCore* other, const BLPoint* p) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_add_transformed_path(BLPathCore* self, const BLPathCore* other, const BLMatrix2D* transform) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_add_reversed_path(BLPathCore* self, const BLPathCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_add_reversed_transformed_path(BLPathCore* self, const BLPathCore* other, const BLMatrix2D* transform) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_add_stroked_path(BLPathCore* self, const BLPathCore* other, const BLStrokeOptionsCore* options, const BLApproximationOptions* approx) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_add_vertex_outline_of_path(BLPathCore* self, const BLPathCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_translate(BLPathCore* self, const BLPoint* p) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_transform(BLPathCore* self, const BLMatrix2D* m) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_fit_to(BLPathCore* self, const BLRect* rect, BLPathFitFlags fit_flags) BL_NOEXCEPT_C;
BL_API bool     BL_CDECL bl_path_equals(const BLPathCore* a, const BLPathCore* b) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_get_info_flags(const BLPathCore* self, uint32_t* flags_out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_get_vertex_bounding_box(const BLPathCore* self, BLBox* box_out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_get_ink_bounding_box(const BLPathCore* self, BLBox* box_out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_get_last_vertex(const BLPathCore* self, BLPoint* vtgx_out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_path_get_closest_vertex(const BLPathCore* self, const BLPoint* p, double max_distance, size_t* index_out, double* distance_out) BL_NOEXCEPT_C;
BL_API BLHitTest BL_CDECL bl_path_hit_test(const BLPathCore* self, const BLPoint* p, BLFillRule fill_rule) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL bl_path_stroke_to_sink(
  const BLPathCore* self,
  const BLStrokeOptionsCore* stroke_options,
  const BLApproximationOptions* approximation_options,
  BLPathCore* a,
  BLPathCore* b,
  BLPathCore* c,
  BLPathStrokeSinkFunc sink, void* user_data) BL_NOEXCEPT_C;

BL_EXTERN_C_END

//! \}
//! \}

//! \addtogroup bl_geometry
//! \{

//! \name BLPath - C++ API
//! \{
#ifdef __cplusplus

//! 2D vector path [C++ API].
class BLPath final : public BLPathCore {
public:
  //! \name Path Segments
  //! \{

  struct MoveTo {
    static constexpr size_t kVertexCount = 1;
    static constexpr size_t kControlSize = 1; // [MOVE]
    static constexpr size_t kByteSize = kVertexCount * sizeof(Vertex) + kControlSize;
    static constexpr BLPathCmd kPathCmd = BL_PATH_CMD_MOVE;

    double x0, y0;
  };

  struct LineTo {
    static constexpr size_t kVertexCount = 1;
    static constexpr size_t kControlSize = 1; // [LINE]
    static constexpr size_t kByteSize = kVertexCount * sizeof(Vertex) + kControlSize;
    static constexpr BLPathCmd kPathCmd = BL_PATH_CMD_LINE;

    double x0, y0;
  };

  struct QuadTo {
    static constexpr size_t kVertexCount = 2;
    static constexpr size_t kControlSize = 1; // [QUAD]
    static constexpr size_t kByteSize = kVertexCount * sizeof(Vertex) + kControlSize;
    static constexpr BLPathCmd kPathCmd = BL_PATH_CMD_QUAD;

    double x0, y0;
    double x1, y1;
  };

  struct ConicTo {
    static constexpr size_t kVertexCount = 2;
    static constexpr size_t kControlSize = 1 + 8 + 1; // [CONIC, WEIGHT, CONIC]
    static constexpr size_t kByteSize = kVertexCount * sizeof(Vertex) + kControlSize;
    static constexpr BLPathCmd kPathCmd = BL_PATH_CMD_CONIC;

    double x0, y0;
    double x1, y1;
    double w;
  };

  struct CubicTo {
    static constexpr size_t kVertexCount = 3;
    static constexpr size_t kControlSize = 1; // [CUBIC]
    static constexpr size_t kByteSize = kVertexCount * sizeof(Vertex) + kControlSize;
    static constexpr BLPathCmd kPathCmd = BL_PATH_CMD_CUBIC;

    double x0, y0;
    double x1, y1;
    double x2, y2;
  };

  struct Close {
    static constexpr size_t kVertexCount = 0;
    static constexpr size_t kControlSize = 1; // [CLOSE]
    static constexpr size_t kByteSize = kVertexCount * sizeof(Vertex) + kControlSize;
    static constexpr BLPathCmd kPathCmd = BL_PATH_CMD_CLOSE;
  };

  //! \}

  //! \cond INTERNAL
  static constexpr uint8_t kDefaultMetaFlags = BLObjectDetail::kMetaFlagD;

  template<typename... Args>
  static consteval size_t _vertex_count_of_segments() noexcept { return (Args::kVertexCount + ... + size_t(0)); }

  template<typename... Args>
  static consteval size_t _control_size_of_segments() noexcept { return (Args::kControlSize + ... + size_t(0)); }

  template<typename... Args>
  static BL_INLINE void _write_segments_to_path_data(Vertex* vertex_ptr, uint8_t* control_rev_ptr, Args&&... args) noexcept {
    auto store_vertex_data = [&](auto&& arg) {
      using T = std::remove_cvref_t<decltype(arg)>;
      if constexpr(T::kVertexCount >= 1) vertex_ptr[0] = Vertex(arg.x0, arg.y0);
      if constexpr(T::kVertexCount >= 2) vertex_ptr[1] = Vertex(arg.x1, arg.y1);
      if constexpr(T::kVertexCount == 3) vertex_ptr[2] = Vertex(arg.x2, arg.y2);
      vertex_ptr += T::kVertexCount;
    };
    (store_vertex_data(bl::Internal::forward<Args>(args)), ...);

    auto store_control_data= [&](auto&& arg) {
      using T = std::remove_cvref_t<decltype(arg)>;
      if constexpr(T::kControlSize == 1) {
        control_rev_ptr[-1] = static_cast<uint8_t>(T::kPathCmd);
      }
      else if constexpr(T::kControlSize == 10) {
        control_rev_ptr[-1] = static_cast<uint8_t>(T::kPathCmd);
        memcpy(control_rev_ptr - 1 - sizeof(double), &arg.w, sizeof(double));
        control_rev_ptr[-10] = static_cast<uint8_t>(T::kPathCmd);
      }
      control_rev_ptr -= T::kControlSize;
    };
    (store_control_data(bl::Internal::forward<Args>(args)), ...);
  }
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLPath() noexcept {
    bl_path_init(this);

    // Assume a default constructed BLPath.
    BL_ASSUME(_d._sso_data[BLObjectDetail::kMetaByteIndex] == kDefaultMetaFlags);
  }

  BL_INLINE_NODEBUG BLPath(BLPath&& other) noexcept {
    bl_path_init_move(this, &other);

    // Assume a default initialized `other`.
    BL_ASSUME(other._d._sso_data[BLObjectDetail::kMetaByteIndex] == kDefaultMetaFlags);
  }

  BL_INLINE_NODEBUG BLPath(const BLPath& other) noexcept {
    bl_path_init_weak(this, &other);
  }

  BL_INLINE_NODEBUG ~BLPath() noexcept {
    if (bl::ObjectLayer::requires_destroy(this)) {
      bl_path_destroy(this);
    }
  }

private:
  struct NoInit{};

  BL_INLINE_NODEBUG explicit BLPath(NoInit) noexcept { /* [[uninitialized]] */ }

public:

  template<typename... Args>
  static BL_INLINE BLPath make_from_segments(Args&&... args) noexcept {
    BLPath path(NoInit{});

    if constexpr (sizeof...(args) > 0u) {
      constexpr size_t kVertexCount = _vertex_count_of_segments<std::remove_cvref_t<Args>...>();
      constexpr size_t kControlSize = _control_size_of_segments<std::remove_cvref_t<Args>...>();

      Vertex* vertex_ptr;
      uint8_t* control_rev_ptr;

      if (bl_path_init_uninitialized(&path, kVertexCount, kControlSize, &vertex_ptr, &control_rev_ptr) == BL_SUCCESS) {
        _write_segments_to_path_data(vertex_ptr, control_rev_ptr, bl::Internal::forward<Args>(args)...);
      }
    }
    else {
      bl_path_init(&path);
    }

    return path;
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE_NODEBUG explicit operator bool() const noexcept { return !is_empty(); }

  BL_INLINE_NODEBUG BLPath& operator=(BLPath&& other) noexcept { bl_path_assign_move(this, &other); return *this; }
  BL_INLINE_NODEBUG BLPath& operator=(const BLPath& other) noexcept { bl_path_assign_weak(this, &other); return *this; }

  [[nodiscard]] BL_INLINE_NODEBUG bool operator==(const BLPath& other) const noexcept { return  equals(other); }
  [[nodiscard]] BL_INLINE_NODEBUG bool operator!=(const BLPath& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  BL_INLINE_NODEBUG BLResult reset() noexcept {
    BLResult result = bl_path_reset(this);

    // Reset operation always succeeds.
    BL_ASSUME(result == BL_SUCCESS);
    // Assume a default constructed BLPath after reset.
    BL_ASSUME(_d._sso_data[BLObjectDetail::kMetaByteIndex] == kDefaultMetaFlags);

    return result;
  }

  BL_INLINE_NODEBUG void swap(BLPathCore& other) noexcept { _d.swap(other._d); }

  //! \}

  //! \name Accessors
  //! \{

  //! Tests whether the path is empty, which means its size equals to zero.
  [[nodiscard]] BL_INLINE_NODEBUG bool is_empty() const noexcept { return _d.dyn.payload_u32 == 0u; }

  //! Returns the size of the path in bytes.
  //!
  //! This size includes the size of the vertex array and also the size of the command buffer.
  [[nodiscard]] BL_INLINE_NODEBUG size_t byte_size() const noexcept { return bl::ObjectLayer::get_size(this); }

  //! Returns the capacity of the path in bytes.
  [[nodiscard]] BL_INLINE_NODEBUG size_t byte_capacity() const noexcept { return bl::ObjectLayer::get_capacity(this); }

  //! Returns the size of the command buffer
  [[nodiscard]] BL_INLINE_NODEBUG size_t control_data_size() const noexcept { return _d.dyn.payload_u32; }

  //! returns the number of bytes vertex data occupies.
  [[nodiscard]] BL_INLINE_NODEBUG size_t vertex_data_size() const noexcept { return _d.dyn.size_u32; }

  //! Returns the number of vertices used by the path.
  //!
  //! \note The number of vertices doesn't correlate with the size of the command buffer. For example
  //! \ref BL_PATH_CMD_LINE uses 1 byte in a command buffer and 1 vertex, however, \ref BL_PATH_CMD_QUAD
  //! uses 1 byte in a command buffer and 2 vertices.
  [[nodiscard]] BL_INLINE_NODEBUG size_t vertex_count() const noexcept { return _d.dyn.size_u32 / sizeof(Vertex); }

  //! Returns path's vertex data (read-only).
  [[nodiscard]] BL_INLINE_NODEBUG const BLPoint* vertex_data() const noexcept {
    return bl::ObjectLayer::get_data<BLPoint>(this);
  }

  //! Returns path's vertex data (read-only).
  [[nodiscard]] BL_INLINE_NODEBUG const uint8_t* control_data_rev() const noexcept {
    return bl::ObjectLayer::get_data<uint8_t>(this) + bl::ObjectLayer::get_capacity(this);
  }

  //! Returns a read-only path data as `BLPathView`.
  [[nodiscard]] BL_INLINE_NODEBUG BLPathView view() const noexcept {
    return {vertex_data(), control_data_rev(), vertex_count(), control_data_size()};
  }

  //! \}

  //! \name Path Construction
  //! \{

  //! Clears the content of the path.
  BL_INLINE_NODEBUG BLResult clear() noexcept {
    return bl_path_clear(this);
  }

  //! Shrinks the capacity of the path to fit the current usage.
  BL_INLINE_NODEBUG BLResult shrink() noexcept {
    return bl_path_shrink(this);
  }

  //! Reserves the capacity of the path to the given `byte_size` - which combines vertices and control data.
  BL_INLINE_NODEBUG BLResult reserve(size_t byte_size) noexcept {
    return bl_path_reserve_byte_size(this, byte_size);
  }

  //! Reserves the capacity of the path for appending at least `byte_size` bytes - which combines vertices and control data.
  BL_INLINE_NODEBUG BLResult reserve_for_append(size_t append_byte_size) noexcept {
    return bl_path_reserve_byte_size_for_append(this, append_byte_size);
  }

  BL_INLINE_NODEBUG BLResult add_uninitialized(
    size_t append_vertex_count,
    size_t append_control_data_size,
    BLPoint** vertex_data_out,
    uint8_t** control_data_rev_out
  ) noexcept {
    return bl_path_add_uninitialized(this, append_vertex_count, append_control_data_size, vertex_data_out, control_data_rev_out);
  }

  BL_INLINE_NODEBUG BLResult assign(BLPathCore&& other) noexcept {
    return bl_path_assign_move(this, &other);
  }

  BL_INLINE_NODEBUG BLResult assign(const BLPathCore& other) noexcept {
    return bl_path_assign_weak(this, &other);
  }

  BL_INLINE_NODEBUG BLResult assign_deep(const BLPathCore& other) noexcept {
    return bl_path_assign_deep(this, &other);
  }

  //! Sets vertex at `vertex_index` to `pt`.
  BL_INLINE_NODEBUG BLResult set_vertex_at(size_t vertex_index, const BLPoint& pt) noexcept {
    return bl_path_set_vertex_at(this, vertex_index, pt.x, pt.y);
  }

  //! Sets vertex at `vertex_index` to `[x, y]`.
  BL_INLINE_NODEBUG BLResult set_vertex_at(size_t vertex_index, double x, double y) noexcept {
    return bl_path_set_vertex_at(this, vertex_index, x, y);
  }

  //! Sets conic weight at `control_index` to `w`.
  //!
  //! \remarks Blend2D would do a basic check to prevent accidental errors when it comes to `control_index` parameter.
  //! These include checking the start and end byte of a conic, to verify that `control_index` really points to a conic
  //! command. However, path data is vertices and control data, which contains commands and weights mixed. It's possible
  //! that if `control_index` lands inside a weight payload, that this function would trash command data instead of conic
  //! weight. So, only use `control_index`, which is 100% known to have a conic - for example the index was returned from
  //! Blend2D API or stored during path construction.
  BL_INLINE_NODEBUG BLResult set_weight_at(size_t control_index, double w) noexcept {
    return bl_path_set_weight_at(this, control_index, w);
  }

  //! Moves to `p0`.
  //!
  //! Appends `BL_PATH_CMD_MOVE[p0]` command to the path.
  BL_INLINE_NODEBUG BLResult move_to(const BLPoint& p0) noexcept {
    return bl_path_move_to(this, p0.x, p0.y);
  }

  //! Moves to `[x0, y0]`.
  //!
  //! Appends `BL_PATH_CMD_MOVE[x0, y0]` command to the path.
  BL_INLINE_NODEBUG BLResult move_to(double x0, double y0) noexcept {
    return bl_path_move_to(this, x0, y0);
  }

  //! Adds line to `p1`.
  //!
  //! Appends `BL_PATH_CMD_LINE[p1]` command to the path.
  BL_INLINE_NODEBUG BLResult line_to(const BLPoint& p1) noexcept {
    return bl_path_line_to(this, p1.x, p1.y);
  }

  //! Adds line to `[x1, y1]`.
  //!
  //! Appends `BL_PATH_CMD_LINE[x1, y1]` command to the path.
  BL_INLINE_NODEBUG BLResult line_to(double x1, double y1) noexcept {
    return bl_path_line_to(this, x1, y1);
  }

  //! Adds a polyline (LineTo) of the given `poly` array of size `count`.
  //!
  //! Appends multiple `BL_PATH_CMD_LINE[x[i], y[i]]` commands to the path depending on `count` parameter.
  BL_INLINE_NODEBUG BLResult poly_to(const BLPoint* poly, size_t count) noexcept {
    return bl_path_poly_to(this, poly, count);
  }

  //! Adds a quadratic curve to `p1` and `p2`.
  //!
  //! Appends `BL_PATH_CMD_QUAD[p1, p2]` command to the path.
  //!
  //! Matches SVG 'Q' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataQuadraticBezierCommands
  BL_INLINE_NODEBUG BLResult quad_to(const BLPoint& p1, const BLPoint& p2) noexcept {
    return bl_path_quad_to(this, p1.x, p1.y, p2.x, p2.y);
  }

  //! Adds a quadratic curve to `[x1, y1]` and `[x2, y2]`.
  //!
  //! Appends `BL_PATH_CMD_QUAD[x1, y1, x2, y2]` command to the path.
  //!
  //! Matches SVG 'Q' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataQuadraticBezierCommands
  BL_INLINE_NODEBUG BLResult quad_to(double x1, double y1, double x2, double y2) noexcept {
    return bl_path_quad_to(this, x1, y1, x2, y2);
  }

  BL_INLINE BLResult conic_to(const BLPoint& p1, const BLPoint& p2, double w) noexcept {
    return bl_path_conic_to(this, p1.x, p1.y, p2.x, p2.y, w);
  }

  BL_INLINE BLResult conic_to(double x1, double y1, double x2, double y2, double w) noexcept {
    return bl_path_conic_to(this, x1, y1, x2, y2, w);
  }

  //! Adds a cubic curve to `p1`, `p2`, `p3`.
  //!
  //! Appends `BL_PATH_CMD_CUBIC[p1, p2, p3]` command to the path.
  //!
  //! Matches SVG 'C' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataCubicBezierCommands
  BL_INLINE_NODEBUG BLResult cubic_to(const BLPoint& p1, const BLPoint& p2, const BLPoint& p3) noexcept {
    return bl_path_cubic_to(this, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
  }

  //! Adds a cubic curve to `[x1, y1]`, `[x2, y2]`, and `[x3, y3]`.
  //!
  //! Appends `BL_PATH_CMD_CUBIC[x1, y1, x2, y2, x3, y3]` command to the path.
  //!
  //! Matches SVG 'C' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataCubicBezierCommands
  BL_INLINE_NODEBUG BLResult cubic_to(double x1, double y1, double x2, double y2, double x3, double y3) noexcept {
    return bl_path_cubic_to(this, x1, y1, x2, y2, x3, y3);
  }

  //! Adds a smooth quadratic curve to `p2`, calculating `p1` from last points.
  //!
  //! Appends `BL_PATH_CMD_QUAD[calculate(p1), p2]` command to the path.
  //!
  //! Matches SVG 'T' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataQuadraticBezierCommands
  BL_INLINE_NODEBUG BLResult smooth_quad_to(const BLPoint& p2) noexcept {
    return bl_path_smooth_quad_to(this, p2.x, p2.y);
  }

  //! Adds a smooth quadratic curve to `[x2, y2]`, calculating `[x1, y1]` from last points.
  //!
  //! Appends `BL_PATH_CMD_QUAD[calculate(x1), calculate(y1), x2, y2]` command to the path.
  //!
  //! Matches SVG 'T' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataQuadraticBezierCommands
  BL_INLINE_NODEBUG BLResult smooth_quad_to(double x2, double y2) noexcept {
    return bl_path_smooth_quad_to(this, x2, y2);
  }

  //! Adds a smooth cubic curve to `p2` and `p3`, calculating `p1` from last points.
  //!
  //! Appends `BL_PATH_CMD_QUAD[calculate(p1), p2, p3]` command to the path.
  //!
  //! Matches SVG 'S' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataCubicBezierCommands
  BL_INLINE_NODEBUG BLResult smooth_cubic_to(const BLPoint& p2, const BLPoint& p3) noexcept {
    return bl_path_smooth_cubic_to(this, p2.x, p2.y, p3.x, p3.y);
  }

  //! Adds a smooth cubic curve to `[x2, y2]` and `[x3, y3]`, calculating `[x1, y1]` from last points.
  //!
  //! Appends `BL_PATH_CMD_CUBIC[calculate(x1), calculate(y1), x2, y2, x3, y3]` command to the path.
  //!
  //! Matches SVG 'S' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataCubicBezierCommands
  BL_INLINE_NODEBUG BLResult smooth_cubic_to(double x2, double y2, double x3, double y3) noexcept {
    return bl_path_smooth_cubic_to(this, x2, y2, x3, y3);
  }

  //! Adds an arc to the path.
  //!
  //! The center of the arc is specified by `c` and radius by `r`. Both `start` and `sweep` angles are in radians.
  //! If the last vertex doesn't match the start of the arc then a `line_to()` would be emitted before adding the arc.
  //! Pass `true` in `force_move_to` to always emit `move_to()` at the beginning of the arc, which starts a new figure.
  BL_INLINE_NODEBUG BLResult arc_to(const BLPoint& c, const BLPoint& r, double start, double sweep, bool force_move_to = false) noexcept {
    return bl_path_arc_to(this, c.x, c.y, r.x, r.y, start, sweep, force_move_to);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult arc_to(double cx, double cy, double rx, double ry, double start, double sweep, bool force_move_to = false) noexcept {
    return bl_path_arc_to(this, cx, cy, rx, ry, start, sweep, force_move_to);
  }

  //! Adds an arc quadrant (90deg) to the path. The first point `p1` specifies
  //! the quadrant corner and the last point `p2` specifies the end point.
  BL_INLINE_NODEBUG BLResult arc_quadrant_to(const BLPoint& p1, const BLPoint& p2) noexcept {
    return bl_path_arc_quadrant_to(this, p1.x, p1.y, p2.x, p2.y);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult arc_quadrant_to(double x1, double y1, double x2, double y2) noexcept {
    return bl_path_arc_quadrant_to(this, x1, y1, x2, y2);
  }

  //! Adds an elliptic arc to the path that follows the SVG specification.
  //!
  //! Matches SVG 'A' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataEllipticalArcCommands
  BL_INLINE_NODEBUG BLResult elliptic_arc_to(const BLPoint& rp, double x_axis_rotation, bool large_arc_flag, bool sweep_flag, const BLPoint& p1) noexcept {
    return bl_path_elliptic_arc_to(this, rp.x, rp.y, x_axis_rotation, large_arc_flag, sweep_flag, p1.x, p1.y);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult elliptic_arc_to(double rx, double ry, double x_axis_rotation, bool large_arc_flag, bool sweep_flag, double x1, double y1) noexcept {
    return bl_path_elliptic_arc_to(this, rx, ry, x_axis_rotation, large_arc_flag, sweep_flag, x1, y1);
  }

  //! Closes the current figure.
  //!
  //! Appends `BL_PATH_CMD_CLOSE` to the path.
  //!
  //! Matches SVG 'Z' path command:
  //!   - https://www.w3.org/TR/SVG/paths.html#PathDataClosePathCommand
  //!
  //! \remarks It's not allowed to close an empty path or to double-close an existing path.
  //! In these cases the function would fail and return \ref BL_ERROR_INVALID_GEOMETRY.
  BL_INLINE_NODEBUG BLResult close() noexcept { return bl_path_close(this); }

  //! \}

  //! \name Adding Multiple Segments
  //!
  //! Adding multiple segments API was designed to provide high-performance path building in case that the user knows
  //! the segments that will be added to the path in advance.
  //!
  //! \{

  template<typename... Args>
  BL_INLINE BLResult add_segments(Args&&... args) noexcept {
    if constexpr (sizeof...(args) > 0u) {
      constexpr size_t kVertexCount = _vertex_count_of_segments<std::remove_cvref_t<Args>...>();
      constexpr size_t kControlSize = _control_size_of_segments<std::remove_cvref_t<Args>...>();

      Vertex* vertex_ptr;
      uint8_t* control_rev_ptr;

      BL_PROPAGATE(add_uninitialized(kVertexCount, kControlSize, &vertex_ptr, &control_rev_ptr));
      _write_segments_to_path_data(vertex_ptr, control_rev_ptr, bl::Internal::forward<Args>(args)...);
    }
    return BL_SUCCESS;
  }

  //! \}

  //! \name Adding Figures
  //!
  //! Adding a figure means starting with a move-to segment. For example `add_box()` would start a new figure
  //! by adding `BL_PATH_CMD_MOVE_TO` segment, and then by adding 3 lines, and finally a close command.
  //!
  //! \{

  //! Adds a closed rectangle to the path specified by `box`.
  BL_INLINE_NODEBUG BLResult add_box(const BLBoxI& box, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return bl_path_add_box_i(this, &box, dir);
  }

  //! Adds a closed rectangle to the path specified by `box`.
  BL_INLINE_NODEBUG BLResult add_box(const BLBox& box, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return bl_path_add_box_d(this, &box, dir);
  }

  //! Adds a closed rectangle to the path specified by `[x0, y0, x1, y1]`.
  BL_INLINE_NODEBUG BLResult add_box(double x0, double y0, double x1, double y1, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_box(BLBox(x0, y0, x1, y1), dir);
  }

  //! Adds a closed rectangle to the path specified by `rect`.
  BL_INLINE_NODEBUG BLResult add_rect(const BLRectI& rect, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return bl_path_add_rect_i(this, &rect, dir);
  }

  //! Adds a closed rectangle to the path specified by `rect`.
  BL_INLINE_NODEBUG BLResult add_rect(const BLRect& rect, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return bl_path_add_rect_d(this, &rect, dir);
  }

  //! Adds a closed rectangle to the path specified by `[x, y, w, h]`.
  BL_INLINE_NODEBUG BLResult add_rect(double x, double y, double w, double h, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_rect(BLRect(x, y, w, h), dir);
  }

  //! Adds a geometry to the path.
  BL_INLINE_NODEBUG BLResult add_geometry(BLGeometryType geometry_type, const void* geometry_data, const BLMatrix2D* m = nullptr, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return bl_path_add_geometry(this, geometry_type, geometry_data, m, dir);
  }

  //! Adds a closed circle to the path.
  BL_INLINE_NODEBUG BLResult add_circle(const BLCircle& circle, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_CIRCLE_D, &circle, nullptr, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_circle(const BLCircle& circle, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_CIRCLE_D, &circle, &transform, dir);
  }

  //! Adds a closed ellipse to the path.
  BL_INLINE_NODEBUG BLResult add_ellipse(const BLEllipse& ellipse, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_ELLIPSE_D, &ellipse, nullptr, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_ellipse(const BLEllipse& ellipse, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_ELLIPSE_D, &ellipse, &transform, dir);
  }

  //! Adds a closed rounded rectangle to the path.
  BL_INLINE_NODEBUG BLResult add_round_rect(const BLRoundRect& rr, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_ROUND_RECT_D, &rr, nullptr, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_round_rect(const BLRoundRect& rr, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_ROUND_RECT_D, &rr, &transform, dir);
  }

  //! Adds an unclosed arc to the path.
  BL_INLINE_NODEBUG BLResult add_arc(const BLArc& arc, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_ARC_D, &arc, nullptr, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_arc(const BLArc& arc, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_ARC_D, &arc, &transform, dir);
  }

  //! Adds a closed chord to the path.
  BL_INLINE_NODEBUG BLResult add_chord(const BLArc& chord, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_CHORD_D, &chord, nullptr, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_chord(const BLArc& chord, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_CHORD_D, &chord, &transform, dir);
  }

  //! Adds a closed pie to the path.
  BL_INLINE_NODEBUG BLResult add_pie(const BLArc& pie, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_PIE_D, &pie, nullptr, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_pie(const BLArc& pie, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_PIE_D, &pie, &transform, dir);
  }

  //! Adds an unclosed line to the path.
  BL_INLINE_NODEBUG BLResult add_line(const BLLine& line, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_LINE_D, &line, nullptr, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_line(const BLLine& line, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_LINE_D, &line, &transform, dir);
  }

  //! Adds a closed triangle.
  BL_INLINE_NODEBUG BLResult add_triangle(const BLTriangle& triangle, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_TRIANGLE_D, &triangle, nullptr, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_triangle(const BLTriangle& triangle, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_TRIANGLE_D, &triangle, &transform, dir);
  }

  //! Adds a polyline.
  BL_INLINE_NODEBUG BLResult add_polyline(const BLSpan<const BLPointI>& poly, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_POLYLINE_I, &poly, nullptr, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_polyline(const BLSpan<const BLPointI>& poly, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_POLYLINE_I, &poly, &transform, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_polyline(const BLPointI* poly, size_t n, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_polyline(BLSpan<const BLPointI>{poly, n}, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_polyline(const BLPointI* poly, size_t n, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_polyline(BLSpan<const BLPointI>{poly, n}, transform, dir);
  }

  //! Adds a polyline.
  BL_INLINE_NODEBUG BLResult add_polyline(const BLSpan<const BLPoint>& poly, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_POLYLINE_D, &poly, nullptr, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_polyline(const BLSpan<const BLPoint>& poly, const BLMatrix2D& transform, BLGeometryDirection dir) {
    return add_geometry(BL_GEOMETRY_TYPE_POLYLINE_D, &poly, &transform, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_polyline(const BLPoint* poly, size_t n, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_polyline(BLSpan<const BLPoint>{poly, n}, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_polyline(const BLPoint* poly, size_t n, const BLMatrix2D& transform, BLGeometryDirection dir) {
    return add_polyline(BLSpan<const BLPoint>{poly, n}, transform, dir);
  }

  //! Adds a polygon.
  BL_INLINE_NODEBUG BLResult add_polygon(const BLSpan<const BLPointI>& poly, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_POLYGON_I, &poly, nullptr, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_polygon(const BLSpan<const BLPointI>& poly, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_POLYGON_I, &poly, &transform, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_polygon(const BLPointI* poly, size_t n, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_polygon(BLSpan<const BLPointI>{poly, n}, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_polygon(const BLPointI* poly, size_t n, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_polygon(BLSpan<const BLPointI>{poly, n}, transform, dir);
  }

  //! Adds a polygon.
  BL_INLINE_NODEBUG BLResult add_polygon(const BLSpan<const BLPoint>& poly, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_POLYGON_D, &poly, nullptr, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_polygon(const BLSpan<const BLPoint>& poly, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_POLYGON_D, &poly, &transform, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_polygon(const BLPoint* poly, size_t n, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_polygon(BLSpan<const BLPoint>{poly, n}, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_polygon(const BLPoint* poly, size_t n, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_polygon(BLSpan<const BLPoint>{poly, n}, transform, dir);
  }

  //! Adds an array of closed boxes.
  BL_INLINE_NODEBUG BLResult add_box_array(const BLSpan<const BLBoxI>& array, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_SPAN_BOX_I, &array, nullptr, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_box_array(const BLSpan<const BLBoxI>& array, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_SPAN_BOX_I, &array, &transform, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_box_array(const BLBoxI* data, size_t n, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_box_array(BLSpan<const BLBoxI>{data, n}, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_box_array(const BLBoxI* data, size_t n, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_box_array(BLSpan<const BLBoxI>{data, n}, transform, dir);
  }

  //! Adds an array of closed boxes.
  BL_INLINE_NODEBUG BLResult add_box_array(const BLSpan<const BLBox>& array, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_SPAN_BOX_D, &array, nullptr, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_box_array(const BLSpan<const BLBox>& array, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_SPAN_BOX_D, &array, &transform, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_box_array(const BLBox* data, size_t n, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_box_array(BLSpan<const BLBox>{data, n}, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_box_array(const BLBox* data, size_t n, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_box_array(BLSpan<const BLBox>{data, n}, transform, dir);
  }

  //! Adds an array of closed rectangles.
  BL_INLINE_NODEBUG BLResult add_rect_array(const BLSpan<const BLRectI>& array, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_SPAN_RECT_I, &array, nullptr, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_rect_array(const BLSpan<const BLRectI>& array, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_SPAN_RECT_I, &array, &transform, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_rect_array(const BLRectI* data, size_t n, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_rect_array(BLSpan<const BLRectI>{data, n}, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_rect_array(const BLRectI* data, size_t n, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_rect_array(BLSpan<const BLRectI>{data, n}, transform, dir);
  }

  //! Adds an array of closed rectangles.
  BL_INLINE_NODEBUG BLResult add_rect_array(const BLSpan<const BLRect>& array, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_SPAN_RECT_D, &array, nullptr, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_rect_array(const BLSpan<const BLRect>& array, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_geometry(BL_GEOMETRY_TYPE_SPAN_RECT_D, &array, &transform, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_rect_array(const BLRect* data, size_t n, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_rect_array(BLSpan<const BLRect>{data, n}, dir);
  }

  //! \overload
  BL_INLINE_NODEBUG BLResult add_rect_array(const BLRect* data, size_t n, const BLMatrix2D& transform, BLGeometryDirection dir = BL_GEOMETRY_DIRECTION_CW) noexcept {
    return add_rect_array(BLSpan<const BLRect>{data, n}, transform, dir);
  }

  //! \}

  //! \name Adding Paths
  //! \{

  //! Adds other `path` to this path.
  BL_INLINE_NODEBUG BLResult add_path(const BLPath& path) noexcept {
    return bl_path_add_path(this, &path);
  }

  //! Adds other `path` translated by `p` to this path.
  BL_INLINE_NODEBUG BLResult add_path(const BLPath& path, const BLPoint& p) noexcept {
    return bl_path_add_translated_path(this, &path, &p);
  }

  //! Adds other `path` transformed by `transform` to this path.
  BL_INLINE_NODEBUG BLResult add_path(const BLPath& path, const BLMatrix2D& transform) noexcept {
    return bl_path_add_transformed_path(this, &path, &transform);
  }

  //! Adds other `path`, but reversed.
  BL_INLINE_NODEBUG BLResult add_reversed_path(const BLPath& path) noexcept {
    return bl_path_add_reversed_path(this, &path);
  }

  //! Adds other `path` transformed by `transform`, but reversed.
  BL_INLINE_NODEBUG BLResult add_reversed_path(const BLPath& path, const BLMatrix2D& transform) noexcept {
    return bl_path_add_reversed_transformed_path(this, &path, &transform);
  }

  //! Adds a stroke of `path` to this path.
  BL_INLINE_NODEBUG BLResult add_stroked_path(const BLPath& path, const BLStrokeOptionsCore& stroke_options, const BLApproximationOptions& approximation_options) noexcept {
    return bl_path_add_stroked_path(this, &path, &stroke_options, &approximation_options);
  }

  //! Adds other `path` to this path.
  BL_INLINE_NODEBUG BLResult add_vertex_outline_of_path(const BLPath& path) noexcept {
    return bl_path_add_vertex_outline_of_path(this, &path);
  }

  //! \}

  //! \name Transformations
  //! \{

  //! Translates the whole path by `p`.
  BL_INLINE_NODEBUG BLResult translate(const BLPoint& p) noexcept {
    return bl_path_translate(this, &p);
  }

  //! Transforms the whole path by matrix `m`.
  BL_INLINE_NODEBUG BLResult transform(const BLMatrix2D& m) noexcept {
    return bl_path_transform(this, &m);
  }

  //! Fits the whole path into the given `rect` according to `fit_flags` (a combination of \ref BLPathFitFlags).
  //!
  //! By default (\ref BL_PATH_FIT_NO_FLAGS) the path is stretched to fill `rect` exactly. Use
  //! \ref BL_PATH_FIT_SCALE_CONTAIN or \ref BL_PATH_FIT_SCALE_COVER to preserve the path's aspect ratio,
  //! optionally combined with the `BL_PATH_FIT_ALIGN_[XY]_[MIN|MID|MAX]` alignment flags.
  BL_INLINE_NODEBUG BLResult fit_to(const BLRect& rect, BLPathFitFlags fit_flags = BL_PATH_FIT_NO_FLAGS) noexcept {
    return bl_path_fit_to(this, &rect, fit_flags);
  }

  //! \}

  //! \name Equality & Comparison
  //! \{

  //! Tests whether this path and the `other` path are equal.
  //!
  //! The equality check is deep. The data of both paths is examined and binary compared (thus a slight difference
  //! like -0 and +0 would make the equality check to fail).
  [[nodiscard]] BL_INLINE_NODEBUG bool equals(const BLPath& other) const noexcept { return bl_path_equals(this, &other); }

  //! \}

  //! \name Path Information
  //! \{

  //! Update a path information if necessary.
  BL_INLINE_NODEBUG BLResult get_info_flags(uint32_t* flags_out) const noexcept {
    return bl_path_get_info_flags(this, flags_out);
  }

  //! Stores a bounding box of all vertices and control points to `box_out`.
  //!
  //! Control box is simply bounds of all vertices the path has without further processing. It contains both on-path
  //! and off-path points. Consider using `get_bounding_box()` if you need a visual bounding box.
  BL_INLINE_NODEBUG BLResult get_vertex_bounding_box(BLBox* box_out) const noexcept {
    return bl_path_get_vertex_bounding_box(this, box_out);
  }

  //! Stores a bounding box of all on-path vertices and curve extrema to `box_out`.
  //!
  //! The bounding box stored to `box_out` could be smaller than a bounding box obtained by `get_control_box()` as it's
  //! calculated by merging only start/end points and curves at their extrema (not control points). The resulting
  //! bounding box represents a visual bounds of the path.
  BL_INLINE_NODEBUG BLResult get_ink_bounding_box(BLBox* box_out) const noexcept {
    return bl_path_get_ink_bounding_box(this, box_out);
  }

  //! Returns the last vertex of the path and stores it to `vtx_out`. If the very last command of the path is
  //! `BL_PATH_CMD_CLOSE` then the path will be iterated in reverse order to match the initial vertex of the last
  //! figure.
  BL_INLINE_NODEBUG BLResult get_last_vertex(BLPoint* vtx_out) const noexcept {
    return bl_path_get_last_vertex(this, vtx_out);
  }

  BL_INLINE_NODEBUG BLResult get_closest_vertex(const BLPoint& p, double max_distance, size_t* index_out) const noexcept {
    double distance_out;
    return bl_path_get_closest_vertex(this, &p, max_distance, index_out, &distance_out);
  }

  BL_INLINE_NODEBUG BLResult get_closest_vertex(const BLPoint& p, double max_distance, size_t* index_out, double* distance_out) const noexcept {
    return bl_path_get_closest_vertex(this, &p, max_distance, index_out, distance_out);
  }

  //! \}

  //! \name Hit Testing
  //! \{

  //! Hit tests the given point `p` by respecting the given `fill_rule`.
  [[nodiscard]] BL_INLINE_NODEBUG BLHitTest hit_test(const BLPoint& p, BLFillRule fill_rule) const noexcept {
    return bl_path_hit_test(this, &p, fill_rule);
  }

  //! \}
};

namespace bl::PathDetail {

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

static BL_INLINE_NODEBUG uint32_t vertex_count_of_cmd(BLPathCmd cmd) noexcept { return (uint32_t(cmd) >> 2); }
static BL_INLINE_NODEBUG bool is_single_vertex_cmd(BLPathCmd cmd) noexcept { return vertex_count_of_cmd(cmd) == 1; }
static BL_INLINE_NODEBUG bool is_double_vertex_cmd(BLPathCmd cmd) noexcept { return vertex_count_of_cmd(cmd) == 2; }
static BL_INLINE_NODEBUG bool is_begin_or_conic_cmd(BLPathCmd cmd) noexcept { return (cmd & 0x3) != 0; }
static BL_INLINE_NODEBUG bool is_move_or_close_cmd(BLPathCmd cmd) noexcept { return (cmd & 0x2) != 0; }

template<typename T>
[[nodiscard]]
static BL_INLINE_NODEBUG T load_weight(const void* p) noexcept {
#if defined(__GNUC__)
  typedef double __attribute__((__may_alias__, __aligned__(1))) UnalignedT;
  return static_cast<T>(*static_cast<const UnalignedT*>(p));
#elif defined(_MSC_VER)
  typedef double __declspec(align(1)) UnalignedT;
  return static_cast<T>(*static_cast<const UnalignedT*>(p));
#else
  T out;
  return memcpy(&out, p, sizeof(T));
  return out;
#endif
}

} // {bl::PathDetail}

//! Path iterator that can iterate over path data.
//!
//! Usage:
//!
//! ```
//! BLPath path = ...;
//! BLPathIterator it(path);
//!
//! while (it.has_command()) {
//!   switch (it.command()) {
//!     case BL_PATH_CMD_MOVE:
//!       printf("MoveTo {%f, %f}\n",
//!         it.vertex(0).x, it.vertex(0).y);
//!       it.advance<BL_PATH_CMD_MOVE>();
//!       break;
//!
//!     case BL_PATH_CMD_LINE:
//!       printf("LineTo {%f, %f}\n",
//!         it.vertex(0).x, it.vertex(0).y);
//!       it.advance<BL_PATH_CMD_LINE>();
//!       break;
//!
//!     case BL_PATH_CMD_QUAD:
//!       printf("QuadTo {%f, %f} {%f, %f}\n",
//!         it.vertex(0).x, it.vertex(0).y,
//!         it.vertex(1).x, it.vertex(1).y);
//!       it.advance<BL_PATH_CMD_QUAD>();
//!       break;
//!
//!     case BL_PATH_CMD_CONIC:
//!       printf("ConicTo {%f, %f} {%f, %f} [W=%f]\n",
//!         it.vertex(0).x, it.vertex(0).y,
//!         it.vertex(1).x, it.vertex(1).y,
//!         it.weight());
//!       it.advance<BL_PATH_CMD_CONIC>();
//!       break;
//!
//!     case BL_PATH_CMD_CUBIC:
//!       printf("CubicTo {%f, %f} {%f, %f} {%f, %f}\n",
//!         it.vertex(0).x, it.vertex(0).y,
//!         it.vertex(1).x, it.vertex(1).y,
//!         it.vertex(2).x, it.vertex(2).y);
//!       it.advance<BL_PATH_CMD_CUBIC>();
//!       break;
//!
//!     case BL_PATH_CMD_CLOSE:
//!       printf("Close\n");
//!       break;
//!
//!     // Should never happen unless path data was corrupted by user, you can
//!     // use default instead of `BL_PATH_CMD_CLOSE`, or both, to check errors.
//!     default:
//!       return;
//!   }
//! }
//!
//! ```
class BLPathIterator {
public:

  //! \name Iterator Structs
  //! \{

  struct Location {
    const BLPath::Vertex* vertex_data;
    const uint8_t* control_rev;
  };

  //! \}

  //! \name Members
  //! \{

  const BLPoint* _vertex_ptr {};
  const uint8_t* _control_rev_ptr {};
  const uint8_t* _control_rev_terminate {};

  //! \}

  //! \name Construction & Destruction
  //! \{

  //! Creates a default-constructed path iterator, which is not initialized to iterate any path.
  //!
  //! Default constructed path iterator always returns false when calling `has_command()`, which makes the
  //! use of all the remaining functions impossible.
  BL_INLINE_NODEBUG BLPathIterator() noexcept = default;
  BL_INLINE_NODEBUG explicit BLPathIterator(BLPathView view) noexcept { reset(view); }
  BL_INLINE_NODEBUG explicit BLPathIterator(const BLPath& path) noexcept { reset(path.view()); }

  //! \}

  //! \name Reset
  //! \{

  //! Resets path iterator to a default-constructed state, which means all pointers are set to nullptr.
  BL_INLINE_NODEBUG void reset() noexcept {
    _vertex_ptr = nullptr;
    _control_rev_ptr = nullptr;
    _control_rev_terminate = nullptr;
  }

  //! Resets the iterator to iterate from the beginning of the given path `view`.
  BL_INLINE_NODEBUG void reset(BLPathView view) noexcept {
    _vertex_ptr = view.vertex_data;
    _control_rev_ptr = view.control_data_rev - 1u;
    _control_rev_terminate = _control_rev_ptr - view.control_data_size;
  }

  //! Resets the iterator to iterate from the beginning of the given `path`.
  BL_INLINE_NODEBUG void reset(const BLPath& path) noexcept { reset(path.view()); }

  //! \}

  //! \name Path Control & Vertex Data
  //! \{

  //! Tests whether there is a command that can be iterated over.
  //!
  //! If `has_command()` returns false, it means that the path was either initially empty, or there is no more
  //! command to iterate. When that happens, the iterator cannot be used to get control data, vertices, weights,
  //! or advanced further.
  BL_INLINE_NODEBUG bool has_command() const noexcept {
    return _control_rev_ptr > _control_rev_terminate;
  }

  //! Returns the current command.
  BL_INLINE BLPathCmd command() const noexcept {
    BL_ASSERT(has_command());
    return BLPathCmd(_control_rev_ptr[0]);
  }

  //! Tests whether the current command is \ref BL_PATH_CMD_MOVE.
  BL_INLINE bool is_move_to() const noexcept { return command() == BL_PATH_CMD_MOVE; }
  //! Tests whether the current command is \ref BL_PATH_CMD_LINE.
  BL_INLINE bool is_line_to() const noexcept { return command() == BL_PATH_CMD_LINE; }
  //! Tests whether the current command is \ref BL_PATH_CMD_QUAD.
  BL_INLINE bool is_quad_to() const noexcept { return command() == BL_PATH_CMD_QUAD; }
  //! Tests whether the current command is \ref BL_PATH_CMD_CONIC.
  BL_INLINE bool is_conic_to() const noexcept { return command() == BL_PATH_CMD_CONIC; }
  //! Tests whether the current command is \ref BL_PATH_CMD_CUBIC.
  BL_INLINE bool is_cubic_to() const noexcept { return command() == BL_PATH_CMD_CUBIC; }
  //! Tests whether the current command is \ref BL_PATH_CMD_CLOSE.
  BL_INLINE bool is_close() const noexcept { return command() == BL_PATH_CMD_CLOSE; }

  //! Returns the remaining size of control data, which the iterator has.
  //!
  //! When there is no remaining control data to iterate, the iterator is at the end. When `has_command()`
  //! returns false `remaining_control_size()` would always return 0.
  BL_INLINE_NODEBUG size_t remaining_control_size() const noexcept {
    return (size_t)((_control_rev_ptr + 1u) - _control_rev_terminate);
  }

  //! Returns the vertex pointer the iterator points to at the moment. Vertex point contains the command
  //! data. LineTo, QuadTo, ConicTo, and CubicTo can access the previous vertex at `-1` as this vertex is
  //! always part of the previous command. MoveTo and Close don't offer a previous vertex.
  BL_INLINE_NODEBUG const BLPoint* vertex_ptr() const noexcept { return _vertex_ptr; }

  //! Returns a vertex at the given `index`. Similar to `vertex_ptr()`, but the index is already applied.
  BL_INLINE_NODEBUG BLPoint vertex(auto index) const noexcept { return _vertex_ptr[index]; }

  //! Returns conic weight.
  //!
  //! This function should be only used when the command is \ref BL_PATH_CMD_CONIC. Use `command()`
  //! or `is_conic_to()` to check for conic curves during iteration, before accessing their weights.
  BL_INLINE_NODEBUG double weight() const noexcept {
    BL_ASSERT(remaining_control_size() >= sizeof(double));
    return bl::PathDetail::load_weight<double>(_control_rev_ptr - 8);
  }

  //! \}

  //! \name Path Position
  //! \{

  BL_INLINE_NODEBUG Location location() const noexcept {
    return Location{_vertex_ptr, _control_rev_ptr};
  }

  //! Returns a relative iterator position (both control and vertex offsets) since the beginning of `path`.
  //!
  //! The `path` passed must be exactly the same path that was used to initialize or reset the iterator.
  //! Passing any other path is undefined behavior as resulting position would be invalid.
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

  //! \}

  //! \name Iterator Advance
  //! \{

  //! \cond INTERNAL
  BL_INLINE_NODEBUG void _advance_vertex_ptr(size_t n) noexcept { _vertex_ptr += n; }
  BL_INLINE_NODEBUG void _advance_control_ptr(size_t n) noexcept { _control_rev_ptr -= n; }
  //! \endcond

  //! Advances both control data pointer and vertex pointer based on `Cmd`.
  //!
  //! \note Each command needs a different advancement. For example MoveTo and LineTo only have a single
  //! vertex, but QuadTo/ConicTo have two, and CubicTo three. In addition, ConicTo also stores weight in
  //! control data, so the advance needs to handle all of these cases. And finally, Close command has no
  //! corresponding vertex, only a single control byte.
  template<BLPathCmd Cmd>
  BL_INLINE_NODEBUG void advance() noexcept {
    _advance_vertex_ptr(bl::PathDetail::PathAdvance<Cmd>::kVertexCount);
    _advance_control_ptr(bl::PathDetail::PathAdvance<Cmd>::kControlSize);
  }

  //! Advances a non-conic command - effectively this advances control pointer by 1 and vertex pointer by
  //! the exact number of vertices the command uses.
  //!
  //! \remarks This function cannot be used for conics as conics use 2 bytes and a weight in control data.
  //! There is an assertion that would verify this in debug mode, but not in release builds.
  BL_INLINE_NODEBUG void advance_non_conic(BLPathCmd cmd) noexcept {
    BL_ASSERT(cmd != BL_PATH_CMD_CONIC);
    advance_vertex_and_control(bl::PathDetail::vertex_count_of_cmd(cmd), 1u);
  }

  //! Advances vertex pointer by `vertex_count` and control pointer by `control_count`. For low-level use
  //! only.
  //!
  //! \remarks This function must be used carefully and should only be used in places where necessary for
  //! either performance or logical reasons. The use of this function thus needs basic knowledge about
  //! Blend2D internals and path data storage. Using this function incorrectly can corrupt the iteration
  //! in the best case or cause undefined behavior in the worst case.
  BL_INLINE_NODEBUG void advance_vertex_and_control(size_t vertex_count, size_t control_count) noexcept {
    _advance_vertex_ptr(vertex_count);
    _advance_control_ptr(control_count);
  }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_CORE_PATH_H_INCLUDED
