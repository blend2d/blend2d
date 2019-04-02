// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#ifndef BLEND2D_BLAPI_IMPL_H
#define BLEND2D_BLAPI_IMPL_H

#include "./blapi.h"

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
//! Atomically increments a reference count `x` by `n`. The old value is returned.
template<typename T>
static BL_INLINE typename std::remove_volatile<T>::type blAtomicFetchIncRef(T* x, typename std::remove_volatile<T>::type n = 1) noexcept {
  typedef typename std::remove_volatile<T>::type RawT;
  return ((std::atomic<RawT>*)x)->fetch_add(n, std::memory_order_relaxed);
}

//! \ingroup blend2d_api_impl
//!
//! Atomically decrements a reference count `x` by `n`. The old value is returned.
template<typename T>
static BL_INLINE typename std::remove_volatile<T>::type blAtomicFetchDecRef(T* x, typename std::remove_volatile<T>::type n = 1) noexcept {
  typedef typename std::remove_volatile<T>::type RawT;
  return ((std::atomic<RawT>*)x)->fetch_sub(n, std::memory_order_acq_rel);
}

//! \}

// ============================================================================
// [Impl API]
// ============================================================================

//! Blend2D supports external data in almost every container. When a container
//! is created from external data it adds a preface before its data, which
//! stores used provided `destroyFunc` and `destroyData`.
//!
//! Use `blImplDestroyExternal` to call destroy function of Impl.
struct BLExternalImplPreface {
  BLDestroyImplFunc destroyFunc;
  void* destroyData;
};

// These are additional functions that are exported and used by various object
// implementations inside Blend2D itself. Since Blend2D 'Impl' can use memory
// pools it's required that any third party code that extends Blend2D must also
// use these functions to allocate and free 'Impl'.

//! \name Impl Memory Management
//! \{

// Implemented in 'blruntime.cpp'.
BL_API_C void*    BL_CDECL blRuntimeAllocImpl(size_t implSize, uint16_t* memPoolDataOut) noexcept;
BL_API_C BLResult BL_CDECL blRuntimeFreeImpl(void* impl_, size_t implSize, uint32_t memPoolData) noexcept;

template<typename Impl>
static BL_INLINE Impl* blRuntimeAllocImplT(size_t implSize, uint16_t* memPoolDataOut) noexcept {
  return static_cast<Impl*>(blRuntimeAllocImpl(implSize, memPoolDataOut));
}

//! \}

//! \name Impl Reference Counting
//! \{

template<typename Impl>
static BL_INLINE bool blImplIsMutable(Impl* impl) noexcept { return impl->refCount == 1; }

template<typename T>
static BL_INLINE T* blImplIncRef(T* impl, size_t n = 1) noexcept {
  if (impl->refCount != 0)
    blAtomicFetchIncRef(&impl->refCount, n);
  return impl;
}

template<typename T>
static BL_INLINE bool blImplDecRefAndTest(T* impl) noexcept {
  return impl->refCount != 0 && blAtomicFetchDecRef(&impl->refCount) == 1;
}

//! \}

//! \name Impl Initialization and Destruction
//! \{

template<typename T>
static BL_INLINE void blImplInit(T* impl, uint32_t implType, uint32_t implTraits, uint16_t memPoolData) noexcept {
  impl->refCount = 1;
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

#endif // BLEND2D_BLAPI_IMPL_H
