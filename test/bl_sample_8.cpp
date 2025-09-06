#include <blend2d.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {
  BLImage img(480, 480, BL_FORMAT_PRGB32);
  BLContext ctx(img);

  const char font_name[] = "ABeeZee-Regular.ttf";
  const char* str = "Hello Blend2D!\n"
                    "I'm a simple multiline text example\n"
                    "that uses GlyphBuffer and GlyphRun!";
  BLRgba32 color(0xFFFFFFFFu);

  BLFontFace face;
  BLResult result = face.create_from_file(font_name);
  if (result != BL_SUCCESS) {
    printf("Failed to load a face (err=%u)\n", result);
    return 1;
  }

  BLFont font;
  font.create_from_face(face, 20.0f);

  BLGlyphBuffer gb;
  BLTextMetrics tm;
  BLFontMetrics fm = font.metrics();
  double y = 190 + fm.ascent;

  ctx.clear_all();
  do {
    const char* nl = strchr(str, '\n');
    gb.set_utf8_text(str, nl ? (size_t)(nl - str) : SIZE_MAX);
    font.shape(gb);
    font.get_text_metrics(gb, tm);

    double x = (tm.bounding_box.x1 - tm.bounding_box.x0);
    ctx.fill_glyph_run(BLPoint((480.0 - x) / 2, y),
                       font, gb.glyph_run(), color);

    y += fm.ascent + fm.descent + fm.line_gap;
    str = nl ? nl + 1 : nullptr;
  } while (str);
  ctx.end();

  img.write_to_file("bl_sample_8.png");
  return 0;
}
