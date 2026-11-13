// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// This is an internal header file that is always included first by each Blend2D test file.

#ifndef BLEND2D_CORE_API_BUILD_TEST_P_H_INCLUDED
#define BLEND2D_CORE_API_BUILD_TEST_P_H_INCLUDED

#include <blend2d/core/api_build_p.h>
#include <blend2d/core/runtime.h>

// Build - Tests
// =============

//! \cond NEVER
// Make sure '#ifdef'ed unit tests are not disabled by IDE.
#if !defined(BL_TEST) && defined(__INTELLISENSE__)
  #define BL_TEST
#endif
//! \endcond

// Include a unit testing package if this is a `bl_test_runner` build.
#if defined(BL_TEST)

#include <blend2d-testing/commons/zero_test.h>

// ZeroTest integration.
template<>
struct ZeroFormatter<BLResult> {
  static constexpr bool kExists = true;

  static inline void stringify(ZeroLib::String& buf, BLResult value) noexcept {
    buf.append(bl_result_as_string(value));
  }
};

//! \cond INTERNAL
#define EXPECT_OK(...) EXPECT_CUSTOM(::ZeroTest::expect_eq(__FILE__, __LINE__, "EXPECT_OK(" #__VA_ARGS__ ")", (__VA_ARGS__), BL_SUCCESS))
//! \endcond

#endif // BL_TEST

#endif // BLEND2D_CORE_API_BUILD_TEST_P_H_INCLUDED
