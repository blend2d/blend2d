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

#ifndef BLEND2D_API_IMPL_H_INCLUDED
#define BLEND2D_API_IMPL_H_INCLUDED

#include "./api.h"
#include "./variant.h"

// This should be the only place in Blend2D that includes <atomic>.
#include <atomic>

//! \addtogroup blend2d_api_impl
//! \{

// ============================================================================
// [Atomic Operations]
// ============================================================================

//! \name Atomic Operations
//! \{

// Atomic operations are used extensively across Blend2D for reference counting
// and caching. You should always use operations defined here as they implement
// all possible cases that Blend2D deals with in a correct way (and if there is
// a bug it makes it fixable in a single place).

//! \ingroup blend2d_api_impl
//!
//! Atomically increments `n` to value `x`. The old value is returned.
template<typename T>
static BL_INLINE typename std::remove_volatile<T>::type blAtomicFetchAdd(
  T* x,
  typename std::remove_volatile<T>::type n = 1,
  std::memory_order order = std::memory_order_relaxed) noexcept {

  typedef typename std::remove_volatile<T>::type RawT;
  return ((std::atomic<RawT>*)x)->fetch_add(n, order);
}

//! \ingroup blend2d_api_impl
//!
//! Atomically decrements `n` from value `x`. The old value is returned.
template<typename T>
static BL_INLINE typename std::remove_volatile<T>::type blAtomicFetchSub(
  T* x,
  typename std::remove_volatile<T>::type n = 1,
  std::memory_order order = std::memory_order_acq_rel) noexcept {

  typedef typename std::remove_volatile<T>::type RawT;
  return ((std::atomic<RawT>*)x)->fetch_sub(n, order);
}

//! \}

// ============================================================================
// [Impl API]
// ============================================================================

//! \name Impl Memory Management
//!
//! These are additional functions that are exported and used by various object
//! implementations inside Blend2D itself. Since Blend2D 'Impl' can use memory
//! pools it's required that any third party code that extends Blend2D must also
//! use these functions to allocate and free Blend2D 'Impl' objects.
//!
//! \{

//! Blend2D supports external data in almost every container. When a container
//! is created from external data it adds a preface before its data, which
//! stores user provided `destroyFunc` and `destroyData`.
//!
//! Use `blImplDestroyExternal` to call destroy function of Impl.
struct BLExternalImplPreface {
  BLDestroyImplFunc destroyFunc;
  void* destroyData;
};

#ifdef __cplusplus
extern "C" {
#endif

// Implemented in 'blruntime.cpp'.
BL_API void*    BL_CDECL blRuntimeAllocImpl(size_t implSize, uint16_t* memPoolDataOut) noexcept;
BL_API void*    BL_CDECL blRuntimeAllocAlignedImpl(size_t implSize, size_t alignment, uint16_t* memPoolDataOut) noexcept;
BL_API BLResult BL_CDECL blRuntimeFreeImpl(void* impl_, size_t implSize, uint32_t memPoolData) noexcept;

#ifdef __cplusplus
} // {Extern:C}
#endif

template<typename Impl>
static BL_INLINE Impl* blRuntimeAllocImplT(size_t implSize, uint16_t* memPoolDataOut) noexcept {
  return static_cast<Impl*>(blRuntimeAllocImpl(implSize, memPoolDataOut));
}

template<typename Impl>
static BL_INLINE Impl* blRuntimeAllocAlignedImplT(size_t implSize, size_t alignment, uint16_t* memPoolDataOut) noexcept {
  return static_cast<Impl*>(blRuntimeAllocAlignedImpl(implSize, alignment, memPoolDataOut));
}

//! \}

//! \name Impl Reference Counting
//! \{

template<typename Impl>
static BL_INLINE bool blImplIsMutable(Impl* impl) noexcept { return impl->refCount == 1; }

template<typename T>
static BL_INLINE T* blImplIncRef(T* impl, size_t n = 1) noexcept {
  if (impl->refCount != SIZE_MAX)
    blAtomicFetchAdd(&impl->refCount, n);
  return impl;
}

template<typename T>
static BL_INLINE bool blImplDecRefAndTest(T* impl) noexcept {
  size_t base = impl->implTraits & 0x3u;
  // Zero `base` means it's a built-in none object or object that doesn't use
  // reference counting. We cannot decrease the reference count of such Impl.
  if (base == 0)
    return false;

  return blAtomicFetchSub(&impl->refCount) == base;
}

//! \}

//! \name Impl Initialization and Destruction
//! \{

static BL_INLINE uint32_t blImplTraitsFromDataAccessFlags(uint32_t dataAccessFlags) noexcept {
  return (dataAccessFlags & BL_DATA_ACCESS_RW) == BL_DATA_ACCESS_RW
    ? BL_IMPL_TRAIT_MUTABLE
    : BL_IMPL_TRAIT_IMMUTABLE;
}

template<typename T>
static BL_INLINE void blImplInit(T* impl, uint32_t implType, uint32_t implTraits, uint16_t memPoolData) noexcept {
  impl->refCount = (implTraits & 0x3u);
  impl->implType = uint8_t(implType);
  impl->implTraits = uint8_t(implTraits);
  impl->memPoolData = memPoolData;
}

template<typename T>
static BL_INLINE T* blImplInitExternal(T* impl, BLDestroyImplFunc destroyFunc, void* destroyData) noexcept {
  BLExternalImplPreface* preface = reinterpret_cast<BLExternalImplPreface*>(impl);
  preface->destroyFunc = destroyFunc;
  preface->destroyData = destroyData;

  impl = reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(impl) + sizeof(BLExternalImplPreface));
  return impl;
}

static BL_INLINE void blImplDestroyExternal(void* impl) noexcept {
  BLExternalImplPreface* preface =
    reinterpret_cast<BLExternalImplPreface*>(
      reinterpret_cast<uint8_t*>(impl) - sizeof(BLExternalImplPreface));
  preface->destroyFunc(impl, preface->destroyData);
}

template<typename T>
static BL_INLINE BLResult blImplReleaseVirt(T* impl) noexcept {
  return blImplDecRefAndTest(impl) ? impl->virt->destroy(impl) : BL_SUCCESS;
}

//! \}

//! \name Miscellaneous
//! \{

template<typename T, typename F>
static BL_INLINE void blAssignFunc(T** dst, F f) noexcept { *(void**)dst = (void*)f; }

//! \}

//! \}

#endif // BLEND2D_API_IMPL_H_INCLUDED
