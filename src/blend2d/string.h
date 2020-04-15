// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_STRING_H
#define BLEND2D_STRING_H

#include "./variant.h"

//! \addtogroup blend2d_api_globals
//! \{

// ============================================================================
// [BLString - Core]
// ============================================================================

//! Byte string [C Interface - Impl].
struct BLStringImpl {
  //! String capacity [in bytes].
  size_t capacity;

  //! Reference count.
  volatile size_t refCount;
  //! Impl type.
  uint8_t implType;
  //! Impl traits.
  uint8_t implTraits;
  //! Memory pool data.
  uint16_t memPoolData;

  //! Reserved, must be zero.
  uint32_t reserved;

  union {
    struct {
      //! String data [null terminated].
      char* data;
      //! String size [in bytes].
      size_t size;
    };
    //! String data and size as `BLStringView`.
    BLStringView view;
  };
};

//! Byte string [C Interface - Core].
struct BLStringCore {
  BLStringImpl* impl;
};

// ============================================================================
// [BLString - C++]
// ============================================================================

#ifdef __cplusplus
//! Byte string [C++ API].
//!
//! Blend2D always uses UTF-8 encoding in public APIs so all strings are assumed
//! UTF-8 by default. However, `BLString` can hold arbitrary byte sequence and
//! act as a raw byte-string when this functionality is required.
class BLString : public BLStringCore {
public:
  //! \cond INTERNAL
  static constexpr const uint32_t kImplType = BL_IMPL_TYPE_STRING;
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  //! Creates a default constructed string.
  //!
  //! Default constructed strings share a built-in "none" instance.
  BL_INLINE BLString() noexcept { this->impl = none().impl; }

  //! Move constructor.
  //!
  //! \note The `other` string is reset by move construction, so its state
  //! after the move operation is the same as a default constructed string.
  BL_INLINE BLString(BLString&& other) noexcept { blVariantInitMove(this, &other); }

  //! Copy constructor, performs weak copy of the data held by the `other` string.
  BL_INLINE BLString(const BLString& other) noexcept { blVariantInitWeak(this, &other); }

  //! Constructor that creates a string from the given `impl`.
  //!
  //! \note The reference count of the passed `impl` is not increased.
  BL_INLINE explicit BLString(BLStringImpl* impl) noexcept { this->impl = impl; }

  //! Destroys the string.
  BL_INLINE ~BLString() noexcept { blStringDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  //! Tests whether the string has any content.
  //!
  //! \note This is essentially the opposite of `empty()`.
  BL_INLINE explicit operator bool() const noexcept { return !empty(); }

  //! Move assignment.
  //!
  //! \note The `other` string is reset by move assignment, so its state
  //! after the move operation is the same as a default constructed string.
  BL_INLINE BLString& operator=(BLString&& other) noexcept { blStringAssignMove(this, &other); return *this; }
  //! Copy assignment, performs weak copy of the data held by the `other` string.
  BL_INLINE BLString& operator=(const BLString& other) noexcept { blStringAssignWeak(this, &other); return *this; }

  BL_INLINE bool operator==(const BLString& other) const noexcept { return  equals(other); }
  BL_INLINE bool operator!=(const BLString& other) const noexcept { return !equals(other); }
  BL_INLINE bool operator< (const BLString& other) const noexcept { return compare(other) <  0; }
  BL_INLINE bool operator<=(const BLString& other) const noexcept { return compare(other) <= 0; }
  BL_INLINE bool operator> (const BLString& other) const noexcept { return compare(other) >  0; }
  BL_INLINE bool operator>=(const BLString& other) const noexcept { return compare(other) >= 0; }

  BL_INLINE bool operator==(const BLStringView& view) const noexcept { return  equals(view); }
  BL_INLINE bool operator!=(const BLStringView& view) const noexcept { return !equals(view); }
  BL_INLINE bool operator< (const BLStringView& view) const noexcept { return compare(view) <  0; }
  BL_INLINE bool operator<=(const BLStringView& view) const noexcept { return compare(view) <= 0; }
  BL_INLINE bool operator> (const BLStringView& view) const noexcept { return compare(view) >  0; }
  BL_INLINE bool operator>=(const BLStringView& view) const noexcept { return compare(view) >= 0; }

  BL_INLINE bool operator==(const char* str) const noexcept { return  equals(str); }
  BL_INLINE bool operator!=(const char* str) const noexcept { return !equals(str); }
  BL_INLINE bool operator< (const char* str) const noexcept { return compare(str) <  0; }
  BL_INLINE bool operator<=(const char* str) const noexcept { return compare(str) <= 0; }
  BL_INLINE bool operator> (const char* str) const noexcept { return compare(str) >  0; }
  BL_INLINE bool operator>=(const char* str) const noexcept { return compare(str) >= 0; }

  //! Returns a character at the given `index`.
  //!
  //! \note This is the same as calling `at(index)`.
  BL_INLINE const char& operator[](size_t index) const noexcept { return at(index); }

  //! \}

  //! \name Common Functionality
  //! \{

  //! Clears the content of the string and releases its data.
  //!
  //! After reset the string content matches a default constructed string.
  BL_INLINE BLResult reset() noexcept { return blStringReset(this); }

  //! Swaps the content of this string with the `other` string.
  BL_INLINE void swap(BLString& other) noexcept { std::swap(this->impl, other.impl); }

  //! Replaces the content of the string by `c` character or multiple characters
  //! if `n` is greater than one.
  BL_INLINE BLResult assign(char c, size_t n = 1) noexcept { return blStringApplyOpChar(this, BL_MODIFY_OP_ASSIGN_FIT, c, n); }

  //! Move assignment, the same as `operator=`, but returns a `BLResult` instead of `this`.
  BL_INLINE BLResult assign(BLString&& other) noexcept { return blStringAssignMove(this, &other); }

  //! Copy assignment, the same as `operator=`, but returns a `BLResult` instead of `this`.
  BL_INLINE BLResult assign(const BLString& other) noexcept { return blStringAssignWeak(this, &other); }

  //! Replaces the string by the content described by the given string `view`.
  BL_INLINE BLResult assign(const BLStringView& view) noexcept { return blStringAssignData(this, view.data, view.size); }

  //! Replaces the string by `str` data of the given length `n`.
  //!
  //! \note The implementation assumes null terminated string if `n` equals to `SIZE_MAX`.
  BL_INLINE BLResult assign(const char* str, size_t n = SIZE_MAX) noexcept { return blStringAssignData(this, str, n); }

  //! Copy assignment, but creates a deep copy of the `other` string instead of weak copy.
  BL_INLINE BLResult assignDeep(const BLString& other) noexcept { return blStringAssignDeep(this, &other); }

  //! Replaces the content of the string by a result of calling `snprintf(fmt, args...)`.
  template<typename... Args>
  BL_INLINE BLResult assignFormat(const char* fmt, Args&&... args) noexcept { return blStringApplyOpFormat(this, BL_MODIFY_OP_ASSIGN_FIT, fmt, std::forward<Args>(args)...); }

  //! Replaces the content of the string by a result of calling `vsnprintf(fmt, ap)`.
  BL_INLINE BLResult assignFormatV(const char* fmt, va_list ap) noexcept { return blStringApplyOpFormatV(this, BL_MODIFY_OP_ASSIGN_FIT, fmt, ap); }

  //! Tests whether the string is empty (has no content).
  //!
  //! Returns `true` if the string's length is zero.
  BL_INLINE bool empty() const noexcept { return impl->size == 0; }

  //! \}

  //! \name Accessors
  //! \{

  //! Returns a character at the given `index`.
  //!
  //! \note Index must be valid and cannot be out of bounds, otherwise the
  //! result is undefined and would trigger an assertion failure in debug mode.
  BL_INLINE const char& at(size_t index) const noexcept {
    BL_ASSERT(index < size());
    return data()[index];
  }

  //! Returns the size of the string [in bytes].
  BL_INLINE size_t size() const noexcept { return impl->size; }
  //! Returns the capacity of the string [in bytes].
  BL_INLINE size_t capacity() const noexcept { return impl->capacity; }

  //! Returns a read-only data of the string.
  BL_INLINE const char* data() const noexcept { return impl->data; }
  //! Returns the end of the string data.
  //!
  //! The returned pointer points to the null terminator, the data still can
  //! be read, but it's not considered as string data by Blend2D anymore.
  BL_INLINE const char* end() const noexcept { return impl->data + impl->size; }

  //! Returns the content of the string as `BLStringView`.
  BL_INLINE const BLStringView& view() const noexcept { return impl->view; }

  //! \}

  //! \name String Manipulation
  //! \{

  //! Clears the content of the string without releasing its dynamically allocated data, if possible.
  BL_INLINE BLResult clear() noexcept { return blStringClear(this); }
  //! Shrinks the capacity of the string to match the actual content.
  BL_INLINE BLResult shrink() noexcept { return blStringShrink(this); }
  //! Reserves at least `n` bytes in the string for further manipulation (most probably appending).
  BL_INLINE BLResult reserve(size_t n) noexcept { return blStringReserve(this, n); }
  //! Resizes the string to `n` and fills the additional data by `fill` pattern.
  BL_INLINE BLResult resize(size_t n, char fill = '\0') noexcept { return blStringResize(this, n, fill); }

  //! Truncates the string length to `n`.
  //!
  //! It does nothing if the the string length is less than `n`.
  BL_INLINE BLResult truncate(size_t n) noexcept { return blStringResize(this, blMin(n, impl->size), '\0'); }

  //! Makes the string mutable.
  //!
  //! This operation checks whether the string is mutable and if not it makes a
  //! deep copy of its content so it can be modified. Please note that you can
  //! only modify the content that is defined by its length property. Even if
  //! the string had higher capacity before `makeMutable()` it's not guaranteed
  //! that the possible new data would match that capacity.
  //!
  //! If you want to make the string mutable for the purpose of appending or
  //! making other modifications please consider using `modifyOp()` and
  //! `insertOp()` instead.
  BL_INLINE BLResult makeMutable(char** dataOut) noexcept { return blStringMakeMutable(this, dataOut); }
  BL_INLINE BLResult modifyOp(uint32_t op, size_t n, char** dataOut) noexcept { return blStringModifyOp(this, op, n, dataOut); }
  BL_INLINE BLResult insertOp(size_t index, size_t n, char** dataOut) noexcept { return blStringInsertOp(this, index, n, dataOut); }

  BL_INLINE BLResult append(char c, size_t n = 1) noexcept { return blStringApplyOpChar(this, BL_MODIFY_OP_APPEND_GROW, c, n); }
  BL_INLINE BLResult append(const BLString& other) noexcept { return blStringApplyOpString(this, BL_MODIFY_OP_APPEND_GROW, &other); }
  BL_INLINE BLResult append(const BLStringView& view) noexcept { return blStringApplyOpData(this, BL_MODIFY_OP_APPEND_GROW, view.data, view.size); }
  BL_INLINE BLResult append(const char* str, size_t n = SIZE_MAX) noexcept { return blStringApplyOpData(this, BL_MODIFY_OP_APPEND_GROW, str, n); }

  template<typename... Args>
  BL_INLINE BLResult appendFormat(const char* fmt, Args&&... args) noexcept { return blStringApplyOpFormat(this, BL_MODIFY_OP_APPEND_GROW, fmt, std::forward<Args>(args)...); }
  BL_INLINE BLResult appendFormatV(const char* fmt, va_list ap) noexcept { return blStringApplyOpFormatV(this, BL_MODIFY_OP_APPEND_GROW, fmt, ap); }

  BL_INLINE BLResult prepend(char c, size_t n = 1) noexcept { return blStringInsertChar(this, 0, c, n); }
  BL_INLINE BLResult prepend(const BLString& other) noexcept { return blStringInsertString(this, 0, &other); }
  BL_INLINE BLResult prepend(const BLStringView& view) noexcept { return blStringInsertData(this, 0, view.data, view.size); }
  BL_INLINE BLResult prepend(const char* str, size_t n = SIZE_MAX) noexcept { return blStringInsertData(this, 0, str, n); }

  BL_INLINE BLResult insert(size_t index, char c, size_t n = 1) noexcept { return blStringInsertChar(this, index, c, n); }
  BL_INLINE BLResult insert(size_t index, const BLString& other) noexcept { return blStringInsertString(this, index, &other); }
  BL_INLINE BLResult insert(size_t index, const BLStringView& view) noexcept { return blStringInsertData(this, index, view.data, view.size); }
  BL_INLINE BLResult insert(size_t index, const char* str, size_t n = SIZE_MAX) noexcept { return blStringInsertData(this, index, str, n); }

  BL_INLINE BLResult remove(const BLRange& range) noexcept { return blStringRemoveRange(this, range.start, range.end); }

  //! \name Equality & Comparison
  //! \{

  //! Returns whether this string and `other` are equal (i.e. their contents match).
  BL_INLINE bool equals(const BLString& other) const noexcept { return blStringEquals(this, &other); }
  //! Returns whether this string and other string `view` are equal.
  BL_INLINE bool equals(const BLStringView& view) const noexcept { return blStringEqualsData(this, view.data, view.size); }
  //! Returns whether this string and the given string data `str` of length `n` are equal.
  BL_INLINE bool equals(const char* str, size_t n = SIZE_MAX) const noexcept { return blStringEqualsData(this, str, n); }

  //! Compares this string with `other` and returns either `-1`, `0`, or `1`.
  BL_INLINE int compare(const BLString& other) const noexcept { return blStringCompare(this, &other); }
  //! Compares this string with other string `view` and returns either `-1`, `0`, or `1`.
  BL_INLINE int compare(const BLStringView& view) const noexcept { return blStringCompareData(this, view.data, view.size); }
  //! Compares this string with other string data and returns either `-1`, `0`, or `1`.
  BL_INLINE int compare(const char* str, size_t n = SIZE_MAX) const noexcept { return blStringCompareData(this, str, n); }

  //! \}

  //! \name Search
  //! \{

  //! Returns the first index at which a given character `c` can be found in
  //! the string, or `SIZE_MAX` if not present.
  BL_INLINE size_t indexOf(char c) const noexcept {
    return indexOf(c, 0);
  }

  //! Returns the index at which a given character `c` can be found in
  //! the string starting from `fromIndex`, or `SIZE_MAX` if not present.
  BL_INLINE size_t indexOf(char c, size_t fromIndex) const noexcept {
    const char* p = data();
    size_t iEnd = size();

    for (size_t i = fromIndex; i < iEnd; i++)
      if (p[i] == c)
        return i;

    return SIZE_MAX;
  }

  //! Returns the last index at which a given character `c` can be found in
  //! the string, or `SIZE_MAX` if not present.
  BL_INLINE size_t lastIndexOf(char c) const noexcept {
    const char* p = data();
    size_t i = size();

    while (--i != SIZE_MAX && !(p[i] == c))
      continue;

    return i;
  }

  //! Returns the index at which a given character `c` can be found in
  //! the string starting from `fromIndex` and ending at `0`, or `SIZE_MAX`
  //! if not present.
  BL_INLINE size_t lastIndexOf(char c, size_t fromIndex) const noexcept {
    const char* p = data();
    size_t i = size() - 1;

    if (i == SIZE_MAX)
      return i;

    i = blMin<size_t>(i, fromIndex);
    while (!(p[i] == c) && --i != SIZE_MAX)
      continue;

    return i;
  }

  //! \}

  static BL_INLINE const BLString& none() noexcept { return reinterpret_cast<const BLString*>(blNone)[kImplType]; }
};
#endif

//! \}

#endif // BLEND2D_STRING_H
