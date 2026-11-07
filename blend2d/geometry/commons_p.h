// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_GEOMETRY_COMMONS_P_H_INCLUDED
#define BLEND2D_GEOMETRY_COMMONS_P_H_INCLUDED

#include <blend2d/axl/commons.h>
#include <blend2d/axl/math_p.h>
#include <blend2d/axl/span_p.h>
#include <blend2d/core/geometry.h>

//! \cond INTERNAL
//! \addtogroup blend2d_geometry
//! \{

namespace bl::Geometry {

// Minimum distance of a line/curve segment to its predecessor that will be processed by path processing pipeline.
// If the segment has a smaller distance than this threshold it would be skipped and considered too close to be
// processed. The reason is to avoid calculating normals from very small, almost zero, distances.
static constexpr double kProcessingDistanceEpsilon = 1e-10;
static constexpr double kProcessingDistanceEpsilonSq = Math::square(kProcessingDistanceEpsilon);

static constexpr double kCurveCollinearityEpsilon = 1e-10;
static constexpr double kCurveCollinearityEpsilonSq = Math::square(kCurveCollinearityEpsilon);

static constexpr double kNormalCollinearityEpsilon = 1e-12;

using Math::lerp;

static BL_INLINE_NODEBUG bool is_zero(const BLPoint& p) noexcept { return (p.x == 0) & (p.y == 0); }

//! \name Geometry Type Size
//! \{

static BL_INLINE bool is_simple_geometry_type(uint32_t geometry_type) noexcept {
  return geometry_type <= BL_GEOMETRY_TYPE_SIMPLE_LAST;
}

BL_HIDDEN extern const LookupTable<uint8_t, BL_GEOMETRY_TYPE_SIMPLE_LAST + 1> geometry_type_size_table;

//! \}

//! \name Validity Checks
//! \{

static BL_INLINE bool is_valid(const BLSizeI& size) noexcept { return axl::bool_and(size.w > 0, size.h > 0); }
static BL_INLINE bool is_valid(const BLSize& size) noexcept { return axl::bool_and(size.w > 0, size.h > 0); }

static BL_INLINE bool is_valid(const BLBoxI& box) noexcept { return axl::bool_and(box.x0 < box.x1, box.y0 < box.y1); }
static BL_INLINE bool is_valid(const BLBox& box) noexcept { return axl::bool_and(box.x0 < box.x1, box.y0 < box.y1); }

static BL_INLINE bool is_valid(const BLRectI& rect) noexcept {
  axl::OverflowFlag of{};
  int x1 = axl::add_overflow(rect.x, rect.w, of);
  int y1 = axl::add_overflow(rect.y, rect.h, of);
  return axl::bool_and(rect.x < x1, rect.y < y1, !axl::did_overflow(of));
}

static BL_INLINE bool is_valid(const BLRect& rect) noexcept {
  double x1 = rect.x + rect.w;
  double y1 = rect.y + rect.h;
  return axl::bool_and(rect.x < x1, rect.y < y1);
}

//! \}

//! \name Vector Operations
//! \{

static BL_INLINE double dot(const BLPoint& a, const BLPoint& b) noexcept { return Math::madd(a.x, b.x, a.y * b.y); }
static BL_INLINE double cross(const BLPoint& a, const BLPoint& b) noexcept { return Math::msub(a.x, b.y, a.y * b.x); }

static BL_INLINE double magnitude_squared(const BLPoint& v) noexcept { return Math::madd(v.x, v.x, v.y * v.y); }
static BL_INLINE double magnitude(const BLPoint& v) noexcept { return Math::sqrt(magnitude_squared(v)); }

static BL_INLINE double length_squared(const BLPoint& a, const BLPoint& b) noexcept { return magnitude_squared(b - a); }
static BL_INLINE double length(const BLPoint& a, const BLPoint& b) noexcept { return Math::sqrt(length_squared(a, b)); }

static BL_INLINE BLPoint normal(const BLPoint& v) noexcept { return BLPoint(-v.y, v.x); }
static BL_INLINE BLPoint unit_vector(const BLPoint& v) noexcept { return v / magnitude(v); }

static BL_INLINE BLPoint line_vector_intersection(const BLPoint& p0, const BLPoint& v0, const BLPoint& p1, const BLPoint& v1) noexcept {
  return p0 + cross(p1 - p0, v1) / cross(v0, v1) * v0;
}

//! Same as `line_vector_intersection()`, but returns `fallback` when directions `v0` and `v1` are (near)
//! parallel - the intersection is then undefined (parallel) or numerically explosive (nearly parallel),
//! including the exactly-collinear `0 / 0` case that would otherwise yield a NaN.
static BL_INLINE BLPoint line_vector_intersection_safe(const BLPoint& p0, const BLPoint& v0, const BLPoint& p1, const BLPoint& v1, const BLPoint& fallback) noexcept {
  double den = cross(v0, v1);

  // Scale-invariant parallelism test: |den| = |v0|*|v1|*|sin(theta)|, so comparing `den^2` against
  // `eps^2 * |v0|^2 * |v1|^2` rejects everything within ~`eps` radians of parallel without a sqrt.
  constexpr double parallel_eps_sq = 1e-12;
  if (den * den <= parallel_eps_sq * magnitude_squared(v0) * magnitude_squared(v1)) {
    return fallback;
  }

  return p0 + cross(p1 - p0, v1) / den * v0;
}

static BL_INLINE double conic_normalized_weight(double w0, double w1, double w2) noexcept {
  return w1 / Math::sqrt(w0 * w2);
}

//! \}

//! \name Box/Rect Operations
//! \{

static BL_INLINE void bound(BLBox& box, const BLPoint& p) noexcept {
  box.reset(bl_min(box.x0, p.x), bl_min(box.y0, p.y),
            bl_max(box.x1, p.x), bl_max(box.y1, p.y));
}

static BL_INLINE void bound(BLBox& box, const BLBox& other) noexcept {
  box.reset(bl_min(box.x0, other.x0), bl_min(box.y0, other.y0),
            bl_max(box.x1, other.x1), bl_max(box.y1, other.y1));
}

static BL_INLINE void bound(BLBoxI& box, const BLBoxI& other) noexcept {
  box.reset(bl_min(box.x0, other.x0), bl_min(box.y0, other.y0),
            bl_max(box.x1, other.x1), bl_max(box.y1, other.y1));
}

static BL_INLINE bool intersect(BLBoxI& dst, const BLBoxI& a, const BLBoxI& b) noexcept {
  dst.reset(bl_max(a.x0, b.x0), bl_max(a.y0, b.y0),
            bl_min(a.x1, b.x1), bl_min(a.y1, b.y1));
  return axl::bool_and(dst.x0 < dst.x1,dst.y0 < dst.y1);
}

static BL_INLINE bool intersect(BLBox& dst, const BLBox& a, const BLBox& b) noexcept {
  dst.reset(bl_max(a.x0, b.x0), bl_max(a.y0, b.y0),
            bl_min(a.x1, b.x1), bl_min(a.y1, b.y1));
  return axl::bool_and(dst.x0 < dst.x1, dst.y0 < dst.y1);
}

static BL_INLINE bool subsumes(const BLBoxI& a, const BLBoxI& b) noexcept {
  return axl::bool_and(a.x0 <= b.x0, a.y0 <= b.y0, a.x1 >= b.x1, a.y1 >= b.y1);
}

static BL_INLINE bool subsumes(const BLBox& a, const BLBox& b) noexcept {
  return axl::bool_and(a.x0 <= b.x0, a.y0 <= b.y0, a.x1 >= b.x1, a.y1 >= b.y1);
}

static BL_INLINE bool overlaps(const BLBoxI& a, const BLBoxI& b) noexcept {
  return axl::bool_and(a.x1 > b.x0, a.y1 > b.y0, a.x0 < b.x1, a.y0 < b.y1);
}

static BL_INLINE bool overlaps(const BLBox& a, const BLBox& b) noexcept {
  return axl::bool_and(a.x1 > b.x0, a.y1 > b.y0, a.x0 < b.x1, a.y0 < b.y1);
}

//! \}

} // {bl::Geometry}

//! \}
//! \endcond

#endif // BLEND2D_GEOMETRY_COMMONS_P_H_INCLUDED
