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

#ifndef BLEND2D_THREADING_ATOMIC_P_H_INCLUDED
#define BLEND2D_THREADING_ATOMIC_P_H_INCLUDED

#include "../api-internal_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [Atomics - Complements <blapi-impl.h>]
// ============================================================================

static BL_INLINE void blAtomicThreadFence(std::memory_order order = std::memory_order_release) noexcept {
  std::atomic_thread_fence(order);
}

template<typename T>
static BL_INLINE typename std::remove_volatile<T>::type blAtomicFetch(const T* p, std::memory_order order = std::memory_order_relaxed) noexcept {
  typedef typename BLInternal::StdInt<sizeof(T), 0>::Type RawT;
  return (typename std::remove_volatile<T>::type)((const std::atomic<RawT>*)p)->load(order);
}

template<typename T>
static BL_INLINE void blAtomicStore(T* p, typename std::remove_volatile<T>::type value, std::memory_order order = std::memory_order_release) noexcept {
  typedef typename BLInternal::StdInt<sizeof(T), 0>::Type RawT;
  return ((std::atomic<RawT>*)p)->store((RawT)value, order);
}

template<typename T>
static BL_INLINE bool blAtomicCompareExchange(T* ptr, typename std::remove_volatile<T>::type* expected,
                                                      typename std::remove_volatile<T>::type desired) noexcept {
  typedef typename std::remove_volatile<T>::type ValueType;
  return std::atomic_compare_exchange_strong(((std::atomic<ValueType>*)ptr), expected, desired);
}

template<typename T>
static BL_INLINE typename std::remove_volatile<T>::type blAtomicFetchOr(
  T* x,
  typename std::remove_volatile<T>::type value,
  std::memory_order order = std::memory_order_seq_cst) noexcept {

  typedef typename std::remove_volatile<T>::type RawT;
  return ((std::atomic<RawT>*)x)->fetch_or(value, order);
}

//! \}
//! \endcond

#endif // BLEND2D_THREADING_ATOMIC_P_H_INCLUDED
