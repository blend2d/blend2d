// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_SCOPEDALLOCATOR_P_H
#define BLEND2D_SCOPEDALLOCATOR_P_H

#include "./api-internal_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLScopedAllocator]
// ============================================================================

//! A simple allocator that can be used to remember allocated memory so it can
//! be then freed in one go. Typically used in areas where some heap allocation
//! is required and at the end of the work it will all be freed.
class BLScopedAllocator {
public:
  BL_NONCOPYABLE(BLScopedAllocator)
  struct Link { Link* next; };

  Link* links;
  uint8_t* poolPtr;
  uint8_t* poolMem;
  uint8_t* poolEnd;

  BL_INLINE BLScopedAllocator() noexcept
    : links(nullptr),
      poolPtr(nullptr),
      poolMem(nullptr),
      poolEnd(nullptr) {}

  BL_INLINE BLScopedAllocator(void* poolMem, size_t poolSize) noexcept
    : links(nullptr),
      poolPtr(static_cast<uint8_t*>(poolMem)),
      poolMem(static_cast<uint8_t*>(poolMem)),
      poolEnd(static_cast<uint8_t*>(poolMem) + poolSize) {}

  BL_INLINE ~BLScopedAllocator() noexcept { reset(); }

  void* alloc(size_t size, size_t alignment = 1) noexcept;
  void reset() noexcept;
};

//! \}
//! \endcond

#endif // BLEND2D_SCOPEDALLOCATOR_P_H
