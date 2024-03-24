#include <blend2d.h>

BLImage render() {
  BLImage img0(400, 400, BL_FORMAT_A8);
  BLImage img1(400, 400, BL_FORMAT_PRGB32);

  BLContext ctx0(img0);
  ctx0.clearAll();
  ctx0.fillCircle(200, 200, 100, BLRgba32(0xA9000000));

  BLContext ctx1(img1);
  ctx1.blitImage(BLPointI(0, 0), img0);

  return img1;
}

int main(int argc, char* argv[]) {
  BLImage img(480, 480, BL_FORMAT_A8);
/*
  BLContext ctx(img);

  ctx.clearAll();

  ctx.fillRect(BLRectI(100, 100, 100, 100), BLRgba32(0xFF000000));
  ctx.fillRect(BLRectI(150, 150, 100, 100), BLRgba32(0x6F000000));
  ctx.fillRect(BLRectI(200, 200, 100, 100), BLRgba32(0x6F000000));

  BLGradient radial(BLRadialGradientValues(25, 25, 25, 25, 30));
  radial.addStop(0.0, BLRgba32(0xFFFFFFFF));
  radial.addStop(1.0, BLRgba32(0x00000000));
  ctx.fillRect(0, 10, 40, 40, radial);

  ctx.end();
*/
  img = render();
  img.convert(BL_FORMAT_PRGB32);
  img.writeToFile("bl_playground.png");
  return 0;
}
