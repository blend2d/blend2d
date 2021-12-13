// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_API_IMPL_H_INCLUDED
#define BLEND2D_API_IMPL_H_INCLUDED

#include "api.h"
#include "object.h"

#include <atomic>

//! \addtogroup blend2d_api_impl
//! \{

//! \name Atomic Operations
//! \{

// Atomic operations are used extensively across Blend2D for reference counting and caching. You should always use
// operations defined here as they implement all possible cases that Blend2D deals with in a correct way (and if
// there is a bug it makes it fixable in a single place).

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

//! \}

#endif // BLEND2D_API_IMPL_H_INCLUDED
