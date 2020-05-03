// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef BLEND2D_ZEROALLOCATOR_P_H_INCLUDED
#define BLEND2D_ZEROALLOCATOR_P_H_INCLUDED

#include "./api-internal_p.h"

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

  //! Zero allocated data.
  uint8_t* data;
  //! Size of the buffer.
  size_t size;

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

  BL_NODISCARD
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
};

//! \}
//! \endcond

#endif // BLEND2D_ZEROALLOCATOR_P_H_INCLUDED
