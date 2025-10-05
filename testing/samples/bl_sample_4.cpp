#include <blend2d.h>
#include <stdio.h>

int main() {
  BLImage img(480, 480, BL_FORMAT_PRGB32);
  BLContext ctx(img);

  ctx.clear_all();

  // Read an image from file.
  BLImage texture;
  BLResult err = texture.read_from_file("Leaves.jpeg");

  if (err != BL_SUCCESS) {
    printf("Failed to load a texture (err=%u)\n", err);
    return 1;
  }

  // Rotate by 45 degrees about a point at [240, 240].
  ctx.rotate(0.785398, 240.0, 240.0);

  // Create a pattern and use it to fill a round rect.
  BLPattern pattern(texture);
  ctx.fill_round_rect(
    BLRoundRect(50.0, 50.0, 380.0, 380.0, 80.5),
    pattern);

  ctx.end();

  img.write_to_file("bl_sample_4.png");
  return 0;
}
