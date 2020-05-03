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

#include "../api-build_p.h"
#if BL_TARGET_ARCH_X86 && !defined(BL_BUILD_NO_JIT)

#include "../pipegen/pipecompiler_p.h"
#include "../pipegen/pipepart_p.h"

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::PipePart - Construction / Destruction]
// ============================================================================

PipePart::PipePart(PipeCompiler* pc, uint32_t partType) noexcept
  : pc(pc),
    cc(pc->cc),
    _partType(uint8_t(partType)),
    _childrenCount(0),
    _maxSimdWidthSupported(0),
    _flags(0),
    _persistentRegs(),
    _spillableRegs(),
    _temporaryRegs(),
    _globalHook(nullptr) {

  memset(_hasLowRegs, 0, sizeof(_hasLowRegs));
  _children[0] = nullptr;
  _children[1] = nullptr;
}

// ============================================================================
// [BLPipeGen::PipePart - Prepare]
// ============================================================================

void PipePart::preparePart() noexcept {
  prepareChildren();
}

void PipePart::prepareChildren() noexcept {
  size_t count = childrenCount();
  for (size_t i = 0; i < count; i++) {
    PipePart* childPart = _children[i];
    if (!(childPart->flags() & kFlagPrepareDone))
      childPart->preparePart();
  }
}

} // {BLPipeGen}

#endif
