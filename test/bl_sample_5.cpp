#include <blend2d.h>

int main(int argc, char* argv[]) {
  BLImage img(480, 480, BL_FORMAT_PRGB32);
  BLContext ctx(img);

  ctx.clearAll();

  // First shape filled with a radial gradient.
  // By default, SRC_OVER composition is used.
  BLGradient radial(
    BLRadialGradientValues(180, 180, 180, 180, 180));
  radial.addStop(0.0, BLRgba32(0xFFFFFFFF));
  radial.addStop(1.0, BLRgba32(0xFFFF6F3F));
  ctx.fillCircle(180, 180, 160, radial);

  // Second shape filled with a linear gradient.
  BLGradient linear(
    BLLinearGradientValues(195, 195, 470, 470));
  linear.addStop(0.0, BLRgba32(0xFFFFFFFF));
  linear.addStop(1.0, BLRgba32(0xFF3F9FFF));

  // Use 'setCompOp()' to change a composition operator.
  ctx.setCompOp(BL_COMP_OP_DIFFERENCE);
  ctx.fillRoundRect(
    BLRoundRect(195, 195, 270, 270, 25), linear);

  ctx.end();

  img.writeToFile("bl_sample_5.png");
  return 0;
}
