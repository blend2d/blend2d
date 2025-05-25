// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_SUPPORT_ZEROALLOCATOR_P_H_INCLUDED
#define BLEND2D_SUPPORT_ZEROALLOCATOR_P_H_INCLUDED

#include "../api-internal_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

BL_HIDDEN void* blZeroAllocatorAlloc(size_t size, size_t* allocatedSize) noexcept;
BL_HIDDEN void* blZeroAllocatorResize(void* prevPtr, size_t prevSize, size_t size, size_t* allocatedSize) noexcept;
BL_HIDDEN void  blZeroAllocatorRelease(void* ptr, size_t size) noexcept;

namespace bl {

//! Memory buffer that is initially zeroed and that must be zeroed upon release.
class ZeroBuffer {
public:
  BL_NONCOPYABLE(ZeroBuffer)

  //! \name Members
  //! \{

  //! Zero allocated data.
  uint8_t* data = nullptr;
  //! Size of the buffer.
  size_t size = 0;

  //! \}

  //! \name Construction & Destruction
  //! \{

  BL_INLINE ZeroBuffer() noexcept = default;

  BL_INLINE ZeroBuffer(ZeroBuffer&& other) noexcept
    : data(other.data),
      size(other.size) {
    other.data = nullptr;
    other.size = 0;
  }

  BL_INLINE ~ZeroBuffer() noexcept {
    if (data)
      blZeroAllocatorRelease(data, size);
  }

  //! \}

  //! \name Allocation
  //! \{

  [[nodiscard]]
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

  //! \}
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_SUPPORT_ZEROALLOCATOR_P_H_INCLUDED
