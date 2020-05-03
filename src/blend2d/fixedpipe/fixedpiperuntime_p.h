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

#ifndef BLEND2D_PIPEGEN_FIXEDPIPERUNTIME_P_H_INCLUDED
#define BLEND2D_PIPEGEN_FIXEDPIPERUNTIME_P_H_INCLUDED

#include "../pipedefs_p.h"
#include "../piperuntime_p.h"
#include "../support_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_fixedpipe
//! \{

// ============================================================================
// [BLFixedPipeRuntime]
// ============================================================================

class BLFixedPipeRuntime : public BLPipeRuntime {
public:
  BLFixedPipeRuntime() noexcept;
  ~BLFixedPipeRuntime() noexcept;

  static BLWrap<BLFixedPipeRuntime> _global;
};

//! \}
//! \endcond

#endif // BLEND2D_PIPEGEN_FIXEDPIPERUNTIME_P_H_INCLUDED
