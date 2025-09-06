#include <blend2d.h>

int main(int argc, char* argv[]) {
  BLImage img(480, 480, BL_FORMAT_PRGB32);
  BLContext ctx(img);

  ctx.clear_all();

  BLGradient linear(BLLinearGradientValues(0, 0, 0, 480));
  linear.add_stop(0.0, BLRgba32(0xFFFFFFFF));
  linear.add_stop(0.5, BLRgba32(0xFFFF1F7F));
  linear.add_stop(1.0, BLRgba32(0xFF1F7FFF));

  BLPath path;
  path.move_to(119, 49);
  path.cubic_to(259, 29, 99, 279, 275, 267);
  path.cubic_to(537, 245, 300, -170, 274, 430);

  // Use 'setStrokeXXX' to change stroke options.
  ctx.set_stroke_width(15);
  ctx.set_stroke_start_cap(BL_STROKE_CAP_ROUND);
  ctx.set_stroke_end_cap(BL_STROKE_CAP_BUTT);

  ctx.stroke_path(path, linear);

  ctx.end();

  img.write_to_file("bl_sample_6.png");
  return 0;
}
