#include <blend2d.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
  BLImage img(480, 480, BL_FORMAT_PRGB32);
  BLContext ctx(img);

  ctx.clear_all();

  // Read an image from file.
  BLImage texture;
  BLResult err = texture.read_from_file("Leaves.jpeg");

  // Handle a possible error.
  if (err != BL_SUCCESS) {
    printf("Failed to load a texture (err=%u)\n", err);
    return 1;
  }

  // Create a pattern and use it to fill a rounded-rect.
  // By default a repeat extend mode is used, but it can
  // be configured to use more extend modes
  BLPattern pattern(texture);
  ctx.fill_round_rect(
    BLRoundRect(40.0, 40.0, 400.0, 400.0, 45.5),
    pattern);

  ctx.end();

  img.write_to_file("bl_sample_3.png");
  return 0;
}
