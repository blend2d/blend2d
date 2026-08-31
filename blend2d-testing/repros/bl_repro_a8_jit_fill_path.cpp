// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/blend2d.h>

#include <stdio.h>

static bool check(BLResult result, const char *operation) noexcept {
  if (result == BL_SUCCESS)
    return true;

  fprintf(stderr, "%s failed (err=%u)\n", operation, result);
  return false;
}

int main() {
  constexpr int width = 33;
  constexpr int height = 2;
  constexpr int rectangle_x0 = 1;
  constexpr int rectangle_y0 = 1;
  constexpr int rectangle_x1 = 31;
  constexpr int rectangle_y1 = 2;

  alignas(16) uint8_t pixels[width * height]{};
  BLImage image;
  if (!check(image.create_from_data(width, height, BL_FORMAT_A8, pixels, width,
                                    BL_DATA_ACCESS_RW),
             "image.create_from_data"))
    return 1;

  BLPath path;
  if (!check(path.add_box(double(rectangle_x0), double(rectangle_y0),
                          double(rectangle_x1), double(rectangle_y1)),
             "path.add_box"))
    return 1;

  BLContext context(image);
  if (!check(context.fill_path(path, BLRgba32(0xFFFFFFFFu)),
             "context.fill_path") ||
      !check(context.end(), "context.end"))
    return 1;

  size_t differing_pixels = 0;
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      const uint8_t expected = y >= rectangle_y0 && y < rectangle_y1 &&
                                       x >= rectangle_x0 && x < rectangle_x1
                                   ? uint8_t(255)
                                   : uint8_t(0);
      const uint8_t actual = pixels[size_t(y) * size_t(width) + size_t(x)];
      if (actual != expected) {
        if (differing_pixels == 0)
          printf("First difference at (%d, %d): expected %u, got %u\n", x, y,
                 unsigned(expected), unsigned(actual));
        differing_pixels++;
      }
    }
  }

  if (differing_pixels != 0) {
    printf("BUG REPRODUCED: %zu pixels differ from the expected rectangle\n",
           differing_pixels);
    return 0;
  }

  fprintf(stderr, "Bug not reproduced: the rectangle is correct\n");
  return 2;
}
