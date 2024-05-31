// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/pipecompiler_p.h"
#include "../../pipeline/jit/pipepart_p.h"
#include "../../tables/tables_p.h"

namespace bl {
namespace Pipeline {
namespace JIT {

PipePart::PipePart(PipeCompiler* pc, PipePartType partType) noexcept
  : pc(pc),
    cc(pc->cc),
    ct(commonTable),
    _partType(partType) {}

void PipePart::preparePart() noexcept {}

} // {JIT}
} // {Pipeline}
} // {bl}

#endif // !BL_BUILD_NO_JIT
