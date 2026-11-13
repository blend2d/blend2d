// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_AXL_BUILD_INTEGRATION_H_INCLUDED
#define BLEND2D_AXL_BUILD_INTEGRATION_H_INCLUDED

// This file acts as an integration that provides the following:
//
// <Result> type:
//   - Result (could be using a Result type from upper namespace if compatible)
//   - kSuccess - a successful result (usually a zero constant)
//   - make_out_of_memory() - makes an out of memory result

#include <blend2d/core/api.h>
#include <blend2d/core/span.h>

namespace bl::axl {

using Result = BLResult;

static constexpr Result kSuccess = BL_SUCCESS;

[[nodiscard]]
static constexpr Result make_out_of_memory() noexcept { return bl_make_error(BL_ERROR_OUT_OF_MEMORY); }

} // {bl::axl}

#endif // BLEND2D_AXL_BUILD_INTEGRATION_H_INCLUDED
