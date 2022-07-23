#include <blend2d.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char* argv[]) {
  BLImage img(480, 480, BL_FORMAT_PRGB32);
  BLContext ctx(img);

  ctx.setCompOp(BL_COMP_OP_SRC_COPY);
  ctx.fillAll();
  ctx.setFillStyle(BLRgba32(0xFFFFFFFF));

  BLFontFace face;
  BLResult err = face.createFromFile("ABeeZee-Regular.ttf");
  if (err) {
    printf("Failed to load a font-face (err=%u)\n", err);
    return 1;
  }

  BLFont font;
  font.createFromFace(face, 20.0f);

  BLFontMetrics fm = font.metrics();
  BLTextMetrics tm;
  BLGlyphBuffer gb;

  BLPoint p(20, 190 + fm.ascent);
  const char* text = "Hello Blend2D!\n"
                     "I'm a simple multiline text example\n"
                     "that uses BLGlyphBuffer and fillGlyphRun!";
  for (;;) {
    const char* end = strchr(text, '\n');
    gb.setUtf8Text(text, end ? (size_t)(end - text) : SIZE_MAX);
    font.shape(gb);
    font.getTextMetrics(gb, tm);

    p.x = (480.0 - (tm.boundingBox.x1 - tm.boundingBox.x0)) / 2.0;
    ctx.fillGlyphRun(p, font, gb.glyphRun());
    p.y += fm.ascent + fm.descent + fm.lineGap;

    if (!end) break;
    text = end + 1;
  }
  ctx.end();
  img.writeToFile("bl_sample_8.png");

  return 0;
}
