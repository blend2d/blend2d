#include <blend2d.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {
  BLImage img(480, 480, BL_FORMAT_PRGB32);
  BLContext ctx(img);

  const char fontName[] = "ABeeZee-Regular.ttf";
  const char* str =
    "Hello Blend2D!\n"
    "I'm a simple multiline text example\n"
    "that uses GlyphBuffer and GlyphRun!";
  BLRgba32 color(0xFFFFFFFFu);

  BLFontFace face;
  BLResult result = face.createFromFile(fontName);
  if (result != BL_SUCCESS) {
    printf("Failed to load a face (err=%u)\n", result);
    return 1;
  }

  BLFont font;
  font.createFromFace(face, 20.0f);

  BLGlyphBuffer gb;
  BLTextMetrics tm;
  BLFontMetrics fm = font.metrics();
  double y = 190 + fm.ascent;

  ctx.clearAll();
  do {
    const char* nl = strchr(str, '\n');
    gb.setUtf8Text(str,
                   nl ? (size_t)(nl - str) : SIZE_MAX);
    font.shape(gb);
    font.getTextMetrics(gb, tm);

    double x = (tm.boundingBox.x1 - tm.boundingBox.x0);
    ctx.fillGlyphRun(BLPoint((480.0 - x) / 2, y),
                     font, gb.glyphRun(), color);

    y += fm.ascent + fm.descent + fm.lineGap;
    str = nl ? nl + 1 : nullptr;
  } while (str);
  ctx.end();

  img.writeToFile("bl_sample_8.png");
  return 0;
}
