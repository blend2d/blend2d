// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_PIPEGEN_FIXEDPIPERUNTIME_P_H
#define BLEND2D_PIPEGEN_FIXEDPIPERUNTIME_P_H

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

#endif // BLEND2D_PIPEGEN_FIXEDPIPERUNTIME_P_H
