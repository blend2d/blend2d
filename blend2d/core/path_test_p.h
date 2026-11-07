// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_PATH_TEST_P_H_INCLUDED
#define BLEND2D_CORE_PATH_TEST_P_H_INCLUDED

#include <blend2d/core/api_build_p.h>

#include <blend2d/axl/commons.h>
#include <blend2d/core/path_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl::PathTests {

static void path_view_dump(BLPathView a) {
  PathIterator it(a);
  size_t vertex_remaining = a.vertex_count;

  while (it.has_command()) {
    switch (it.command()) {
      case BL_PATH_CMD_MOVE:
        if (vertex_remaining < 1u) {
          TEST_LOG("  MoveTo [Vertex array exhausted]");
          return;
        }

        TEST_LOG("  MoveTo [%g, %g]", it.vertex(0).x, it.vertex(0).y);
        it.advance_cmd<BL_PATH_CMD_MOVE>();
        vertex_remaining -= 1u;
        continue;

      case BL_PATH_CMD_LINE:
        if (vertex_remaining < 1u) {
          TEST_LOG("  LineTo [Vertex array exhausted]");
          return;
        }

        TEST_LOG("  LineTo [%g, %g]", it.vertex(0).x, it.vertex(0).y);
        it.advance_cmd<BL_PATH_CMD_LINE>();
        vertex_remaining -= 1u;
        continue;

      case BL_PATH_CMD_QUAD:
        if (vertex_remaining < 2u) {
          TEST_LOG("  QuadTo [Vertex array exhausted]");
          return;
        }

        TEST_LOG("  QuadTo [%g, %g] [%g, %g]",
          it.vertex(0).x, it.vertex(0).y,
          it.vertex(1).x, it.vertex(1).y);
        it.advance_cmd<BL_PATH_CMD_QUAD>();
        vertex_remaining -= 2u;
        continue;

      case BL_PATH_CMD_CONIC:
        if (vertex_remaining < 2u) {
          TEST_LOG("  ConicTo [Vertex array exhausted]");
          return;
        }

        TEST_LOG("  ConicTo [%g, %g] [%g, %g] [W=%g]",
          it.vertex(0).x, it.vertex(0).y,
          it.vertex(1).x, it.vertex(1).y, it.weight());
        it.advance_cmd<BL_PATH_CMD_CONIC>();
        vertex_remaining -= 2u;
        continue;

      case BL_PATH_CMD_CUBIC:
        if (vertex_remaining < 3u) {
          TEST_LOG("  CubicTo [Vertex array exhausted]");
          return;
        }

        TEST_LOG("  CubicTo [%g, %g] [%g, %g] [%g, %g]",
          it.vertex(0).x, it.vertex(0).y,
          it.vertex(1).x, it.vertex(1).y,
          it.vertex(2).x, it.vertex(2).y);
        it.advance_cmd<BL_PATH_CMD_CUBIC>();
        vertex_remaining -= 3u;
        continue;

      case BL_PATH_CMD_CLOSE:
        TEST_LOG("  Close");
        it.advance_cmd<BL_PATH_CMD_CLOSE>();
        continue;
    }

    TEST_LOG("  Invalid command {%u}", uint32_t(it.command()));
    return;
  }
}

static bool path_view_equals(BLPathView a, BLPathView b) {
  if (a.vertex_count != b.vertex_count || a.control_data_size != b.control_data_size) {
    return false;
  }

  if (memcmp(a.control_data_rev - a.control_data_size, b.control_data_rev - b.control_data_size, a.control_data_size) != 0) {
    return false;
  }

  if (memcmp(a.vertex_data, b.vertex_data, a.vertex_count * sizeof(BLPoint)) != 0) {
    return false;
  }

  return true;
}

static bool path_view_equals_ulp(BLPathView a, BLPathView b, uint64_t max_ulp_diff) {
  if (a.vertex_count != b.vertex_count || a.control_data_size != b.control_data_size) {
    return false;
  }

  if (memcmp(a.control_data_rev - a.control_data_size, b.control_data_rev - b.control_data_size, a.control_data_size) != 0) {
    return false;
  }

  for (size_t i = 0; i < a.vertex_count; i++) {
    BLPoint va = a.vertex_data[i];
    BLPoint vb = b.vertex_data[i];

    uint64_t x_ulp_diff = ZeroTest::ulp_diff(va.x, vb.x);
    uint64_t y_ulp_diff = ZeroTest::ulp_diff(va.y, vb.y);

    uint64_t ulp_diff = axl::max(x_ulp_diff, y_ulp_diff);
    if (ulp_diff > max_ulp_diff) {
      return false;
    }
  }

  return true;
}

static bool path_view_check_equality(BLPathView a, BLPathView b, const char* description) {
  if (!path_view_equals(a, b)) {
    TEST_LOG("Path inequality detected for path (%s):", description);
    path_view_dump(a);
    TEST_LOG("Expected:");
    path_view_dump(b);
    return false;
  }

  return true;
}

static bool path_view_check_equality_ulp(BLPathView a, BLPathView b, uint64_t max_ulp_diff, const char* description) {
  if (!path_view_equals_ulp(a, b, max_ulp_diff)) {
    TEST_LOG("Path inequality detected for path (%s):", description);
    path_view_dump(a);
    TEST_LOG("Expected:");
    path_view_dump(b);
    return false;
  }

  return true;
}

} // {bl::PathTests}

//! \}
//! \endcond

#endif // BLEND2D_CORE_PATH_TEST_P_H_INCLUDED
