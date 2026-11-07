// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_AXL_MEM_ALLOC_H_INCLUDED
#define BLEND2D_AXL_MEM_ALLOC_H_INCLUDED

#include <blend2d/axl/build_integration.h>

#include <stdlib.h>

namespace bl::axl {

//! \addtogroup blend2d_axl
//! \{

// Memory Allocation
// =================

template<typename T = void>
static inline T* mem_alloc(size_t size) noexcept {
  return static_cast<T*>(::malloc(size));
}

template<typename T = void>
static inline T* mem_alloc_zeroed(size_t size) noexcept {
  return static_cast<T*>(::calloc(size, 1u));
}

template<typename T = void>
static inline T* mem_realloc(T* ptr, size_t size) noexcept {
  return static_cast<T*>(::realloc(static_cast<void*>(ptr), size));
}

template<typename T>
static inline void mem_free(T* ptr) noexcept {
  return ::free(static_cast<void*>(ptr));
}

//! \}

} // {bl::axl}

#endif // BLEND2D_AXL_MEM_ALLOC_H_INCLUDED
