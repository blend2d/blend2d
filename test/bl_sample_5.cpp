#include <blend2d.h>

int main(int argc, char* argv[]) {
  BLImage img(480, 480, BL_FORMAT_PRGB32);
  BLContext ctx(img);

  ctx.clear_all();

  // First shape filled with a radial gradient.
  // By default, SRC_OVER composition is used.
  BLGradient radial(
    BLRadialGradientValues(180, 180, 180, 180, 180));
  radial.add_stop(0.0, BLRgba32(0xFFFFFFFF));
  radial.add_stop(1.0, BLRgba32(0xFFFF6F3F));
  ctx.fill_circle(180, 180, 160, radial);

  // Second shape filled with a linear gradient.
  BLGradient linear(
    BLLinearGradientValues(195, 195, 470, 470));
  linear.add_stop(0.0, BLRgba32(0xFFFFFFFF));
  linear.add_stop(1.0, BLRgba32(0xFF3F9FFF));

  // Use 'set_comp_op()' to change a composition operator.
  ctx.set_comp_op(BL_COMP_OP_DIFFERENCE);
  ctx.fill_round_rect(
    BLRoundRect(195, 195, 270, 270, 25), linear);

  ctx.end();

  img.write_to_file("bl_sample_5.png");
  return 0;
}
