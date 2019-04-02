// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLBITARRAY_P_H
#define BLEND2D_BLBITARRAY_P_H

#include "./blbitarray.h"
#include "./blsupport_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLBitArray - Internal]
// ============================================================================

namespace BLInternal {

struct BitAssign    { template<typename T> static BL_INLINE T op(T a, T b) noexcept { BL_UNUSED(a); return  b; } };
struct BitAssignNot { template<typename T> static BL_INLINE T op(T a, T b) noexcept { BL_UNUSED(a); return ~b; } };
struct BitAnd       { template<typename T> static BL_INLINE T op(T a, T b) noexcept { return  a &  b; } };
struct BitAndNot    { template<typename T> static BL_INLINE T op(T a, T b) noexcept { return  a & ~b; } };
struct BitNotAnd    { template<typename T> static BL_INLINE T op(T a, T b) noexcept { return ~a &  b; } };
struct BitOr        { template<typename T> static BL_INLINE T op(T a, T b) noexcept { return  a |  b; } };
struct BitXor       { template<typename T> static BL_INLINE T op(T a, T b) noexcept { return  a ^  b; } };

} // {BLInternal}

// ============================================================================
// [BLBitArray - Utilities]
// ============================================================================

template<typename T, class BitOp, class FullOp>
static BL_INLINE void blBitArrayOpInternal(T* buf, size_t index, size_t count) noexcept {
  if (count == 0)
    return;

  constexpr size_t kTSizeInBits = blBitSizeOf<T>();
  size_t vecIndex = index / kTSizeInBits; // T[]
  size_t bitIndex = index % kTSizeInBits; // T[][]

  buf += vecIndex;

  // The first BitWord requires special handling to preserve bits outside the fill region.
  constexpr T kFillMask = blBitOnes<T>();
  size_t firstNBits = blMin<size_t>(kTSizeInBits - bitIndex, count);

  buf[0] = BitOp::op(buf[0], (kFillMask >> (kTSizeInBits - firstNBits)) << bitIndex);
  buf++;
  count -= firstNBits;

  // All bits between the first and last affected BitWords can be just filled.
  while (count >= kTSizeInBits) {
    buf[0] = FullOp::op(buf[0], kFillMask);
    buf++;
    count -= kTSizeInBits;
  }

  // The last BitWord requires special handling as well.
  if (count)
    buf[0] = BitOp::op(buf[0], kFillMask >> (kTSizeInBits - count));
}

//! Fill `count` bits in bit-vector `buf` starting at bit-index `index`.
template<typename T>
static BL_INLINE void blBitArrayFillInternal(T* buf, size_t index, size_t count) noexcept { blBitArrayOpInternal<T, BLInternal::BitOr, BLInternal::BitAssign>(buf, index, count); }

//! Clear `count` bits in bit-vector `buf` starting at bit-index `index`.
template<typename T>
static BL_INLINE void blBitArrayClearInternal(T* buf, size_t index, size_t count) noexcept { blBitArrayOpInternal<T, BLInternal::BitAndNot, BLInternal::BitAssignNot>(buf, index, count); }

// ============================================================================
// [BLBitWordIterator]
// ============================================================================

//! Iterates over each bit in a number which is set to 1.
//!
//! Example of use:
//!
//! ```
//! uint32_t bitsToIterate = 0x110F;
//! BLBitWordIterator<uint32_t> it(bitsToIterate);
//!
//! while (it.hasNext()) {
//!   uint32_t bitIndex = it.next();
//!   printf("Bit at %u is set\n", unsigned(bitIndex));
//! }
//! ```
template<typename T>
class BLBitWordIterator {
public:
  BL_INLINE explicit BLBitWordIterator(T bitWord) noexcept
    : _bitWord(bitWord) {}

  BL_INLINE void init(T bitWord) noexcept { _bitWord = bitWord; }
  BL_INLINE bool hasNext() const noexcept { return _bitWord != 0; }

  BL_INLINE uint32_t next() noexcept {
    BL_ASSERT(_bitWord != 0);
    uint32_t index = blBitCtz(_bitWord);
    _bitWord ^= T(1u) << index;
    return index;
  }

  T _bitWord;
};

// ============================================================================
// [BLBitVectorIterator]
// ============================================================================

template<typename T>
class BLBitVectorIterator {
public:
  BL_INLINE BLBitVectorIterator(const T* data, size_t numBitWords, size_t start = 0) noexcept {
    init(data, numBitWords, start);
  }

  BL_INLINE void init(const T* data, size_t numBitWords, size_t start = 0) noexcept {
    const T* ptr = data + (start / blBitSizeOf<T>());
    size_t idx = blAlignDown(start, blBitSizeOf<T>());
    size_t end = numBitWords * blBitSizeOf<T>();

    T bitWord = T(0);
    if (idx < end) {
      bitWord = *ptr++ & (blBitOnes<T>() << (start % blBitSizeOf<T>()));
      while (!bitWord && (idx += blBitSizeOf<T>()) < end)
        bitWord = *ptr++;
    }

    _ptr = ptr;
    _idx = idx;
    _end = end;
    _current = bitWord;
  }

  BL_INLINE bool hasNext() const noexcept {
    return _current != T(0);
  }

  BL_INLINE size_t next() noexcept {
    T bitWord = _current;
    BL_ASSERT(bitWord != T(0));

    uint32_t bit = blBitCtz(bitWord);
    bitWord ^= T(1u) << bit;

    size_t n = _idx + bit;
    while (!bitWord && (_idx += blBitSizeOf<T>()) < _end)
      bitWord = *_ptr++;

    _current = bitWord;
    return n;
  }

  BL_INLINE size_t peekNext() const noexcept {
    BL_ASSERT(_current != T(0));
    return _idx + blBitCtz(_current);
  }

  const T* _ptr;
  size_t _idx;
  size_t _end;
  T _current;
};

// ============================================================================
// [BLBitVectorFlipIterator]
// ============================================================================

template<typename T>
class BLBitVectorFlipIterator {
public:
  BL_INLINE BLBitVectorFlipIterator(const T* data, size_t numBitWords, size_t start = 0, T xorMask = 0) noexcept {
    init(data, numBitWords, start, xorMask);
  }

  BL_INLINE void init(const T* data, size_t numBitWords, size_t start = 0, T xorMask = 0) noexcept {
    const T* ptr = data + (start / blBitSizeOf<T>());
    size_t idx = blAlignDown(start, blBitSizeOf<T>());
    size_t end = numBitWords * blBitSizeOf<T>();

    T bitWord = T(0);
    if (idx < end) {
      bitWord = (*ptr++ ^ xorMask) & (blBitOnes<T>() << (start % blBitSizeOf<T>()));
      while (!bitWord && (idx += blBitSizeOf<T>()) < end)
        bitWord = *ptr++ ^ xorMask;
    }

    _ptr = ptr;
    _idx = idx;
    _end = end;
    _current = bitWord;
    _xorMask = xorMask;
  }

  BL_INLINE bool hasNext() const noexcept {
    return _current != T(0);
  }

  BL_INLINE size_t next() noexcept {
    T bitWord = _current;
    BL_ASSERT(bitWord != T(0));

    uint32_t bit = blBitCtz(bitWord);
    bitWord ^= T(1u) << bit;

    size_t n = _idx + bit;
    while (!bitWord && (_idx += blBitSizeOf<T>()) < _end)
      bitWord = *_ptr++ ^ _xorMask;

    _current = bitWord;
    return n;
  }

  BL_INLINE size_t nextAndFlip() noexcept {
    T bitWord = _current;
    BL_ASSERT(bitWord != T(0));

    uint32_t bit = blBitCtz(bitWord);
    bitWord ^= blBitOnes<T>() << bit;
    _xorMask ^= blBitOnes<T>();

    size_t n = _idx + bit;
    while (!bitWord && (_idx += blBitSizeOf<T>()) < _end)
      bitWord = *_ptr++ ^ _xorMask;

    _current = bitWord;
    return n;
  }

  BL_INLINE size_t peekNext() const noexcept {
    BL_ASSERT(_current != T(0));
    return _idx + blBitCtz(_current);
  }

  const T* _ptr;
  size_t _idx;
  size_t _end;
  T _current;
  T _xorMask;
};

// ============================================================================
// [BLFixedBitArray]
// ============================================================================

template<typename T, size_t N>
class BLFixedBitArray {
public:
  enum : size_t {
    kSizeOfTInBits = blBitSizeOf<T>(),
    kFixedArraySize = (N + kSizeOfTInBits - 1) / kSizeOfTInBits
  };

  BL_INLINE bool bitAt(size_t index) const noexcept {
    BL_ASSERT(index < N);
    return bool((data[index / kSizeOfTInBits] >> (index % kSizeOfTInBits)) & 0x1);
  }

  BL_INLINE void setAt(size_t index) noexcept {
    BL_ASSERT(index < N);
    data[index / kSizeOfTInBits] |= T(1) << (index % kSizeOfTInBits);
  }

  BL_INLINE void setAt(size_t index, bool value) noexcept {
    BL_ASSERT(index < N);

    T clrMask = T(1    ) << (index % kSizeOfTInBits);
    T setMask = T(value) << (index % kSizeOfTInBits);
    data[index / kSizeOfTInBits] = (data[index / kSizeOfTInBits] & ~clrMask) | setMask;
  }

  BL_INLINE void clearAt(size_t index) noexcept {
    BL_ASSERT(index < N);
    data[index / kSizeOfTInBits] &= ~(T(1) << (index % kSizeOfTInBits));
  }

  BL_INLINE void clearAll() noexcept {
    for (size_t i = 0; i < kFixedArraySize; i++)
      data[i] = 0;
  }

  BL_INLINE void setAll() noexcept {
    for (size_t i = 0; i < kFixedArraySize; i++)
      data[i] = blBitOnes<T>();
  }

  T data[kFixedArraySize];
};

//! \}
//! \endcond

#endif // BLEND2D_BLBITARRAY_P_H
