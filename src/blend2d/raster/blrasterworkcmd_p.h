// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_RASTER_BLRASTERWORKCMD_P_H
#define BLEND2D_RASTER_BLRASTERWORKCMD_P_H

#include "../blgeometry_p.h"
#include "../blimage.h"
#include "../blpath.h"
#include "../blregion.h"
#include "../blzeroallocator_p.h"
#include "../blzoneallocator_p.h"
#include "../raster/bledgebuilder_p.h"
#include "../raster/blrasterdefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

//! Raster worker command information.
//!
//! Commands are only used by asynchronous rendering context implementation.
enum BLRasterWorkCmdInfo : uint32_t {
  BL_RASTER_WORK_CMD_TYPE_FILL_RECTA_I16 = 0x00u,
  BL_RASTER_WORK_CMD_TYPE_FILL_RECTA_I32 = 0x01u,
  BL_RASTER_WORK_CMD_TYPE_FILL_RECTU     = 0x02u,
  BL_RASTER_WORK_CMD_TYPE_FILL_ANALYTIC  = 0x03u,
  BL_RASTER_WORK_CMD_TYPE_MASK           = 0x0Fu,

  BL_RASTER_WORK_CMD_FLAG_FETCH_DATA     = 0x10u,
  BL_RASTER_WORK_CMD_FLAG_ALPHA_DATA     = 0x20u,
  BL_RASTER_WORK_CMD_FLAG_MASK           = 0xF0u
};

template<typename T>
struct BLRasterWorkCmdFillRect {
  T x0, y0, x1, y1;
};

struct BLRasterWorkCmdFillAnalytic {
  // TODO:
};

struct BLRasterWorkQueue {
  BLZoneAllocator zone;

  uint8_t* cmdBuf;
  uint8_t* cmdPtr;
  uint8_t* cmdEnd;
  uint8_t* workPtr;

  BL_INLINE BLRasterWorkQueue() noexcept
    : zone(65536 - BLZoneAllocator::kBlockOverhead),
      cmdBuf(nullptr),
      cmdPtr(nullptr),
      cmdEnd(nullptr),
      workPtr(nullptr) {}

  BL_INLINE BLResult reset(size_t capacity) noexcept {
    zone.clear();

    cmdBuf = zone.allocT<uint8_t>(capacity);
    cmdPtr = cmdBuf;
    cmdEnd = cmdBuf + capacity;
    workPtr = zone.ptr<uint8_t>();

    if (cmdBuf)
      return BL_SUCCESS;
    else
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }

  BL_INLINE size_t index() const noexcept { return (size_t)(cmdPtr - cmdBuf); }
  BL_INLINE size_t capacity() const noexcept { return (size_t)(cmdEnd - cmdBuf); }
};

//! \}
//! \endcond

#endif // BLEND2D_RASTER_BLRASTERWORKCMD_P_H
