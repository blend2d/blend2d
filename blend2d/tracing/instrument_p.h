// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_TRACING_INSTRUMENT_P_H_INCLUDED
#define BLEND2D_TRACING_INSTRUMENT_P_H_INCLUDED

#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/runtime.h>

#include <chrono>

//! \cond INTERNAL
//! \addtogroup blend2d_profile
//! \{

namespace bl::Tracing {

struct Time {
  uint64_t ns {};

  using TimePoint = std::chrono::high_resolution_clock::time_point;

  BL_INLINE TimePoint start() const noexcept {
    return std::chrono::high_resolution_clock::now();
  }

  BL_INLINE void end(TimePoint start_time) noexcept {
    TimePoint final_time = std::chrono::high_resolution_clock::now();
    ns += std::chrono::duration<uint64_t, std::nano>(final_time - start_time).count();
  }

  Time operator+(const Time& other) const noexcept { return Time{ns + other.ns}; }
};

struct Count {
  uint64_t value {};

  BL_INLINE void increment() noexcept { value++; }
};

static void output(const char* key, const Count& count) noexcept {
  bl_runtime_message_fmt("%-28s: %10llu [count]\n", key, (unsigned long long)count.value);
}

static void output(const char* key, const Time& time) noexcept {
  bl_runtime_message_fmt("%-28s: %10.1f [ms]\n", key, (double)time.ns / 1000000u);
}

#define BL_TRACING_OUTPUT(what) do { ::bl::Tracing::output(#what, what); } while (0)

} // {bl::Profile}

//! \}
//! \endcond

#endif // BLEND2D_TRACING_INSTRUMENT_P_H_INCLUDED
