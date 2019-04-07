// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_PIPEGEN_BLFIXEDPIPERUNTIME_P_H
#define BLEND2D_PIPEGEN_BLFIXEDPIPERUNTIME_P_H

#include "../blpipedefs_p.h"
#include "../blpiperuntime_p.h"
#include "../blsupport_p.h"

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

#endif // BLEND2D_PIPEGEN_BLFIXEDPIPERUNTIME_P_H
