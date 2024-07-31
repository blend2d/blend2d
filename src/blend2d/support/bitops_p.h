// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_SUPPORT_BITOPS_P_H_INCLUDED
#define BLEND2D_SUPPORT_BITOPS_P_H_INCLUDED

#include "../api-internal_p.h"
#include "../support/traits_p.h"
#include "../support/intops_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

//! \name Constants
//! \{

//! Defines an ordering of bits in a bit-word or bit-array.
enum class BitOrder : uint32_t {
  //! Least significant bit is considered first.
  kLSB = 0,
  //! Most significant bit is considered first.
  kMSB = 1,
  //! Bit ordering used in public interface.
  kPublic = kMSB,
  //! Bit ordering that is fastest on the given architecture, but used only internally.
  kPrivate = BL_TARGET_ARCH_X86 ? kLSB : kMSB
};

//! \}

namespace BitOperator {
namespace {

//! \name Bit Operators
//! \{

struct Assign {
  template<typename T>
  static BL_INLINE_NODEBUG T op(const T&, const T& b) noexcept { return b; }

  template<typename T>
  static BL_INLINE_NODEBUG T opMasked(const T& a, const T& b, T mask) noexcept { return (a & ~mask) | (b & mask); }
};

struct AssignNot {
  template<typename T>
  static BL_INLINE_NODEBUG T op(const T&, const T& b) noexcept { return ~b; }

  template<typename T>
  static BL_INLINE_NODEBUG T opMasked(const T& a, const T& b, T mask) noexcept { return (a & ~mask) | (~b & mask); }
};

struct And {
  template<typename T>
  static BL_INLINE_NODEBUG T op(const T& a, const T& b) noexcept { return a & b; }

  template<typename T>
  static BL_INLINE_NODEBUG T opMasked(const T& a, const T& b, T mask) noexcept { return a & (b | ~mask); }
};

struct AndNot {
  template<typename T>
  static BL_INLINE_NODEBUG T op(const T& a, const T& b) noexcept { return a & ~b; }

  template<typename T>
  static BL_INLINE_NODEBUG T opMasked(const T& a, const T& b, T mask) noexcept { return a & ~(b & mask); }
};

struct NotAnd {
  template<typename T>
  static BL_INLINE_NODEBUG T op(const T& a, const T& b) noexcept { return ~a & b; }

  template<typename T>
  static BL_INLINE_NODEBUG T opMasked(const T& a, const T& b, T mask) noexcept { return (a ^ mask) & (b | ~mask); }
};

struct Or {
  template<typename T>
  static BL_INLINE_NODEBUG T op(const T& a, const T& b) noexcept { return a | b; }

  template<typename T>
  static BL_INLINE_NODEBUG T opMasked(const T& a, const T& b, T mask) noexcept { return a | (b & mask); }
};

struct Xor {
  template<typename T>
  static BL_INLINE_NODEBUG T op(const T& a, const T& b) noexcept { return a ^ b; }

  template<typename T>
  static BL_INLINE_NODEBUG T opMasked(const T& a, const T& b, T mask) noexcept { return a ^ (b & mask); }
};

//! \}

} // {anonymous}
} // {BitOperator}

namespace {

//! \name Parametrized Bit Operators
//! \{

//! Parametrized bit operations.
//!
//! This class acts as a namespace and allows to parametrize how bits are stored in a BitWord. The reason for
//! parametrization is architecture constraints. X86 architecture prefers LSB ordering, because of the performance
//! of BSF and TZCNT instructions. Since TZCNT instruction is BSF with REP prefix compilers can savely emit TZCNT
//! instead of BSF, but it's not possible to emit LZCNT instead of BSR as LZCNT returns a different result (count
//! of zeros instead of first zero index).
//!
//! ARM and other architectures only implement LZCNT (count leading zeros) and counting trailing zeros means emitting
//! more instructions to workaround the missing instruction.
template<BitOrder kBO, typename T>
struct ParametrizedBitOps {
  typedef typename std::make_unsigned<T>::type U;

  static constexpr BitOrder kBitOrder = kBO;
  static constexpr BitOrder kReverseBitOrder = BitOrder(uint32_t(kBO) ^ 1u);

  enum : uint32_t {
    kIsLSB = (kBO == BitOrder::kLSB),
    kIsMSB = (kBO == BitOrder::kMSB),

    kNumBits = IntOps::bitSizeOf<T>(),
    kBitMask = kNumBits - 1
  };

  static BL_INLINE_NODEBUG constexpr T zero() noexcept { return T(0); }
  static BL_INLINE_NODEBUG constexpr T ones() noexcept { return IntOps::allOnes<T>(); }

  template<typename Index>
  static BL_INLINE_NODEBUG constexpr bool hasBit(const T& x, const Index& index) noexcept {
    return kIsLSB ? bool((x >> index) & 0x1) : bool((x >> (index ^ Index(kBitMask))) & 0x1);
  }

  template<typename Count>
  static BL_INLINE_NODEBUG constexpr T shiftToStart(const T& x, const Count& y) noexcept {
    return kIsLSB ? IntOps::shr(x, y) : IntOps::shl(x, y);
  }

  template<typename Count>
  static BL_INLINE_NODEBUG constexpr T shiftToEnd(const T& x, const Count& y) noexcept {
    return kIsLSB ? IntOps::shl(x, y) : IntOps::shr(x, y);
  }

  template<typename Count>
  static BL_INLINE_NODEBUG constexpr T nonZeroStartMask(const Count& count = 1) noexcept {
    return kIsLSB ? IntOps::nonZeroLsbMask<T>(count) : IntOps::nonZeroMsbMask<T>(count);
  }

  template<typename Index, typename Count>
  static BL_INLINE_NODEBUG constexpr T nonZeroStartMask(const Count& count, const Index& index) noexcept {
    return shiftToEnd(nonZeroStartMask(count), index);
  }

  template<typename N>
  static BL_INLINE_NODEBUG constexpr T nonZeroEndMask(const N& n = 1) noexcept {
    return kIsLSB ? IntOps::nonZeroMsbMask<T>(n) : IntOps::nonZeroLsbMask<T>(n);
  }

  template<typename Index, typename Count>
  static BL_INLINE_NODEBUG constexpr T nonZeroEndMask(const Count& count, const Index& index) noexcept {
    return shiftToStart(nonZeroEndMask(count), index);
  }

  template<typename Index>
  static BL_INLINE_NODEBUG constexpr T indexAsMask(const Index& index) noexcept {
    return kIsLSB ? IntOps::shl(T(1), index) : IntOps::shr(IntOps::nonZeroMsbMask<T>(), index);
  }

  template<typename Index, typename Value>
  static BL_INLINE_NODEBUG constexpr T indexAsMask(const Index& index, const Value& value) noexcept {
    return kIsLSB ? IntOps::shl(T(value), index) : IntOps::shr(T(value) << kBitMask, index);
  }

  static BL_INLINE_NODEBUG uint32_t countZerosFromStart(const T& x) noexcept {
    return kIsLSB ? IntOps::ctz(x) : IntOps::clz(x);
  }

  static BL_INLINE_NODEBUG uint32_t countZerosFromEnd(const T& x) noexcept {
    return kIsLSB ? IntOps::clz(x) : IntOps::ctz(x);
  }

  static BL_INLINE int compare(const T& x, const T& y) noexcept {
    T xv = kIsLSB ? IntOps::bitSwap(x) : x;
    T yv = kIsLSB ? IntOps::bitSwap(y) : y;

    return int(xv > yv) - int(xv < yv);
  }

  static BL_INLINE bool bitArrayTestBit(const T* buf, size_t index) noexcept {
    size_t vecIndex = index / kNumBits;
    size_t bitIndex = index % kNumBits;

    return (buf[vecIndex] & indexAsMask(bitIndex)) != 0u;
  }

  static BL_INLINE void bitArraySetBit(T* buf, size_t index) noexcept {
    size_t vecIndex = index / kNumBits;
    size_t bitIndex = index % kNumBits;

    buf[vecIndex] |= indexAsMask(bitIndex);
  }

  static BL_INLINE void bitArrayOrBit(T* buf, size_t index, bool value) noexcept {
    size_t vecIndex = index / kNumBits;
    size_t bitIndex = index % kNumBits;

    buf[vecIndex] |= indexAsMask(bitIndex, value);
  }

  static BL_INLINE void bitArrayClearBit(T* buf, size_t index) noexcept {
    size_t vecIndex = index / kNumBits;
    size_t bitIndex = index % kNumBits;

    buf[vecIndex] &= ~indexAsMask(bitIndex);
  }

  template<class BitOp, class FullOp>
  static BL_INLINE void bitArrayOp(T* buf, size_t index, size_t count) noexcept {
    if (count == 0)
      return;

    size_t vecIndex = index / kNumBits; // T[]
    size_t bitIndex = index % kNumBits; // T[][]

    // The first BitWord requires special handling to preserve bits outside the fill region.
    size_t firstNBits = blMin<size_t>(kNumBits - bitIndex, count);
    T firstNBitsMask = shiftToEnd(nonZeroStartMask(firstNBits), bitIndex);

    buf[vecIndex] = BitOp::op(buf[vecIndex], firstNBitsMask);
    count -= firstNBits;
    if (count == 0)
      return;
    vecIndex++;

    // All bits between the first and last affected BitWords can be just filled.
    while (count >= kNumBits) {
      buf[vecIndex] = FullOp::op(buf[vecIndex], ones());
      vecIndex++;
      count -= kNumBits;
    }

    // The last BitWord requires special handling as well.
    if (count) {
      T lastNBitsMask = nonZeroStartMask(count);
      buf[vecIndex] = BitOp::op(buf[vecIndex], lastNBitsMask);
    }
  }

  template<class BitOp>
  static BL_INLINE void bitArrayCombineWords(T* dst, const T* src, size_t count) noexcept {
    for (size_t i = 0; i < count; i++)
      dst[i] = BitOp::op(dst[i], src[i]);
  }

  //! Fills `count` of bits in bit-vector `buf` starting at bit-index `index`.
  static BL_INLINE void bitArrayFill(T* buf, size_t index, size_t count) noexcept {
    bitArrayOp<BitOperator::Or, BitOperator::Assign>(buf, index, count);
  }

  static BL_INLINE void bitArrayAnd(T* buf, size_t index, size_t count) noexcept {
    bitArrayOp<BitOperator::And, BitOperator::Assign>(buf, index, count);
  }

  //! Clears `count` of bits in bit-vector `buf` starting at bit-index `index`.
  static BL_INLINE void bitArrayClear(T* buf, size_t index, size_t count) noexcept {
    bitArrayOp<BitOperator::AndNot, BitOperator::AssignNot>(buf, index, count);
  }

  static BL_INLINE void bitArrayNotAnd(T* buf, size_t index, size_t count) noexcept {
    bitArrayOp<BitOperator::NotAnd, BitOperator::Assign>(buf, index, count);
  }

  template<typename IndexType>
  static BL_INLINE bool bitArrayFirstBit(const T* data, size_t count, IndexType* indexOut) noexcept {
    for (size_t i = 0; i < count; i++) {
      T bits = data[i];
      if (bits) {
        *indexOut = IndexType(countZerosFromStart(bits) + i * kNumBits);
        return true;
      }
    }

    *indexOut = IntOps::allOnes<IndexType>();
    return false;
  }

  template<typename IndexType>
  static BL_INLINE bool bitArrayLastBit(const T* data, size_t count, IndexType* indexOut) noexcept {
    size_t i = count;
    while (i) {
      T bits = data[--i];
      if (bits) {
        *indexOut = IndexType((kBitMask - countZerosFromEnd(bits)) + i * kNumBits);
        return true;
      }
    }

    *indexOut = IntOps::allOnes<IndexType>();
    return false;
  }

  //! Iterates over each bit in a number which is set to 1.
  //!
  //! Example of use:
  //!
  //! ```
  //! uint32_t bitsToIterate = 0x110F;
  //! BitIterator<uint32_t> it(bitsToIterate);
  //!
  //! while (it.hasNext()) {
  //!   uint32_t bitIndex = it.next();
  //!   printf("Bit at %u is set\n", unsigned(bitIndex));
  //! }
  //! ```
  class BitIterator {
  public:
    T _bitWord;

    BL_INLINE_NODEBUG explicit BitIterator(T bitWord = 0) noexcept
      : _bitWord(bitWord) {}

    BL_INLINE_NODEBUG BitIterator(const BitIterator& other) noexcept = default;
    BL_INLINE_NODEBUG BitIterator& operator=(const BitIterator& other) noexcept = default;

    BL_INLINE_NODEBUG void init(T bitWord) noexcept { _bitWord = bitWord; }
    BL_INLINE_NODEBUG bool hasNext() const noexcept { return _bitWord != 0; }

    BL_INLINE uint32_t next() noexcept {
      BL_ASSERT(_bitWord != 0);
      uint32_t index = countZerosFromStart(_bitWord);
      _bitWord ^= indexAsMask(index);
      return index;
    }
  };

  //! Iterates over each bit in a BitWord, but shifts each iterated index by `kBitsPerChunkShift`.
  //!
  //! This class is used for very specific needs, currently only necessary on AArch64 targets when it comes to
  //! SIMD to GP vector mask handling, essentially workarounding the missing x86's `[V]PMOVMSKB` instruction.
  template<uint32_t kBitsPerChunkShift>
  class BitChunkIterator {
  public:
    T _bitWord;

    BL_INLINE_NODEBUG BitChunkIterator(T bitWord = 0) noexcept
      : _bitWord(bitWord) {}

    BL_INLINE_NODEBUG BitChunkIterator(const BitChunkIterator& other) noexcept = default;
    BL_INLINE_NODEBUG BitChunkIterator& operator=(const BitChunkIterator& other) noexcept = default;

    BL_INLINE_NODEBUG void init(T bitWord) noexcept { _bitWord = bitWord; }
    BL_INLINE_NODEBUG bool hasNext() const noexcept { return _bitWord != 0; }

    BL_INLINE uint32_t next() noexcept {
      BL_ASSERT(_bitWord != 0);
      uint32_t index = countZerosFromStart(_bitWord);
      _bitWord ^= indexAsMask(index);
      return index >> kBitsPerChunkShift;
    }
  };

  class BitVectorIterator {
  public:
    const T* _ptr;
    size_t _idx;
    size_t _end;
    T _current;

    BL_INLINE BitVectorIterator(const T* data, size_t numBitWords, size_t start = 0) noexcept {
      init(data, numBitWords, start);
    }

    BL_INLINE void init(const T* data, size_t numBitWords, size_t start = 0) noexcept {
      const T* ptr = data + (start / kNumBits);

      size_t idx = IntOps::alignDown(start, kNumBits);
      size_t end = numBitWords * kNumBits;

      T bitWord = T(0);
      if (idx < end) {
        T firstNBitsMask = shiftToEnd(ones(), start % kNumBits);
        bitWord = *ptr++ & firstNBitsMask;
        while (!bitWord && (idx += kNumBits) < end)
          bitWord = *ptr++;
      }

      _ptr = ptr;
      _idx = idx;
      _end = end;
      _current = bitWord;
    }

    BL_INLINE_NODEBUG bool hasNext() const noexcept {
      return _current != T(0);
    }

    BL_INLINE size_t next() noexcept {
      BL_ASSERT(_current != T(0));

      uint32_t cnt = countZerosFromStart(_current);
      T bitWord = _current ^ indexAsMask(cnt);

      size_t n = _idx + cnt;
      while (!bitWord && (_idx += kNumBits) < _end)
        bitWord = *_ptr++;

      _current = bitWord;
      return n;
    }

    BL_INLINE size_t peekNext() const noexcept {
      BL_ASSERT(_current != T(0));
      return _idx + countZerosFromStart(_current);
    }
  };

  class BitVectorFlipIterator {
  public:
    BL_INLINE BitVectorFlipIterator(const T* data, size_t numBitWords, size_t start = 0, T xorMask = 0) noexcept {
      init(data, numBitWords, start, xorMask);
    }

    BL_INLINE void init(const T* data, size_t numBitWords, size_t start = 0, T xorMask = 0) noexcept {
      const T* ptr = data + (start / kNumBits);

      size_t idx = IntOps::alignDown(start, kNumBits);
      size_t end = numBitWords * kNumBits;

      T bitWord = T(0);
      if (idx < end) {
        T firstNBitsMask = shiftToEnd(ones(), start % kNumBits);
        bitWord = (*ptr++ ^ xorMask) & firstNBitsMask;
        while (!bitWord && (idx += kNumBits) < end)
          bitWord = *ptr++ ^ xorMask;
      }

      _ptr = ptr;
      _idx = idx;
      _end = end;
      _current = bitWord;
      _xorMask = xorMask;
    }

    BL_INLINE_NODEBUG T xorMask() const noexcept { return _xorMask; }
    BL_INLINE_NODEBUG bool hasNext() const noexcept { return _current != T(0); }

    BL_INLINE size_t next() noexcept {
      BL_ASSERT(_current != T(0));

      uint32_t cnt = countZerosFromStart(_current);
      T bitWord = _current ^ indexAsMask(cnt);

      size_t n = _idx + cnt;
      while (!bitWord && (_idx += kNumBits) < _end)
        bitWord = *_ptr++ ^ _xorMask;

      _current = bitWord;
      return n;
    }

    BL_INLINE size_t nextAndFlip() noexcept {
      BL_ASSERT(_current != T(0));

      uint32_t cnt = countZerosFromStart(_current);
      T bitWord = _current ^ shiftToEnd(ones(), cnt);
      _xorMask ^= ones();

      size_t n = _idx + cnt;
      while (!bitWord && (_idx += kNumBits) < _end)
        bitWord = *_ptr++ ^ _xorMask;

      _current = bitWord;
      return n;
    }

    BL_INLINE size_t peekNext() const noexcept {
      BL_ASSERT(_current != T(0));
      return _idx + countZerosFromStart(_current);
    }

    const T* _ptr;
    size_t _idx;
    size_t _end;
    T _current;
    T _xorMask;
  };
};

//! \}

using PublicBitWordOps = ParametrizedBitOps<BitOrder::kPublic, BLBitWord>;
using PrivateBitWordOps = ParametrizedBitOps<BitOrder::kPrivate, BLBitWord>;

} // {anonymous}

// TODO: REMOVE, FOR COMPATIBILITY ONLY.

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
class BitWordIterator {
public:
  BL_INLINE explicit BitWordIterator(T bitWord) noexcept
    : _bitWord(bitWord) {}

  BL_INLINE void init(T bitWord) noexcept { _bitWord = bitWord; }
  BL_INLINE bool hasNext() const noexcept { return _bitWord != 0; }

  BL_INLINE uint32_t next() noexcept {
    BL_ASSERT(_bitWord != 0);
    uint32_t index = IntOps::ctz(_bitWord);
    _bitWord ^= T(1u) << index;
    return index;
  }

  T _bitWord;
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_SUPPORT_BITOPS_P_H_INCLUDED
