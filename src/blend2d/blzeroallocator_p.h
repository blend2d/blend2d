// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLZEROALLOCATOR_P_H
#define BLEND2D_BLZEROALLOCATOR_P_H

#include "./blapi-internal_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLZeroMem]
// ============================================================================

BL_HIDDEN void* blZeroAllocatorAlloc(size_t size, size_t* allocatedSize) noexcept;
BL_HIDDEN void* blZeroAllocatorResize(void* prevPtr, size_t prevSize, size_t size, size_t* allocatedSize) noexcept;
BL_HIDDEN void  blZeroAllocatorRelease(void* ptr, size_t size) noexcept;

// ============================================================================
// [BLZeroBuffer]
// ============================================================================

//! Memory buffer that is initially zeroed and that must be zeroed upon release.
class BLZeroBuffer {
public:
  BL_NONCOPYABLE(BLZeroBuffer)

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  BL_INLINE BLZeroBuffer() noexcept
    : data(nullptr),
      size(0) {}

  BL_INLINE BLZeroBuffer(BLZeroBuffer&& other) noexcept
    : data(other.data),
      size(other.size) {
    other.data = nullptr;
    other.size = 0;
  }

  BL_INLINE ~BLZeroBuffer() noexcept {
    if (data)
      blZeroAllocatorRelease(data, size);
  }

  // --------------------------------------------------------------------------
  // [Allocation]
  // --------------------------------------------------------------------------

  BL_INLINE BLResult ensure(size_t minimumSize) noexcept {
    if (minimumSize <= size)
      return BL_SUCCESS;

    data = static_cast<uint8_t*>(blZeroAllocatorResize(data, size, minimumSize, &size));
    return data ? BL_SUCCESS : blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }

  BL_INLINE void release() noexcept {
    if (data) {
      blZeroAllocatorRelease(data, size);
      data = nullptr;
      size = 0;
    }
  }

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Zero allocated data.
  uint8_t* data;
  //! Size of the buffer.
  size_t size;
};

//! \}
//! \endcond

#endif // BLEND2D_BLZEROALLOCATOR_P_H
