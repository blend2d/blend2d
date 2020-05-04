// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_GEOMETRY_P_H_INCLUDED
#define BLEND2D_GEOMETRY_P_H_INCLUDED

#include "geometry.h"
#include "math_p.h"
#include "support/fixedarray_p.h"
#include "support/intops_p.h"
#include "support/lookuptable_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

template <typename T>
struct BLVecOperators {
  struct Neg { static BL_INLINE T op(T a) { return T(-a); } };
  struct Abs { static BL_INLINE T op(T a) { return T(a < 0 ? -a : a); } };

  struct Add { static BL_INLINE T op(T a, T b) { return a + b; } };
  struct Sub { static BL_INLINE T op(T a, T b) { return a - b; } };
  struct Mul { static BL_INLINE T op(T a, T b) { return a * b; } };
  struct Div { static BL_INLINE T op(T a, T b) { return a / b; } };
  struct Min { static BL_INLINE T op(T a, T b) { return blMin(a, b); } };
  struct Max { static BL_INLINE T op(T a, T b) { return blMax(a, b); } };
};

template<typename T, size_t N>
struct BLVecT {};

template<typename T>
struct BLVecT<T, 2> {
  T x, y;

  typedef BLVecT<T, 2> V;

  BL_INLINE BLVecT<T, 2>() noexcept = default;
  BL_INLINE BLVecT<T, 2>(const V& other) noexcept = default;

  template<typename U>
  BL_INLINE BLVecT<T, 2>(const BLVecT<U, 2>& other) noexcept
    : x(T(other.x)),
      y(T(other.y)) {}

  explicit BL_INLINE BLVecT<T, 2>(T s) noexcept
    : x(s),
      y(s) {}

  BL_INLINE BLVecT<T, 2>(T ax, T ay) noexcept
    : x(ax),
      y(ay) {}

  BL_INLINE V& operator=(const V& other) noexcept {
    assign(other.x, other.y);
    return *this;
  }

  BL_INLINE void assign(T s) noexcept {
    assign(s, s);
  }

  BL_INLINE void assign(T ax, T ay) noexcept {
    x = ax;
    y = ay;
  }

  template<typename Op>
  static BL_INLINE V unaryOp(const V& a) noexcept {
    return V(Op::op(a.x),
             Op::op(a.y));
  }

  template<typename Op>
  static BL_INLINE V binaryOp(const V& a, const V& b) noexcept {
    return V(Op::op(a.x, b.x),
             Op::op(a.y, b.y));
  }

  template<typename Op>
  static BL_INLINE V binaryOp(const T& a, const V& b) noexcept {
    return V(Op::op(a, b.x),
             Op::op(a, b.y));
  }

  template<typename Op>
  static BL_INLINE V binaryOp(const V& a, const T& b) noexcept {
    return V(Op::op(a.x, b),
             Op::op(a.y, b));
  }
};

template<typename T>
struct BLVecT<T, 3> {
  T x, y, z;

  typedef BLVecT<T, 3> V;

  BL_INLINE BLVecT<T, 3>() noexcept = default;
  BL_INLINE BLVecT<T, 3>(const V& other) noexcept = default;

  template<typename U>
  BL_INLINE BLVecT<T, 3>(const BLVecT<U, 3>& other) noexcept
    : x(T(other.x)),
      y(T(other.y)),
      z(T(other.z)) {}

  explicit BL_INLINE BLVecT<T, 3>(T s) noexcept
    : x(s),
      y(s),
      z(s) {}

  BL_INLINE BLVecT<T, 3>(T ax, T ay, T az) noexcept
    : x(ax),
      y(ay),
      z(az) {}

  BL_INLINE V& operator=(const V& other) noexcept {
    assign(other.x, other.y, other.z);
    return *this;
  }

  BL_INLINE void assign(T s) noexcept {
    assign(s, s, s);
  }

  BL_INLINE void assign(T ax, T ay, T az) noexcept {
    x = ax;
    y = ay;
    z = az;
  }

  template<typename Op>
  static BL_INLINE V unaryOp(const V& a) noexcept {
    return V(Op::op(a.x),
             Op::op(a.y),
             Op::op(a.z));
  }

  template<typename Op>
  static BL_INLINE V binaryOp(const V& a, const V& b) noexcept {
    return V(Op::op(a.x, b.x),
             Op::op(a.y, b.y),
             Op::op(a.z, b.z));
  }

  template<typename Op>
  static BL_INLINE V binaryOp(const T& a, const V& b) noexcept {
    return V(Op::op(a, b.x),
             Op::op(a, b.y),
             Op::op(a, b.z));
  }

  template<typename Op>
  static BL_INLINE V binaryOp(const V& a, const T& b) noexcept {
    return V(Op::op(a.x, b),
             Op::op(a.y, b),
             Op::op(a.z, b));
  }
};

template<typename T>
struct BLVecT<T, 4> {
  T x, y, z, w;

  typedef BLVecT<T, 4> V;

  BL_INLINE BLVecT<T, 4>() noexcept = default;
  BL_INLINE BLVecT<T, 4>(const V& other) noexcept = default;

  template<typename U>
  BL_INLINE BLVecT<T, 4>(const BLVecT<U, 4>& other) noexcept
    : x(T(other.x)),
      y(T(other.y)),
      z(T(other.z)),
      w(T(other.w)) {}

  explicit BL_INLINE BLVecT<T, 4>(T s) noexcept
    : x(s),
      y(s),
      z(s),
      w(s) {}

  BL_INLINE BLVecT<T, 4>(T ax, T ay, T az, T aw) noexcept
    : x(ax),
      y(ay),
      z(az),
      w(aw) {}

  BL_INLINE V& operator=(const V& other) noexcept {
    assign(other.x, other.y, other.z, other.w);
    return *this;
  }

  BL_INLINE void assign(T s) noexcept {
    assign(s, s, s, s);
  }

  BL_INLINE void assign(T ax, T ay, T az, T aw) noexcept {
    x = ax;
    y = ay;
    z = az;
    w = aw;
  }

  template<typename Op>
  static BL_INLINE V unaryOp(const V& a) noexcept {
    return V(Op::op(a.x),
             Op::op(a.y),
             Op::op(a.z),
             Op::op(a.w));
  }

  template<typename Op>
  static BL_INLINE V binaryOp(const V& a, const V& b) noexcept {
    return V(Op::op(a.x, b.x),
             Op::op(a.y, b.y),
             Op::op(a.z, b.z),
             Op::op(a.w, b.w));
  }

  template<typename Op>
  static BL_INLINE V binaryOp(const T& a, const V& b) noexcept {
    return V(Op::op(a, b.x),
             Op::op(a, b.y),
             Op::op(a, b.z),
             Op::op(a, b.w));
  }

  template<typename Op>
  static BL_INLINE V binaryOp(const V& a, const T& b) noexcept {
    return V(Op::op(a.x, b),
             Op::op(a.y, b),
             Op::op(a.z, b),
             Op::op(a.w, b));
  }
};

typedef BLVecT<float, 2> BLVec2F;
typedef BLVecT<float, 3> BLVec3F;
typedef BLVecT<float, 4> BLVec4F;

typedef BLVecT<double, 2> BLVec2D;
typedef BLVecT<double, 3> BLVec3D;
typedef BLVecT<double, 4> BLVec4D;

#define V BLVecT<T, N>

template<typename T, size_t N>
static BL_INLINE V operator-(const V& a) noexcept { return V::template unaryOp<typename BLVecOperators<T>::Neg>(a); }

template<typename T, size_t N>
static BL_INLINE V operator+(const V& a, const V& b) noexcept { return V::template binaryOp<typename BLVecOperators<T>::Add>(a, b); }
template<typename T, size_t N>
static BL_INLINE V operator-(const V& a, const V& b) noexcept { return V::template binaryOp<typename BLVecOperators<T>::Sub>(a, b); }
template<typename T, size_t N>
static BL_INLINE V operator*(const V& a, const V& b) noexcept { return V::template binaryOp<typename BLVecOperators<T>::Mul>(a, b); }
template<typename T, size_t N>
static BL_INLINE V operator/(const V& a, const V& b) noexcept { return V::template binaryOp<typename BLVecOperators<T>::Div>(a, b); }

template<typename T, size_t N>
static BL_INLINE V operator+(const V& a, const T& b) noexcept { return V::template binaryOp<typename BLVecOperators<T>::Add>(a, b); }
template<typename T, size_t N>
static BL_INLINE V operator-(const V& a, const T& b) noexcept { return V::template binaryOp<typename BLVecOperators<T>::Sub>(a, b); }
template<typename T, size_t N>
static BL_INLINE V operator*(const V& a, const T& b) noexcept { return V::template binaryOp<typename BLVecOperators<T>::Mul>(a, b); }
template<typename T, size_t N>
static BL_INLINE V operator/(const V& a, const T& b) noexcept { return V::template binaryOp<typename BLVecOperators<T>::Div>(a, b); }

template<typename T, size_t N>
static BL_INLINE V operator+(const T& a, const V& b) noexcept { return V::template binaryOp<typename BLVecOperators<T>::Add>(a, b); }
template<typename T, size_t N>
static BL_INLINE V operator-(const T& a, const V& b) noexcept { return V::template binaryOp<typename BLVecOperators<T>::Sub>(a, b); }
template<typename T, size_t N>
static BL_INLINE V operator*(const T& a, const V& b) noexcept { return V::template binaryOp<typename BLVecOperators<T>::Mul>(a, b); }
template<typename T, size_t N>
static BL_INLINE V operator/(const T& a, const V& b) noexcept { return V::template binaryOp<typename BLVecOperators<T>::Div>(a, b); }

template<typename T, size_t N>
static BL_INLINE V& operator+=(V& a, const V& b) noexcept { a = V::template binaryOp<typename BLVecOperators<T>::Add>(a, b); return a; }
template<typename T, size_t N>
static BL_INLINE V& operator-=(V& a, const V& b) noexcept { a = V::template binaryOp<typename BLVecOperators<T>::Sub>(a, b); return a; }
template<typename T, size_t N>
static BL_INLINE V& operator*=(V& a, const V& b) noexcept { a = V::template binaryOp<typename BLVecOperators<T>::Mul>(a, b); return a; }
template<typename T, size_t N>
static BL_INLINE V& operator/=(V& a, const V& b) noexcept { a = V::template binaryOp<typename BLVecOperators<T>::Div>(a, b); return a; }

template<typename T, size_t N>
static BL_INLINE V& operator+=(V& a, const T& b) noexcept { a = V::template binaryOp<typename BLVecOperators<T>::Add>(a, b); return a; }
template<typename T, size_t N>
static BL_INLINE V& operator-=(V& a, const T& b) noexcept { a = V::template binaryOp<typename BLVecOperators<T>::Sub>(a, b); return a; }
template<typename T, size_t N>
static BL_INLINE V& operator*=(V& a, const T& b) noexcept { a = V::template binaryOp<typename BLVecOperators<T>::Mul>(a, b); return a; }
template<typename T, size_t N>
static BL_INLINE V& operator/=(V& a, const T& b) noexcept { a = V::template binaryOp<typename BLVecOperators<T>::Div>(a, b); return a; }

template<typename T, size_t N>
BL_INLINE V blAbs(const V& a) noexcept { return V::template unaryOp<typename BLVecOperators<T>::Abs>(a); }

template<typename T, size_t N>
BL_INLINE V blMin(const V& a, const V& b) noexcept { return V::template binaryOp<typename BLVecOperators<T>::Min>(a, b); }

template<typename T, size_t N>
BL_INLINE V blMax(const V& a, const V& b) noexcept { return V::template binaryOp<typename BLVecOperators<T>::Max>(a, b); }

#undef V

static BL_INLINE double blLengthSq(const BLVec2D& v) noexcept { return v.x * v.x + v.y * v.y; }
static BL_INLINE double blLengthSq(const BLVec2D& a, const BLVec2D& b) noexcept { return blLengthSq(b - a); }

static BL_INLINE double blLength(const BLVec2D& v) noexcept { return blSqrt(blLengthSq(v)); }
static BL_INLINE double blLength(const BLVec2D& a, const BLVec2D& b) noexcept { return blSqrt(blLengthSq(a, b)); }

static BL_INLINE BLVec2D blNormal(const BLVec2D& v) noexcept { return BLVec2D(-v.y, v.x); }
static BL_INLINE BLVec2D blUnitVector(const BLVec2D& v) noexcept { return v / blLength(v); }

static BL_INLINE double blDotProduct(const BLVec2D& a, const BLVec2D& b) noexcept { return a.x * b.x + a.y * b.y; }
static BL_INLINE double blCrossProduct(const BLVec2D& a, const BLVec2D& b) noexcept { return a.x * b.y - a.y * b.x; }

/*
static BL_INLINE bool blIsNaN(const BLPoint& p) noexcept { return blIsNaN(p.x, p.y); }
static BL_INLINE bool blIsFinite(const BLPoint& p) noexcept { return blIsFinite(p.x, p.y); }
static BL_INLINE bool blIsFinite(const BLBox& b) noexcept { return blIsFinite(b.x0, b.y0, b.x1, b.y1); }
static BL_INLINE bool blIsFinite(const BLRect& r) noexcept { return blIsFinite(r.x, r.y, r.w, r.h); }
*/

//! \}
//! \endcond

#endif // BLEND2D_GEOMETRY_P_H_INCLUDED
