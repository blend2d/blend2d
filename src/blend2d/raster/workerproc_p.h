// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_WORKERPROC_P_H_INCLUDED
#define BLEND2D_RASTER_WORKERPROC_P_H_INCLUDED

#include "../threading/thread_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace bl {
namespace RasterEngine {
namespace WorkerProc {

BL_HIDDEN void processWorkData(WorkData* workData, RenderBatch* batch) noexcept;
BL_HIDDEN void BL_CDECL workerThreadEntry(BLThread* thread, void* data) noexcept;

} // {WorkerProc}
} // {RasterEngine}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_WORKERPROC_P_H_INCLUDED
