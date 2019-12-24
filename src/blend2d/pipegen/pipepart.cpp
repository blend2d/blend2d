// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

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
