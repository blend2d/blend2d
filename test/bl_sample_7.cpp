#include <blend2d.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
  BLImage img(480, 480, BL_FORMAT_PRGB32);
  BLContext ctx(img);

  ctx.setCompOp(BL_COMP_OP_SRC_COPY);
  ctx.fillAll();

  BLFontFace face;
  BLResult err = face.createFromFile("ABeeZee-Regular.ttf");

  // We must handle a possible error returned by the loader.
  if (err) {
    printf("Failed to load a font-face (err=%u)\n", err);
    return 1;
  }

  BLFont font;
  font.createFromFace(face, 50.0f);

  ctx.setFillStyle(BLRgba32(0xFFFFFFFF));
  ctx.fillUtf8Text(BLPoint(60, 80), font, "Hello Blend2D!");

  ctx.rotate(0.785398);
  ctx.fillUtf8Text(BLPoint(250, 80), font, "Rotated Text");

  ctx.end();
  img.writeToFile("bl_sample_7.png");

  return 0;
}
