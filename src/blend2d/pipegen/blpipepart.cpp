// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#include "../pipegen/blpipecompiler_p.h"
#include "../pipegen/blpipepart_p.h"

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::PipePart - Construction / Destruction]
// ============================================================================

PipePart::PipePart(PipeCompiler* pc, uint32_t partType) noexcept
  : pc(pc),
    cc(pc->cc),
    _partType(partType),
    _childrenCount(0),
    _maxOptLevelSupported(kOptLevel_None),
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
