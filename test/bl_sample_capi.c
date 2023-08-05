#include <blend2d.h>

int main(int argc, char* argv[]) {
  BLResult r;
  BLImageCore img;
  BLContextCore ctx;

  r = blImageInitAs(&img, 480, 480, BL_FORMAT_PRGB32);
  if (r != BL_SUCCESS)
    return 1;

  r = blContextInitAs(&ctx, &img, NULL);
  if (r != BL_SUCCESS) {
    // Image has been already created, so destroy it.
    blImageDestroy(&img);
    return 1;
  }

  blContextClearAll(&ctx);

  // First shape filled with a radial gradient.
  // By default, SRC_OVER composition is used.
  BLGradientCore radial;
  BLRadialGradientValues radialValues = {
    180, 180, 180, 180, 180
  };

  blGradientInitAs(&radial,
    BL_GRADIENT_TYPE_RADIAL, &radialValues,
    BL_EXTEND_MODE_PAD, NULL, 0, NULL);
  blGradientAddStopRgba32(&radial, 0.0, 0xFFFFFFFFu);
  blGradientAddStopRgba32(&radial, 1.0, 0xFFFF6F3Fu);

  BLCircle circle = {180, 180, 160};
  blContextFillGeometryExt(&ctx,
    BL_GEOMETRY_TYPE_CIRCLE, &circle, &radial);

  // Unused styles must be destroyed.
  blGradientDestroy(&radial);

  // Second shape filled with a linear gradient.
  BLGradientCore linear;
  BLLinearGradientValues linearValues = {
    195, 195, 470, 470
  };

  blGradientInitAs(&linear,
    BL_GRADIENT_TYPE_LINEAR, &linearValues,
    BL_EXTEND_MODE_PAD, NULL, 0, NULL);
  blGradientAddStopRgba32(&linear, 0.0, 0xFFFFFFFFu);
  blGradientAddStopRgba32(&linear, 1.0, 0xFF3F9FFFu);

  // Use 'blContextSetCompOp()' to change a composition operator.
  blContextSetCompOp(&ctx, BL_COMP_OP_DIFFERENCE);

  BLRoundRect roundRect = { 195, 195, 270, 270, 25, 25 };
  blContextFillGeometryExt(&ctx,
    BL_GEOMETRY_TYPE_ROUND_RECT, &roundRect, &linear);

  // Unused styles must be destroyed.
  blGradientDestroy(&linear);

  // Finalize the rendering and destroy the rendering context.
  blContextDestroy(&ctx);

  // An example of querying a codec from Blend2D internal codecs.
  BLImageCodecCore codec;
  blImageCodecInitByName(&codec, "PNG", SIZE_MAX, NULL);
  blImageWriteToFile(&img, "bl_sample_capi.png", &codec);
  blImageCodecDestroy(&codec);

  blImageDestroy(&img);
  return 0;
}
