// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

// Nothing complex is required here - this file is mostly useful for testing whether <blend2d.h> header can be
// included in C mode. All other tests use C++ so this is the only way to make sure that we don't have a broken
// build.

#include <blend2d.h>
#include <blend2d-debug.h>

int main(int argc, const char* argv[]) {
  BLImageCore image;
  BLContextCore ctx;
  BLPathCore path;

  blImageInitAs(&image, 100, 100, BL_FORMAT_PRGB32);

  blContextInitAs(&ctx, &image, NULL);
  blContextClearAll(&ctx);

  blPathInit(&path);
  blPathMoveTo(&path, 25, 25);
  blPathLineTo(&path, 25, 75);
  blPathLineTo(&path, 75, 50);
  blContextSetFillStyleRgba32(&ctx, 0xFFFFFFFF);
  blContextFillPathD(&ctx, &path);
  blPathDestroy(&path);

  blContextDestroy(&ctx);
  blImageDestroy(&image);

  return 0;
}
