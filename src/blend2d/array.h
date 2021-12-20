// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_ARRAY_H_INCLUDED
#define BLEND2D_ARRAY_H_INCLUDED

#include "object.h"

//! \addtogroup blend2d_api_globals
//! \{

//! \name BLArray [C API]
//!
//! Array functionality is provided by \ref BLArrayCore in C API and wrapped by \ref BLArray<T> template in C++ API.
//!
//! C API users must call either generic functions with `Item` suffix or correct specialized functions in case
//! of typed arrays. For example if you create a `BLArray<uint32_t>` in C then you can only modify it through
//! functions that have either `U32` or `Item` suffix. Arrays of signed types are treated as arrays of unsigned
//! types at API level as there is no difference between them from an implementation perspective.
//!
//! \{

//! \cond INTERNAL
//! Array container [Impl].
struct BLArrayImpl BL_CLASS_INHERITS(BLObjectImpl) {
  void* data;
  //! Array size [in items].
  size_t size;
  //! Array capacity [in items].
  size_t capacity;

#ifdef __cplusplus
  //! Returns the pointer to the `data` casted to `T*`.
  template<typename T>
  BL_NODISCARD
  BL_INLINE T* dataAs() noexcept { return (T*)data; }

  //! Returns the pointer to the `data` casted to `const T*`.
  template<typename T>
  BL_NODISCARD
  BL_INLINE const T* dataAs() const noexcept { return (const T*)data; }
#endif
};
//! \endcond

//! Array container [C API].
struct BLArrayCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL

#ifdef __cplusplus
  //! \cond INTERNAL

  template<typename T>
  BL_NODISCARD
  BL_INLINE T& dcast() noexcept { return static_cast<T&>(*this); }

  template<typename T>
  BL_NODISCARD
  BL_INLINE const T& dcast() const noexcept { return static_cast<const T&>(*this); }

  //! \endcond
#endif
};

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blArrayInit(BLArrayCore* self, BLObjectType arrayType) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayInitMove(BLArrayCore* self, BLArrayCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayInitWeak(BLArrayCore* self, const BLArrayCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayDestroy(BLArrayCore* self) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blArrayReset(BLArrayCore* self) BL_NOEXCEPT_C;

BL_API size_t BL_CDECL blArrayGetSize(const BLArrayCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API size_t BL_CDECL blArrayGetCapacity(const BLArrayCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API size_t BL_CDECL blArrayGetItemSize(BLArrayCore* self) BL_NOEXCEPT_C BL_PURE;
BL_API const void* BL_CDECL blArrayGetData(const BLArrayCore* self) BL_NOEXCEPT_C BL_PURE;

BL_API BLResult BL_CDECL blArrayClear(BLArrayCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayShrink(BLArrayCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayReserve(BLArrayCore* self, size_t n) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayResize(BLArrayCore* self, size_t n, const void* fill) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blArrayMakeMutable(BLArrayCore* self, void** dataOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayModifyOp(BLArrayCore* self, BLModifyOp op, size_t n, void** dataOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayInsertOp(BLArrayCore* self, size_t index, size_t n, void** dataOut) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blArrayAssignMove(BLArrayCore* self, BLArrayCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayAssignWeak(BLArrayCore* self, const BLArrayCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayAssignDeep(BLArrayCore* self, const BLArrayCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayAssignData(BLArrayCore* self, const void* data, size_t n) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayAssignExternalData(BLArrayCore* self, void* data, size_t size, size_t capacity, BLDataAccessFlags dataAccessFlags, BLDestroyExternalDataFunc destroyFunc, void* userData) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blArrayAppendU8(BLArrayCore* self, uint8_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayAppendU16(BLArrayCore* self, uint16_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayAppendU32(BLArrayCore* self, uint32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayAppendU64(BLArrayCore* self, uint64_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayAppendF32(BLArrayCore* self, float value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayAppendF64(BLArrayCore* self, double value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayAppendItem(BLArrayCore* self, const void* item) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayAppendData(BLArrayCore* self, const void* data, size_t n) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blArrayInsertU8(BLArrayCore* self, size_t index, uint8_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayInsertU16(BLArrayCore* self, size_t index, uint16_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayInsertU32(BLArrayCore* self, size_t index, uint32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayInsertU64(BLArrayCore* self, size_t index, uint64_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayInsertF32(BLArrayCore* self, size_t index, float value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayInsertF64(BLArrayCore* self, size_t index, double value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayInsertItem(BLArrayCore* self, size_t index, const void* item) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayInsertData(BLArrayCore* self, size_t index, const void* data, size_t n) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blArrayReplaceU8(BLArrayCore* self, size_t index, uint8_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayReplaceU16(BLArrayCore* self, size_t index, uint16_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayReplaceU32(BLArrayCore* self, size_t index, uint32_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayReplaceU64(BLArrayCore* self, size_t index, uint64_t value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayReplaceF32(BLArrayCore* self, size_t index, float value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayReplaceF64(BLArrayCore* self, size_t index, double value) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayReplaceItem(BLArrayCore* self, size_t index, const void* item) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayReplaceData(BLArrayCore* self, size_t rStart, size_t rEnd, const void* data, size_t n) BL_NOEXCEPT_C;

BL_API BLResult BL_CDECL blArrayRemoveIndex(BLArrayCore* self, size_t index) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blArrayRemoveRange(BLArrayCore* self, size_t rStart, size_t rEnd) BL_NOEXCEPT_C;

BL_API bool BL_CDECL blArrayEquals(const BLArrayCore* a, const BLArrayCore* b) BL_NOEXCEPT_C BL_PURE;

BL_END_C_DECLS

//! \}

#ifdef __cplusplus
//! \cond INTERNAL
//! \name Internals behind BLArray<T> implementation.
//! \{
namespace BLInternal {
namespace {

template<typename T, uint32_t TypeCategory>
struct ArrayTraitsByCategory {
  static constexpr const uint32_t kArrayType = BL_OBJECT_TYPE_NULL;

  typedef T CompatibleType;
  static BL_INLINE const T& pass(const T& arg) noexcept { return arg; }
};

template<typename T>
struct ArrayTraitsByCategory<T, kTypeCategoryObject> {
  static constexpr const uint32_t kArrayType = BL_OBJECT_TYPE_ARRAY_OBJECT;

  typedef T CompatibleType;
  static BL_INLINE const CompatibleType& pass(const T& arg) noexcept { return arg; }
};

template<typename T>
struct ArrayTraitsByCategory<T, kTypeCategoryPtr> {
  static constexpr const uint32_t kArrayType =
    sizeof(T) == 4 ? BL_OBJECT_TYPE_ARRAY_UINT32 : BL_OBJECT_TYPE_ARRAY_UINT64;

  typedef typename StdInt<sizeof(T), 1>::Type CompatibleType;
  static BL_INLINE CompatibleType pass(const T& arg) noexcept { return (CompatibleType)arg; }
};

template<typename T>
struct ArrayTraitsByCategory<T, kTypeCategoryInt> {
  static constexpr const uint32_t kArrayType =
    sizeof(T) == 1 && std::is_signed  <T>::value ? BL_OBJECT_TYPE_ARRAY_INT8   :
    sizeof(T) == 1 && std::is_unsigned<T>::value ? BL_OBJECT_TYPE_ARRAY_UINT8  :
    sizeof(T) == 2 && std::is_signed  <T>::value ? BL_OBJECT_TYPE_ARRAY_INT16  :
    sizeof(T) == 2 && std::is_unsigned<T>::value ? BL_OBJECT_TYPE_ARRAY_UINT16 :
    sizeof(T) == 4 && std::is_signed  <T>::value ? BL_OBJECT_TYPE_ARRAY_INT32  :
    sizeof(T) == 4 && std::is_unsigned<T>::value ? BL_OBJECT_TYPE_ARRAY_UINT32 :
    sizeof(T) == 8 && std::is_signed  <T>::value ? BL_OBJECT_TYPE_ARRAY_INT64  :
    sizeof(T) == 8 && std::is_unsigned<T>::value ? BL_OBJECT_TYPE_ARRAY_UINT64 : BL_OBJECT_TYPE_NULL;

  typedef typename StdInt<sizeof(T), 1>::Type CompatibleType;
  static BL_INLINE CompatibleType pass(const T& arg) noexcept { return (CompatibleType)arg; }
};

template<typename T>
struct ArrayTraitsByCategory<T, kTypeCategoryFloat> {
  static constexpr const uint32_t kArrayType =
    sizeof(T) == 4 ? BL_OBJECT_TYPE_ARRAY_FLOAT32 :
    sizeof(T) == 8 ? BL_OBJECT_TYPE_ARRAY_FLOAT64 : BL_OBJECT_TYPE_NULL;

  typedef T CompatibleType;
  static BL_INLINE CompatibleType pass(const T& arg) noexcept { return (CompatibleType)arg; }
};

template<typename T>
struct ArrayTraitsByCategory<T, kTypeCategoryStruct> {
  static constexpr const uint32_t kArrayType =
    sizeof(T) ==  1 ? BL_OBJECT_TYPE_ARRAY_STRUCT_1  :
    sizeof(T) ==  2 ? BL_OBJECT_TYPE_ARRAY_STRUCT_2  :
    sizeof(T) ==  3 ? BL_OBJECT_TYPE_ARRAY_STRUCT_3  :
    sizeof(T) ==  4 ? BL_OBJECT_TYPE_ARRAY_STRUCT_4  :
    sizeof(T) ==  6 ? BL_OBJECT_TYPE_ARRAY_STRUCT_6  :
    sizeof(T) ==  8 ? BL_OBJECT_TYPE_ARRAY_STRUCT_8  :
    sizeof(T) == 10 ? BL_OBJECT_TYPE_ARRAY_STRUCT_10 :
    sizeof(T) == 12 ? BL_OBJECT_TYPE_ARRAY_STRUCT_12 :
    sizeof(T) == 16 ? BL_OBJECT_TYPE_ARRAY_STRUCT_16 :
    sizeof(T) == 20 ? BL_OBJECT_TYPE_ARRAY_STRUCT_20 :
    sizeof(T) == 24 ? BL_OBJECT_TYPE_ARRAY_STRUCT_24 :
    sizeof(T) == 32 ? BL_OBJECT_TYPE_ARRAY_STRUCT_32 : BL_OBJECT_TYPE_NULL;

  typedef T CompatibleType;
  static BL_INLINE CompatibleType pass(const T& arg) noexcept { return (CompatibleType)arg; }
};

template<typename T>
struct ArrayTraits : public ArrayTraitsByCategory<T, TypeTraits<T>::kCategory> {};

template<typename T>
BL_INLINE const T& firstInVarArgs(const T& arg) noexcept { return arg; }
template<typename T, typename... Args>
BL_INLINE const T& firstInVarArgs(const T& arg, Args&&...) noexcept { return arg; }

template<typename T, typename Arg0>
BL_INLINE void copyToUninitialized(T* dst, Arg0&& src) noexcept {
  blCallCtor(*dst, std::forward<Arg0>(src));
}

template<typename T, typename Arg0, typename... Args>
BL_INLINE void copyToUninitialized(T* dst, Arg0&& arg0, Args&&... args) noexcept {
  copyToUninitialized(dst + 0, std::forward<Arg0>(arg0));
  copyToUninitialized(dst + 1, std::forward<Args>(args)...);
}

template<typename T> BL_INLINE BLResult appendItem(BLArrayCore* self, const T& item) noexcept { return blArrayAppendItem(self, &item); }
template<> BL_INLINE BLResult appendItem(BLArrayCore* self, const uint8_t& item) noexcept { return blArrayAppendU8(self, item); }
template<> BL_INLINE BLResult appendItem(BLArrayCore* self, const uint16_t& item) noexcept { return blArrayAppendU16(self, item); }
template<> BL_INLINE BLResult appendItem(BLArrayCore* self, const uint32_t& item) noexcept { return blArrayAppendU32(self, item); }
template<> BL_INLINE BLResult appendItem(BLArrayCore* self, const uint64_t& item) noexcept { return blArrayAppendU64(self, item); }
template<> BL_INLINE BLResult appendItem(BLArrayCore* self, const float& item) noexcept { return blArrayAppendF32(self, item); }
template<> BL_INLINE BLResult appendItem(BLArrayCore* self, const double& item) noexcept { return blArrayAppendF64(self, item); }

template<typename T> BL_INLINE BLResult insertItem(BLArrayCore* self, size_t index, const T& item) noexcept { return blArrayInsertItem(self, index, &item); }
template<> BL_INLINE BLResult insertItem(BLArrayCore* self, size_t index, const uint8_t& item) noexcept { return blArrayInsertU8(self, index, item); }
template<> BL_INLINE BLResult insertItem(BLArrayCore* self, size_t index, const uint16_t& item) noexcept { return blArrayInsertU16(self, index, item); }
template<> BL_INLINE BLResult insertItem(BLArrayCore* self, size_t index, const uint32_t& item) noexcept { return blArrayInsertU32(self, index, item); }
template<> BL_INLINE BLResult insertItem(BLArrayCore* self, size_t index, const uint64_t& item) noexcept { return blArrayInsertU64(self, index, item); }
template<> BL_INLINE BLResult insertItem(BLArrayCore* self, size_t index, const float& item) noexcept { return blArrayInsertF32(self, index, item); }
template<> BL_INLINE BLResult insertItem(BLArrayCore* self, size_t index, const double& item) noexcept { return blArrayInsertF64(self, index, item); }

template<typename T> BL_INLINE BLResult replaceItem(BLArrayCore* self, size_t index, const T& item) noexcept { return blArrayReplaceItem(self, index, &item); }
template<> BL_INLINE BLResult replaceItem(BLArrayCore* self, size_t index, const uint8_t& item) noexcept { return blArrayReplaceU8(self, index, item); }
template<> BL_INLINE BLResult replaceItem(BLArrayCore* self, size_t index, const uint16_t& item) noexcept { return blArrayReplaceU16(self, index, item); }
template<> BL_INLINE BLResult replaceItem(BLArrayCore* self, size_t index, const uint32_t& item) noexcept { return blArrayReplaceU32(self, index, item); }
template<> BL_INLINE BLResult replaceItem(BLArrayCore* self, size_t index, const uint64_t& item) noexcept { return blArrayReplaceU64(self, index, item); }
template<> BL_INLINE BLResult replaceItem(BLArrayCore* self, size_t index, const float& item) noexcept { return blArrayReplaceF32(self, index, item); }
template<> BL_INLINE BLResult replaceItem(BLArrayCore* self, size_t index, const double& item) noexcept { return blArrayReplaceF64(self, index, item); }

} // {anonymous}
} // {BLInternal}

//! \}
//! \endcond
#endif

//! \name BLArray C++ API
//! \{
#ifdef __cplusplus

//! Array container (template) [C++ API].
template<typename T>
class BLArray : public BLArrayCore {
public:
  //! \cond INTERNAL
  //! Array traits of `T`.
  typedef BLInternal::ArrayTraits<T> Traits;

  //! Implementation type of this BLArray<T> matching `T` traits.
  enum : uint32_t {
    kArrayType = Traits::kArrayType,
    kSSOCapacity = BLObjectDetail::kStaticDataSize / uint32_t(sizeof(T))
  };

  static_assert(uint32_t(kArrayType) != BL_OBJECT_TYPE_NULL,
                "Type 'T' cannot be used with 'BLArray<T>' as it's either non-trivial or non-specialized");

  BL_INLINE BLArrayImpl* _impl() const noexcept { return static_cast<BLArrayImpl*>(_d.impl); }
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  //! Creates a default constructed array.
  BL_INLINE BLArray() noexcept {
    _d.initStatic((BLObjectType)kArrayType, BLObjectInfo::packFields(0u, kSSOCapacity));
  }

  //! Move constructor.
  //!
  //! \note The `other` array is always reset by a move construction, so it becomes an empty array.
  BL_INLINE BLArray(BLArray&& other) noexcept {
    _d = other._d;
    other._d.initStatic((BLObjectType)kArrayType, BLObjectInfo::packFields(0u, kSSOCapacity));
  }

  //! Copy constructor, performs weak copy of the data held by the `other` array.
  BL_INLINE BLArray(const BLArray& other) noexcept { blArrayInitWeak(this, &other); }

  //! Destroys the array.
  BL_INLINE ~BLArray() noexcept { blArrayDestroy(this); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  //! Tests whether the array has items. Returns `true` if the array is not empty.
  //!
  //! \note This is essentially the opposite of `empty()`.
  BL_INLINE explicit operator bool() const noexcept { return !empty(); }

  //! Move assignment.
  //!
  //! \note The `other` array is reset by move assignment, so its state after the move operation is the same as
  //! the default constructed array.
  BL_INLINE BLArray& operator=(BLArray&& other) noexcept { blArrayAssignMove(this, &other); return *this; }
  //! Copy assignment, performs weak copy of the data held by the `other` array.
  BL_INLINE BLArray& operator=(const BLArray& other) noexcept { blArrayAssignWeak(this, &other); return *this; }

  //! Returns true if this and `other` arrays are equal.
  BL_NODISCARD
  BL_INLINE bool operator==(const BLArray& other) noexcept { return equals(other); }

  //! Returns true if this and `other` arrays are not equal.
  BL_NODISCARD
  BL_INLINE bool operator!=(const BLArray& other) noexcept { return !equals(other); }

  //! Returns a read-only reference to the array item at the given `index`.
  //!
  //! \note This is the same as calling `at(index)`.
  BL_NODISCARD
  BL_INLINE const T& operator[](size_t index) const noexcept { return at(index); }

  //! \}

  //! \name Common Functionality
  //! \{

  //! Resets the array into a default constructed state by clearing its content and releasing its memory.
  BL_INLINE BLResult reset() noexcept { return blArrayReset(this); }

  //! Swaps the content of this array with the `other` array.
  BL_INLINE void swap(BLArray<T>& other) noexcept { _d.swap(other._d); }

  //! \}

  //! \name Accessors
  //! \{

  //! Tests whether the array is empty.
  BL_NODISCARD
  BL_INLINE bool empty() const noexcept { return size() == 0; }

  //! Returns the size of the array (number of items).
  BL_NODISCARD
  BL_INLINE size_t size() const noexcept { return _d.sso() ? size_t(_d.aField()) : _impl()->size; }

  //! Returns the capacity of the array (number of items).
  BL_NODISCARD
  BL_INLINE size_t capacity() const noexcept { return _d.sso() ? size_t(_d.bField()) : _impl()->capacity; }

  //! Returns a pointer to the array data.
  BL_NODISCARD
  BL_INLINE const T* data() const noexcept { return _d.sso() ? (const T*)(_d.char_data) : (const T*)_impl()->data; }

  //! Returns a pointer to the array data (iterator compatibility).
  BL_NODISCARD
  BL_INLINE const T* begin() const noexcept { return data(); }

  //! Returns a pointer to the end of array data (iterator compatibility).
  BL_NODISCARD
  BL_INLINE const T* end() const noexcept {
    return _d.sso() ? (const T*)(_d.char_data) + size_t(_d.aField())
                    : (const T*)_impl()->data + _impl()->size;
  }

  //! Returns the array data as `BLArrayView<T>`.
  BL_NODISCARD
  BL_INLINE BLArrayView<T> view() const noexcept { return BLArrayView<T> { data(), size() }; }

  //! Returns a read-only reference to the array item at the given `index`.
  //!
  //! \note The index must be valid, which means it has to be less than the array length. Accessing items out of range
  //! is undefined behavior that would be catched by assertions in debug builds.
  BL_NODISCARD
  BL_INLINE const T& at(size_t index) const noexcept {
    BL_ASSERT(index < size());
    return data()[index];
  }

  //! Returns a read-only reference to the first item.
  //!
  //! \note The array must have at least one item othewise calling `first()` would point to the end of the array,
  //! which is not initialized, and such reference would be invalid. Debug builds would catch this condition with
  //! an assertion.
  BL_NODISCARD
  BL_INLINE const T& first() const noexcept { return at(0); }

  //! Returns a read-only reference to the last item.
  //!
  //! \note The array must have at least one item othewise calling `last()`  would point to the end of the array,
  //! which is not initialized, and such reference would be invalid. Debug builds would catch this condition with
  //! an assertion.
  BL_NODISCARD
  BL_INLINE const T& last() const noexcept { return at(size() - 1); }

  //! \}

  //! \name Data Manipulation
  //! \{

  //! Clears the content of the array.
  //!
  //! \note If the array uses a dynamically allocated memory and the instance is mutable the memory won't be released,
  //! it will be reused instead. Consider using `reset()` if you want to release the memory in such case instead.
  BL_INLINE BLResult clear() noexcept { return blArrayClear(this); }

  //! Shrinks the capacity of the array to fit its length.
  //!
  //! Some array operations like `append()` may grow the array more than necessary to make it faster when such
  //! manipulation operations are called consecutively. When you are done with modifications and you know the
  //! lifetime of the array won't be short you can use `shrink()` to fit its memory requirements to the number
  //! of items it stores, which could optimize the application's memory requirements.
  BL_INLINE BLResult shrink() noexcept { return blArrayShrink(this); }

  //! Reserves the array capacity to hold at least `n` items.
  BL_INLINE BLResult reserve(size_t n) noexcept { return blArrayReserve(this, n); }

  //! Truncates the length of the array to maximum `n` items.
  //!
  //! If the length of the array is less than `n`n then truncation does nothing.
  BL_INLINE BLResult truncate(size_t n) noexcept { return blArrayResize(this, blMin(n, size()), nullptr); }

  //! Resizes the array to `n` items.
  //!
  //! If `n` is greater than the array length then all new items will be
  //! initialized by `fill` item.
  BL_INLINE BLResult resize(size_t n, const T& fill) noexcept { return blArrayResize(this, n, &fill); }

  //! Makes the array mutable by possibly creating a deep copy of the data if it's either read-only or shared with
  //! another array. Stores the pointer to the beginning of mutable data in `dataOut`.
  //!
  //! ```
  //! BLArray<uint8_t> a;
  //! if (a.append(0, 1, 2, 3, 4, 5, 6, 7) != BL_SUCCESS) {
  //!   // Handle error condition.
  //! }
  //!
  //! uint8_t* data;
  //! if (a.makeMutable(&data) != BL_SUCCESS) {
  //!   // Handle error condition.
  //! }
  //!
  //! // `data` is a mutable pointer to array content of 8 items.
  //! data[0] = 100;
  //!
  //! // Calling array member functions (or C API) could invalidate `data`.
  //! a.append(9); // You shouldn't use `data` afterwards.
  //! ```
  BL_INLINE BLResult makeMutable(T** dataOut) noexcept {
    return blArrayMakeMutable(this, (void**)dataOut);
  }

  //! Modify operation is similar to `makeMutable`, however, the `op` argument specifies the desired array operation,
  //! see \ref BLModifyOp. The pointer returned in `dataOut` points to the first item to be either assigned or
  //! appended and it points to an unititialized memory.
  //!
  //! Please note that assignments mean to wipe out the whole array content and to set the length of the array to `n`.
  //! The caller is responsible for initializing the data returned in `dataOut`.
  BL_INLINE BLResult modifyOp(BLModifyOp op, size_t n, T** dataOut) noexcept {
    return blArrayModifyOp(this, op, n, (void**)dataOut);
  }

  //! Insert operation, the semantics is similar to `modifyOp()`, however, items are inserted at the given `index`
  //! instead of assigned or appended.
  //!
  //! The caller is responsible for initializing the data returned in `dataOut`.
  BL_INLINE BLResult insertOp(size_t index, size_t n, T** dataOut) noexcept {
    return blArrayInsertOp(this, index, n, (void**)dataOut);
  }

  //! Similar to `modifyOp()`, but the items to assign/append to the array are given after the `op` argument.
  //!
  //! \note This is a varidic template that makes such modification easier if you have a constant number of items
  //! to assign or append.
  template<typename... Args>
  BL_INLINE BLResult modify_v(BLModifyOp op, Args&&... args) noexcept {
    T* dst;
    BL_PROPAGATE(blArrayModifyOp(this, op, sizeof...(args), (void**)&dst));
    BLInternal::copyToUninitialized(dst, std::forward<Args>(args)...);
    return BL_SUCCESS;
  }

  //! Move assignment, the same as `operator=`, but returns a `BLResult` instead of `this`.
  BL_INLINE BLResult assign(BLArray<T>&& other) noexcept {
    return blArrayAssignMove(this, &other);
  }

  //! Copy assignment, the same as `operator=`, but returns a `BLResult` instead of `this`.
  BL_INLINE BLResult assign(const BLArray<T>& other) noexcept {
    return blArrayAssignWeak(this, &other);
  }

  //! Copy assignment, but creates a deep copy of the `other` array instead of weak copy.
  BL_INLINE BLResult assignDeep(const BLArray<T>& other) noexcept {
    return blArrayAssignDeep(this, &other);
  }

  //! Replaces the content of the array with variadic number of items passed in `args...`.
  template<typename... Args>
  BL_INLINE BLResult assign_v(Args&&... args) noexcept {
    return modify_v(BL_MODIFY_OP_ASSIGN_FIT, std::forward<Args>(args)...);
  }

  //! Replaces the content of the array by items in the passed array `view`.
  //!
  //! \note The implementation can handle `view` pointing to the array's data as well, so it's possible to create
  //! a slice of the array if required.
  BL_INLINE BLResult assignData(const BLArrayView<T>& view) noexcept {
    return blArrayAssignData(this, (const void*)view.data, view.size);
  }

  //! Replaces the content of the array `items` of length `n`.
  //!
  //! \note The implementation can handle items pointing to the array's data as well, so it's possible to create
  //! a slice of the array if required.
  BL_INLINE BLResult assignData(const T* items, size_t n) noexcept {
    return blArrayAssignData(this, (const void*)items, n);
  }

  //! Assign an external buffer to the array, which would replace the existing content.
  //!
  //! \param data External data buffer to use (cannot be NULL).
  //! \param size Size of the data buffer in items.
  //! \param capacity Capacity of the buffer, cannot be zero or smaller than `size`.
  //! \param accessFlags Flags that describe whether the data is read-only or read-write, see `BLDataAccessFlags`.
  //! \param destroyFunc A function that would be called when the array is destroyed (can be null if you don't need it).
  //! \param userData User data passed to `destroyFunc`.
  BL_INLINE BLResult assignExternalData(
    T* data,
    size_t size,
    size_t capacity,
    BLDataAccessFlags accessFlags,
    BLDestroyExternalDataFunc destroyFunc = nullptr,
    void* userData = nullptr) noexcept {

    return blArrayAssignExternalData(this, data, size, capacity, accessFlags, destroyFunc, userData);
  }

  //! Appends a variadic number of items items passed in `args...` to the array.
  //!
  //! \note The data in `args` cannot point to the same data that the array holds as the function that prepares the
  //! append operation has no way to know about the source (it only makes space for new data). It's an undefined
  //! behavior in such case.
  template<typename... Args>
  BL_INLINE BLResult append(Args&&... args) noexcept {
    if (sizeof...(args) == 1)
      return BLInternal::appendItem(this, Traits::pass(BLInternal::firstInVarArgs(std::forward<Args>(args)...)));
    else
      return modify_v(BL_MODIFY_OP_APPEND_GROW, std::forward<Args>(args)...);
  }

  //! Appends items to the array of the given array `view`.
  //!
  //! \note The implementation guarantees that a `view` pointing to the array data itself would work.
  BL_INLINE BLResult appendData(const BLArrayView<T>& view) noexcept {
    return blArrayAppendData(this, (const void*)view.data, view.size);
  }

  //! Appends `items` to the array of length `n`.
  //!
  //! \note The implementation guarantees that a `items` pointing to the array data itself would work.
  BL_INLINE BLResult appendData(const T* items, size_t n) noexcept {
    return blArrayAppendData(this, (const void*)items, n);
  }

  //! Prepends a variadic number of items items passed in `args...` to the array.
  //!
  //! \note The data in `args` cannot point to the same data that the array holds as the function that prepares the
  //! prepend operation has no way to know about the source (it only makes space for new data). It's an undefined
  //! behavior in such case.
  template<typename... Args>
  BL_INLINE BLResult prepend(Args&&... args) noexcept {
    if (sizeof...(args) == 1)
      return BLInternal::insertItem(this, 0, Traits::pass(BLInternal::firstInVarArgs(std::forward<Args>(args)...)));
    else
      return insert(0, std::forward<Args>(args)...);
  }

  //! Prepends items to the array of the given array `view`.
  //!
  //! \note The implementation guarantees that a `view` pointing to the array data itself would work.
  BL_INLINE BLResult prependData(const BLArrayView<T>& view) noexcept {
    return blArrayInsertData(this, 0, (const void*)view.data, view.size);
  }

  //! Prepends `items` to the array of length `n`.
  //!
  //! \note The implementation guarantees that a `items` pointing to the array data itself would work.
  BL_INLINE BLResult prependData(const T* items, size_t n) noexcept {
    return blArrayInsertData(this, 0, (const void*)items, n);
  }

  //! Inserts a variadic number of items items passed in `args...` at the given `index`.
  //!
  //! \note The data in `args` cannot point to the same data that the array holds as the function that prepares the
  //! insert operation has no way to know about the source (it only makes space for new data). It's an undefined
  //! behavior in such case.
  template<typename... Args>
  BL_INLINE BLResult insert(size_t index, Args&&... args) noexcept {
    if (sizeof...(args) == 1) {
      return BLInternal::insertItem(this, index, Traits::pass(BLInternal::firstInVarArgs(std::forward<Args>(args)...)));
    }
    else {
      T* dst;
      BL_PROPAGATE(blArrayInsertOp(this, index, sizeof...(args), (void**)&dst));
      BLInternal::copyToUninitialized(dst, std::forward<Args>(args)...);
      return BL_SUCCESS;
    }
  }

  //! Inserts items to the array of the given array `view` at the given `index`.
  //!
  //! \note The implementation guarantees that a `view` pointing to the array data itself would work.
  BL_INLINE BLResult insertData(size_t index, const BLArrayView<T>& view) noexcept {
    return blArrayInsertData(this, index, (const void*)view.data, view.size);
  }

  //! Prepends `items` to the array of length `n` at the given `index`.
  //!
  //! \note The implementation guarantees that a `items` pointing to the array data itself would work.
  BL_INLINE BLResult insertData(size_t index, const T* items, size_t n) noexcept {
    return blArrayInsertData(this, index, (const void*)items, n);
  }

  //! Replaces an item at the given `index` by `item`.
  BL_INLINE BLResult replace(size_t index, const T& item) noexcept {
    return BLInternal::replaceItem(this, index, Traits::pass(item));
  }

  //! Replaces the given `range` of items by the given array `view`.
  BL_INLINE BLResult replaceData(const BLRange& range, BLArrayView<T>& view) noexcept {
    return blArrayReplaceData(this, range.start, range.end, (const void*)view.data, view.size);
  }

  //! Replaces the given `range` of items by `items` of length `n`.
  BL_INLINE BLResult replaceData(const BLRange& range, const T* items, size_t n) noexcept {
    return blArrayReplaceData(this, range.start, range.end, items, n);
  }

  //! Removes an item at the given `index`.
  BL_INLINE BLResult remove(size_t index) noexcept {
    return blArrayRemoveIndex(this, index);
  }

  //! Removes a `range` of items.
  BL_INLINE BLResult remove(const BLRange& range) noexcept {
    return blArrayRemoveRange(this, range.start, range.end);
  }

  //! \}

  //! \name Equality & Comparison
  //! \{

  //! Returnsn whether the content of this array and `other` matches.
  BL_NODISCARD
  BL_INLINE bool equals(const BLArray<T>& other) const noexcept {
    return blArrayEquals(this, &other);
  }

  //! \}

  //! \name Search
  //! \{

  //! Returns the first index at which a given `item` can be found in the array, or `SIZE_MAX` if not found.
  BL_NODISCARD
  BL_INLINE size_t indexOf(const T& item) const noexcept {
    return indexOf(item, 0);
  }

  //! Returns the index at which a given `item` can be found in the array starting from `fromIndex`, or `SIZE_MAX`
  //! if not present.
  BL_NODISCARD
  BL_INLINE size_t indexOf(const T& item, size_t fromIndex) const noexcept {
    const T* p = data();
    size_t iEnd = size();

    for (size_t i = fromIndex; i < iEnd; i++)
      if (p[i] == item)
        return i;

    return SIZE_MAX;
  }

  //! Returns the last index at which a given `item` can be found in the array, or `SIZE_MAX` if not present.
  BL_NODISCARD
  BL_INLINE size_t lastIndexOf(const T& item) const noexcept {
    const T* p = data();
    size_t i = size();

    while (--i != SIZE_MAX && !(p[i] == item))
      continue;

    return i;
  }

  //! Returns the index at which a given `item` can be found in the array starting from `fromIndex` and ending
  //! at `0`, or `SIZE_MAX` if not present.
  BL_NODISCARD
  BL_INLINE size_t lastIndexOf(const T& item, size_t fromIndex) const noexcept {
    const T* p = data();
    size_t i = size() - 1;

    if (i == SIZE_MAX)
      return i;

    i = blMin<size_t>(i, fromIndex);
    while (!(p[i] == item) && --i != SIZE_MAX)
      continue;

    return i;
  }

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_ARRAY_H_INCLUDED
