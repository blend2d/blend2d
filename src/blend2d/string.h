// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_STRING_H_INCLUDED
#define BLEND2D_STRING_H_INCLUDED

#include "object.h"

//! \addtogroup blend2d_api_globals
//! \{

//! \name BLString - C API
//!
//! \{

//! \cond INTERNAL
//! Byte string [Impl].
struct BLStringImpl BL_CLASS_INHERITS(BLObjectImpl) {
  //! String size [in bytes].
  size_t size;
  //! String data capacity [in bytes].
  size_t capacity;

#ifdef __cplusplus
  BL_INLINE BLStringImpl() noexcept = default;

  BL_INLINE constexpr BLStringImpl(size_t sizeIn, size_t capacityIn) noexcept
    : size(sizeIn),
      capacity(capacityIn) {}

  //! String data [null terminated] follows BLStringImpl data.
  BL_NODISCARD
  BL_INLINE char* data() noexcept { return reinterpret_cast<char*>(this) + sizeof(BLStringImpl); }
#endif
};
//! \endcond

//! Byte string [C API].
struct BLStringCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
};

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blStringInit(BLStringCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringInitMove(BLStringCore* self, BLStringCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringInitWeak(BLStringCore* self, const BLStringCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringInitWithData(BLStringCore* self, const char* str, size_t size) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringDestroy(BLStringCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringReset(BLStringCore* self) BL_NOEXCEPT_C;
BL_API const char* BL_CDECL blStringGetData(const BLStringCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API size_t BL_CDECL blStringGetSize(const BLStringCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API size_t BL_CDECL blStringGetCapacity(const BLStringCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API BLResult BL_CDECL blStringClear(BLStringCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringShrink(BLStringCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringReserve(BLStringCore* self, size_t n) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringResize(BLStringCore* self, size_t n, char fill) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringMakeMutable(BLStringCore* self, char** dataOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringModifyOp(BLStringCore* self, BLModifyOp op, size_t n, char** dataOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringInsertOp(BLStringCore* self, size_t index, size_t n, char** dataOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringAssignMove(BLStringCore* self, BLStringCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringAssignWeak(BLStringCore* self, const BLStringCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringAssignDeep(BLStringCore* self, const BLStringCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringAssignData(BLStringCore* self, const char* str, size_t n) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringApplyOpChar(BLStringCore* self, BLModifyOp op, char c, size_t n) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringApplyOpData(BLStringCore* self, BLModifyOp op, const char* str, size_t n) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringApplyOpString(BLStringCore* self, BLModifyOp op, const BLStringCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringApplyOpFormat(BLStringCore* self, BLModifyOp op, const char* fmt, ...) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringApplyOpFormatV(BLStringCore* self, BLModifyOp op, const char* fmt, va_list ap) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringInsertChar(BLStringCore* self, size_t index, char c, size_t n) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringInsertData(BLStringCore* self, size_t index, const char* str, size_t n) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringInsertString(BLStringCore* self, size_t index, const BLStringCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringRemoveIndex(BLStringCore* self, size_t index) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blStringRemoveRange(BLStringCore* self, size_t rStart, size_t rEnd) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blStringEquals(const BLStringCore* a, const BLStringCore* b) BL_NOEXCEPT_C BL_PURE;
BL_API bool BL_CDECL blStringEqualsData(const BLStringCore* self, const char* str, size_t n) BL_NOEXCEPT_C BL_PURE;
BL_API int BL_CDECL blStringCompare(const BLStringCore* a, const BLStringCore* b) BL_NOEXCEPT_C BL_PURE;
BL_API int BL_CDECL blStringCompareData(const BLStringCore* self, const char* str, size_t n) BL_NOEXCEPT_C BL_PURE;

BL_END_C_DECLS

//! \}

//! \name BLString - C++ API
//! \{
#ifdef __cplusplus

//! Byte string [C++ API].
//!
//! Blend2D always uses UTF-8 encoding in public APIs so all strings are assumed UTF-8 by default. However, `BLString`
//! can hold arbitrary byte sequence and act as a raw byte-string when this functionality is required.
class BLString : public BLStringCore {
public:
  //! \cond INTERNAL
  enum : uint32_t {
    //! Capacity of an SSO string - depends actually on architecture endianness.
    kSSOCapacity =
      BL_BYTE_ORDER == 1234
        ? BLObjectDetail::kStaticDataSize + 2u
        : BLObjectDetail::kStaticDataSize - 1u,

    //! String signature of an empty SSO string (with size XORed with `kSSOCapacity`).
    //!
    //! This mask can be used to get quickly SSO string size.
    kSSOEmptySignature = (BL_OBJECT_INFO_MARKER_FLAG) |
                         (BL_OBJECT_TYPE_STRING << BL_OBJECT_INFO_TYPE_SHIFT) |
                         (kSSOCapacity << BL_OBJECT_INFO_A_SHIFT)

  };

  BL_INLINE BLStringImpl* _impl() const noexcept { return static_cast<BLStringImpl*>(_d.impl); }
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  //! Creates an empty string.
  BL_INLINE BLString() noexcept {
    _d.initStatic(BL_OBJECT_TYPE_STRING, BLObjectInfo::packFields(kSSOCapacity));
  }

  //! Move constructor.
  //!
  //! \note The `other` string is always reset by a move construction, so it becomes an empty string.
  BL_INLINE BLString(BLString&& other) noexcept {
    _d = other._d;
    other._d.initStatic(BL_OBJECT_TYPE_STRING, BLObjectInfo::packFields(kSSOCapacity));
  }

  //! Copy constructor, performs weak copy of the data held by the `other` string.
  BL_INLINE BLString(const BLString& other) noexcept { blStringInitWeak(this, &other); }

  //! Constructor that creates a string from the given string `view`.
  //!
  //! \note See other constructors for more details.
  BL_INLINE explicit BLString(const BLStringView& view) noexcept { blStringInitWithData(this, view.data, view.size); }

  //! Constructor that creates a string from the given data specified by `str` and `size`. If `size` is `SIZE_MAX`
  //! the string is assumed to be null terminated.
  //!
  //! This is a convenience function that doesn't provide error handling. If size exceeds small string capacity
  //! and dynamic allocation failed then a default empty string would be constructed.
  BL_INLINE explicit BLString(const char* str, size_t size = SIZE_MAX) noexcept { blStringInitWithData(this, str, size); }

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
  //! \note The `other` string is reset by move construction, so it becomes an empty string.
  BL_INLINE BLString& operator=(BLString&& other) noexcept { blStringAssignMove(this, &other); return *this; }
  //! Copy assignment, performs weak copy of the data held by the `other` string.
  BL_INLINE BLString& operator=(const BLString& other) noexcept { blStringAssignWeak(this, &other); return *this; }

  BL_NODISCARD BL_INLINE bool operator==(const BLString& other) const noexcept { return  equals(other); }
  BL_NODISCARD BL_INLINE bool operator!=(const BLString& other) const noexcept { return !equals(other); }
  BL_NODISCARD BL_INLINE bool operator< (const BLString& other) const noexcept { return compare(other) <  0; }
  BL_NODISCARD BL_INLINE bool operator<=(const BLString& other) const noexcept { return compare(other) <= 0; }
  BL_NODISCARD BL_INLINE bool operator> (const BLString& other) const noexcept { return compare(other) >  0; }
  BL_NODISCARD BL_INLINE bool operator>=(const BLString& other) const noexcept { return compare(other) >= 0; }

  BL_NODISCARD BL_INLINE bool operator==(const BLStringView& view) const noexcept { return  equals(view); }
  BL_NODISCARD BL_INLINE bool operator!=(const BLStringView& view) const noexcept { return !equals(view); }
  BL_NODISCARD BL_INLINE bool operator< (const BLStringView& view) const noexcept { return compare(view) <  0; }
  BL_NODISCARD BL_INLINE bool operator<=(const BLStringView& view) const noexcept { return compare(view) <= 0; }
  BL_NODISCARD BL_INLINE bool operator> (const BLStringView& view) const noexcept { return compare(view) >  0; }
  BL_NODISCARD BL_INLINE bool operator>=(const BLStringView& view) const noexcept { return compare(view) >= 0; }

  BL_NODISCARD BL_INLINE bool operator==(const char* str) const noexcept { return  equals(str); }
  BL_NODISCARD BL_INLINE bool operator!=(const char* str) const noexcept { return !equals(str); }
  BL_NODISCARD BL_INLINE bool operator< (const char* str) const noexcept { return compare(str) <  0; }
  BL_NODISCARD BL_INLINE bool operator<=(const char* str) const noexcept { return compare(str) <= 0; }
  BL_NODISCARD BL_INLINE bool operator> (const char* str) const noexcept { return compare(str) >  0; }
  BL_NODISCARD BL_INLINE bool operator>=(const char* str) const noexcept { return compare(str) >= 0; }

  //! Returns a character at the given `index`.
  //!
  //! \note This is the same as calling `at(index)`.
  BL_NODISCARD
  BL_INLINE const char& operator[](size_t index) const noexcept { return at(index); }

  //! \}

  //! \name Common Functionality
  //! \{

  //! Clears the content of the string and releases its data.
  //!
  //! After reset the string content matches a default constructed string.
  BL_INLINE BLResult reset() noexcept { return blStringReset(this); }

  //! Swaps the content of this string with the `other` string.
  BL_INLINE void swap(BLString& other) noexcept { _d.swap(other._d); }

  //! \}

  //! \name Accessors
  //! \{

  //! Tests whether the string is empty (has no content).
  //!
  //! Returns `true` if the string's length is zero.
  BL_NODISCARD
  BL_INLINE bool empty() const noexcept { return size() == 0; }

  //! Returns a character at the given `index`.
  //!
  //! \note Index must be valid and cannot be out of bounds - there is an assertion.
  BL_NODISCARD
  BL_INLINE const char& at(size_t index) const noexcept {
    BL_ASSERT(index <= size());
    return data()[index];
  }

  //! Returns the size of the string [in bytes].
  BL_NODISCARD
  BL_INLINE size_t size() const noexcept {
    return _d.sso() ? size_t((_d.info.bits ^ kSSOEmptySignature) >> BL_OBJECT_INFO_A_SHIFT) : _impl()->size;
  }

  //! Returns the capacity of the string [in bytes].
  BL_NODISCARD
  BL_INLINE size_t capacity() const noexcept { return _d.sso() ? size_t(kSSOCapacity) : _impl()->capacity; }

  //! Returns a pointer to the data of the string.
  BL_NODISCARD
  BL_INLINE const char* data() const noexcept { return _d.sso() ? _d.char_data : _impl()->data(); }

  //! Returns a pointer to the beginning of string data (iterator compatibility).
  BL_NODISCARD
  BL_INLINE const char* begin() const noexcept { return data(); }

  //! Returns a pointer to the end of string data (iterator compatibility).
  //!
  //! The returned pointer points to the string null terminator.
  BL_NODISCARD
  BL_INLINE const char* end() const noexcept { return data() + size(); }

  //! Returns the content of the string as `BLStringView`.
  BL_NODISCARD
  BL_INLINE BLStringView view() const noexcept { return BLStringView { data(), size() }; }

  //! \}

  //! \name Data Manipulation
  //! \{

  //! Clears the content of the string without releasing its dynamically allocated data, if possible.
  BL_INLINE BLResult clear() noexcept { return blStringClear(this); }
  //! Shrinks the capacity of the string to match the actual content.
  BL_INLINE BLResult shrink() noexcept { return blStringShrink(this); }
  //! Reserves at least `n` bytes in the string for further manipulation (most probably appending).
  BL_INLINE BLResult reserve(size_t n) noexcept { return blStringReserve(this, n); }
  //! Resizes the string to `n` and fills the additional data by `fill` pattern.
  BL_INLINE BLResult resize(size_t n, char fill = '\0') noexcept { return blStringResize(this, n, fill); }

  //! Makes the string mutable.
  //!
  //! This operation checks whether the string is mutable and if not it makes a deep copy of its content so it can be
  //! modified. Please note that you can only modify the content that is defined by its length property. Even if the
  //! string had higher capacity before `makeMutable()` it's not guaranteed that the possible new data would match that
  //! capacity.
  //!
  //! If you want to make the string mutable for the purpose of appending or making other modifications please consider
  //! using `modifyOp()` and `insertOp()` instead.
  BL_INLINE BLResult makeMutable(char** dataOut) noexcept { return blStringMakeMutable(this, dataOut); }
  BL_INLINE BLResult modifyOp(BLModifyOp op, size_t n, char** dataOut) noexcept { return blStringModifyOp(this, op, n, dataOut); }
  BL_INLINE BLResult insertOp(size_t index, size_t n, char** dataOut) noexcept { return blStringInsertOp(this, index, n, dataOut); }

  //! Replaces the content of the string by `c` character or multiple characters if `n` is greater than one.
  BL_INLINE BLResult assign(char c, size_t n = 1) noexcept { return blStringApplyOpChar(this, BL_MODIFY_OP_ASSIGN_FIT, c, n); }

  //! Move assignment, the same as `operator=()`, but returns a `BLResult` instead of `this`.
  BL_INLINE BLResult assign(BLString&& other) noexcept { return blStringAssignMove(this, &other); }

  //! Copy assignment, the same as `operator=()`, but returns a `BLResult` instead of `this`.
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

  //! Truncates the string length to `n`.
  //!
  //! It does nothing if the the string length is less than `n`.
  BL_INLINE BLResult truncate(size_t n) noexcept { return n < size() ? blStringResize(this, n, '\0') : BLResult(BL_SUCCESS); }

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

  BL_INLINE BLResult remove(size_t index) noexcept { return blStringRemoveIndex(this, index); }
  BL_INLINE BLResult remove(const BLRange& range) noexcept { return blStringRemoveRange(this, range.start, range.end); }

  //! \name Equality & Comparison
  //! \{

  //! Returns whether this string and `other` are equal (i.e. their contents match).
  BL_NODISCARD
  BL_INLINE bool equals(const BLString& other) const noexcept { return blStringEquals(this, &other); }

  //! Returns whether this string and other string `view` are equal.
  BL_NODISCARD
  BL_INLINE bool equals(const BLStringView& view) const noexcept { return blStringEqualsData(this, view.data, view.size); }

  //! Returns whether this string and the given string data `str` of length `n` are equal.
  BL_NODISCARD
  BL_INLINE bool equals(const char* str, size_t n = SIZE_MAX) const noexcept { return blStringEqualsData(this, str, n); }

  //! Compares this string with `other` and returns either `-1`, `0`, or `1`.
  BL_NODISCARD
  BL_INLINE int compare(const BLString& other) const noexcept { return blStringCompare(this, &other); }

  //! Compares this string with other string `view` and returns either `-1`, `0`, or `1`.
  BL_NODISCARD
  BL_INLINE int compare(const BLStringView& view) const noexcept { return blStringCompareData(this, view.data, view.size); }

  //! Compares this string with other string data and returns either `-1`, `0`, or `1`.
  BL_NODISCARD
  BL_INLINE int compare(const char* str, size_t n = SIZE_MAX) const noexcept { return blStringCompareData(this, str, n); }

  //! \}

  //! \name Search
  //! \{

  //! Returns the first index at which a given character `c` can be found in the string, or `SIZE_MAX` if not present.
  BL_NODISCARD
  BL_INLINE size_t indexOf(char c) const noexcept { return indexOf(c, 0); }

  //! Returns the index at which a given character `c` can be found in the string starting from `fromIndex`, or `SIZE_MAX`
  //! if not present.
  BL_NODISCARD
  BL_INLINE size_t indexOf(char c, size_t fromIndex) const noexcept {
    size_t iEnd = size();
    const char* p = data();

    for (size_t i = fromIndex; i < iEnd; i++)
      if (p[i] == c)
        return i;

    return SIZE_MAX;
  }

  //! Returns the last index at which a given character `c` can be found in the string, or `SIZE_MAX` if not present.
  BL_NODISCARD
  BL_INLINE size_t lastIndexOf(char c) const noexcept {
    size_t i = size();
    const char* p = data();

    while (--i != SIZE_MAX && !(p[i] == c))
      continue;

    return i;
  }

  //! Returns the index at which a given character `c` can be found in the string starting from `fromIndex` and ending
  //! at `0`, or `SIZE_MAX` if not present.
  BL_NODISCARD
  BL_INLINE size_t lastIndexOf(char c, size_t fromIndex) const noexcept {
    size_t i = size() - 1;
    const char* p = data();

    if (i == SIZE_MAX)
      return i;

    i = blMin<size_t>(i, fromIndex);
    while (!(p[i] == c) && --i != SIZE_MAX)
      continue;

    return i;
  }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_STRING_H_INCLUDED
