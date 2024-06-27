// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_SUPPORT_ARENABITARRAY_P_H_INCLUDED
#define BLEND2D_SUPPORT_ARENABITARRAY_P_H_INCLUDED

#include "../support/arenaallocator_p.h"
#include "../support/bitops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

//! Arena allocated bit array that uses `T` as an underlying bitword.
//!
//! T is usually wither `uint32_t` for compatibility with public API or `BLBitWord`, for maximum performance.
template<typename T>
class ArenaBitArray {
public:
  BL_NONCOPYABLE(ArenaBitArray)

  typedef ParametrizedBitOps<BitOrder::kMSB, T> Ops;

  //! \name Constants
  //! \{

  enum : uint32_t { kTSizeInBits = sizeof(T) * 8u };

  //! \}

  //! \name Members
  //! \{

  //! Bits.
  T* _data = nullptr;
  //! Size of the bit array (in bits).
  uint32_t _size = 0;
  //! Capacity of the bit array (in bits).
  uint32_t _capacity = 0;

  //! \}

  //! \cond INTERNAL
  //! \name Internal
  //! \{

  static BL_INLINE uint32_t _wordsPerBits(uint32_t bitCount) noexcept {
    return ((bitCount + kTSizeInBits - 1) / kTSizeInBits);
  }

  static BL_INLINE void _zeroBits(T* dst, uint32_t wordCount) noexcept {
    for (uint32_t i = 0; i < wordCount; i++)
      dst[i] = 0;
  }

  static BL_INLINE void _fillBits(T* dst, uint32_t wordCount) noexcept {
    for (uint32_t i = 0; i < wordCount; i++)
      dst[i] = ~T(0);
  }

  static BL_INLINE void _copyBits(T* dst, const T* src, uint32_t wordCount) noexcept {
    for (uint32_t i = 0; i < wordCount; i++)
      dst[i] = src[i];
  }

  //! \}
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE ArenaBitArray() noexcept {}

  BL_INLINE ArenaBitArray(ArenaBitArray&& other) noexcept
    : _data(other._data),
      _size(other._size),
      _capacity(other._capacity) {}

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_INLINE bool operator==(const ArenaBitArray& other) const noexcept { return  eq(other); }
  BL_INLINE bool operator!=(const ArenaBitArray& other) const noexcept { return !eq(other); }

  //! \}

  //! \name Accessors
  //! \{

  //! Tests whether the bit array is empty (has no bits).
  BL_INLINE bool empty() const noexcept { return _size == 0; }
  //! Returns the size of this bit array (in bits).
  BL_INLINE uint32_t size() const noexcept { return _size; }
  //! Returns the capacity of this bit array (in bits).
  BL_INLINE uint32_t capacity() const noexcept { return _capacity; }

  //! Returns the size of the `T[]` array in `T` units.
  BL_INLINE uint32_t sizeInWords() const noexcept { return _wordsPerBits(_size); }
  //! Returns the capacity of the `T[]` array in `T` units.
  BL_INLINE uint32_t capacityInWords() const noexcept { return _wordsPerBits(_capacity); }

  //! REturns bit array data as `T[]`.
  BL_INLINE T* data() noexcept { return _data; }
  //! \overload
  BL_INLINE const T* data() const noexcept { return _data; }

  //! \}

  //! \name Utilities
  //! \{

  BL_INLINE void swap(ArenaBitArray& other) noexcept {
    BLInternal::swap(_data, other._data);
    BLInternal::swap(_size, other._size);
    BLInternal::swap(_capacity, other._capacity);
  }

  BL_INLINE void clear() noexcept {
    _size = 0;
  }

  BL_INLINE void reset() noexcept {
    _data = nullptr;
    _size = 0;
    _capacity = 0;
  }

  BL_INLINE void truncate(uint32_t newSize) noexcept {
    _size = blMin(_size, newSize);
    _clearUnusedBits();
  }

  BL_INLINE bool bitAt(uint32_t index) const noexcept {
    BL_ASSERT(index < _size);
    return Ops::bitArrayTestBit(_data, index);
  }

  BL_INLINE void setBit(uint32_t index) noexcept {
    BL_ASSERT(index < _size);
    Ops::bitArraySetBit(_data, index);
  }

  BL_INLINE void fillBits(uint32_t start, uint32_t count) noexcept {
    BL_ASSERT(start <= _size);
    BL_ASSERT(_size - start >= count);

    Ops::bitArrayFill(_data, start, count);
  }

  BL_INLINE void fillAll() noexcept {
    _fillBits(_data, _wordsPerBits(_size));
    _clearUnusedBits();
  }

  BL_INLINE void clearBit(uint32_t index) noexcept {
    BL_ASSERT(index < _size);
    Ops::bitArrayClearBit(_data, index);
  }

  BL_INLINE void clearBits(uint32_t start, uint32_t count) noexcept {
    BL_ASSERT(start <= _size);
    BL_ASSERT(_size - start >= count);

    Ops::bitArrayClear(_data, start, count);
  }

  BL_INLINE void clearAll() noexcept {
    _zeroBits(_data, _wordsPerBits(_size));
  }

  //! Performs a logical bitwise AND between bits specified in this array and bits in `other`. If `other` has less
  //! bits than `this` then all remaining bits are set to zero.
  //!
  //! \note The size of the BitVector is unaffected by this operation.
  BL_INLINE void and_(const ArenaBitArray& other) noexcept {
    T* dst = _data;
    const T* src = other._data;

    uint32_t thisBitWordCount = sizeInWords();
    uint32_t otherBitWordCount = other.sizeInWords();
    uint32_t commonBitWordCount = blMin(thisBitWordCount, otherBitWordCount);

    uint32_t i = 0;
    while (i < commonBitWordCount) {
      dst[i] = dst[i] & src[i];
      i++;
    }

    while (i < thisBitWordCount) {
      dst[i] = 0;
      i++;
    }
  }

  //! Performs a logical bitwise AND between bits specified in this array and negated bits in `other`. If `other`
  //! has less bits than `this` then all remaining bits are kept intact.
  //!
  //! \note The size of the BitVector is unaffected by this operation.
  BL_INLINE void andNot(const ArenaBitArray& other) noexcept {
    T* dst = _data;
    const T* src = other._data;

    uint32_t commonBitWordCount = _wordsPerBits(blMin(_size, other._size));
    for (uint32_t i = 0; i < commonBitWordCount; i++)
      dst[i] = dst[i] & ~src[i];
  }

  //! Performs a logical bitwise OP between bits specified in this array and bits in `other`. If `other` has less
  //! bits than `this` then all remaining bits are kept intact.
  //!
  //! \note The size of the BitVector is unaffected by this operation.
  BL_INLINE void or_(const ArenaBitArray& other) noexcept {
    T* dst = _data;
    const T* src = other._data;

    uint32_t commonBitWordCount = _wordsPerBits(blMin(_size, other._size));
    for (uint32_t i = 0; i < commonBitWordCount; i++)
      dst[i] = dst[i] | src[i];
    _clearUnusedBits();
  }

  BL_INLINE void _clearUnusedBits() noexcept {
    uint32_t idx = _size / kTSizeInBits;
    uint32_t bit = _size % kTSizeInBits;

    if (!bit)
      return;
    _data[idx] &= Ops::nonZeroStartMask(bit);
  }

  BL_INLINE bool eq(const ArenaBitArray& other) const noexcept {
    if (_size != other._size)
      return false;

    const T* aData = _data;
    const T* bData = other._data;
    uint32_t count = sizeInWords();

    for (uint32_t i = 0; i < count; i++)
      if (aData[i] != bData[i])
        return false;

    return true;
  }

  //! \}

  //! \name Memory Management
  //! \{

  BL_INLINE void release(ArenaAllocator* allocator) noexcept {
    if (!_data)
      return;

    allocator->release(_data, _capacity / 8u);
    reset();
  }

  BL_INLINE BLResult resize(ArenaAllocator* allocator, uint32_t newSize, bool newBitsValue = false) noexcept {
    return _resize(allocator, newSize, newSize, newBitsValue);
  }

  BL_NOINLINE BLResult _resize(ArenaAllocator* allocator, uint32_t newSize, uint32_t capacityHint, bool newBitsValue) noexcept {
    BL_ASSERT(capacityHint >= newSize);

    if (newSize <= _size) {
      // The size after the resize is lesser than or equal to the current size.
      _size = newSize;
      _clearUnusedBits();
      return BL_SUCCESS;
    }

    uint32_t oldSize = _size;
    T* data = _data;

    if (newSize > _capacity) {
      // Realloc needed, calculate the minimum capacity (in bytes) required.
      uint32_t capacityInBits = IntOps::alignUp<uint32_t>(capacityHint, kTSizeInBits);

      if (BL_UNLIKELY(capacityInBits < newSize))
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);

      // Normalize to bytes.
      uint32_t capacityInBytes = capacityInBits / 8;

      T* newData = static_cast<T*>(allocator->alloc(capacityInBytes, alignof(T)));
      if (BL_UNLIKELY(!newData))
        return blTraceError(BL_ERROR_OUT_OF_MEMORY);

      _copyBits(newData, data, _wordsPerBits(oldSize));

      if (data)
        allocator->release(data, capacityInBytes);
      data = newData;

      _data = data;
      _capacity = capacityInBits;
    }

    // Start (of the old size) and end (of the new size) bits
    uint32_t idx = oldSize / kTSizeInBits;
    uint32_t startBit = oldSize % kTSizeInBits;

    // Set new bits to either 0 or 1. The `pattern` is used to set multiple bits per word and contains either all
    // zeros or all ones.
    T pattern = IntOps::bitMaskFromBool<T>(newBitsValue);

    // First initialize the last word of the old size.
    if (startBit)
      data[idx++] |= Ops::shiftToEnd(pattern, startBit);

    // Initialize all words after the last word of the old size.
    uint32_t endIdx = _wordsPerBits(newSize);
    while (idx < endIdx)
      data[idx++] = pattern;

    _size = newSize;
    _clearUnusedBits();
    return BL_SUCCESS;
  }

  //! \}
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_SUPPORT_ARENABITARRAY_P_H_INCLUDED
