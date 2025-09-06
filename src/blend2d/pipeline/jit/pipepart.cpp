// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/pipecompiler_p.h"
#include "../../pipeline/jit/pipepart_p.h"
#include "../../tables/tables_p.h"

namespace bl::Pipeline::JIT {

PipePart::PipePart(PipeCompiler* pc, PipePartType part_type) noexcept
  : pc(pc),
    cc(pc->cc),
    ct(common_table),
    _part_type(part_type) {}

void PipePart::prepare_part() noexcept {}

} // {bl::Pipeline::JIT}

#endif // !BL_BUILD_NO_JIT
