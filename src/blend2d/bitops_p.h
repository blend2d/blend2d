// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_BITOPS_P_H
#define BLEND2D_BITOPS_P_H

#include "./support_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [Constants]
// ============================================================================

enum BLBitOrder : uint32_t {
  BL_BIT_ORDER_LSB = 0,
  BL_BIT_ORDER_MSB = 1,

  //! Bit ordering used by public API.
  BL_BIT_ORDER_PUBLIC = BL_BIT_ORDER_MSB,
  //! Bit ordering that is fastest on the given architecture.
  BL_BIT_ORDER_PRIVATE = BL_TARGET_ARCH_X86 ? BL_BIT_ORDER_LSB : BL_BIT_ORDER_MSB
};

// ============================================================================
// [BLBitOperator]
// ============================================================================

namespace {
namespace BLBitOperator {

struct Assign    { template<typename T> static BL_INLINE T op(T a, T b) noexcept { BL_UNUSED(a); return  b; } };
struct AssignNot { template<typename T> static BL_INLINE T op(T a, T b) noexcept { BL_UNUSED(a); return ~b; } };
struct And       { template<typename T> static BL_INLINE T op(T a, T b) noexcept { return  a &  b; } };
struct AndNot    { template<typename T> static BL_INLINE T op(T a, T b) noexcept { return  a & ~b; } };
struct NotAnd    { template<typename T> static BL_INLINE T op(T a, T b) noexcept { return ~a &  b; } };
struct Or        { template<typename T> static BL_INLINE T op(T a, T b) noexcept { return  a |  b; } };
struct Xor       { template<typename T> static BL_INLINE T op(T a, T b) noexcept { return  a ^  b; } };

} // {BLBitOperator}
} // {anonymous}

// ============================================================================
// [BLParametrizedBitOps]
// ============================================================================

namespace {

//! Parametrized bit operations.
//!
//! This class acts as a namespace and allows to parametrize how bits are stored
//! in a BitWord. The reason for parametrization is architecture constraints.
//! X86 architecture prefers LSB ordering, because of the performance of BSF and
//! TZCNT instructions. Since TZCNT instruction is BSF with REP prefix compilers
//! can savely emit TZCNT instead of BSF, but it's not possible to emit LZCNT
//! instead of BSR as LZCNT returns a different result (count of zeros instead
//! of first zero index).
//!
//! ARM and other architectures only implement LZCNT (count leading zeros) and
//! counting trailing zeros means emitting more instructions to workaround the
//! missing instruction.
template<uint32_t BitOrder, typename T>
struct BLParametrizedBitOps {
  typedef typename std::make_unsigned<T>::type U;

  enum : uint32_t {
    kBitOrder = BitOrder,
    kReverseBitOrder = BitOrder ^ 1u,
    kIsLSB = (kBitOrder == BL_BIT_ORDER_LSB),
    kIsMSB = (kBitOrder == BL_BIT_ORDER_MSB),

    kNumBits = blBitSizeOf<T>()
  };

  static BL_INLINE constexpr T zero() noexcept { return T(0); }
  static BL_INLINE constexpr T ones() noexcept { return blBitOnes<T>(); }

  template<typename Index>
  static BL_INLINE constexpr bool hasBit(const T& x, const Index& index) noexcept {
    return kIsLSB ? bool((x >> index) & 0x1) : bool((x >> (index ^ Index(kNumBits - 1))) & 0x1);
  }

  template<typename N>
  static BL_INLINE constexpr T indexAsMask(const N& n) noexcept {
    return kIsLSB ? blBitShl(T(1), n) : blBitShr(blNonZeroMsbMask<T>(), n);
  }

  template<typename N>
  static BL_INLINE constexpr T nonZeroBitMask(const N& n = 1) noexcept {
    return kIsLSB ? blNonZeroLsbMask<T>(n) : blNonZeroMsbMask<T>(n);
  }

  template<typename Count>
  static BL_INLINE constexpr T shiftForward(const T& x, const Count& y) noexcept {
    return kIsLSB ? blBitShl(x, y) : blBitShr(x, y);
  }

  template<typename Count>
  static BL_INLINE constexpr T shiftBackward(const T& x, const Count& y) noexcept {
    return kIsLSB ? blBitShr(x, y) : blBitShl(x, y);
  }

  static BL_INLINE uint32_t countZerosForward(const T& x) noexcept {
    return kIsLSB ? blBitCtz(x) : blBitClz(x);
  }

  static BL_INLINE uint32_t countZerosBackward(const T& x) noexcept {
    return kIsLSB ? blBitClz(x) : blBitCtz(x);
  }

  static BL_INLINE int compare(const T& x, const T& y) noexcept {
    T xv = kIsLSB ? blBitSwap(x) : x;
    T yv = kIsLSB ? blBitSwap(y) : y;

    return int(xv > yv) - int(xv < yv);
  }

  static BL_INLINE void bitArraySetBit(T* buf, size_t index) noexcept {
    size_t vecIndex = index / kNumBits;
    size_t bitIndex = index % kNumBits;

    buf[vecIndex] |= indexAsMask(bitIndex);
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
    T firstNBitsMask = shiftForward(nonZeroBitMask(firstNBits), bitIndex);

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
      T lastNBitsMask = nonZeroBitMask(count);
      buf[vecIndex] = BitOp::op(buf[vecIndex], lastNBitsMask);
    }
  }

  //! Fill `count` of bits in bit-vector `buf` starting at bit-index `index`.
  static BL_INLINE void bitArrayFill(T* buf, size_t index, size_t count) noexcept {
    bitArrayOp<BLBitOperator::Or, BLBitOperator::Assign>(buf, index, count);
  }

  //! Clear `count` of bits in bit-vector `buf` starting at bit-index `index`.
  static BL_INLINE void bitArrayClear(T* buf, size_t index, size_t count) noexcept {
    bitArrayOp<BLBitOperator::AndNot, BLBitOperator::AssignNot>(buf, index, count);
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

    BL_INLINE explicit BitIterator(T bitWord = 0) noexcept
      : _bitWord(bitWord) {}

    BL_INLINE void init(T bitWord) noexcept { _bitWord = bitWord; }
    BL_INLINE bool hasNext() const noexcept { return _bitWord != 0; }

    BL_INLINE uint32_t next() noexcept {
      BL_ASSERT(_bitWord != 0);
      uint32_t index = countZerosForward(_bitWord);
      _bitWord ^= indexAsMask(index);
      return index;
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

      size_t idx = blAlignDown(start, kNumBits);
      size_t end = numBitWords * kNumBits;

      T bitWord = T(0);
      if (idx < end) {
        T firstNBitsMask = shiftForward(ones(), start % kNumBits);
        bitWord = *ptr++ & firstNBitsMask;
        while (!bitWord && (idx += kNumBits) < end)
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
      BL_ASSERT(_current != T(0));

      uint32_t cnt = countZerosForward(_current);
      T bitWord = _current ^ indexAsMask(cnt);

      size_t n = _idx + cnt;
      while (!bitWord && (_idx += kNumBits) < _end)
        bitWord = *_ptr++;

      _current = bitWord;
      return n;
    }

    BL_INLINE size_t peekNext() const noexcept {
      BL_ASSERT(_current != T(0));
      return _idx + countZerosForward(_current);
    }
  };

  class BitVectorFlipIterator {
  public:
    BL_INLINE BitVectorFlipIterator(const T* data, size_t numBitWords, size_t start = 0, T xorMask = 0) noexcept {
      init(data, numBitWords, start, xorMask);
    }

    BL_INLINE void init(const T* data, size_t numBitWords, size_t start = 0, T xorMask = 0) noexcept {
      const T* ptr = data + (start / kNumBits);

      size_t idx = blAlignDown(start, kNumBits);
      size_t end = numBitWords * kNumBits;

      T bitWord = T(0);
      if (idx < end) {
        T firstNBitsMask = shiftForward(ones(), start % kNumBits);
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

    BL_INLINE bool hasNext() const noexcept {
      return _current != T(0);
    }

    BL_INLINE size_t next() noexcept {
      BL_ASSERT(_current != T(0));

      uint32_t cnt = countZerosForward(_current);
      T bitWord = _current ^ indexAsMask(cnt);

      size_t n = _idx + cnt;
      while (!bitWord && (_idx += kNumBits) < _end)
        bitWord = *_ptr++ ^ _xorMask;

      _current = bitWord;
      return n;
    }

    BL_INLINE size_t nextAndFlip() noexcept {
      BL_ASSERT(_current != T(0));

      uint32_t cnt = countZerosForward(_current);
      T bitWord = _current ^ shiftForward(ones(), cnt);
      _xorMask ^= ones();

      size_t n = _idx + cnt;
      while (!bitWord && (_idx += kNumBits) < _end)
        bitWord = *_ptr++ ^ _xorMask;

      _current = bitWord;
      return n;
    }

    BL_INLINE size_t peekNext() const noexcept {
      BL_ASSERT(_current != T(0));
      return _idx + countZerosForward(_current);
    }

    const T* _ptr;
    size_t _idx;
    size_t _end;
    T _current;
    T _xorMask;
  };
};

} // {anonymous}

template<typename T>
using BLLSBBitOps = BLParametrizedBitOps<BL_BIT_ORDER_LSB, T>;

template<typename T>
using BLMSBBitOps = BLParametrizedBitOps<BL_BIT_ORDER_MSB, T>;

template<typename T>
using BLPublicBitOps = BLParametrizedBitOps<BL_BIT_ORDER_PUBLIC, T>;

template<typename T>
using BLPrivateBitOps = BLParametrizedBitOps<BL_BIT_ORDER_PRIVATE, T>;

// ============================================================================
// [BLPopCountContext]
// ============================================================================

namespace {

// Simple PopCount context designed to take advantage of HW PopCount support.
template<typename T>
class BLPopCountSimpleContext {
  uint32_t _counter;

  BL_INLINE BLPopCountSimpleContext() noexcept
    : _counter(0) {}

  BL_INLINE void reset() noexcept {
    _counter = 0;
  }

  BL_INLINE uint32_t get() const noexcept {
    return _counter;
  }

  BL_INLINE void addPopulation(uint32_t v) noexcept {
    _counter += v;
  }

  BL_INLINE void addItem(const T& x) noexcept {
    _counter += blPopCount(x);
  }

  BL_INLINE void addArray(const T* data, size_t n) noexcept {
    while (n) {
      _counter += blPopCount(data[0]);
      data++;
      n--;
    }
  }
};

// Harley-Seal PopCount from Hacker's Delight, Second Edition.
//
// This is one of the best implementation if the hardware doesn't provide POPCNT
// instruction. For our purposes this is a good deal as the minimum size of each
// page in `BLBitSet` is 8 to 16 BitWords depending on target architecture.
template<typename T>
class BLPopCountHarleySealContext {
public:
  uint32_t _counter;
  T _ones;
  T _twos;
  T _fours;

  BL_INLINE BLPopCountHarleySealContext() noexcept
    : _counter(0),
      _ones(0),
      _twos(0),
      _fours(0) {}

  BL_INLINE void reset() noexcept {
    _counter = 0;
    _ones = 0;
    _twos = 0;
    _fours = 0;
  }

  BL_INLINE uint32_t get() const noexcept {
    return _counter + 4 * blPopCount(_fours) + 2 * blPopCount(_twos) + blPopCount(_ones);
  }

  BL_INLINE void addPopulation(uint32_t v) noexcept {
    _counter += v;
  }

  BL_INLINE void addItem(const T& x) noexcept {
    _counter += blPopCount(x);
  }

  BL_INLINE void addArray(const T* data, size_t n) noexcept {
    uint32_t eightsCount = 0;
    while (n >= 8) {
      T twosA, twosB;
      T foursA, foursB;
      T eights;

      CSA(twosA, _ones, _ones, data[0], data[1]);
      CSA(twosB, _ones, _ones, data[2], data[3]);
      CSA(foursA, _twos, _twos, twosA, twosB);
      CSA(twosA, _ones, _ones, data[4], data[5]);
      CSA(twosB, _ones, _ones, data[6], data[7]);
      CSA(foursB, _twos, _twos, twosA, twosB);
      CSA(eights, _fours, _fours, foursA, foursB);

      eightsCount += blPopCount(eights);
      data += 8;
      n -= 8;
    }

    _counter += 8 * eightsCount;
    while (n) {
      _counter += blPopCount(data[0]);
      data++;
      n--;
    }
  }

  static BL_INLINE void CSA(T& h, T& l, T a, T b, T c) noexcept {
    T u = a ^ b;
    h = (a & b) | (u & c);
    l = u ^ c;
  }
};

#if defined(BL_TARGET_OPT_POPCNT)
template<typename T>
using BLPopCountContext = BLPopCountSimpleContext<T>;
#else
template<typename T>
using BLPopCountContext = BLPopCountHarleySealContext<T>;
#endif

} // {anonymous}

// ============================================================================
// [BLBitWordIterator]
// ============================================================================

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

//! \}
//! \endcond

#endif // BLEND2D_BITOPS_P_H
