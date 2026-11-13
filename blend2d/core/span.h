// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_SPAN_H_INCLUDED
#define BLEND2D_CORE_SPAN_H_INCLUDED

#include <blend2d/core/api.h>

//! \addtogroup bl_containers
//! \{

#ifdef __cplusplus
//! A non-owning view of contiguous data of type `T`.
template<typename T>
class BLSpan {
public:
  T* _data {};
  size_t _size {};

  //! Resets the span to its default constructed state, which sets its `data` to `nullptr` and `size` to `0`.
  BL_INLINE_CONSTEXPR void reset() noexcept { *this = BLSpan{}; }
  //! Resets the span to point to `data` of the given `size`.
  BL_INLINE_CONSTEXPR void reset(const T* data, size_t size) noexcept { *this = BLSpan{data, size}; }

  //! Tests whether the span is empty - returns `true` when `size() == 0u`.
  [[nodiscard]] BL_INLINE_CONSTEXPR bool is_empty() const noexcept { return _size == 0u; }
  //! Returns pointer to the beginning of data.
  [[nodiscard]] BL_INLINE_CONSTEXPR T* data() const noexcept { return _data; }
  //! Returns span size.
  [[nodiscard]] BL_INLINE_CONSTEXPR size_t size() const noexcept { return _size; }

  //! Returns the beginning of span data.
  [[nodiscard]] BL_INLINE_CONSTEXPR T* begin() const noexcept requires (!std::is_void_v<T>) { return _data; }
  //! Returns the end of span data (the end doesn't point to valid data).
  [[nodiscard]] BL_INLINE_CONSTEXPR T* end() const noexcept requires (!std::is_void_v<T>) { return _data + _size; }

  //! \name Equality & Comparison
  //! \{

  //! Returns whether this span is equal with `other` by first comparing span sizes and
  //! then all their elements if both sizes match.
  //!
  //! \note This function can be used to compare spans that use a different data-type,
  //! however, such data types must be comparable or have overloaded equality operator.
  template<typename X>
  [[nodiscard]] BL_INLINE bool equals(BLSpan<X> other) const noexcept {
    size_t size = _size;

    if (size != other.size()) {
      return false;
    }

    for (size_t i = 0u; i < size; i++) {
      if (!(_data[i] == other._data[i])) {
        return false;
      }
    }

    return true;
  }

  //! \}

  //! \name Search
  //! \{

  //! Returns the first index at which a given `item` can be found in the array, or `SIZE_MAX` if not found.
  [[nodiscard]] BL_INLINE size_t index_of(const T& item) const noexcept {
    return index_of(item, 0);
  }

  //! Returns the index at which a given `item` can be found in the array starting from `from_index`, or `SIZE_MAX`
  //! if not present.
  [[nodiscard]] BL_INLINE size_t index_of(const T& item, size_t from_index) const noexcept {
    const T* self_data = data();
    size_t self_size = size();

    for (size_t i = from_index; i < self_size; i++) {
      if (self_data[i] == item) {
        return i;
      }
    }
    return SIZE_MAX;
  }

  //! Returns the last index at which a given `item` can be found in the array, or `SIZE_MAX` if not present.
  [[nodiscard]] BL_INLINE size_t last_index_of(const T& item) const noexcept {
    const T* self_data = data();
    size_t i = size();

    while (--i != SIZE_MAX && !(self_data[i] == item)) {
      continue;
    }
    return i;
  }

  //! Returns the index at which a given `item` can be found in the array starting from `from_index` and ending
  //! at `0`, or `SIZE_MAX` if not present.
  [[nodiscard]] BL_INLINE size_t last_index_of(const T& item, size_t from_index) const noexcept {
    const T* self_data = data();
    size_t i = size() - 1u;

    if (i == SIZE_MAX) {
      return i;
    }

    i = bl_min<size_t>(i, from_index);
    while (!(self_data[i] == item) && --i != SIZE_MAX) {
      continue;
    }

    return i;
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  [[nodiscard]] BL_INLINE auto& operator[](size_t index) noexcept requires (!std::is_void_v<T>) {
    BL_ASSERT_BOUNDS(index < _size);
    return _data[index];
  }

  //! Returns whether this span is equal with `other`.
  template<typename X> [[nodiscard]] BL_INLINE bool operator==(BLSpan<X> other) noexcept { return equals(other); }

  //! Returns whether this span is equal with `other`.
  template<typename X> [[nodiscard]] BL_INLINE bool operator!=(BLSpan<X> other) noexcept { return !equals(other); }

  //! \}
};

using BLStringView = BLSpan<const char>;

template<typename S, typename T>
concept BLSpanConvertible = requires(S& s) {
  { s.data() } -> std::convertible_to<T*>;
  { s.size() } -> std::convertible_to<size_t>;

  // Require matching element types while allowing added constness.
  requires std::same_as<std::remove_cv_t<std::remove_pointer_t<std::remove_cvref_t<decltype(s.data())>>>,
                        std::remove_cv_t<T>>;
};

namespace bl { using StringView = BLStringView; }
namespace bl { template<typename T> using Span = BLSpan<T>; }

#else
typedef struct {
  const char* _data;
  size_t _size;
} BLStringView;
#endif

//! \}

#endif // BLEND2D_CORE_SPAN_H_INCLUDED
