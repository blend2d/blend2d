// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib


#ifndef BLEND2D_AXL_ARRAY_P_H_INCLUDED
#define BLEND2D_AXL_ARRAY_P_H_INCLUDED

#include <blend2d/axl/commons.h>
#include <blend2d/axl/span_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_support
//! \{

namespace bl::axl {

//! Enhanced array similar to `std::array<T, N>`, with extended features, like the possibility
//! to use enums in operator[] or to convert the array to `Span` via `span()` and `span_mut()`.
//!
//! \note The array has C semantics - the elements in the array are not initialized unless
//! the have a default constructor.
template<typename T, size_t N>
struct Array {
  //! \name Members
  //! \{

  //! The underlying array data, use `data()` to access it.
  T _data[N];

  //! \}

  //! \name Equality & Comparison
  //! \{

  BL_INLINE_CONSTEXPR bool equals(const Array& other) const noexcept {
    for (size_t i = 0; i < N; i++) {
      if (_data[i] != other._data[i]) {
        return false;
      }
    }
    return true;
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  template<typename Index>
  BL_INLINE_CONSTEXPR T& operator[](const Index& index) noexcept {
    BL_ASSERT(as_std_uint(index) < N);
    return _data[as_std_uint(index)];
  }

  template<typename Index>
  BL_INLINE_CONSTEXPR const T& operator[](const Index& index) const noexcept {
    BL_ASSERT(as_std_uint(index) < N);
    return _data[as_std_uint(index)];
  }

  BL_INLINE_CONSTEXPR bool operator==(const Array& other) const noexcept { return equals(other); }
  BL_INLINE_CONSTEXPR bool operator!=(const Array& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Accessors
  //! \{

  [[nodiscard]]
  BL_INLINE_CONSTEXPR Span<const T> span() const noexcept { return Span<const T>(_data, N); }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR Span<T> span_mut() noexcept { return Span<T>(_data, N); }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR bool is_empty() const noexcept { return false; }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR size_t size() const noexcept { return N; }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR T* data() noexcept { return _data; }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR const T* data() const noexcept { return _data; }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR T& front() noexcept { return _data[0]; }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR const T& front() const noexcept { return _data[0]; }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR T& back() noexcept { return _data[N - 1]; }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR const T& back() const noexcept { return _data[N - 1]; }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR T* begin() noexcept { return _data; }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR T* end() noexcept { return _data + N; }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR const T* begin() const noexcept { return _data; }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR const T* end() const noexcept { return _data + N; }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR const T* cbegin() const noexcept { return _data; }

  [[nodiscard]]
  BL_INLINE_CONSTEXPR const T* cend() const noexcept { return _data + N; }

  //! \}

  //! \name Utilities
  //! \{

  BL_INLINE void swap(Array& other) noexcept {
    for (size_t i = 0; i < N; i++) {
      axl::swap(_data[i], other._data[i]);
    }
  }

  BL_INLINE void fill(const T& value) noexcept {
    for (size_t i = 0; i < N; i++) {
      _data[i] = value;
    }
  }

  BL_INLINE void copy_from(const Array& other) noexcept {
    for (size_t i = 0; i < N; i++) {
      _data[i] = other._data[i];
    }
  }

  template<typename Operator>
  BL_INLINE void combine(const Array& other) noexcept {
    for (size_t i = 0; i < N; i++) {
      _data[i] = Operator::op(_data[i], other._data[i]);
    }
  }

  template<typename Operator>
  BL_INLINE T aggregate(T initial_value = T()) const noexcept {
    T value = initial_value;
    for (size_t i = 0; i < N; i++) {
      value = Operator::op(value, _data[i]);
    }
    return value;
  }

  template<typename Fn>
  BL_INLINE void for_each(Fn&& fn) noexcept {
    for (size_t i = 0; i < N; i++) {
      fn(_data[i]);
    }
  }
  //! \}
};

} // {bl::axl}

//! \}
//! \endcond

#endif // BLEND2D_AXL_ARRAY_P_H_INCLUDED
