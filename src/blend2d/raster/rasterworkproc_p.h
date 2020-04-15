// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_RASTER_RASTERWORKPROC_P_H
#define BLEND2D_RASTER_RASTERWORKPROC_P_H

#include "../threading/thread_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

BL_HIDDEN void blRasterWorkProc(BLRasterWorkData* workData) noexcept;

BL_HIDDEN void BL_CDECL blRasterWorkThreadEntry(BLThread* thread, void* data) noexcept;
BL_HIDDEN void BL_CDECL blRasterWorkThreadDone(BLThread* thread, void* data) noexcept;

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERWORKPROC_P_H
