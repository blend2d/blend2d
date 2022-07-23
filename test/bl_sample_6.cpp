#include <blend2d.h>

int main(int argc, char* argv[]) {
  BLImage img(480, 480, BL_FORMAT_PRGB32);
  BLContext ctx(img);

  ctx.setCompOp(BL_COMP_OP_SRC_COPY);
  ctx.fillAll();

  BLGradient linear(BLLinearGradientValues(0, 0, 0, 480));
  linear.addStop(0.0, BLRgba32(0xFFFFFFFF));
  linear.addStop(1.0, BLRgba32(0xFF1F7FFF));

  BLPath path;
  path.moveTo(119, 49);
  path.cubicTo(259, 29, 99, 279, 275, 267);
  path.cubicTo(537, 245, 300, -170, 274, 430);

  ctx.setCompOp(BL_COMP_OP_SRC_OVER);
  ctx.setStrokeStyle(linear);
  ctx.setStrokeWidth(15);
  ctx.setStrokeStartCap(BL_STROKE_CAP_ROUND);
  ctx.setStrokeEndCap(BL_STROKE_CAP_BUTT);
  ctx.strokePath(path);

  ctx.end();
  img.writeToFile("bl_sample_6.png");

  return 0;
}
