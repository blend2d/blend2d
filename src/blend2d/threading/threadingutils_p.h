// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_THREADING_THREADINGUTILS_P_H_INCLUDED
#define BLEND2D_THREADING_THREADINGUTILS_P_H_INCLUDED

#include "../api-internal_p.h"

#ifndef _WIN32
  #include <sys/time.h>
#endif

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace BLThreadingUtils {

#ifndef _WIN32
static void getAbsTimeForWaitCondition(struct timespec& out, uint64_t microseconds) noexcept {
  struct timeval now;
  gettimeofday(&now, nullptr);

  out.tv_sec = now.tv_sec + int64_t(microseconds / 1000000u);
  out.tv_nsec = (now.tv_usec + int64_t(microseconds % 1000000u)) * 1000;
  out.tv_sec += out.tv_nsec / 1000000000;
  out.tv_nsec %= 1000000000;
}
#endif

} // {BLThreadingUtils}

//! \}
//! \endcond

#endif // BLEND2D_THREADING_THREADINGUTILS_P_H_INCLUDED
