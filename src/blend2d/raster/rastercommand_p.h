// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_RASTER_RASTERCOMMAND_P_H
#define BLEND2D_RASTER_RASTERCOMMAND_P_H

#include "../geometry_p.h"
#include "../image.h"
#include "../path.h"
#include "../pipedefs_p.h"
#include "../region.h"
#include "../zeroallocator_p.h"
#include "../zoneallocator_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rasterdefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

struct BLRasterFetchData;

//! Raster command type.
enum BLRasterCommandType : uint32_t {
  BL_RASTER_COMMAND_TYPE_NONE = 0,
  BL_RASTER_COMMAND_TYPE_FILL_RECTA = 1,
  BL_RASTER_COMMAND_TYPE_FILL_RECTU = 2,
  BL_RASTER_COMMAND_TYPE_FILL_ANALYTIC = 3
};

//! Raster command flags.
enum BLRasterCommandFlags : uint32_t {
  BL_RASTER_COMMAND_FLAG_FETCH_DATA = 0x01u
};

//! Raster command data.
struct BLRasterCommand {
  uint8_t type;
  uint8_t flags;
  uint16_t alpha;
  uint32_t pipeSignature;

  union {
    BLPipeFetchData::Solid solidData;
    BLRasterFetchData* fetchData;
  };

  union {
    BLRectI rect;
    BLEdgeStorage<int>* edgeStorage;
  };
};

class BLRasterCommandQueue {
public:
  BL_NONCOPYABLE(BLRasterCommandQueue)

  uint8_t* _cmdBuf;
  uint8_t* _cmdPtr;
  uint8_t* _cmdEnd;
  size_t _queueCapacity;

  BL_INLINE BLRasterCommandQueue() noexcept
    : _cmdBuf(nullptr),
      _cmdPtr(nullptr),
      _cmdEnd(nullptr),
      _queueCapacity(0) {}

  BL_INLINE ~BLRasterCommandQueue() noexcept {
    free(_cmdBuf);
  }

  BL_INLINE void swap(BLRasterCommandQueue& other) noexcept {
    std::swap(_cmdBuf, other._cmdBuf);
    std::swap(_cmdPtr, other._cmdPtr);
    std::swap(_cmdEnd, other._cmdEnd);
    std::swap(_queueCapacity, other._queueCapacity);
  }

  BL_INLINE bool empty() const noexcept {
    return _cmdPtr == _cmdBuf;
  }

  BL_INLINE bool full() const noexcept {
    return _cmdPtr == _cmdEnd;
  }

  BL_INLINE size_t queueSize() const noexcept {
    return (size_t)(_cmdPtr - _cmdBuf) / sizeof(BLRasterCommand);
  }

  BL_INLINE size_t queueCapacity() const noexcept {
    return _queueCapacity;
  }

  BL_INLINE BLResult clear() noexcept {
    _cmdPtr = _cmdBuf;
    return BL_SUCCESS;
  }

  BL_INLINE BLResult reset() noexcept {
    free(_cmdBuf);
    _cmdBuf = nullptr;
    _cmdPtr = nullptr;
    _cmdEnd = nullptr;
    _queueCapacity = 0;
    return BL_SUCCESS;
  }

  BLResult reset(size_t capacity) noexcept {
    BL_ASSERT(capacity > 0);
    if (capacity != _queueCapacity) {
      uint8_t* newPtr = nullptr;

      if (capacity == 0) {
        free(_cmdBuf);
      }
      else {
        newPtr = static_cast<uint8_t*>(realloc(_cmdBuf, capacity * sizeof(BLRasterCommand)));
        if (!newPtr)
          return blTraceError(BL_ERROR_OUT_OF_MEMORY);
      }

      _cmdBuf = newPtr;
      _cmdEnd = _cmdBuf + capacity * sizeof(BLRasterCommand);
      _queueCapacity = capacity;
    }

    _cmdPtr = _cmdBuf;
    return BL_SUCCESS;
  }
};

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERCOMMAND_P_H
