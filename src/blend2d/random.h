// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef BLEND2D_RANDOM_H_INCLUDED
#define BLEND2D_RANDOM_H_INCLUDED

#include "./api.h"

//! \addtogroup blend2d_api_globals
//! \{

// ============================================================================
// [BLRandom]
// ============================================================================

//! Simple pseudo random number generator.
//!
//! The current implementation uses a PRNG called `XORSHIFT+`, which has 64-bit
//! seed, 128 bits of state, and full period `2^128 - 1`.
//!
//! Based on a paper by Sebastiano Vigna:
//!   http://vigna.di.unimi.it/ftp/papers/xorshiftplus.pdf
struct BLRandom {
  uint64_t data[2];

  // --------------------------------------------------------------------------
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
  //! Random number generator would only be equivalent to `other` if it was
  //! initialized from the same seed and has the same internal state.
  BL_NODISCARD
  BL_INLINE bool equals(const BLRandom& other) const noexcept {
    return blEquals(this->data[0], other.data[0]) &
           blEquals(this->data[1], other.data[1]);
  }

  //! \}

  //! \name Random Numbers
  //! \{

  //! Returns the next pseudo-random `uint64_t` value and advances its state.
  BL_NODISCARD
  BL_INLINE uint64_t nextUInt64() noexcept { return blRandomNextUInt64(this); }

  //! Returns the next pseudo-random `uint32_t` value and advances its state.
  BL_NODISCARD
  BL_INLINE uint32_t nextUInt32() noexcept { return blRandomNextUInt32(this); }

  //! Returns the next pseudo-random `double` precision floating point in [0..1)
  //! range and advances its state.
  BL_NODISCARD
  BL_INLINE double nextDouble() noexcept { return blRandomNextDouble(this); }

  //! \}
  #endif
  // --------------------------------------------------------------------------
};

//! \}

#endif // BLEND2D_RANDOM_H_INCLUDED
