// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_UTILS_POPCNT_P_H_INCLUDED
#define BLEND2D_UTILS_POPCNT_P_H_INCLUDED

#include <blend2d/axl/commons.h>

//! \cond INTERNAL
//! \addtogroup blend2d_support
//! \{

namespace bl::MemOps {
namespace {

//! \name Memory Ops
//! \{

template<class CombineOp, typename T>
static BL_INLINE void combine(T* dst, const T* src, size_t count) noexcept {
  for (size_t i = 0; i < count; i++)
    dst[i] = CombineOp::op(dst[i], src[i]);
}

template<class CombineOp, typename T>
static BL_INLINE void combine_small(T* dst, const T* src, size_t count) noexcept {
  BL_NOUNROLL
  for (size_t i = 0; i < count; i++)
    dst[i] = CombineOp::op(dst[i], src[i]);
}

//! \}

//! \name Memory Test
//! \{

template<typename T>
static BL_INLINE bool test_small_t(const T* p, size_t count, const T& value) noexcept {
  BL_NOUNROLL
  for (size_t i = 0; i < count; i++)
    if (p[i] != value)
      return false;
  return true;
}

//! \}

} // {anonymous}
} // {bl::MemOps}

namespace bl::Utils {

using Internal::IntByType;
using Internal::UIntByType;

namespace {

//! \name Bit Scanning
//! \{

// Simple PopCount context designed to take advantage of HW PopCount support.
template<typename T>
class PopCounterSimple {
public:
  uint32_t _counter = 0;

  BL_INLINE_NODEBUG void reset() noexcept { _counter = 0; }
  BL_INLINE_NODEBUG uint32_t get() const noexcept { return _counter; }
  BL_INLINE_NODEBUG void add_population(uint32_t v) noexcept { _counter += v; }
  BL_INLINE void add_item(const T& x) noexcept { _counter += axl::popcnt(x); }

  BL_INLINE void add_array(const T* data, size_t n) noexcept {
    while (n) {
      _counter += axl::popcnt(data[0]);
      data++;
      n--;
    }
  }
};

// Harley-Seal PopCount from Hacker's Delight, Second Edition.
//
// This is one of the best implementation if the hardware doesn't provide POPCNT instruction.
template<typename T>
class PopCounterHarleySeal {
public:
  uint32_t _counter = 0;
  T _ones = 0;
  T _twos = 0;
  T _fours = 0;

  BL_INLINE void reset() noexcept {
    _counter = 0;
    _ones = 0;
    _twos = 0;
    _fours = 0;
  }

  BL_INLINE uint32_t get() const noexcept {
    return _counter + 4 * axl::popcnt(_fours) + 2 * axl::popcnt(_twos) + axl::popcnt(_ones);
  }

  BL_INLINE void add_population(uint32_t v) noexcept {
    _counter += v;
  }

  BL_INLINE void add_item(const T& x) noexcept {
    _counter += axl::popcnt(x);
  }

  BL_INLINE void add_array(const T* data, size_t n) noexcept {
    uint32_t eights_count = 0;
    while (n >= 8) {
      T twos_a, twos_b;
      T fours_a, fours_b;
      T eights;

      axl::csa(twos_a, _ones, _ones, data[0], data[1]);
      axl::csa(twos_b, _ones, _ones, data[2], data[3]);
      axl::csa(fours_a, _twos, _twos, twos_a, twos_b);
      axl::csa(twos_a, _ones, _ones, data[4], data[5]);
      axl::csa(twos_b, _ones, _ones, data[6], data[7]);
      axl::csa(fours_b, _twos, _twos, twos_a, twos_b);
      axl::csa(eights, _fours, _fours, fours_a, fours_b);

      eights_count += axl::popcnt(eights);
      data += 8;
      n -= 8;
    }

    _counter += 8 * eights_count;
    while (n) {
      _counter += axl::popcnt(data[0]);
      data++;
      n--;
    }
  }
};

#if defined(BL_TARGET_OPT_POPCNT)
template<typename T>
using PopCounter = PopCounterSimple<T>;
#else
template<typename T>
using PopCounter = PopCounterHarleySeal<T>;
#endif

//! \}

} // {anonymous}
} // {bl::Utils}

namespace bl::IntOps { using namespace bl::axl; }

//! \}
//! \endcond

#endif // BLEND2D_UTILS_POPCNT_P_H_INCLUDED
