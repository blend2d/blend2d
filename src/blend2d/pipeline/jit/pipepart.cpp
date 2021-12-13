// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if BL_TARGET_ARCH_X86 && !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/pipecompiler_p.h"
#include "../../pipeline/jit/pipepart_p.h"

namespace BLPipeline {
namespace JIT {

PipePart::PipePart(PipeCompiler* pc, PipePartType partType) noexcept
  : pc(pc),
    cc(pc->cc),
    _partType(partType) {}

void PipePart::preparePart() noexcept {}

} // {JIT}
} // {BLPipeline}

#endif
