// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_AXL_MATHCONST_P_H_INCLUDED
#define BLEND2D_AXL_MATHCONST_P_H_INCLUDED

#include <blend2d/core/api_internal_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_support
//! \{

namespace bl {
namespace Math {

//! \name Math Constants
//! \{

static constexpr double kPI            = 3.14159265358979323846;  //!< pi.
static constexpr double kPI_MUL_1p5    = 4.71238898038468985769;  //!< pi * 1.5.
static constexpr double kPI_MUL_2      = 6.28318530717958647692;  //!< pi * 2.
static constexpr double kPI_DIV_2      = 1.57079632679489661923;  //!< pi / 2.
static constexpr double kPI_DIV_3      = 1.04719755119659774615;  //!< pi / 3.
static constexpr double kPI_DIV_4      = 0.78539816339744830962;  //!< pi / 4.
static constexpr double kSQRT_0p5      = 0.70710678118654746172;  //!< sqrt(0.5).
static constexpr double kSQRT_2        = 1.41421356237309504880;  //!< sqrt(2).
static constexpr double kSQRT_3        = 1.73205080756887729353;  //!< sqrt(3).

static constexpr double kCosAcute      = -0.99999999999847689;    //!< Threshold for acute (179.9999 degrees) angles.
static constexpr double kCosObtuse     = 0.99999999999847689;     //!< Threshold for obtuse (0.0001 degrees) angles.

static constexpr double kAfter0        = 1e-40;                   //!< Safe value after 0.0 for root finding/intervals.
static constexpr double kBefore1       = 0.999999999999999889;    //!< Safe value before 1.0 for root finding/intervals.

// Threshold for curve splitting (to avoid tiny tail curves).
static constexpr double kCurveMinT     = 5e-6;
static constexpr double kCurveMaxT     = 1.0 - kCurveMinT;

static constexpr double kANGLE_EPSILON = 1e-8;

} // {Math}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_AXL_MATHCONST_P_H_INCLUDED
