// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RANDOM_H_INCLUDED
#define BLEND2D_RANDOM_H_INCLUDED

#include "api.h"

//! \addtogroup blend2d_api_globals
//! \{

//! \name BLRandom - C API
//! \{

BL_BEGIN_C_DECLS

BL_API BLResult BL_CDECL blRandomReset(BLRandom* self, uint64_t seed) BL_NOEXCEPT_C;
BL_API uint32_t BL_CDECL blRandomNextUInt32(BLRandom* self) BL_NOEXCEPT_C;
BL_API uint64_t BL_CDECL blRandomNextUInt64(BLRandom* self) BL_NOEXCEPT_C;
BL_API double BL_CDECL blRandomNextDouble(BLRandom* self) BL_NOEXCEPT_C;

BL_END_C_DECLS

//! \}

//! \name BLRandom - C/C++ API
//! \{

//! Simple pseudo random number generator based on `XORSHIFT+`, which has 64-bit seed, 128 bits of state, and full
//! period `2^128 - 1`.
//!
//! Based on a paper by Sebastiano Vigna:
//!   http://vigna.di.unimi.it/ftp/papers/xorshiftplus.pdf
struct BLRandom {
  //! PRNG state.
  uint64_t data[2];

#ifdef __cplusplus
  //! \name Construction & Destruction
  //! \{

  BL_INLINE BLRandom() noexcept = default;
  BL_INLINE BLRandom(const BLRandom&) noexcept = default;

  BL_INLINE explicit BLRandom(uint64_t seed) noexcept { reset(seed); }

  //! \}

  //! \name Overloaded Operators
  //! \{

  BL_NODISCARD BL_INLINE bool operator==(const BLRandom& other) const noexcept { return  equals(other); }
  BL_NODISCARD BL_INLINE bool operator!=(const BLRandom& other) const noexcept { return !equals(other); }

  //! \}

  //! \name Common Functionality
  //! \{

  //! Resets the random number generator to the given `seed`.
  //!
  //! Always returns `BL_SUCCESS`.
  BL_INLINE BLResult reset(uint64_t seed = 0) noexcept { return blRandomReset(this, seed); }

  //! Tests whether the random number generator is equivalent to `other`.
  //!
  //! \note It would return true only when its internal state matches `other`'s internal state.
  BL_NODISCARD
  BL_INLINE bool equals(const BLRandom& other) const noexcept {
    return blEquals(this->data[0], other.data[0]) &
           blEquals(this->data[1], other.data[1]);
  }

  //! \}

  //! \name Random Numbers
  //! \{

  //! Returns the next pseudo-random `uint64_t` value and advances PRNG state.
  BL_NODISCARD
  BL_INLINE uint64_t nextUInt64() noexcept { return blRandomNextUInt64(this); }

  //! Returns the next pseudo-random `uint32_t` value and advances PRNG state.
  BL_NODISCARD
  BL_INLINE uint32_t nextUInt32() noexcept { return blRandomNextUInt32(this); }

  //! Returns the next pseudo-random `double` precision floating point in [0..1) range and advances PRNG state.
  BL_NODISCARD
  BL_INLINE double nextDouble() noexcept { return blRandomNextDouble(this); }

  //! \}
#endif
};

//! \}

//! \}

#endif // BLEND2D_RANDOM_H_INCLUDED
