#include <blend2d.h>

int main(int argc, char* argv[]) {
  BLImage img(480, 480, BL_FORMAT_PRGB32);
  BLContext ctx(img);

  ctx.clearAll();

  // Coordinates can be specified now or changed
  // later via BLGradient accessors.
  BLGradient linear(
    BLLinearGradientValues(0, 0, 0, 480));

  // Color stops can be added in any order.
  linear.addStop(0.0, BLRgba32(0xFFFFFFFF));
  linear.addStop(0.5, BLRgba32(0xFF5FAFDF));
  linear.addStop(1.0, BLRgba32(0xFF2F5FDF));

  // `setFillStyle()` can be used for both colors
  // and styles. Alternatively, a color or style
  // can be passed explicitly to a render function.
  ctx.setFillStyle(linear);

  // Rounded rect will be filled with the linear
  // gradient.
  ctx.fillRoundRect(40.0, 40.0, 400.0, 400.0, 45.5);
  ctx.end();

  img.writeToFile("bl_sample_2.png");
  return 0;
}
