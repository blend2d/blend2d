// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_THREADING_ATOMIC_P_H_INCLUDED
#define BLEND2D_THREADING_ATOMIC_P_H_INCLUDED

#include "../api-internal_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! \name Threading - Atomic Operations
//! \{

static BL_INLINE void blAtomicThreadFence(std::memory_order order = std::memory_order_release) noexcept {
  std::atomic_thread_fence(order);
}

template<typename T>
static BL_INLINE typename std::remove_volatile<T>::type blAtomicFetch(
  const T* p,
  std::memory_order order = std::memory_order_relaxed) noexcept {

  typedef typename BLInternal::StdInt<sizeof(T), 0>::Type RawT;
  return (typename std::remove_volatile<T>::type)((const std::atomic<RawT>*)p)->load(order);
}

template<typename T>
static BL_INLINE void blAtomicStore(
  T* p,
  typename std::remove_volatile<T>::type value,
  std::memory_order order = std::memory_order_release) noexcept {

  typedef typename BLInternal::StdInt<sizeof(T), 0>::Type RawT;
  return ((std::atomic<RawT>*)p)->store((RawT)value, order);
}

template<typename T>
static BL_INLINE bool blAtomicCompareExchange(
  T* ptr,
  typename std::remove_volatile<T>::type* expected,
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

template<typename T>
static BL_INLINE typename std::remove_volatile<T>::type blAtomicFetchAnd(
  T* x,
  typename std::remove_volatile<T>::type value,
  std::memory_order order = std::memory_order_seq_cst) noexcept {

  typedef typename std::remove_volatile<T>::type RawT;
  return ((std::atomic<RawT>*)x)->fetch_and(value, order);
}

//! \}

//! \}
//! \endcond

#endif // BLEND2D_THREADING_ATOMIC_P_H_INCLUDED
