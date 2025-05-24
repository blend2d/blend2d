// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#if defined(BL_TARGET_OPT_SSE2)

#include "../codec/pngopssimdimpl_p.h"

namespace bl::Png::Ops {

void initFuncTable_SSE2(FunctionTable& ft) noexcept {
  initSimdFunctions(ft);
}

} // {bl::Png::Ops}

#endif // BL_TARGET_OPT_SSE2
