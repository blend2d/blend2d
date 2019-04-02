// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLSTRING_H
#define BLEND2D_BLSTRING_H

#include "./blvariant.h"

//! \addtogroup blend2d_api_globals
//! \{

// ============================================================================
// [BLString - Core]
// ============================================================================

//! Byte string [C Interface - Impl].
struct BLStringImpl {
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

  //! Reserved, will be part of string data.
  uint8_t reserved[4];
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

  BL_INLINE BLString() noexcept { this->impl = none().impl; }
  BL_INLINE BLString(BLString&& other) noexcept { blVariantInitMove(this, &other); }
  BL_INLINE BLString(const BLString& other) noexcept { blVariantInitWeak(this, &other); }
  BL_INLINE explicit BLString(BLStringImpl* impl) noexcept { this->impl = impl; }
  BL_INLINE ~BLString() noexcept { blStringReset(this); }

  BL_INLINE BLString& operator=(BLString&& other) noexcept { blStringAssignMove(this, &other); return *this; }
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

  BL_INLINE explicit operator bool() const noexcept { return !empty(); }
  BL_INLINE char operator[](size_t index) const noexcept { return at(index); }

  BL_INLINE char at(size_t index) const noexcept {
    BL_ASSERT(index < size());
    return data()[index];
  }

  BL_INLINE bool empty() const noexcept { return impl->size == 0; }
  BL_INLINE size_t size() const noexcept { return impl->size; }
  BL_INLINE size_t capacity() const noexcept { return impl->capacity; }

  BL_INLINE const char* data() const noexcept { return impl->data; }
  BL_INLINE const char* end() const noexcept { return impl->data + impl->size; }

  BL_INLINE const BLStringView& view() const noexcept { return impl->view; }

  //! Clear the content of the string without releasing its dynamically allocated data, if possible.
  BL_INLINE BLResult reset() noexcept { return blStringReset(this); }
  BL_INLINE void swap(BLString& other) noexcept { std::swap(this->impl, other.impl); }

  BL_INLINE BLResult clear() noexcept { return blStringClear(this); }
  BL_INLINE BLResult shrink() noexcept { return blStringShrink(this); }
  BL_INLINE BLResult reserve(size_t n) noexcept { return blStringReserve(this, n); }
  BL_INLINE BLResult resize(size_t n, char fill = '\0') noexcept { return blStringResize(this, n, fill); }
  //! Truncate the string length to `n`.
  BL_INLINE BLResult truncate(size_t n) noexcept { return blStringResize(this, blMin(n, impl->size), '\0'); }

  BL_INLINE BLResult makeMutable(char** dataOut) noexcept { return blStringMakeMutable(this, dataOut); }
  BL_INLINE BLResult modifyOp(uint32_t op, size_t n, char** dataOut) noexcept { return blStringModifyOp(this, op, n, dataOut); }
  BL_INLINE BLResult insertOp(size_t index, size_t n, char** dataOut) noexcept { return blStringInsertOp(this, index, n, dataOut); }

  BL_INLINE BLResult assign(char c, size_t n = 1) noexcept { return blStringApplyOpChar(this, BL_MODIFY_OP_ASSIGN_FIT, c, n); }
  BL_INLINE BLResult assign(BLString&& other) noexcept { return blStringAssignMove(this, &other); }
  BL_INLINE BLResult assign(const BLStringCore& other) noexcept { return blStringAssignWeak(this, &other); }
  BL_INLINE BLResult assign(const BLStringView& view) noexcept { return blStringAssignData(this, view.data, view.size); }
  BL_INLINE BLResult assign(const char* str, size_t n = SIZE_MAX) noexcept { return blStringAssignData(this, str, n); }
  BL_INLINE BLResult assignDeep(const BLStringCore& other) noexcept { return blStringAssignDeep(this, &other); }

  template<typename... Args>
  BL_INLINE BLResult assignFormat(const char* fmt, Args&&... args) noexcept { return blStringApplyOpFormat(this, BL_MODIFY_OP_ASSIGN_FIT, fmt, std::forward<Args>(args)...); }
  BL_INLINE BLResult assignFormatV(const char* fmt, va_list ap) noexcept { return blStringApplyOpFormatV(this, BL_MODIFY_OP_ASSIGN_FIT, fmt, ap); }

  BL_INLINE BLResult append(char c, size_t n = 1) noexcept { return blStringApplyOpChar(this, BL_MODIFY_OP_APPEND_GROW, c, n); }
  BL_INLINE BLResult append(const BLStringCore& other) noexcept { return blStringApplyOpString(this, BL_MODIFY_OP_APPEND_GROW, &other); }
  BL_INLINE BLResult append(const BLStringView& view) noexcept { return blStringApplyOpData(this, BL_MODIFY_OP_APPEND_GROW, view.data, view.size); }
  BL_INLINE BLResult append(const char* str, size_t n = SIZE_MAX) noexcept { return blStringApplyOpData(this, BL_MODIFY_OP_APPEND_GROW, str, n); }

  template<typename... Args>
  BL_INLINE BLResult appendFormat(const char* fmt, Args&&... args) noexcept { return blStringApplyOpFormat(this, BL_MODIFY_OP_APPEND_GROW, fmt, std::forward<Args>(args)...); }
  BL_INLINE BLResult appendFormatV(const char* fmt, va_list ap) noexcept { return blStringApplyOpFormatV(this, BL_MODIFY_OP_APPEND_GROW, fmt, ap); }

  BL_INLINE BLResult prepend(char c, size_t n = 1) noexcept { return blStringInsertChar(this, 0, c, n); }
  BL_INLINE BLResult prepend(const BLStringCore& other) noexcept { return blStringInsertString(this, 0, &other); }
  BL_INLINE BLResult prepend(const BLStringView& view) noexcept { return blStringInsertData(this, 0, view.data, view.size); }
  BL_INLINE BLResult prepend(const char* str, size_t n = SIZE_MAX) noexcept { return blStringInsertData(this, 0, str, n); }

  BL_INLINE BLResult insert(size_t index, char c, size_t n = 1) noexcept { return blStringInsertChar(this, index, c, n); }
  BL_INLINE BLResult insert(size_t index, const BLStringCore& other) noexcept { return blStringInsertString(this, index, &other); }
  BL_INLINE BLResult insert(size_t index, const BLStringView& view) noexcept { return blStringInsertData(this, index, view.data, view.size); }
  BL_INLINE BLResult insert(size_t index, const char* str, size_t n = SIZE_MAX) noexcept { return blStringInsertData(this, index, str, n); }

  BL_INLINE BLResult remove(const BLRange& range) noexcept { return blStringRemoveRange(this, &range); }

  BL_INLINE bool equals(const BLString& other) const noexcept { return blStringEquals(this, &other); }
  BL_INLINE bool equals(const BLStringView& view) const noexcept { return blStringEqualsData(this, view.data, view.size); }
  BL_INLINE bool equals(const char* str, size_t n = SIZE_MAX) const noexcept { return blStringEqualsData(this, str, n); }

  BL_INLINE int compare(const BLString& other) const noexcept { return blStringCompare(this, &other); }
  BL_INLINE int compare(const BLStringView& view) const noexcept { return blStringCompareData(this, view.data, view.size); }
  BL_INLINE int compare(const char* str, size_t n = SIZE_MAX) const noexcept { return blStringCompareData(this, str, n); }

  BL_INLINE size_t indexOf(char c) const noexcept {
    return indexOf(c, 0);
  }

  BL_INLINE size_t indexOf(char c, size_t fromIndex) const noexcept {
    const char* p = data();
    size_t iEnd = size();

    for (size_t i = fromIndex; i < iEnd; i++)
      if (p[i] == c)
        return i;

    return SIZE_MAX;
  }

  BL_INLINE size_t lastIndexOf(char c) const noexcept {
    const char* p = data();
    size_t i = size();

    while (--i != SIZE_MAX && !(p[i] == c))
      continue;

    return i;
  }

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

  static BL_INLINE const BLString& none() noexcept { return reinterpret_cast<const BLString&>(blNone[kImplType]); }
};
#endif

//! \}

#endif // BLEND2D_BLSTRING_H
