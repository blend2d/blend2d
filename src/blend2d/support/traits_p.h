// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_SUPPORT_TRAITS_P_H_INCLUDED
#define BLEND2D_SUPPORT_TRAITS_P_H_INCLUDED

#include "../api-internal_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace BLTraits {
namespace {

//! \name Numeric Limits
//! \{

//! Returns the minimum value of type `T`.
//!
//! \note `T` should be either integer or floating point.
template<typename T>
BL_NODISCARD
static BL_INLINE constexpr T minValue() noexcept { return std::numeric_limits<T>::lowest(); }

//! Returns the maximum value of type `T`.
//!
//! \note `T` should be either integer or floating point.
template<typename T>
BL_NODISCARD
static BL_INLINE constexpr T maxValue() noexcept { return std::numeric_limits<T>::max(); }

//! \}

} // {anonymous}
} // {BLTraits}

//! \}
//! \endcond

#endif // BLEND2D_SUPPORT_TRAITS_P_H_INCLUDED
