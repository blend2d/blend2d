#include <blend2d.h>
#include <stdio.h>

int main() {
  BLImage img(480, 480, BL_FORMAT_PRGB32);
  BLContext ctx(img);

  const char font_name[] = "ABeeZee-Regular.ttf";
  const char regular_text[] = "Hello Blend2D!";
  const char rotated_text[] = "Rotated Text";

  ctx.clear_all();

  // Load font-face and handle a possible error.
  BLFontFace face;
  BLResult result = face.create_from_file(font_name);
  if (result != BL_SUCCESS) {
    printf("Failed to load a font (err=%u)\n", result);
    return 1;
  }

  BLFont font;
  font.create_from_face(face, 50.0f);

  ctx.set_fill_style(BLRgba32(0xFFFFFFFF));
  ctx.fill_utf8_text(BLPoint(60, 80), font, regular_text);

  ctx.rotate(0.785398);
  ctx.fill_utf8_text(BLPoint(250, 80), font, rotated_text);

  ctx.end();

  img.write_to_file("bl_sample_7.png");
  return 0;
}
