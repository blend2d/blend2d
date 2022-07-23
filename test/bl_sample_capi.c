#include <blend2d.h>

int main(int argc, char* argv[]) {
  BLResult r;
  BLImageCore img;
  BLContextCore ctx;
  BLGradientCore gradient;

  r = blImageInitAs(&img, 256, 256, BL_FORMAT_PRGB32);
  if (r != BL_SUCCESS)
    return 1;

  r = blContextInitAs(&ctx, &img, NULL);
  if (r != BL_SUCCESS)
    return 1;

  BLLinearGradientValues values = { 0, 0, 256, 256 };
  r = blGradientInitAs(&gradient,
    BL_GRADIENT_TYPE_LINEAR, &values,
    BL_EXTEND_MODE_PAD, NULL, 0, NULL);
  if (r != BL_SUCCESS)
    return 1;

  blGradientAddStopRgba32(&gradient, 0.0, 0xFFFFFFFFu);
  blGradientAddStopRgba32(&gradient, 0.5, 0xFFFFAF00u);
  blGradientAddStopRgba32(&gradient, 1.0, 0xFFFF0000u);

  blContextSetFillStyle(&ctx, &gradient);
  blContextFillAll(&ctx);
  blGradientDestroy(&gradient);

  BLCircle circle;
  circle.cx = 128;
  circle.cy = 128;
  circle.r = 64;

  blContextSetCompOp(&ctx, BL_COMP_OP_EXCLUSION);
  blContextSetFillStyleRgba32(&ctx, 0xFF00FFFFu);
  blContextFillGeometry(&ctx, BL_GEOMETRY_TYPE_CIRCLE, &circle);

  blContextEnd(&ctx);

  // An example of querying a codec from Blend2D internal codecs.
  BLImageCodecCore codec;
  blImageCodecInitByName(&codec, "PNG", SIZE_MAX, NULL);
  blImageWriteToFile(&img, "bl_sample_capi.png", &codec);
  blImageCodecDestroy(&codec);

  blImageDestroy(&img);
  return 0;
}
