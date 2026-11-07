// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_THREADING_ATOMIC_P_H_INCLUDED
#define BLEND2D_THREADING_ATOMIC_P_H_INCLUDED

#include <blend2d/core/api_internal_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! \name Threading - Atomic Operations
//! \{

namespace bl::Atomic {
namespace {

BL_INLINE_NODEBUG void thread_fence(std::memory_order order = std::memory_order::seq_cst) noexcept { std::atomic_thread_fence(order); }

template<typename T> [[nodiscard]] BL_INLINE_NODEBUG T fetch(const T& rv, std::memory_order order) noexcept { return std::atomic_ref<T>(const_cast<T&>(rv)).load(order); }
template<typename T> [[nodiscard]] BL_INLINE_NODEBUG T fetch_relaxed(const T& rv) noexcept { return fetch(rv, std::memory_order::relaxed); }
template<typename T> [[nodiscard]] BL_INLINE_NODEBUG T fetch_strong(const T& rv) noexcept { return fetch(rv, std::memory_order::acquire); }

template<typename T> BL_INLINE_NODEBUG void store(T& rv, T x, std::memory_order order) noexcept { return std::atomic_ref<T>(rv).store(x, order); }
template<typename T> BL_INLINE_NODEBUG void store_relaxed(T& rv, T x) noexcept { return store(rv, x, std::memory_order::relaxed); }
template<typename T> BL_INLINE_NODEBUG void store_strong(T& rv, T x) noexcept { return store(rv, x, std::memory_order::release); }

template<typename T> BL_INLINE_NODEBUG T fetch_add(T& rv, T x, std::memory_order order) noexcept { return std::atomic_ref<T>(rv).fetch_add(x, order); }
template<typename T> BL_INLINE_NODEBUG T fetch_sub(T& rv, T x, std::memory_order order) noexcept { return std::atomic_ref<T>(rv).fetch_sub(x, order); }
template<typename T> BL_INLINE_NODEBUG T fetch_and(T& rv, T x, std::memory_order order) noexcept { return std::atomic_ref<T>(rv).fetch_and(x, order); }
template<typename T> BL_INLINE_NODEBUG T fetch_or(T& rv, T x, std::memory_order order) noexcept { return std::atomic_ref<T>(rv).fetch_or(x, order); }

template<typename T> BL_INLINE_NODEBUG T fetch_add_relaxed(T& rv, T x = T(1)) noexcept { return fetch_add(rv, x, std::memory_order::relaxed); }
template<typename T> BL_INLINE_NODEBUG T fetch_sub_relaxed(T& rv, T x = T(1)) noexcept { return fetch_sub(rv, x, std::memory_order::relaxed); }
template<typename T> BL_INLINE_NODEBUG T fetch_and_relaxed(T& rv, T x) noexcept { return fetch_and(rv, x, std::memory_order::relaxed); }
template<typename T> BL_INLINE_NODEBUG T fetch_or_relaxed(T& rv, T x) noexcept { return fetch_or(rv, x, std::memory_order::relaxed); }

template<typename T> BL_INLINE_NODEBUG T fetch_add_acq_rel(T& rv, T x = T(1)) noexcept { return fetch_add(rv, x, std::memory_order::acq_rel); }
template<typename T> BL_INLINE_NODEBUG T fetch_sub_acq_rel(T& rv, T x = T(1)) noexcept { return fetch_sub(rv, x, std::memory_order::acq_rel); }
template<typename T> BL_INLINE_NODEBUG T fetch_and_acq_rel(T& rv, T x) noexcept { return fetch_and(rv, x, std::memory_order::acq_rel); }
template<typename T> BL_INLINE_NODEBUG T fetch_or_acq_rel(T& rv, T x) noexcept { return fetch_or(rv, x, std::memory_order::acq_rel); }

template<typename T> BL_INLINE_NODEBUG T fetch_add_seq_cst(T& rv, T x) noexcept { return fetch_add(rv, x, std::memory_order::seq_cst); }
template<typename T> BL_INLINE_NODEBUG T fetch_sub_seq_cst(T& rv, T x) noexcept { return fetch_sub(rv, x, std::memory_order::seq_cst); }
template<typename T> BL_INLINE_NODEBUG T fetch_and_seq_cst(T& rv, T x) noexcept { return fetch_and(rv, x, std::memory_order::seq_cst); }
template<typename T> BL_INLINE_NODEBUG T fetch_or_seq_cst(T& rv, T x) noexcept { return fetch_or(rv, x, std::memory_order::seq_cst); }

template<typename T> BL_INLINE_NODEBUG T fetch_add_rc(T& rv, T x = T(1)) noexcept { return fetch_add(rv, x, std::memory_order::relaxed); }
template<typename T> BL_INLINE_NODEBUG T fetch_sub_rc(T& rv, T x = T(1)) noexcept { return fetch_sub(rv, x, std::memory_order::acq_rel); }

template<typename T>
BL_INLINE_NODEBUG bool compare_exchange(T& rv, T& expected, T desired) noexcept {
  return std::atomic_ref<T>(rv).compare_exchange_strong(expected, desired);
}

} // {anonymous}
} // {bl}

//! \}

//! \}
//! \endcond

#endif // BLEND2D_THREADING_ATOMIC_P_H_INCLUDED
