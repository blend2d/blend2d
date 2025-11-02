#include <blend2d/blend2d.h>

int main() {
  // Use constructor or `create()` function to
  // allocate a new image data of the required
  // format.
  BLImage img(480, 480, BL_FORMAT_PRGB32);

  // Attach a rendering context into `img`.
  BLContext ctx(img);

  // Clearing the image would make it transparent.
  ctx.clear_all();

  // Create a path having cubic curves.
  BLPath path;
  path.move_to(26, 31);
  path.cubic_to(642, 132, 587, -136, 25, 464);
  path.cubic_to(882, 404, 144, 267, 27, 31);

  // Fill a path with opaque white - 0xAARRGGBB.
  ctx.fill_path(path, BLRgba32(0xFFFFFFFF));

  // Detach the rendering context from `img`.
  ctx.end();

  // Let's use some built-in codecs provided by Blend2D.
  img.write_to_file("bl_sample_1.png");

  return 0;
}
