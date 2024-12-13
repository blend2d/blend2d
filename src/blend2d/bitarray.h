// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_BITARRAY_H
#define BLEND2D_BITARRAY_H

#include "object.h"

//! \addtogroup bl_c_api
//! \{

//! \name BLBitArray - C API
//! \{

//! BitArray container [C API].
struct BLBitArrayCore BL_CLASS_INHERITS(BLObjectCore) {
  BL_DEFINE_OBJECT_DETAIL
  BL_DEFINE_OBJECT_DCAST(BLBitArray)
};

//! \cond INTERNAL
//! BitArray container [C API Impl].
struct BLBitArrayImpl BL_CLASS_INHERITS(BLObjectImpl) {
  //! Size in bit units.
  uint32_t size;
  //! Capacity in bit-word units.
  uint32_t capacity;

#ifdef __cplusplus
  //! Pointer to array data.
  BL_INLINE_NODEBUG uint32_t* data() noexcept { return reinterpret_cast<uint32_t*>(this + 1); }

  //! Pointer to array data (const).
  BL_INLINE_NODEBUG const uint32_t* data() const noexcept { return reinterpret_cast<const uint32_t*>(this + 1); }
#endif
};
//! \endcond

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blBitArrayInit(BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayInitMove(BLBitArrayCore* self, BLBitArrayCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayInitWeak(BLBitArrayCore* self, const BLBitArrayCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayDestroy(BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayReset(BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayAssignMove(BLBitArrayCore* self, BLBitArrayCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayAssignWeak(BLBitArrayCore* self, const BLBitArrayCore* other) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayAssignWords(BLBitArrayCore* self, const uint32_t* wordData, uint32_t wordCount) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blBitArrayIsEmpty(const BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API uint32_t BL_CDECL blBitArrayGetSize(const BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API uint32_t BL_CDECL blBitArrayGetWordCount(const BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API uint32_t BL_CDECL blBitArrayGetCapacity(const BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API const uint32_t* BL_CDECL blBitArrayGetData(const BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API uint32_t BL_CDECL blBitArrayGetCardinality(const BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API uint32_t BL_CDECL blBitArrayGetCardinalityInRange(const BLBitArrayCore* self, uint32_t startBit, uint32_t endBit) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blBitArrayHasBit(const BLBitArrayCore* self, uint32_t bitIndex) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blBitArrayHasBitsInRange(const BLBitArrayCore* self, uint32_t startBit, uint32_t endBit) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blBitArraySubsumes(const BLBitArrayCore* a, const BLBitArrayCore* b) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blBitArrayIntersects(const BLBitArrayCore* a, const BLBitArrayCore* b) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blBitArrayGetRange(const BLBitArrayCore* self, uint32_t* startOut, uint32_t* endOut) BL_NOEXCEPT_C;
BL_API bool BL_CDECL blBitArrayEquals(const BLBitArrayCore* a, const BLBitArrayCore* b) BL_NOEXCEPT_C;
BL_API int BL_CDECL blBitArrayCompare(const BLBitArrayCore* a, const BLBitArrayCore* b) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayClear(BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayResize(BLBitArrayCore* self, uint32_t nBits) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayReserve(BLBitArrayCore* self, uint32_t nBits) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayShrink(BLBitArrayCore* self) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArraySetBit(BLBitArrayCore* self, uint32_t bitIndex) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayFillRange(BLBitArrayCore* self, uint32_t startBit, uint32_t endBit) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayFillWords(BLBitArrayCore* self, uint32_t bitIndex, const uint32_t* wordData, uint32_t wordCount) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayClearBit(BLBitArrayCore* self, uint32_t bitIndex) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayClearRange(BLBitArrayCore* self, uint32_t startBit, uint32_t endBit) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayClearWord(BLBitArrayCore* self, uint32_t bitIndex, uint32_t wordValue) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayClearWords(BLBitArrayCore* self, uint32_t bitIndex, const uint32_t* wordData, uint32_t wordCount) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayReplaceOp(BLBitArrayCore* self, uint32_t nBits, uint32_t** dataOut) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayReplaceBit(BLBitArrayCore* self, uint32_t bitIndex, bool bitValue) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayReplaceWord(BLBitArrayCore* self, uint32_t bitIndex, uint32_t wordValue) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayReplaceWords(BLBitArrayCore* self, uint32_t bitIndex, const uint32_t* wordData, uint32_t wordCount) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayAppendBit(BLBitArrayCore* self, bool bitValue) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayAppendWord(BLBitArrayCore* self, uint32_t wordValue) BL_NOEXCEPT_C;
BL_API BLResult BL_CDECL blBitArrayAppendWords(BLBitArrayCore* self, const uint32_t* wordData, uint32_t wordCount) BL_NOEXCEPT_C;

// TODO: Future API (BitArray).
/*
BL_API BLResult BL_CDECL blBitArrayCombine(BLBitArrayCore* dst, const BLBitArrayCore* a, const BLBitArrayCore* b, BLBooleanOp booleanOp) BL_NOEXCEPT_C;
*/

BL_END_C_DECLS

//! \}

//! \}

//! \addtogroup bl_containers
//! \{

//! \name BLBitArray - C++ API
//! \{
#ifdef __cplusplus

//! BitArray container [C++ API].
class BLBitArray final : public BLBitArrayCore {
public:
  //! \cond INTERNAL
  //! \name Internals
  //! \{

  enum : uint32_t {
    //! Number of words that can be used by SSO representation.
    kSSOWordCount = 3,

    //! Signature of SSO representation of an empty BitArray.
    kSSOEmptySignature = BLObjectInfo::packTypeWithMarker(BL_OBJECT_TYPE_BIT_ARRAY)
  };

  BL_INLINE_NODEBUG BLBitArrayImpl* _impl() const noexcept { return static_cast<BLBitArrayImpl*>(_d.impl); }

  //! \}
  //! \endcond

  //! \name Construction & Destruction
  //! \{

  BL_INLINE_NODEBUG BLBitArray() noexcept {
    _d.initStatic(BLObjectInfo{kSSOEmptySignature});
  }

  BL_INLINE_NODEBUG BLBitArray(BLBitArray&& other) noexcept {
    _d = other._d;
    other._d.initStatic(BLObjectInfo{kSSOEmptySignature});
  }

  BL_INLINE_NODEBUG BLBitArray(const BLBitArray& other) noexcept { blBitArrayInitWeak(this, &other); }

  //! Destroys the BitArray.
  BL_INLINE_NODEBUG ~BLBitArray() noexcept {
    if (BLInternal::objectNeedsCleanup(_d.info.bits))
      blBitArrayDestroy(this);
  }

  //! \}

  //! \name Overloaded Operators
  //! \{

  //! Tests whether the BitArray has a content.
  //!
  //! \note This is essentially the opposite of `empty()`.
  BL_INLINE_NODEBUG explicit operator bool() const noexcept { return !empty(); }

  //! Move assignment.
  //!
  //! \note The `other` BitArray is reset by move assignment, so its state after the move operation is the same as
  //! a default constructed BitArray.
  BL_INLINE_NODEBUG BLBitArray& operator=(BLBitArray&& other) noexcept { blBitArrayAssignMove(this, &other); return *this; }
  //! Copy assignment, performs weak copy of the data held by the `other` BitArray.
  BL_INLINE_NODEBUG BLBitArray& operator=(const BLBitArray& other) noexcept { blBitArrayAssignWeak(this, &other); return *this; }

  BL_INLINE_NODEBUG bool operator==(const BLBitArray& other) const noexcept { return equals(other); }
  BL_INLINE_NODEBUG bool operator!=(const BLBitArray& other) const noexcept { return !equals(other); }
  BL_INLINE_NODEBUG bool operator<(const BLBitArray& other) const noexcept { return compare(other) < 0; }
  BL_INLINE_NODEBUG bool operator<=(const BLBitArray& other) const noexcept { return compare(other) <= 0; }
  BL_INLINE_NODEBUG bool operator>(const BLBitArray& other) const noexcept { return compare(other) > 0; }
  BL_INLINE_NODEBUG bool operator>=(const BLBitArray& other) const noexcept { return compare(other) >= 0; }

  //! \}

  //! \name Common Functionality
  //! \{

  //! Clears the content of the BitArray and releases its data.
  //!
  //! After reset the BitArray content matches a default constructed instance.
  BL_INLINE_NODEBUG BLResult reset() noexcept { return blBitArrayReset(this); }

  //! Swaps the content of this string with the `other` string.
  BL_INLINE_NODEBUG void swap(BLBitArrayCore& other) noexcept { _d.swap(other._d); }

  //! \name Accessors
  //! \{

  //! Tests whether the BitArray is empty (has no content).
  //!
  //! Returns `true` if the BitArray's size is zero.
  BL_INLINE_NODEBUG bool empty() const noexcept { return blBitArrayIsEmpty(this); }

  //! Returns the size of the BitArray in bits.
  BL_INLINE_NODEBUG uint32_t size() const noexcept { return _d.sso() ? uint32_t(_d.pField()) : _impl()->size; }

  //! Returns number of BitWords this BitArray uses.
  BL_INLINE_NODEBUG uint32_t wordCount() const noexcept {
    return sizeof(void*) >= 8 ? (uint32_t((uint64_t(size()) + 31u) / 32u))
                              : (size() / 32u + uint32_t((size() & 31u) != 0u));
  }

  //! Returns the capacity of the BitArray in bits.
  BL_INLINE_NODEBUG uint32_t capacity() const noexcept { return _d.sso() ? uint32_t(kSSOWordCount * 32u) : _impl()->capacity; }

  //! Returns the number of bits set in the BitArray.
  BL_INLINE_NODEBUG uint32_t cardinality() const noexcept { return blBitArrayGetCardinality(this); }

  //! Returns the number of bits set in the given `[startBit, endBit)` range.
  BL_INLINE_NODEBUG uint32_t cardinalityInRange(uint32_t startBit, uint32_t endBit) const noexcept { return blBitArrayGetCardinalityInRange(this, startBit, endBit); }

  //! Returns bit data.
  BL_INLINE_NODEBUG const uint32_t* data() const noexcept { return _d.sso() ? _d.u32_data : _impl()->data(); }

  //! \}

  //! \name Test Operations
  //! \{

  //! Returns a bit-value at the given `bitIndex`.
  BL_INLINE_NODEBUG bool hasBit(uint32_t bitIndex) const noexcept { return blBitArrayHasBit(this, bitIndex); }
  //! Returns whether the bit-set has at least on bit in the given `[startBit, endbit)` range.
  BL_INLINE_NODEBUG bool hasBitsInRange(uint32_t startBit, uint32_t endBit) const noexcept { return blBitArrayHasBitsInRange(this, startBit, endBit); }

  //! Returns whether this BitArray subsumes `other`.
  BL_INLINE_NODEBUG bool subsumes(const BLBitArrayCore& other) const noexcept { return blBitArraySubsumes(this, &other); }
  //! Returns whether this BitArray intersects with `other`.
  BL_INLINE_NODEBUG bool intersects(const BLBitArrayCore& other) const noexcept { return blBitArrayIntersects(this, &other); }

  //! \}

  //! \name Equality & Comparison
  //! \{

  //! Returns whether this BitArray and `other` are bitwise equal.
  BL_INLINE_NODEBUG bool equals(const BLBitArrayCore& other) const noexcept { return blBitArrayEquals(this, &other); }
  //! Compares this BitArray with `other` and returns either `-1`, `0`, or `1`.
  BL_INLINE_NODEBUG int compare(const BLBitArrayCore& other) const noexcept { return blBitArrayCompare(this, &other); }

  //! \}

  //! \name Content Manipulation
  //! \{

  //! Move assignment, the same as `operator=`, but returns a `BLResult` instead of `this`.
  BL_INLINE_NODEBUG BLResult assign(BLBitArrayCore&& other) noexcept { return blBitArrayAssignMove(this, &other); }

  //! Copy assignment, the same as `operator=`, but returns a `BLResult` instead of `this`.
  BL_INLINE_NODEBUG BLResult assign(const BLBitArrayCore& other) noexcept { return blBitArrayAssignWeak(this, &other); }

  //! Replaces the content of the BitArray by bits specified by `wordData` of size `wordCount` [the size is in uint32_t units].
  BL_INLINE_NODEBUG BLResult assignWords(const uint32_t* wordData, uint32_t wordCount) noexcept { return blBitArrayAssignWords(this, wordData, wordCount); }

  //! Clears the content of the BitArray without releasing its dynamically allocated data, if possible.
  BL_INLINE_NODEBUG BLResult clear() noexcept { return blBitArrayClear(this); }

  //! Resizes the BitArray so its size matches `nBits`.
  BL_INLINE_NODEBUG BLResult resize(uint32_t nBits) noexcept { return blBitArrayResize(this, nBits); }

  //! Reserves `nBits` in the BitArray (capacity would match `nBits`) without changing its size.
  BL_INLINE_NODEBUG BLResult reserve(uint32_t nBits) noexcept { return blBitArrayResize(this, nBits); }

  //! Shrinks the capacity of the BitArray to match the actual content with the intention to save memory.
  BL_INLINE_NODEBUG BLResult shrink() noexcept { return blBitArrayShrink(this); }

  //! Sets a bit to true at the given `bitIndex`.
  BL_INLINE_NODEBUG BLResult setBit(uint32_t bitIndex) noexcept { return blBitArraySetBit(this, bitIndex); }

  //! Fills bits in `[startBit, endBit)` range to true.
  BL_INLINE_NODEBUG BLResult fillRange(uint32_t startBit, uint32_t endBit) noexcept { return blBitArrayFillRange(this, startBit, endBit); }

  //! Fills bits starting from `bitIndex` specified by `wordData` and `wordCount` to true (zeros in wordData are ignored).
  //!
  //! \note This operation uses an `OR` operator - bits in `wordData` are combined with OR operator with existing bits in BitArray.
  BL_INLINE_NODEBUG BLResult fillWords(uint32_t bitIndex, const uint32_t* wordData, uint32_t wordCount) noexcept { return blBitArrayFillWords(this, bitIndex, wordData, wordCount); }

  //! Sets a bit to false at the given `bitIndex`.
  BL_INLINE_NODEBUG BLResult clearBit(uint32_t bitIndex) noexcept { return blBitArrayClearBit(this, bitIndex); }

  //! Sets bits in `[startBit, endBit)` range to false.
  BL_INLINE_NODEBUG BLResult clearRange(uint32_t startBit, uint32_t endBit) noexcept { return blBitArrayClearRange(this, startBit, endBit); }

  //! Sets bits starting from `bitIndex` specified by `wordValue` to false (zeros in wordValue are ignored).
  //!
  //! \note This operation uses an `AND_NOT` operator - bits in `wordData` are negated and then combined with AND operator with existing bits in BitArray.
  BL_INLINE_NODEBUG BLResult clearWord(uint32_t bitIndex, uint32_t wordValue) noexcept { return blBitArrayClearWord(this, bitIndex, wordValue); }

  //! Sets bits starting from `bitIndex` specified by `wordData` and `wordCount` to false (zeros in wordData are ignored).
  //!
  //! \note This operation uses an `AND_NOT` operator - bits in `wordData` are negated and then combined with AND operator with existing bits in BitArray.
  BL_INLINE_NODEBUG BLResult clearWords(uint32_t bitIndex, const uint32_t* wordData, uint32_t wordCount) noexcept { return blBitArrayClearWords(this, bitIndex, wordData, wordCount); }

  //! Makes the BitArray mutable with the intention to replace all bits of it.
  //!
  //! \note All bits in the BitArray will be set to zero.
  BL_INLINE_NODEBUG BLResult replaceOp(uint32_t nBits, uint32_t** dataOut) noexcept { return blBitArrayReplaceOp(this, nBits, dataOut); }

  //! Replaces a bit in the BitArray at the given `bitIndex` to match `bitValue`.
  BL_INLINE_NODEBUG BLResult replaceBit(uint32_t bitIndex, bool bitValue) noexcept { return blBitArrayReplaceBit(this, bitIndex, bitValue); }

  //! Replaces bits starting from `bitIndex` to match the bits specified by `wordValue`.
  //!
  //! \note Replaced bits from BitArray are not combined by using any operator, `wordValue` is copied as is, thus
  //! replaces fully the existing bits.
  BL_INLINE_NODEBUG BLResult replaceWord(uint32_t bitIndex, uint32_t wordValue) noexcept { return blBitArrayReplaceWord(this, bitIndex, wordValue); }

  //! Replaces bits starting from `bitIndex` to match the bits specified by `wordData` and `wordCount`.
  //!
  //! \note Replaced bits from BitArray are not combined by using any operator, `wordData` is copied as is, thus
  //! replaces fully the existing bits.
  BL_INLINE_NODEBUG BLResult replaceWords(uint32_t bitIndex, const uint32_t* wordData, uint32_t wordCount) noexcept { return blBitArrayReplaceWords(this, bitIndex, wordData, wordCount); }

  //! Appends a bit `bitValue` to the BitArray.
  BL_INLINE_NODEBUG BLResult appendBit(bool bitValue) noexcept { return blBitArrayAppendBit(this, bitValue); }

  //! Appends a single word `wordValue` to the BitArray.
  BL_INLINE_NODEBUG BLResult appendWord(uint32_t wordValue) noexcept { return blBitArrayAppendWord(this, wordValue); }

  //! Appends whole words to the BitArray.
  BL_INLINE_NODEBUG BLResult appendWords(const uint32_t* wordData, uint32_t wordCount) noexcept { return blBitArrayAppendWords(this, wordData, wordCount); }

  /*
  // TODO: Future API (BitArray).

  BL_INLINE_NODEBUG BLResult and_(const BLBitArrayCore& other) noexcept { return blBitArrayCombine(this, this, &other, BL_BOOLEAN_OP_AND); }
  BL_INLINE_NODEBUG BLResult or_(const BLBitArrayCore& other) noexcept { return blBitArrayCombine(this, this, &other, BL_BOOLEAN_OP_OR); }
  BL_INLINE_NODEBUG BLResult xor_(const BLBitArrayCore& other) noexcept { return blBitArrayCombine(this, this, &other, BL_BOOLEAN_OP_XOR); }
  BL_INLINE_NODEBUG BLResult andNot(const BLBitArrayCore& other) noexcept { return blBitArrayCombine(this, this, &other, BL_BOOLEAN_OP_AND_NOT); }
  BL_INLINE_NODEBUG BLResult notAnd(const BLBitArrayCore& other) noexcept { return blBitArrayCombine(this, this, &other, BL_BOOLEAN_OP_NOT_AND); }
  BL_INLINE_NODEBUG BLResult combine(const BLBitArrayCore& other, BLBooleanOp booleanOp) noexcept { return blBitArrayCombine(this, this, &other, booleanOp); }

  static BL_INLINE_NODEBUG BLResult and_(BLBitArrayCore& dst, const BLBitArrayCore& a, const BLBitArrayCore& b) noexcept { return blBitArrayCombine(&dst, &a, &b, BL_BOOLEAN_OP_AND); }
  static BL_INLINE_NODEBUG BLResult or_(BLBitArrayCore& dst, const BLBitArrayCore& a, const BLBitArrayCore& b) noexcept { return blBitArrayCombine(&dst, &a, &b, BL_BOOLEAN_OP_OR); }
  static BL_INLINE_NODEBUG BLResult xor_(BLBitArrayCore& dst, const BLBitArrayCore& a, const BLBitArrayCore& b) noexcept { return blBitArrayCombine(&dst, &a, &b, BL_BOOLEAN_OP_XOR); }
  static BL_INLINE_NODEBUG BLResult andNot(BLBitArrayCore& dst, const BLBitArrayCore& a, const BLBitArrayCore& b) noexcept { return blBitArrayCombine(&dst, &a, &b, BL_BOOLEAN_OP_AND_NOT); }
  static BL_INLINE_NODEBUG BLResult notAnd(BLBitArrayCore& dst, const BLBitArrayCore& a, const BLBitArrayCore& b) noexcept { return blBitArrayCombine(&dst, &a, &b, BL_BOOLEAN_OP_NOT_AND); }
  static BL_INLINE_NODEBUG BLResult combine(BLBitArrayCore& dst, const BLBitArrayCore& a, const BLBitArrayCore& b, BLBooleanOp booleanOp) noexcept { return blBitArrayCombine(&dst, &a, &b, booleanOp); }
  */

  //! \}
};

#endif
//! \}

//! \}

#endif // BLEND2D_BITARRAY_H
