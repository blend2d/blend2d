#include <blend2d.h>

int main(int argc, char* argv[]) {
  BLResult r;
  BLImageCore img;
  BLContextCore ctx;

  r = bl_image_init_as(&img, 480, 480, BL_FORMAT_PRGB32);
  if (r != BL_SUCCESS)
    return 1;

  r = bl_context_init_as(&ctx, &img, NULL);
  if (r != BL_SUCCESS) {
    // Image has been already created, so destroy it.
    bl_image_destroy(&img);
    return 1;
  }

  bl_context_clear_all(&ctx);

  // First shape filled with a radial gradient.
  // By default, SRC_OVER composition is used.
  BLGradientCore radial;
  BLRadialGradientValues radial_values = {
    180, 180, 180, 180, 180
  };

  bl_gradient_init_as(&radial,
    BL_GRADIENT_TYPE_RADIAL, &radial_values,
    BL_EXTEND_MODE_PAD, NULL, 0, NULL);
  bl_gradient_add_stop_rgba32(&radial, 0.0, 0xFFFFFFFFu);
  bl_gradient_add_stop_rgba32(&radial, 1.0, 0xFFFF6F3Fu);

  BLCircle circle = {180, 180, 160};
  bl_context_fill_geometry_ext(&ctx,
    BL_GEOMETRY_TYPE_CIRCLE, &circle, &radial);

  // Unused styles must be destroyed.
  bl_gradient_destroy(&radial);

  // Second shape filled with a linear gradient.
  BLGradientCore linear;
  BLLinearGradientValues linear_values = {
    195, 195, 470, 470
  };

  bl_gradient_init_as(&linear,
    BL_GRADIENT_TYPE_LINEAR, &linear_values,
    BL_EXTEND_MODE_PAD, NULL, 0, NULL);
  bl_gradient_add_stop_rgba32(&linear, 0.0, 0xFFFFFFFFu);
  bl_gradient_add_stop_rgba32(&linear, 1.0, 0xFF3F9FFFu);

  // Use 'bl_context_set_comp_op()' to change a composition operator.
  bl_context_set_comp_op(&ctx, BL_COMP_OP_DIFFERENCE);

  BLRoundRect round_rect = { 195, 195, 270, 270, 25, 25 };
  bl_context_fill_geometry_ext(&ctx,
    BL_GEOMETRY_TYPE_ROUND_RECT, &round_rect, &linear);

  // Unused styles must be destroyed.
  bl_gradient_destroy(&linear);

  // Finalize the rendering and destroy the rendering context.
  bl_context_destroy(&ctx);

  // An example of querying a codec from Blend2D internal codecs.
  BLImageCodecCore codec;
  bl_image_codec_init_by_name(&codec, "PNG", SIZE_MAX, NULL);
  bl_image_write_to_file(&img, "bl_sample_capi.png", &codec);
  bl_image_codec_destroy(&codec);

  bl_image_destroy(&img);
  return 0;
}
