// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_STRING_H_INCLUDED
#define BLEND2D_CORE_STRING_H_INCLUDED

#include <blend2d/core/object_layer.h>
#include <blend2d/core/span.h>

//! \addtogroup bl_c_api
//! \{

//! \name BLString - C API
//! \{

//! \cond INTERNAL

#if defined(__cplusplus)
//! Byte string [Impl].
struct BLStringImpl {};
#endif // __cplusplus

//! \endcond

//! Byte string [C API].
struct BLStringCore BL_INHERITS_FROM(BLObjectCore) {
  BL_DEFINE_OBJECT(BLString, bl::ObjectLayer::ContainerInfo<
    BL_OBJECT_TYPE_STRING,
    bl::ObjectLayer::ObjectMode::kAny,
    bl::ObjectLayer::ObjectLayout::kNative,
    BLObjectDetail::kSSOCapacity, 0u, BLObjectDetail::kMetaByteIndex>
  )
};

BL_EXTERN_C_BEGIN

BL_API BLResult BL_CDECL bl_string_init(BLStringCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_init_move(BLStringCore* self, BLStringCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_init_weak(BLStringCore* self, const BLStringCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_init_with_data(BLStringCore* self, const char* data, size_t data_size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_init_with_cstr(BLStringCore* self, const char* cstr) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_init_with_spans(BLStringCore* self, const BLStringView* spans, size_t count) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_destroy(BLStringCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_reset(BLStringCore* self) BL_NOEXCEPT_C;
BL_API const char* BL_CDECL bl_string_get_data(const BLStringCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API size_t BL_CDECL bl_string_get_size(const BLStringCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API size_t BL_CDECL bl_string_get_capacity(const BLStringCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API BLResult BL_CDECL bl_string_clear(BLStringCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_shrink(BLStringCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_reserve(BLStringCore* self, size_t capacity) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_resize(BLStringCore* self, size_t new_size, char fill) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_truncate(BLStringCore* self, size_t truncate_size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_make_mutable(BLStringCore* self, char** data_out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_assign_move(BLStringCore* self, BLStringCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_assign_weak(BLStringCore* self, const BLStringCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_assign_char(BLStringCore* self, char c, size_t count) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_assign_data(BLStringCore* self, const char* data, size_t data_size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_assign_cstr(BLStringCore* self, const char* cstr) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_assign_format(BLStringCore* self, const char* fmt, ...) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_assign_format_v(BLStringCore* self, const char* fmt, va_list ap) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_assign_uninitialized(BLStringCore* self, size_t data_size, char** data_out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_append_char(BLStringCore* self, char c, size_t count) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_append_data(BLStringCore* self, const char* data, size_t data_size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_append_cstr(BLStringCore* self, const char* cstr) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_append_string(BLStringCore* self, const BLStringCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_append_format(BLStringCore* self, const char* fmt, ...) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_append_format_v(BLStringCore* self, const char* fmt, va_list ap) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_append_uninitialized(BLStringCore* self, size_t data_size, char** data_out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_insert_char(BLStringCore* self, size_t index, char c, size_t count) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_insert_data(BLStringCore* self, size_t index, const char* data, size_t data_size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_insert_cstr(BLStringCore* self, size_t index, const char* cstr) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_insert_string(BLStringCore* self, size_t index, const BLStringCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_insert_uninitialized(BLStringCore* self, size_t index, size_t data_size, char** data_out) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_remove_index(BLStringCore* self, size_t index) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL bl_string_remove_range(BLStringCore* self, size_t start, size_t end) BL_NOEXCEPT_C;
BL_API bool BL_CDECL bl_string_equals(const BLStringCore* a, const BLStringCore* b) BL_NOEXCEPT_C BL_PURE;
BL_API bool BL_CDECL bl_string_equals_data(const BLStringCore* self, const char* data, size_t data_size) BL_NOEXCEPT_C BL_PURE;
BL_API bool BL_CDECL bl_string_equals_cstr(const BLStringCore* self, const char* cstr) BL_NOEXCEPT_C BL_PURE;
BL_API int BL_CDECL bl_string_compare(const BLStringCore* a, const BLStringCore* b) BL_NOEXCEPT_C BL_PURE;
BL_API int BL_CDECL bl_string_compare_data(const BLStringCore* self, const char* data, size_t data_size) BL_NOEXCEPT_C BL_PURE;
BL_API int BL_CDECL bl_string_compare_cstr(const BLStringCore* self, const char* cstr) BL_NOEXCEPT_C BL_PURE;

BL_EXTERN_C_END

//! \}
//! \}

//! \addtogroup bl_containers
//! \{

//! \name BLString - C++ API
//! \{
#ifdef __cplusplus

//! Byte string [C++ API].
//!
//! Blend2D always uses UTF-8 encoding in public APIs so all strings are assumed UTF-8 by default. However, `BLString`
//! doesn't guarantee any assumptions about the encoding of the data it holds. It can hold arbitrary byte sequence and
//! act as a raw byte-string when this functionality is desired.
class BLString final : public BLStringCore {
public:
  //! \name Construction & Destruction
  //! \{

  //! Creates an empty string.
  BL_INLINE_NODEBUG BLString() noexcept {
    bl::ObjectLayer::init_sso(this);
  }

  //! Move constructor.
  //!
  //! \note The `other` string is always reset by a move construction, so it becomes an empty string.
  BL_INLINE_NODEBUG BLString(BLString&& other) noexcept {
    _d = other._d;
    bl::ObjectLayer::init_sso(&other);
  }

  //! Copy constructor, performs weak copy of the data held by the `other` string.
  BL_INLINE_NODEBUG BLString(const BLString& other) noexcept {
    if (bl::ObjectLayer::is_comptime_sso(&other) && other.size() <= Info::kSSOCapacity) {
      size_t size = other.size();
      bl::ObjectLayer::init_sso_with_data(this, size, BLSpan<const char>(bl::ObjectLayer::sso_data<char>(&other), size));
      return;
    }

    bl_string_init_weak(this, &other);
  }

  //! Constructor that creates a string from the given `data` of size `data_size`.
  //!
  //! \remarks This is a convenience function that doesn't provide error handling. If size exceeds static
  //! string capacity (SSO) and dynamic allocation failed then a default empty string would be constructed.
  BL_INLINE_NODEBUG explicit BLString(const char* data, size_t data_size) noexcept {
    if BL_BUILTIN_CONSTANT_P(data_size) {
      if (data_size <= Info::kSSOCapacity) {
        bl::ObjectLayer::init_sso_with_data(this, data_size, BLSpan<const char>(data, data_size));
        return;
      }
    }

    bl_string_init_with_data(this, data, data_size);
  }

  //! Constructor that creates a string from a null-terminated string `cstr`.
  //!
  //! \remarks This is a convenience function that doesn't provide error handling. If size exceeds static
  //! string capacity (SSO) and dynamic allocation failed then a default empty string would be constructed.
  BL_INLINE_NODEBUG explicit BLString(const char* cstr) noexcept {
    if BL_BUILTIN_CONSTANT_P(strlen(cstr)) {
      size_t cstr_length = strlen(cstr);
      if (cstr_length <= Info::kSSOCapacity) {
        bl::ObjectLayer::init_sso_with_data(this, cstr_length, BLSpan<const char>(cstr, cstr_length));
      }
      else {
        bl_string_init_with_data(this, cstr, cstr_length);
      }
      return;
    }

    bl_string_init_with_cstr(this, cstr);
  }

  //! Constructor that creates a string from the given string `view`.
  //!
  //! \note See other constructors for more details.
  BL_INLINE_NODEBUG explicit BLString(BLStringView span) noexcept
    : BLString(span.data(), span.size()) {}

  //! Constructor that creates a string from a list of string views.
  //!
  //! \note See other constructors for more details.
  BL_INLINE_NODEBUG explicit BLString(BLSpan<const BLStringView> spans) noexcept {
    bl_string_init_with_spans(this, spans.data(), spans.size());
  }

  //! Destroys the string.
  BL_INLINE_NODEBUG ~BLString() noexcept {
    if (bl::ObjectLayer::requires_destroy(this)) {
      bl_string_destroy(this);
    }
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  //! Tests whether the string has a content.
  //!
  //! \note This is essentially the opposite of `is_empty()`.
  BL_INLINE_NODEBUG explicit operator bool() const noexcept { return !is_empty(); }

  //! Move assignment.
  //!
  //! \note The `other` string is reset by move construction, so it becomes an empty string.
  BL_INLINE_NODEBUG BLString& operator=(BLString&& other) noexcept { bl_string_assign_move(this, &other); return *this; }
  //! Copy assignment, performs weak copy of the data held by the `other` string.
  BL_INLINE_NODEBUG BLString& operator=(const BLString& other) noexcept { bl_string_assign_weak(this, &other); return *this; }

  [[nodiscard]] BL_INLINE_NODEBUG bool operator==(const BLString& other) const noexcept { return  equals(other); }
  [[nodiscard]] BL_INLINE_NODEBUG bool operator!=(const BLString& other) const noexcept { return !equals(other); }
  [[nodiscard]] BL_INLINE_NODEBUG bool operator< (const BLString& other) const noexcept { return compare(other) <  0; }
  [[nodiscard]] BL_INLINE_NODEBUG bool operator<=(const BLString& other) const noexcept { return compare(other) <= 0; }
  [[nodiscard]] BL_INLINE_NODEBUG bool operator> (const BLString& other) const noexcept { return compare(other) >  0; }
  [[nodiscard]] BL_INLINE_NODEBUG bool operator>=(const BLString& other) const noexcept { return compare(other) >= 0; }

  [[nodiscard]] BL_INLINE_NODEBUG bool operator==(const BLStringView s) const noexcept { return  equals(s); }
  [[nodiscard]] BL_INLINE_NODEBUG bool operator!=(const BLStringView s) const noexcept { return !equals(s); }
  [[nodiscard]] BL_INLINE_NODEBUG bool operator< (const BLStringView s) const noexcept { return compare(s) <  0; }
  [[nodiscard]] BL_INLINE_NODEBUG bool operator<=(const BLStringView s) const noexcept { return compare(s) <= 0; }
  [[nodiscard]] BL_INLINE_NODEBUG bool operator> (const BLStringView s) const noexcept { return compare(s) >  0; }
  [[nodiscard]] BL_INLINE_NODEBUG bool operator>=(const BLStringView s) const noexcept { return compare(s) >= 0; }

  [[nodiscard]] BL_INLINE_NODEBUG bool operator==(const char* s) const noexcept { return equals(s); }
  [[nodiscard]] BL_INLINE_NODEBUG bool operator!=(const char* s) const noexcept { return !equals(s); }
  [[nodiscard]] BL_INLINE_NODEBUG bool operator< (const char* s) const noexcept { return compare(s) <  0; }
  [[nodiscard]] BL_INLINE_NODEBUG bool operator<=(const char* s) const noexcept { return compare(s) <= 0; }
  [[nodiscard]] BL_INLINE_NODEBUG bool operator> (const char* s) const noexcept { return compare(s) >  0; }
  [[nodiscard]] BL_INLINE_NODEBUG bool operator>=(const char* s) const noexcept { return compare(s) >= 0; }

  //! Returns the content of the string as `BLStringView`.
  [[nodiscard]] BL_INLINE_NODEBUG operator BLStringView() const noexcept { return {data(), size()}; }

  //! Returns a character at the given `index`.
  //!
  //! \note This is the same as calling `at(index)`.
  [[nodiscard]]
  BL_INLINE_NODEBUG const char& operator[](size_t index) const noexcept { return at(index); }

  //! \}

  //! \name Common Functionality
  //! \{

  //! Clears the content of the string and releases its data.
  //!
  //! After reset the string content matches a default constructed string.
  BL_INLINE_NODEBUG BLResult reset() noexcept { return bl_string_reset(this); }

  //! Swaps the content of this string with the `other` string.
  BL_INLINE_NODEBUG void swap(BLString& other) noexcept { _d.swap(other._d); }

  //! \}

  //! \name Accessors
  //! \{

  //! Returns the size of the string [in bytes].
  [[nodiscard]] BL_INLINE_NODEBUG size_t size() const noexcept { return bl::ObjectLayer::get_size(this); }
  //! Returns the capacity of the string [in bytes].
  [[nodiscard]] BL_INLINE_NODEBUG size_t capacity() const noexcept { return bl::ObjectLayer::get_capacity(this); }
  //! Tests whether the string is empty (its `size() == 0`).
  [[nodiscard]] BL_INLINE_NODEBUG bool is_empty() const noexcept { return size() == 0u; }

  //! Returns a pointer to the data of the string (always const).
  [[nodiscard]] BL_INLINE_NODEBUG const char* data() const noexcept { return bl::ObjectLayer::get_data<char>(this); }
  //! Returns a pointer to the beginning of string data (iterator compatibility).
  [[nodiscard]] BL_INLINE_NODEBUG const char* begin() const noexcept { return data(); }
  //! Returns a pointer to the end of string data (iterator compatibility).
  //!
  //! The returned pointer points to the null terminator of the string.
  [[nodiscard]] BL_INLINE_NODEBUG const char* end() const noexcept { return data() + size(); }

  //! Returns the content of the string as `BLStringView`.
  [[nodiscard]] BL_INLINE_NODEBUG BLStringView span() const noexcept { return {data(), size()}; }

  //! Returns a character at the given `index`.
  //!
  //! \note Index must be valid and cannot be out of bounds - there is an assertion.
  [[nodiscard]] BL_INLINE const char& at(size_t index) const noexcept {
    BL_ASSERT_BOUNDS(index <= size());
    return data()[index];
  }

  //! \}

  //! \name Data Manipulation
  //! \{

  //! Clears the content of the string without releasing its dynamically allocated data, if possible.
  BL_INLINE_NODEBUG BLResult clear() noexcept {
    return bl_string_clear(this);
  }

  //! Shrinks the capacity of the string to match the actual content.
  BL_INLINE_NODEBUG BLResult shrink() noexcept {
    return bl_string_shrink(this);
  }

  //! Reserves at least `n` bytes in the string for further manipulation (most probably appending).
  BL_INLINE_NODEBUG BLResult reserve(size_t capacity) noexcept {
    return bl_string_reserve(this, capacity);
  }

  //! Resizes the string to exactly `new_size` and fills the additional data by `fill` pattern if `new_size`
  //! is greater than the current length.
  BL_INLINE_NODEBUG BLResult resize(size_t new_size, char fill = '\0') noexcept {
    return bl_string_resize(this, new_size, fill);
  }

  //! Truncates the string length to `truncated_size`. It does nothing when the length of the string is less
  //! than `truncated_size`.
  BL_INLINE_NODEBUG BLResult truncate(size_t truncated_size) noexcept {
    return bl_string_truncate(this, truncated_size);
  }

  //! Makes the string mutable.
  //!
  //! This operation checks whether the string is mutable and if not it makes a deep copy of its content so it can be
  //! modified. Please note that you can only modify the content that is defined by its length property. Even if the
  //! string had higher capacity before `make_mutable()` it's not guaranteed that the possible new data would match that
  //! capacity.
  //!
  //! If you want to make the string mutable for the purpose of assigning, appending, prepending, or inserting data,
  //! consider using \ref assign_uninitialized(), \ref append_uninitialized(), \ref prepend_uninitialized(), and
  //! \ref insert_uninitialized() - these functions can be used to make string mutable and to reserve space for the
  //! required operation, expecting the user to fill the uninitialized data.
  BL_INLINE_NODEBUG BLResult make_mutable(char** data_out) noexcept {
    return bl_string_make_mutable(this, data_out);
  }

  //! Copy assignment, the same as `operator=()`, but returns a `BLResult` instead of `this`.
  BL_INLINE_NODEBUG BLResult assign(const BLString& s) noexcept {
    return bl_string_assign_weak(this, &s);
  }

  //! Move assignment, the same as `operator=()`, but returns a `BLResult` instead of `this`.
  BL_INLINE_NODEBUG BLResult assign(BLString&& s) noexcept {
    return bl_string_assign_move(this, &s);
  }

  //! Copy assignment, but creates a deep copy of the `other` string instead of weak copy.
  BL_INLINE_NODEBUG BLResult assign_deep(const BLString& other) noexcept {
    return assign(other.span());
  }

  //! Replaces the content of the string by `c` character or multiple characters if `count` is greater than one.
  BL_INLINE_NODEBUG BLResult assign(char c, size_t count = 1) noexcept {
    return bl_string_assign_char(this, c, count);
  }

  //! Replaces the string by `str` data of the given length `data_size`.
  BL_INLINE_NODEBUG BLResult assign(const char* data, size_t data_size) noexcept {
    if BL_BUILTIN_CONSTANT_P(data_size) {
      if (bl::ObjectLayer::is_comptime_sso(this) && data_size < Info::kSSOCapacity) {
        // ALIASING: data can alias the current SSO string content, so use a temporary BLString data.
        BLStringCore tmp;
        bl::ObjectLayer::init_sso_with_data(&tmp, data_size, BLSpan<const char>(data, data_size));
        _d = tmp._d;
        return BL_SUCCESS;
      }
    }

    return bl_string_assign_data(this, data, data_size);
  }

  //! Replaces the string by a null-terminated `cstr`.
  BL_INLINE_NODEBUG BLResult assign(const char* cstr) noexcept {
    if BL_BUILTIN_CONSTANT_P(strlen(cstr)) {
      return assign(cstr, strlen(cstr));
    }

    return bl_string_assign_cstr(this, cstr);
  }

  //! Replaces the string by the content described by the given string `view`.
  BL_INLINE_NODEBUG BLResult assign(BLStringView span) noexcept {
    return bl_string_assign_data(this, span.data(), span.size());
  }

  //! Replaces the content of the string by a result of calling `snprintf(fmt, args...)`.
  template<typename... Args>
  BL_INLINE_NODEBUG BLResult assign_format(const char* fmt, Args&&... args) noexcept {
    return bl_string_assign_format(this, fmt, bl::Internal::forward<Args>(args)...);
  }

  //! Replaces the content of the string by a result of calling `vsnprintf(fmt, ap)`.
  BL_INLINE_NODEBUG BLResult assign_format_v(const char* fmt, va_list ap) noexcept {
    return bl_string_assign_format_v(this, fmt, ap);
  }

  BL_INLINE_NODEBUG BLResult assign_uninitialized(size_t data_size, char** data_out) noexcept {
    return bl_string_assign_uninitialized(this, data_size, data_out);
  }

  BL_INLINE_NODEBUG BLResult append(char c, size_t count = 1) noexcept {
    return bl_string_append_char(this, c, count);
  }

  BL_INLINE_NODEBUG BLResult append(const char* data, size_t data_size) noexcept {
    if (bl::ObjectLayer::is_comptime_sso(this) && Info::kSSOCapacity - bl::ObjectLayer::sso_size(this) >= data_size) {
      return bl::ObjectLayer::append_sso_data(this, BLSpan<const char>(data, data_size));
    }

    return bl_string_append_data(this, data, data_size);
  }

  BL_INLINE_NODEBUG BLResult append(const char* cstr) noexcept {
    if BL_BUILTIN_CONSTANT_P(strlen(cstr)) {
      return append(cstr, strlen(cstr));
    }

    return bl_string_append_cstr(this, cstr);
  }

  BL_INLINE_NODEBUG BLResult append(const BLString& s) noexcept {
    return bl_string_append_string(this, &s);
  }

  BL_INLINE_NODEBUG BLResult append(BLStringView span) noexcept {
    return append(span.data(), span.size());
  }

  template<typename... Args>
  BL_INLINE_NODEBUG BLResult append_format(const char* fmt, Args&&... args) noexcept {
    return bl_string_append_format(this, fmt, bl::Internal::forward<Args>(args)...);
  }

  BL_INLINE_NODEBUG BLResult append_format_v(const char* fmt, va_list ap) noexcept {
    return bl_string_append_format_v(this, fmt, ap);
  }

  BL_INLINE_NODEBUG BLResult append_uninitialized(size_t data_size, char** data_out) noexcept {
    return bl_string_append_uninitialized(this, data_size, data_out);
  }

  BL_INLINE_NODEBUG BLResult prepend(char c, size_t n = 1) noexcept {
    return bl_string_insert_char(this, 0u, c, n);
  }

  BL_INLINE_NODEBUG BLResult prepend(const char* data, size_t data_size) noexcept {
    return bl_string_insert_data(this, 0u, data, data_size);
  }

  BL_INLINE_NODEBUG BLResult prepend(const char* cstr) noexcept {
    if BL_BUILTIN_CONSTANT_P(strlen(cstr)) {
      return prepend(cstr, strlen(cstr));
    }
    else {
      return bl_string_insert_cstr(this, 0u, cstr);
    }
  }

  BL_INLINE_NODEBUG BLResult prepend(const BLString& s) noexcept {
    return bl_string_insert_string(this, 0u, &s);
  }

  BL_INLINE_NODEBUG BLResult prepend(BLStringView span) noexcept {
    return prepend(span.data(), span.size());
  }

  BL_INLINE_NODEBUG BLResult prepend_uninitialized(size_t data_size, char** data_out) noexcept {
    return bl_string_insert_uninitialized(this, 0u, data_size, data_out);
  }

  BL_INLINE_NODEBUG BLResult insert(size_t index, char c, size_t n = 1) noexcept {
    return bl_string_insert_char(this, index, c, n);
  }

  BL_INLINE_NODEBUG BLResult insert(size_t index, const char* data, size_t data_size) noexcept {
    return bl_string_insert_data(this, index, data, data_size);
  }

  BL_INLINE_NODEBUG BLResult insert(size_t index, const char* cstr) noexcept {
    if BL_BUILTIN_CONSTANT_P(strlen(cstr)) {
      return insert(index, cstr, strlen(cstr));
    }
    else {
      return bl_string_insert_cstr(this, index, cstr);
    }
  }

  BL_INLINE_NODEBUG BLResult insert(size_t index, const BLString& s) noexcept {
    return bl_string_insert_string(this, index, &s);
  }

  BL_INLINE_NODEBUG BLResult insert(size_t index, BLStringView span) noexcept {
    return insert(index, span.data(), span.size());
  }

  BL_INLINE_NODEBUG BLResult insert_uninitialized(size_t index, size_t data_size, char** data_out) noexcept {
    return bl_string_insert_uninitialized(this, index, data_size, data_out);
  }

  BL_INLINE_NODEBUG BLResult remove(size_t index) noexcept {
    return bl_string_remove_index(this, index);
  }

  BL_INLINE_NODEBUG BLResult remove(BLRange range) noexcept {
    return bl_string_remove_range(this, range.start, range.end);
  }

  //! \name Equality & Comparison
  //! \{

  //! Returns whether this string is equal with `other` (i.e. their contents match).
  [[nodiscard]] BL_INLINE_NODEBUG bool equals(const BLStringCore& other) const noexcept {
    return bl_string_equals(this, &other);
  }

  //! Returns whether this string is equal with the content of `span`.
  [[nodiscard]] BL_INLINE_NODEBUG bool equals(BLStringView span) const noexcept {
    return bl_string_equals_data(this, span.data(), span.size());
  }

  //! Returns whether this string is equal with the content of span given as `data` and `data_size`.
  [[nodiscard]] BL_INLINE_NODEBUG bool equals(const char* data, size_t data_size) const noexcept {
    return bl_string_equals_data(this, data, data_size);
  }

  //! Returns whether this string is equal with the given null terminated string `cstr`.
  [[nodiscard]] BL_INLINE_NODEBUG bool equals(const char* cstr) const noexcept {
    if BL_BUILTIN_CONSTANT_P(strlen(cstr)) {
      return equals(cstr, strlen(cstr));
    }
    else {
      return bl_string_equals_cstr(this, cstr);
    }
  }

  //! Compares this string with `other` string.
  [[nodiscard]] BL_INLINE_NODEBUG int compare(const BLString& other) const noexcept {
    return bl_string_compare(this, &other);
  }

  //! Compares this string with the content of `span`.
  [[nodiscard]] BL_INLINE_NODEBUG int compare(BLStringView span) const noexcept {
    return bl_string_compare_data(this, span.data(), span.size());
  }

  //! Compares this string with the content of span given as `data` and `data_size`.
  [[nodiscard]] BL_INLINE_NODEBUG int compare(const char* data, size_t data_size) const noexcept {
    return bl_string_compare_data(this, data, data_size);
  }

  //! Compares this string with a null terminated string `cstr`.
  [[nodiscard]] BL_INLINE_NODEBUG int compare(const char* cstr) const noexcept {
    if BL_BUILTIN_CONSTANT_P(strlen(cstr)) {
      return compare(cstr, strlen(cstr));
    }
    else {
      return bl_string_compare_cstr(this, cstr);
    }
  }

  //! \}

  //! \name Search
  //! \{

  //! Returns the first index at which a given character `c` can be found in the string, or `SIZE_MAX` if not present.
  [[nodiscard]] BL_INLINE size_t index_of(char c) const noexcept {
    return span().index_of(c);
  }

  //! Returns the index at which a given character `c` can be found in the string starting from `from_index`, or `SIZE_MAX`
  //! if not present.
  [[nodiscard]] BL_INLINE size_t index_of(char c, size_t from_index) const noexcept {
    return span().index_of(c, from_index);
  }

  //! Returns the last index at which a given character `c` can be found in the string, or `SIZE_MAX` if not present.
  [[nodiscard]] BL_INLINE size_t last_index_of(char c) const noexcept {
    return span().last_index_of(c);
  }

  //! Returns the index at which a given character `c` can be found in the string starting from `from_index` and ending
  //! at `0`, or `SIZE_MAX` if not present.
  [[nodiscard]] BL_INLINE size_t last_index_of(char c, size_t from_index) const noexcept {
    return span().last_index_of(c, from_index);
  }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_CORE_STRING_H_INCLUDED
