#include <blend2d.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
  BLImage img(480, 480, BL_FORMAT_PRGB32);
  BLContext ctx(img);

  const char fontName[] = "ABeeZee-Regular.ttf";
  const char regularText[] = "Hello Blend2D!";
  const char rotatedText[] = "Rotated Text";

  ctx.clearAll();

  // Load font-face and handle a possible error.
  BLFontFace face;
  BLResult result = face.createFromFile(fontName);
  if (result != BL_SUCCESS) {
    printf("Failed to load a font (err=%u)\n", result);
    return 1;
  }

  BLFont font;
  font.createFromFace(face, 50.0f);

  ctx.setFillStyle(BLRgba32(0xFFFFFFFF));
  ctx.fillUtf8Text(BLPoint(60, 80), font, regularText);

  ctx.rotate(0.785398);
  ctx.fillUtf8Text(BLPoint(250, 80), font, rotatedText);

  ctx.end();

  img.writeToFile("bl_sample_7.png");
  return 0;
}
