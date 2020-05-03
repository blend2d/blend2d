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

#ifndef BLEND2D_SCOPEDALLOCATOR_P_H_INCLUDED
#define BLEND2D_SCOPEDALLOCATOR_P_H_INCLUDED

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

#endif // BLEND2D_SCOPEDALLOCATOR_P_H_INCLUDED
