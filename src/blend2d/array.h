// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_ARRAY_H
#define BLEND2D_ARRAY_H

#include "./variant.h"

// ============================================================================
// [BLArray - Core]
// ============================================================================

//! \addtogroup blend2d_api_globals
//! \{

//! Array container [C Interface - Impl].
struct BLArrayImpl {
  //! Array capacity.
  size_t capacity;

  //! Reference count.
  volatile size_t refCount;
  //! Impl type.
  uint8_t implType;
  //! Impl traits.
  uint8_t implTraits;
  //! Memory pool data.
  uint16_t memPoolData;

  //! Item size in bytes.
  uint8_t itemSize;
  //! Function dispatch used to handle arrays that don't store simple items.
  uint8_t dispatchType;
  //! Reserved, must be set to 0.
  uint8_t reserved[2];

  union {
    struct {
      //! Array data (as `void`).
      void* data;
      //! Array size.
      size_t size;
    };
    //! Array data and size as a view.
    BLDataView view;
  };

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  //! Returns the pointer to the `data` casted to `T*`.
  template<typename T>
  BL_INLINE T* dataAs() noexcept { return (T*)data; }
  //! Returns the pointer to the `data` casted to `const T*`.
  template<typename T>
  BL_INLINE const T* dataAs() const noexcept { return (const T*)data; }

  #endif
  // --------------------------------------------------------------------------
};

//! Array container [C Interface - Core].
struct BLArrayCore {
  BLArrayImpl* impl;

  // --------------------------------------------------------------------------
  #ifdef __cplusplus

  template<typename T>
  BL_INLINE T& dcast() noexcept { return reinterpret_cast<T&>(*this); }
  template<typename T>
  BL_INLINE const T& dcast() const noexcept { return reinterpret_cast<const T&>(*this); }

  #endif
  // --------------------------------------------------------------------------
};

//! \}

// ============================================================================
// [BLArray - Internal]
// ============================================================================

//! \cond INTERNAL
#ifdef __cplusplus
//! \ingroup blend2d_internal
//!
//! Internals behind BLArray<T> implementation.
namespace BLArrayInternal {
  // These are required to properly use the C API from C++ BLArray<T>. Category
  // provides a rough overview of `BLArray<T>` type category (like int, float)
  // and the other APIs provide some basic traits that the implementation needs.

  enum TypeCategory {
    TYPE_CATEGORY_UNKNOWN = 0,
    TYPE_CATEGORY_VAR     = 1,
    TYPE_CATEGORY_PTR     = 2,
    TYPE_CATEGORY_INT     = 3,
    TYPE_CATEGORY_FP      = 4,
    TYPE_CATEGORY_STRUCT  = 5
  };

  template<typename T>
  struct TypeCategoryOf {
    enum {
      kValue = std::is_pointer<T>::value ? TYPE_CATEGORY_PTR :
               std::is_integral<T>::value ? TYPE_CATEGORY_INT :
               std::is_floating_point<T>::value ? TYPE_CATEGORY_FP : TYPE_CATEGORY_STRUCT
    };
  };

  template<> struct TypeCategoryOf<BLVariant     > { enum { kValue = TYPE_CATEGORY_VAR }; };
  template<> struct TypeCategoryOf<BLString      > { enum { kValue = TYPE_CATEGORY_VAR }; };
  template<> struct TypeCategoryOf<BLPath        > { enum { kValue = TYPE_CATEGORY_VAR }; };
  template<> struct TypeCategoryOf<BLRegion      > { enum { kValue = TYPE_CATEGORY_VAR }; };
  template<> struct TypeCategoryOf<BLImage       > { enum { kValue = TYPE_CATEGORY_VAR }; };
  template<> struct TypeCategoryOf<BLImageCodec  > { enum { kValue = TYPE_CATEGORY_VAR }; };
  template<> struct TypeCategoryOf<BLImageDecoder> { enum { kValue = TYPE_CATEGORY_VAR }; };
  template<> struct TypeCategoryOf<BLImageEncoder> { enum { kValue = TYPE_CATEGORY_VAR }; };
  template<> struct TypeCategoryOf<BLPattern     > { enum { kValue = TYPE_CATEGORY_VAR }; };
  template<> struct TypeCategoryOf<BLGradient    > { enum { kValue = TYPE_CATEGORY_VAR }; };
  template<> struct TypeCategoryOf<BLContext     > { enum { kValue = TYPE_CATEGORY_VAR }; };
  template<> struct TypeCategoryOf<BLGlyphBuffer > { enum { kValue = TYPE_CATEGORY_VAR }; };
  template<> struct TypeCategoryOf<BLFont        > { enum { kValue = TYPE_CATEGORY_VAR }; };
  template<> struct TypeCategoryOf<BLFontFace    > { enum { kValue = TYPE_CATEGORY_VAR }; };
  template<> struct TypeCategoryOf<BLFontData    > { enum { kValue = TYPE_CATEGORY_VAR }; };
  template<> struct TypeCategoryOf<BLFontManager > { enum { kValue = TYPE_CATEGORY_VAR }; };

  template<typename T, int TypeCategory>
  struct ArrayTraitsByCategory {
    static constexpr const uint32_t kImplType = BL_IMPL_TYPE_NULL;

    typedef T CompatibleType;
    static BL_INLINE const T& pass(const T& arg) noexcept { return arg; }
  };

  template<typename T>
  struct ArrayTraitsByCategory<T, TYPE_CATEGORY_VAR> {
    static constexpr const uint32_t kImplType = BL_IMPL_TYPE_ARRAY_VAR;

    typedef T CompatibleType;
    static BL_INLINE const CompatibleType& pass(const T& arg) noexcept { return arg; }
  };

  template<typename T>
  struct ArrayTraitsByCategory<T, TYPE_CATEGORY_PTR> {
    static constexpr const uint32_t kImplType =
      sizeof(T) == 4 ? BL_IMPL_TYPE_ARRAY_U32 : BL_IMPL_TYPE_ARRAY_U64;

    typedef typename BLInternal::StdInt<sizeof(T), 1>::Type CompatibleType;
    static BL_INLINE CompatibleType pass(const T& arg) noexcept { return (CompatibleType)arg; }
  };

  template<typename T>
  struct ArrayTraitsByCategory<T, TYPE_CATEGORY_INT> {
    static constexpr const uint32_t kImplType =
      sizeof(T) == 1 && std::is_signed  <T>::value ? BL_IMPL_TYPE_ARRAY_I8  :
      sizeof(T) == 1 && std::is_unsigned<T>::value ? BL_IMPL_TYPE_ARRAY_U8  :
      sizeof(T) == 2 && std::is_signed  <T>::value ? BL_IMPL_TYPE_ARRAY_I16 :
      sizeof(T) == 2 && std::is_unsigned<T>::value ? BL_IMPL_TYPE_ARRAY_U16 :
      sizeof(T) == 4 && std::is_signed  <T>::value ? BL_IMPL_TYPE_ARRAY_I32 :
      sizeof(T) == 4 && std::is_unsigned<T>::value ? BL_IMPL_TYPE_ARRAY_U32 :
      sizeof(T) == 8 && std::is_signed  <T>::value ? BL_IMPL_TYPE_ARRAY_I64 :
      sizeof(T) == 8 && std::is_unsigned<T>::value ? BL_IMPL_TYPE_ARRAY_U64 : BL_IMPL_TYPE_NULL;

    typedef typename BLInternal::StdInt<sizeof(T), 1>::Type CompatibleType;
    static BL_INLINE CompatibleType pass(const T& arg) noexcept { return (CompatibleType)arg; }
  };

  template<typename T>
  struct ArrayTraitsByCategory<T, TYPE_CATEGORY_FP> {
    static constexpr const uint32_t kImplType =
      sizeof(T) == 4 ? BL_IMPL_TYPE_ARRAY_F32 :
      sizeof(T) == 8 ? BL_IMPL_TYPE_ARRAY_F64 : BL_IMPL_TYPE_NULL;

    typedef T CompatibleType;
    static BL_INLINE CompatibleType pass(const T& arg) noexcept { return (CompatibleType)arg; }
  };

  template<typename T>
  struct ArrayTraitsByCategory<T, TYPE_CATEGORY_STRUCT> {
    static constexpr const uint32_t kImplType =
      sizeof(T) ==  1 ? BL_IMPL_TYPE_ARRAY_STRUCT_1  :
      sizeof(T) ==  2 ? BL_IMPL_TYPE_ARRAY_STRUCT_2  :
      sizeof(T) ==  3 ? BL_IMPL_TYPE_ARRAY_STRUCT_3  :
      sizeof(T) ==  4 ? BL_IMPL_TYPE_ARRAY_STRUCT_4  :
      sizeof(T) ==  6 ? BL_IMPL_TYPE_ARRAY_STRUCT_6  :
      sizeof(T) ==  8 ? BL_IMPL_TYPE_ARRAY_STRUCT_8  :
      sizeof(T) == 10 ? BL_IMPL_TYPE_ARRAY_STRUCT_10 :
      sizeof(T) == 12 ? BL_IMPL_TYPE_ARRAY_STRUCT_12 :
      sizeof(T) == 16 ? BL_IMPL_TYPE_ARRAY_STRUCT_16 :
      sizeof(T) == 20 ? BL_IMPL_TYPE_ARRAY_STRUCT_20 :
      sizeof(T) == 24 ? BL_IMPL_TYPE_ARRAY_STRUCT_24 :
      sizeof(T) == 32 ? BL_IMPL_TYPE_ARRAY_STRUCT_32 : BL_IMPL_TYPE_NULL;

    typedef T CompatibleType;
    static BL_INLINE CompatibleType pass(const T& arg) noexcept { return (CompatibleType)arg; }
  };

  template<typename T>
  struct ArrayTraits : public ArrayTraitsByCategory<T, TypeCategoryOf<T>::kValue> {};

  template<typename T>
  BL_INLINE const T& first(const T& arg) noexcept { return arg; }
  template<typename T, typename... Args>
  BL_INLINE const T& first(const T& arg, Args&&...) noexcept { return arg; }

  template<typename T, typename Arg0>
  BL_INLINE void copyToUninitialized(T* dst, Arg0&& src) noexcept {
    // Otherwise MSVC would emit null checks...
    BL_ASSUME(dst != nullptr);
    new(BLInternal::PlacementNew { dst }) T(std::forward<Arg0>(src));
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
} // {BLArrayInternal}

#endif
//! \endcond

// ============================================================================
// [BLArray - C++]
// ============================================================================

//! \addtogroup blend2d_api_globals
//! \{

#ifdef __cplusplus
//! Array container (template) [C++ API].
template<typename T>
class BLArray : public BLArrayCore {
public:
  //! \cond INTERNAL
  //! Array traits of `T`.
  typedef BLArrayInternal::ArrayTraits<T> Traits;

  //! Implementation type of this BLArray<T> matching `T` traits.
  static constexpr const uint32_t kImplType = Traits::kImplType;

  static_assert(uint32_t(kImplType) != BL_IMPL_TYPE_NULL,
                "Type 'T' cannot be used with 'BLArray<T>' as it's either non-trivial or non-specialized");
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  //! Creates a default constructed array.
  //!
  //! Default constructed arrays share a built-in "none" instance.
  BL_INLINE BLArray() noexcept { this->impl = none().impl; }

  //! Move constructor.
  //!
  //! \note The `other` array is reset by move construction, so its state
  //! after the move operation is the same as a default constructed array.
  BL_INLINE BLArray(BLArray&& other) noexcept { blVariantInitMove(this, &other); }

  //! Copy constructor, performs weak copy of the data held by the `other` array.
  BL_INLINE BLArray(const BLArray& other) noexcept { blVariantInitWeak(this, &other); }

  //! Constructor that creates an array from the given `impl`.
  //!
  //! \note The reference count of the passed `impl` is not increased.
  BL_INLINE explicit BLArray(BLArrayImpl* impl) noexcept { this->impl = impl; }

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
  //! \note The `other` array is reset by move assignment, so its state
  //! after the move operation is the same as a default constructed array.
  BL_INLINE BLArray& operator=(BLArray&& other) noexcept { blArrayAssignMove(this, &other); return *this; }
  //! Copy assignment, performs weak copy of the data held by the `other` array.
  BL_INLINE BLArray& operator=(const BLArray& other) noexcept { blArrayAssignWeak(this, &other); return *this; }

  //! Returns true if this and `other` arrays are equal.
  BL_INLINE bool operator==(const BLArray& other) noexcept { return  equals(other); }
  //! Returns true if this and `other` arrays are not equal.
  BL_INLINE bool operator!=(const BLArray& other) noexcept { return !equals(other); }

  //! Returns a read-only reference to the array item at the given `index`.
  //!
  //! \note This is the same as calling `at(index)`.
  BL_INLINE const T& operator[](size_t index) const noexcept { return at(index); }

  //! \}

  //! \name Common Functionality
  //! \{

  //! Resets the array into a default constructed state by clearing its content
  //! and releasing its memory.
  BL_INLINE BLResult reset() noexcept { return blArrayReset(this); }

  //! Swaps the content of this array with the `other` array.
  BL_INLINE void swap(BLArray<T>& other) noexcept { std::swap(this->impl, other.impl); }

  //! Move assignment, the same as `operator=`, but returns a `BLResult` instead of `this`.
  BL_INLINE BLResult assign(BLArray<T>&& other) noexcept { return blArrayAssignMove(this, &other); }
  //! Copy assignment, the same as `operator=`, but returns a `BLResult` instead of `this`.
  BL_INLINE BLResult assign(const BLArray<T>& other) noexcept { return blArrayAssignWeak(this, &other); }

  //! Copy assignment, but creates a deep copy of the `other` array instead of weak copy.
  BL_INLINE BLResult assignDeep(const BLArray<T>& other) noexcept { return blArrayAssignDeep(this, &other); }

  //! Replaces the content of the array with variadic number of items passed in `args...`.
  template<typename... Args>
  BL_INLINE BLResult assign_v(Args&&... args) noexcept { return modify_v(BL_MODIFY_OP_ASSIGN_FIT, std::forward<Args>(args)...); }

  //! Replaces the content of the array by items in the passed array `view`.
  //!
  //! \note The implementation can handle `view` pointing to the array's data
  //! as well, so it's possible to create a slice of the array if required.
  BL_INLINE BLResult assignView(const BLArrayView<T>& view) noexcept { return blArrayAssignView(this, (const void*)view.data, view.size); }

  //! Replaces the content of the array `items` of length `n`.
  //!
  //! \note The implementation can handle items pointing to the array's data
  //! as well, so it's possible to create a slice of the array if required.
  BL_INLINE BLResult assignView(const T* items, size_t n) noexcept { return blArrayAssignView(this, (const void*)items, n); }

  //! Tests whether the array is a built-in null instance.
  BL_INLINE bool isNone() const noexcept { return (impl->implTraits & BL_IMPL_TRAIT_NULL) != 0; }
  //! Tests whether the array is empty.
  BL_INLINE bool empty() const noexcept { return impl->size == 0; }

  //! Returnsn whether the content of this array and `other` matches.
  BL_INLINE bool equals(const BLArray<T>& other) const noexcept { return blArrayEquals(this, &other); }

  //! \}

  //! \name Create Functionality
  //! \{

  //! Create array that uses an external `data` buffer.
  //!
  //! \param data External data buffer to use (cannot be NULL).
  //! \param size Size of the data buffer in items.
  //! \param capacity Capacity of the buffer, cannot be zero or smaller than `size`.
  //! \param dataAccessFlags Flags that describe whether the data is read-only or read-write, see `BLDataAccessFlags`.
  //! \param destroyFunc A function that would be called when the array is destroyed (can be null if you don't need it).
  //! \param destroyData Data passed to `destroyFunc`.
  //!
  //! \note The old content of the array is destroyed and replaced with an Impl
  //! that uses the external data passed.
  BL_INLINE BLResult createFromData(T* data, size_t size, size_t capacity, uint32_t dataAccessFlags, BLDestroyImplFunc destroyFunc = nullptr, void* destroyData = nullptr) noexcept {
    return blArrayCreateFromData(this, data, size, capacity, dataAccessFlags, destroyFunc, destroyData);
  }

  //! \}

  //! \name Array Storage
  //! \{

  //! Returns the size of the array (number of items).
  BL_INLINE size_t size() const noexcept { return impl->size; }
  //! Returns the capacity of the array (number of items).
  BL_INLINE size_t capacity() const noexcept { return impl->capacity; }

  //! Returns a pointer to the array data.
  BL_INLINE const T* data() const noexcept { return static_cast<const T*>(impl->data); }
  //! Returns a pointer to the end of array data.
  BL_INLINE const T* end() const noexcept { return static_cast<const T*>(impl->data) + impl->size; }

  //! Returns the array data as `BLArrayView<T>`.
  BL_INLINE const BLArrayView<T>& view() const noexcept {
    BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_STRICT_ALIASING)
    return reinterpret_cast<const BLArrayView<T>&>(impl->view);
    BL_DIAGNOSTIC_POP
  }

  //! Returns a read-only reference to the array item at the given `index`.
  //!
  //! \note The index must be valid, which means it has to be less than the
  //! array length. Accessing items out of range is undefined behavior that
  //! would be catched by assertions in debug builds.
  BL_INLINE const T& at(size_t index) const noexcept {
    BL_ASSERT(index < impl->size);
    return data()[index];
  }

  //! Returns a read-only reference to the first item.
  //!
  //! \note The array must have at least one item othewise calling `first()`
  //! would point to the end of the array, which is not initialized, and such
  //! reference would be invalid. Debug builds would catch this condition with
  //! an assertion.
  BL_INLINE const T& first() const noexcept { return at(0); }

  //! Returns a read-only reference to the last item.
  //!
  //! \note The array must have at least one item othewise calling `last()`
  //! would point to the end of the array, which is not initialized, and such
  //! reference would be invalid. Debug builds would catch this condition with
  //! an assertion.
  BL_INLINE const T& last() const noexcept { return at(impl->size - 1); }

  //! Clears the content of the array.
  //!
  //! \note If the array used dynamically allocated memory and the instance
  //! is mutable the memory won't be released, instead, it will be ready for
  //! reuse. Consider using `reset()` if you want to release the memory instead.
  BL_INLINE BLResult clear() noexcept { return blArrayClear(this); }

  //! Shrinks the capacity of the array to fit its length.
  //!
  //! Some array operations like `append()` may grow the array more than necessary
  //! to make it faster when such manipulation operations are called consecutively.
  //! When you are done with modifications and you know the lifetime of the array
  //! won't be short you can use `shrink()` to fit its memory requirements to the
  //! number of items it stores, which could optimize the application's memory
  //! requirements.
  BL_INLINE BLResult shrink() noexcept { return blArrayShrink(this); }

  //! Reserves the array capacity to hold at least `n` items.
  BL_INLINE BLResult reserve(size_t n) noexcept { return blArrayReserve(this, n); }

  //! Truncates the length of the array to maximum `n` items.
  //!
  //! If the length of the array is less than `n`n then truncation does nothing.
  BL_INLINE BLResult truncate(size_t n) noexcept { return blArrayResize(this, blMin(n, impl->size), nullptr); }

  //! Resizes the array to `n` items.
  //!
  //! If `n` is greater than the array length then all new items will be
  //! initialized by `fill` item.
  BL_INLINE BLResult resize(size_t n, const T& fill) noexcept { return blArrayResize(this, n, &fill); }

  //! \}

  //! \name Array Manipulation
  //! \{

  //! Makes the array mutable by possibly creating a deep copy of the data if
  //! it's either read-only or shared with another array. Stores the pointer
  //! to the beginning of mutable data in `dataOut`.
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
  //! // Calling array member functions (or C-API) could invalidate `data`.
  //! a.append(9); // You shouldn't use `data` afterwards.
  //! ```
  BL_INLINE BLResult makeMutable(T** dataOut) noexcept { return blArrayMakeMutable(this, (void**)dataOut); }

  //! Modify operation is similar to `makeMutable`, however, the `op` argument
  //! specifies the desired array operation, see \ref BLModifyOp. The pointer
  //! returned in `dataOut` points to the first item to be either assigned or
  //! appended and it points to an unititialized memory.
  //!
  //! Please note that assignments mean to wipe out the whole array content
  //! and to set the length of the array to `n`. The caller is responsible
  //! for initializing the data returned in `dataOut`.
  BL_INLINE BLResult modifyOp(uint32_t op, size_t n, T** dataOut) noexcept { return blArrayModifyOp(this, op, n, (void**)dataOut); }

  //! Insert operation, the semantics is similar to `modifyOp()`, however,
  //! items are inserted at the given `index` instead of assigned or appended.
  //!
  //! The caller is responsible for initializing the data returned in `dataOut`.
  BL_INLINE BLResult insertOp(size_t index, size_t n, T** dataOut) noexcept { return blArrayInsertOp(this, index, n, (void**)dataOut); }

  //! Similar to `modifyOp()`, but the items to assign/append to the array
  //! are given after the `op` argument.
  //!
  //! \note This is a varidic template that makes such modification easier
  //! if you have a constant number of items to assign or append.
  template<typename... Args>
  BL_INLINE BLResult modify_v(uint32_t op, Args&&... args) noexcept {
    T* dst;
    BL_PROPAGATE(blArrayModifyOp(this, op, sizeof...(args), (void**)&dst));
    BLArrayInternal::copyToUninitialized(dst, std::forward<Args>(args)...);
    return BL_SUCCESS;
  }

  //! Appends a variadic number of items items passed in `args...` to the array.
  template<typename... Args>
  BL_INLINE BLResult append(Args&&... args) noexcept {
    if (sizeof...(args) == 1)
      return BLArrayInternal::appendItem(this, Traits::pass(BLArrayInternal::first(std::forward<Args>(args)...)));
    else
      return modify_v(BL_MODIFY_OP_APPEND_GROW, std::forward<Args>(args)...);
  }

  //! Appends items to the array of the given array `view`.
  //!
  //! \note The implementation can handle `view` pointing to the array's data
  //! as well, so it's possible to duplicate the given items by appending them.
  BL_INLINE BLResult appendView(const BLArrayView<T>& view) noexcept { return blArrayAppendView(this, (const void*)view.data, view.size); }

  //! Appends `items` to the array of length `n`.
  //!
  //! \note The implementation can handle `view` pointing to the array's data
  //! as well, so it's possible to duplicate the given items by appending them.
  BL_INLINE BLResult appendView(const T* items, size_t n) noexcept { return blArrayAppendView(this, (const void*)items, n); }

  //! Prepends a variadic number of items items passed in `args...` to the array.
  template<typename... Args>
  BL_INLINE BLResult prepend(Args&&... args) noexcept {
    if (sizeof...(args) == 1)
      return BLArrayInternal::insertItem(this, 0, Traits::pass(BLArrayInternal::first(std::forward<Args>(args)...)));
    else
      return insert(0, std::forward<Args>(args)...);
  }

  //! Prepends items to the array of the given array `view`.
  //!
  //! \note The implementation can handle `view` pointing to the array's data
  //! as well, so it's possible to duplicate the given items by prepending them.
  BL_INLINE BLResult prependView(const BLArrayView<T>& view) noexcept { return blArrayInsertView(this, 0, (const void*)view.data, view.size); }

  //! Prepends `items` to the array of length `n`.
  //!
  //! \note The implementation can handle `view` pointing to the array's data
  //! as well, so it's possible to duplicate the given items by prepending them.
  BL_INLINE BLResult prependView(const T* items, size_t n) noexcept { return blArrayInsertView(this, 0, (const void*)items, n); }

  //! Inserts a variadic number of items items passed in `args...` at the given `index`.
  template<typename... Args>
  BL_INLINE BLResult insert(size_t index, Args&&... args) noexcept {
    if (sizeof...(args) == 1) {
      return BLArrayInternal::insertItem(this, index, Traits::pass(BLArrayInternal::first(std::forward<Args>(args)...)));
    }
    else {
      T* dst;
      BL_PROPAGATE(blArrayInsertOp(this, index, sizeof...(args), (void**)&dst));
      BLArrayInternal::copyToUninitialized(dst, std::forward<Args>(args)...);
      return BL_SUCCESS;
    }
  }

  //! Inserts items to the array of the given array `view` at the given `index`.
  //!
  //! \note The implementation can handle `view` pointing to the array's data
  //! as well, so it's possible to duplicate the given items by inserting them.
  BL_INLINE BLResult insertView(size_t index, const BLArrayView<T>& view) noexcept { return blArrayInsertView(this, index, (const void*)view.data, view.size); }

  //! Prepends `items` to the array of length `n` at the given `index`.
  //!
  //! \note The implementation can handle `view` pointing to the array's data
  //! as well, so it's possible to duplicate the given items by inserting them.
  BL_INLINE BLResult insertView(size_t index, const T* items, size_t n) noexcept { return blArrayInsertView(this, index, (const void*)items, n); }

  //! Replaces an item at the given `index` by `item`.
  BL_INLINE BLResult replace(size_t index, const T& item) noexcept { return BLArrayInternal::replaceItem(this, index, Traits::pass(item)); }

  //! Replaces the given `range` of items by the given array `view`.
  BL_INLINE BLResult replaceView(const BLRange& range, BLArrayView<T>& view) noexcept { return blArrayReplaceView(this, range.start, range.end, (const void*)view.data, view.size); }

  //! Replaces the given `range` of items by `items` of length `n`.
  BL_INLINE BLResult replaceView(const BLRange& range, const T* items, size_t n) noexcept { return blArrayReplaceView(this, range.start, range.end, items, n); }

  //! Removes an item at the given `index`.
  BL_INLINE BLResult remove(size_t index) noexcept { return blArrayRemoveIndex(this, index); }
  //! Removes a `range` of items.
  BL_INLINE BLResult remove(const BLRange& range) noexcept { return blArrayRemoveRange(this, range.start, range.end); }

  //! \}

  //! \name Search
  //! \{

  //! Returns the first index at which a given `item` can be found in the
  //! array, or `SIZE_MAX` if not present.
  BL_INLINE size_t indexOf(const T& item) const noexcept {
    return indexOf(item, 0);
  }

  //! Returns the index at which a given `item` can be found in the array
  //! starting from `fromIndex`, or `SIZE_MAX` if not present.
  BL_INLINE size_t indexOf(const T& item, size_t fromIndex) const noexcept {
    const T* p = data();
    size_t iEnd = size();

    for (size_t i = fromIndex; i < iEnd; i++)
      if (p[i] == item)
        return i;

    return SIZE_MAX;
  }

  //! Returns the last index at which a given `item` can be found in the array,
  //! or `SIZE_MAX` if not present.
  BL_INLINE size_t lastIndexOf(const T& item) const noexcept {
    const T* p = data();
    size_t i = size();

    while (--i != SIZE_MAX && !(p[i] == item))
      continue;

    return i;
  }

  //! Returns the index at which a given `item` can be found in the array
  //! starting from `fromIndex` and ending at `0`, or `SIZE_MAX` if not present.
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

  static BL_INLINE const BLArray<T>& none() noexcept { return reinterpret_cast<const BLArray<T>*>(blNone)[kImplType]; }
};
#endif

//! \}

#endif // BLEND2D_ARRAY_H
