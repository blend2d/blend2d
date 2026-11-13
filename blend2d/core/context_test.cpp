// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_test_p.h>
#if defined(BL_TEST)

#include <blend2d/core/context_p.h>
#include <blend2d/core/gradient_p.h>
#include <blend2d/core/image_p.h>
#include <blend2d/core/pattern_p.h>

// Context - Tests
// ===============

namespace bl::ContextTests {

static void test_context_state(BLContext& ctx) {
  // Prepare non-solid styles.
  BLGradient gradient;
  EXPECT_OK(gradient.create(BLLinearGradientValues(0.0, 0.0, 0.0, 100.0)));
  EXPECT_OK(gradient.add_stop(0.0, BLRgba32(0x00000000u)));
  EXPECT_OK(gradient.add_stop(1.0, BLRgba32(0xFF0000FFu)));

  BLImage image(16, 16, BL_FORMAT_PRGB32);
  BLPattern pattern(image);

  TEST_LOG("Testing state management of global composition options");
  {
    EXPECT_OK(ctx.set_comp_op(BL_COMP_OP_SRC_ATOP));
    EXPECT_OK(ctx.set_global_alpha(0.5));

    EXPECT_EQ(ctx.comp_op(), BL_COMP_OP_SRC_ATOP);
    EXPECT_EQ(ctx.global_alpha(), 0.5);

    EXPECT_OK(ctx.save());
    EXPECT_OK(ctx.set_comp_op(BL_COMP_OP_MULTIPLY));
    EXPECT_OK(ctx.set_global_alpha(0.9));
    EXPECT_EQ(ctx.comp_op(), BL_COMP_OP_MULTIPLY);
    EXPECT_EQ(ctx.global_alpha(), 0.9);
    EXPECT_OK(ctx.restore());

    EXPECT_EQ(ctx.comp_op(), BL_COMP_OP_SRC_ATOP);
    EXPECT_EQ(ctx.global_alpha(), 0.5);

    EXPECT_OK(ctx.set_comp_op(BL_COMP_OP_SRC_OVER));
    EXPECT_OK(ctx.set_global_alpha(1.0));
  }

  TEST_LOG("Testing state management of global, fill, and stroke alpha values");
  {
    EXPECT_OK(ctx.set_global_alpha(0.5));
    EXPECT_OK(ctx.set_fill_alpha(0.3));
    EXPECT_OK(ctx.set_stroke_alpha(0.4));

    EXPECT_EQ(ctx.global_alpha(), 0.5);
    EXPECT_EQ(ctx.fill_alpha(), 0.3);
    EXPECT_EQ(ctx.stroke_alpha(), 0.4);

    EXPECT_OK(ctx.save());

    EXPECT_OK(ctx.set_global_alpha(1.0));
    EXPECT_OK(ctx.set_fill_alpha(0.7));
    EXPECT_OK(ctx.set_stroke_alpha(0.8));

    EXPECT_EQ(ctx.global_alpha(), 1.0);
    EXPECT_EQ(ctx.fill_alpha(), 0.7);
    EXPECT_EQ(ctx.stroke_alpha(), 0.8);

    EXPECT_OK(ctx.restore());

    EXPECT_EQ(ctx.global_alpha(), 0.5);
    EXPECT_EQ(ctx.fill_alpha(), 0.3);
    EXPECT_EQ(ctx.stroke_alpha(), 0.4);

    EXPECT_OK(ctx.set_global_alpha(0.1));
    EXPECT_OK(ctx.set_fill_alpha(1.0));
    EXPECT_OK(ctx.set_stroke_alpha(0.9));

    EXPECT_EQ(ctx.global_alpha(), 0.1);
    EXPECT_EQ(ctx.fill_alpha(), 1.0);
    EXPECT_EQ(ctx.stroke_alpha(), 0.9);
  }

  TEST_LOG("Testing state management of styles (Rgba, Rgba32, Rgba64)");
  {
    BLRgba32 rgba32;
    BLRgba64 rgba64;

    // Initial style.
    BLRgba fill_color(0.1f, 0.2f, 0.3f, 0.4f);
    BLRgba stroke_color(0.5f, 0.6f, 0.7f, 0.8f);

    EXPECT_OK(ctx.set_fill_style(fill_color));
    EXPECT_OK(ctx.set_stroke_style(stroke_color));

    BLVar fill_style_var;
    BLVar stroke_style_var;

    EXPECT_OK(ctx.get_fill_style(fill_style_var));
    EXPECT_OK(ctx.get_stroke_style(stroke_style_var));

    EXPECT_TRUE(fill_style_var.is_rgba());
    EXPECT_TRUE(stroke_style_var.is_rgba());

    EXPECT_EQ(fill_style_var.as<BLRgba>(), fill_color);
    EXPECT_EQ(stroke_style_var.as<BLRgba>(), stroke_color);

    // Save and change the style.
    {
      EXPECT_OK(ctx.save());
      BLRgba new_fill_color(0.9f, 0.8f, 0.7f, 0.6f);
      BLRgba new_stroke_color(0.7f, 0.6f, 0.5f, 0.4f);

      EXPECT_OK(ctx.set_fill_style(new_fill_color));
      EXPECT_OK(ctx.set_stroke_style(new_stroke_color));

      EXPECT_OK(ctx.get_fill_style(fill_style_var));
      EXPECT_OK(ctx.get_stroke_style(stroke_style_var));

      EXPECT_TRUE(fill_style_var.is_rgba());
      EXPECT_TRUE(stroke_style_var.is_rgba());

      EXPECT_EQ(fill_style_var.as<BLRgba>(), new_fill_color);
      EXPECT_EQ(stroke_style_var.as<BLRgba>(), new_stroke_color);

      EXPECT_OK(ctx.set_fill_style(BLRgba32(0xFFEEDDCCu)));
      EXPECT_OK(ctx.set_stroke_style(BLRgba64(0x9999AAAABBBBCCCCu)));

      EXPECT_OK(ctx.get_fill_style(fill_style_var));
      EXPECT_OK(ctx.get_stroke_style(stroke_style_var));

      EXPECT_TRUE(fill_style_var.is_rgba32());
      EXPECT_TRUE(stroke_style_var.is_rgba64());

      EXPECT_OK(fill_style_var.to_rgba32(&rgba32));
      EXPECT_OK(stroke_style_var.to_rgba64(&rgba64));

      EXPECT_EQ(rgba32, BLRgba32(0xFFEEDDCCu));
      EXPECT_EQ(rgba64, BLRgba64(0x9999AAAABBBBCCCCu));
      EXPECT_OK(ctx.restore());
    }

    // Now we should observe the initial style that was active before save().
    EXPECT_OK(ctx.get_fill_style(fill_style_var));
    EXPECT_OK(ctx.get_stroke_style(stroke_style_var));

    EXPECT_TRUE(fill_style_var.is_rgba());
    EXPECT_TRUE(stroke_style_var.is_rgba());

    EXPECT_EQ(fill_style_var.as<BLRgba>(), fill_color);
    EXPECT_EQ(stroke_style_var.as<BLRgba>(), stroke_color);
  }

  TEST_LOG("Testing state management of styles (Rgba32, Gradient)");
  {
    BLRgba32 rgba32;

    BLVar fill_style_var;
    BLVar stroke_style_var;

    // Initial style.
    EXPECT_OK(ctx.set_fill_style(gradient));
    EXPECT_OK(ctx.set_stroke_style(BLRgba32(0x44332211u)));

    EXPECT_OK(ctx.get_fill_style(fill_style_var));
    EXPECT_OK(ctx.get_stroke_style(stroke_style_var));

    EXPECT_TRUE(fill_style_var.is_gradient());
    EXPECT_TRUE(stroke_style_var.is_rgba32());

    EXPECT_EQ(fill_style_var.as<BLGradient>(), gradient);
    EXPECT_OK(stroke_style_var.to_rgba32(&rgba32));
    EXPECT_EQ(rgba32, BLRgba32(0x44332211u));

    // Save and change the style.
    {
      EXPECT_OK(ctx.save());
      EXPECT_OK(ctx.set_fill_style(BLRgba32(0xFFFFFFFFu)));
      EXPECT_OK(ctx.set_stroke_style(BLRgba32(0x00000000u)));

      EXPECT_OK(ctx.get_fill_style(fill_style_var));
      EXPECT_OK(ctx.get_stroke_style(stroke_style_var));

      EXPECT_TRUE(fill_style_var.is_rgba32());
      EXPECT_TRUE(stroke_style_var.is_rgba32());

      EXPECT_OK(fill_style_var.to_rgba32(&rgba32));
      EXPECT_EQ(rgba32, BLRgba32(0xFFFFFFFFu));

      EXPECT_OK(stroke_style_var.to_rgba32(&rgba32));
      EXPECT_EQ(rgba32, BLRgba32(0x00000000u));
      EXPECT_OK(ctx.restore());
    }

    // Now we should observe the initial style that was active before save().
    EXPECT_OK(ctx.get_fill_style(fill_style_var));
    EXPECT_OK(ctx.get_stroke_style(stroke_style_var));

    EXPECT_TRUE(fill_style_var.is_gradient());
    EXPECT_TRUE(stroke_style_var.is_rgba32());

    EXPECT_EQ(fill_style_var.as<BLGradient>(), gradient);
    EXPECT_OK(stroke_style_var.to_rgba32(&rgba32));
    EXPECT_EQ(rgba32, BLRgba32(0x44332211u));
  }

  TEST_LOG("Testing fill and stroke style swapping (Rgba32)");
  {
    BLRgba32 a(0xFF000000u);
    BLRgba32 b(0xFFFFFFFFu);

    double alpha_a = 0.5;
    double alpha_b = 0.7;

    BLVar va;
    BLVar vb;

    BLRgba32 va_rgba32;
    BLRgba32 vb_rgba32;

    EXPECT_OK(ctx.set_fill_style(a));
    EXPECT_OK(ctx.set_stroke_style(b));

    ctx.set_fill_alpha(alpha_a);
    ctx.set_stroke_alpha(alpha_b);

    // Swap styles twice - first time to swap, second time to revert the swap.
    for (uint32_t i = 0; i < 2; i++) {
      for (uint32_t j = 0; j < 2; j++) {
        BLContextStyleSwapMode mode = BLContextStyleSwapMode(j);

        EXPECT_OK(ctx.swap_styles(mode));
        EXPECT_OK(ctx.get_fill_style(va));
        EXPECT_OK(ctx.get_stroke_style(vb));

        EXPECT_TRUE(va.is_rgba32());
        EXPECT_TRUE(vb.is_rgba32());
        EXPECT_OK(va.to_rgba32(&va_rgba32));
        EXPECT_OK(vb.to_rgba32(&vb_rgba32));

        Internal::swap(a, b);
        EXPECT_EQ(a, va_rgba32);
        EXPECT_EQ(b, vb_rgba32);

        if (mode == BL_CONTEXT_STYLE_SWAP_MODE_STYLES_WITH_ALPHA) {
          Internal::swap(alpha_a, alpha_b);
          EXPECT_EQ(ctx.fill_alpha(), alpha_a);
          EXPECT_EQ(ctx.stroke_alpha(), alpha_b);
        }
      }
    }
  }

  TEST_LOG("Testing fill and stroke style swapping (Gradient, Pattern)");
  {
    BLVar va;
    BLVar vb;

    EXPECT_OK(ctx.set_fill_style(gradient));
    EXPECT_OK(ctx.set_stroke_style(pattern));

    // First swap.
    EXPECT_OK(ctx.swap_styles(BL_CONTEXT_STYLE_SWAP_MODE_STYLES));
    EXPECT_OK(ctx.get_fill_style(va));
    EXPECT_OK(ctx.get_stroke_style(vb));

    EXPECT_TRUE(va.is_pattern());
    EXPECT_TRUE(vb.is_gradient());

    EXPECT_EQ(va.as<BLPattern>(), pattern);
    EXPECT_EQ(vb.as<BLGradient>(), gradient);

    // Second swap.
    EXPECT_OK(ctx.swap_styles(BL_CONTEXT_STYLE_SWAP_MODE_STYLES));
    EXPECT_OK(ctx.get_fill_style(va));
    EXPECT_OK(ctx.get_stroke_style(vb));

    EXPECT_TRUE(va.is_gradient());
    EXPECT_TRUE(vb.is_pattern());

    EXPECT_EQ(va.as<BLGradient>(), gradient);
    EXPECT_EQ(vb.as<BLPattern>(), pattern);
  }

  TEST_LOG("Testing state management of fill options");
  {
    BLFillRule initial_fill_rule = ctx.fill_rule();

    EXPECT_OK(ctx.save());

    EXPECT_OK(ctx.set_fill_rule(BL_FILL_RULE_EVEN_ODD));
    EXPECT_EQ(ctx.fill_rule(), BL_FILL_RULE_EVEN_ODD);

    EXPECT_OK(ctx.set_fill_rule(BL_FILL_RULE_NON_ZERO));
    EXPECT_EQ(ctx.fill_rule(), BL_FILL_RULE_NON_ZERO);

    EXPECT_OK(ctx.set_fill_rule(
      initial_fill_rule == BL_FILL_RULE_NON_ZERO
        ? BL_FILL_RULE_EVEN_ODD
        : BL_FILL_RULE_NON_ZERO));
    EXPECT_OK(ctx.restore());

    EXPECT_EQ(ctx.fill_rule(), initial_fill_rule);
  }

  TEST_LOG("Testing state management of stroke options");
  {
    BLArray<float> dashes;
    dashes.append(1.0f, 2.0f, 3.0f, 4.0f);

    EXPECT_OK(ctx.save());

    EXPECT_OK(ctx.set_stroke_width(2.0f));
    EXPECT_EQ(ctx.stroke_width(), 2.0f);

    EXPECT_OK(ctx.set_stroke_miter_limit(10.0f));
    EXPECT_EQ(ctx.stroke_miter_limit(), 10.0f);

    EXPECT_OK(ctx.set_stroke_join(BL_STROKE_JOIN_ROUND));
    EXPECT_EQ(ctx.stroke_join(), BL_STROKE_JOIN_ROUND);

    EXPECT_OK(ctx.set_stroke_start_cap(BL_STROKE_CAP_ROUND_REV));
    EXPECT_EQ(ctx.stroke_start_cap(), BL_STROKE_CAP_ROUND_REV);

    EXPECT_OK(ctx.set_stroke_end_cap(BL_STROKE_CAP_TRIANGLE_REV));
    EXPECT_EQ(ctx.stroke_end_cap(), BL_STROKE_CAP_TRIANGLE_REV);

    EXPECT_OK(ctx.set_stroke_dash_array(dashes));
    EXPECT_EQ(ctx.stroke_dash_array(), dashes.span());

    EXPECT_OK(ctx.set_stroke_dash_offset(5.0f));
    EXPECT_EQ(ctx.stroke_dash_offset(), 5.0f);

    BLStrokeOptions opt = ctx.stroke_options();
    EXPECT_EQ(opt, ctx.stroke_options());
    EXPECT_EQ(opt.width(), 2.0f);
    EXPECT_EQ(opt.miter_limit(), 10.0f);
    EXPECT_EQ(opt.join(), BL_STROKE_JOIN_ROUND);
    EXPECT_EQ(opt.start_cap(), BL_STROKE_CAP_ROUND_REV);
    EXPECT_EQ(opt.end_cap(), BL_STROKE_CAP_TRIANGLE_REV);
    EXPECT_EQ(opt.cap_by_position(BL_STROKE_CAP_POSITION_START), BL_STROKE_CAP_ROUND_REV);
    EXPECT_EQ(opt.cap_by_position(BL_STROKE_CAP_POSITION_END), BL_STROKE_CAP_TRIANGLE_REV);
    EXPECT_EQ(opt.dash_array(), dashes.span());
    EXPECT_EQ(opt.dash_offset(), 5.0f);

    EXPECT_OK(ctx.restore());

    EXPECT_OK(ctx.save());
    EXPECT_OK(ctx.set_stroke_options(opt));
    EXPECT_EQ(ctx.stroke_width(), 2.0f);
    EXPECT_EQ(ctx.stroke_miter_limit(), 10.0f);
    EXPECT_EQ(ctx.stroke_join(), BL_STROKE_JOIN_ROUND);
    EXPECT_EQ(ctx.stroke_start_cap(), BL_STROKE_CAP_ROUND_REV);
    EXPECT_EQ(ctx.stroke_end_cap(), BL_STROKE_CAP_TRIANGLE_REV);
    EXPECT_EQ(ctx.stroke_dash_array(), dashes.span());
    EXPECT_EQ(ctx.stroke_dash_offset(), 5.0f);

    BLStrokeOptions opt2 = ctx.stroke_options();
    EXPECT_EQ(opt, opt2);

    EXPECT_OK(ctx.restore());
  }

  TEST_LOG("Testing state management of transformations");
  {
    BLMatrix2D transform = BLMatrix2D::make_scaling(2.0);
    EXPECT_OK(ctx.apply_transform(transform));
    EXPECT_EQ(ctx.user_transform(), transform);
    EXPECT_EQ(ctx.meta_transform(), BLMatrix2D::make_identity());

    EXPECT_OK(ctx.save());
    EXPECT_OK(ctx.user_to_meta());
    EXPECT_EQ(ctx.meta_transform(), transform);
    EXPECT_EQ(ctx.user_transform(), BLMatrix2D::make_identity());
    EXPECT_OK(ctx.restore());

    EXPECT_EQ(ctx.user_transform(), transform);
    EXPECT_EQ(ctx.meta_transform(), BLMatrix2D::make_identity());

    EXPECT_OK(ctx.reset_transform());
    EXPECT_EQ(ctx.user_transform(), BLMatrix2D::make_identity());
  }
}

// NOTE: The purpose of these tests is to see whether the render call is clipped properly and that there is no out
// of bounds access or a failed assertion. The tests on CI are run with sanitizers so NaNs, Infs, and other extremely
// high numbers are great to verify whether we are not hitting UB in places where FetchData is initialized.
static void test_context_blit_fill_clip(BLContext& ctx) {
  const double kNaN = Math::nan<double>();
  const double kInf = Math::inf<double>();

  int cw = int(ctx.target_width());
  int ch = int(ctx.target_height());
  int sw = 23;
  int sh = 23;
  BLImage sprite(sw, sh, BL_FORMAT_PRGB32);

  {
    BLContext sctx(sprite);
    sctx.fill_all(BLRgba32(0xFFFFFFFF));
  }

  BLMatrix2D matrix_data[] = {
    BLMatrix2D::make_identity(),
    BLMatrix2D::make_translation(11.3, 11.9),
    BLMatrix2D::make_scaling(100.0, 100.0),
    BLMatrix2D::make_scaling(-100.0, -100.0),
    BLMatrix2D::make_scaling(1, 0.000001),
    BLMatrix2D::make_scaling(0.000001, 1),
    BLMatrix2D::make_scaling(0.000001, 0.000001),
    BLMatrix2D::make_scaling(1e-20, 1e-20),
    BLMatrix2D::make_scaling(1e-100, 1e-100),
  };

  BLPointI pointi_data[] = {
    BLPointI(        0,         0),
    BLPointI(        0,        -1),
    BLPointI(       -1,         0),
    BLPointI(       -1,        -1),
    BLPointI(        0,    ch - 1),
    BLPointI(   cw - 1,         0),
    BLPointI(   cw - 1,    ch - 1),
    BLPointI(        0,   -sh + 1),
    BLPointI(  -sw + 1,         0),
    BLPointI(  -sw + 1,   -sh + 1),
    BLPointI(INT32_MIN,         0),
    BLPointI(INT32_MIN,        -1),
    BLPointI(        0, INT32_MIN),
    BLPointI(       -1, INT32_MIN),
    BLPointI(INT32_MIN, INT32_MIN),
    BLPointI(INT32_MAX,         0),
    BLPointI(INT32_MAX,        -1),
    BLPointI(        0, INT32_MAX),
    BLPointI(       -1, INT32_MAX),
    BLPointI(INT32_MAX, INT32_MAX)
  };

  BLPoint pointd_data[] = {
    BLPoint(       0.0,       0.0),
    BLPoint(       0.0,       0.3),
    BLPoint(       0.3,       0.0),
    BLPoint(       0.3,       0.3),
    BLPoint(       0.0,       100),
    BLPoint(       100,       0.0),
    BLPoint(       100,       100),
    BLPoint(       0.0,  -sh+1e-1),
    BLPoint(       0.0,  -sh+1e-2),
    BLPoint(       0.0,  -sh+1e-3),
    BLPoint(       0.0,  -sh+1e-4),
    BLPoint(       0.0,  -sh+1e-5),
    BLPoint(       0.0,  -sh+1e-6),
    BLPoint(       0.0,  -sh+1e-7),
    BLPoint(         0,     -sh+1),
    BLPoint(  -sw+1e-1,       0.0),
    BLPoint(  -sw+1e-2,       0.0),
    BLPoint(  -sw+1e-3,       0.0),
    BLPoint(  -sw+1e-4,       0.0),
    BLPoint(  -sw+1e-5,       0.0),
    BLPoint(  -sw+1e-6,       0.0),
    BLPoint(  -sw+1e-7,       0.0),
    BLPoint(     -sw+1,         0),
    BLPoint(         0,    ch-0.1),
    BLPoint(         0,   ch-0.01),
    BLPoint(         0,  ch-0.001),
    BLPoint(         0, ch-0.0001),
    BLPoint(         0,ch-0.00001),
    BLPoint(    cw-0.1,         0),
    BLPoint(   cw-0.01,         0),
    BLPoint(  cw-0.001,         0),
    BLPoint( cw-0.0001,         0),
    BLPoint(cw-0.00001,         0),
    BLPoint(cw-0.00001,ch-0.00001),
    BLPoint(   -1000.0,       0.0),
    BLPoint(-1000000.0,       0.0),
    BLPoint(       0.0,   -1000.0),
    BLPoint(       0.0,-1000000.0),
    BLPoint(   -1000.0,   -1000.0),
    BLPoint(-1000000.0,-1000000.0),
    BLPoint(     -1e50,     -1e50),
    BLPoint(    -1e100,    -1e100),
    BLPoint(    -1e200,    -1e200),
    BLPoint(      1e50,      1e50),
    BLPoint(     1e100,     1e100),
    BLPoint(     1e200,     1e200),
    BLPoint(      kInf,       0.0),
    BLPoint(       0.0,      kInf),
    BLPoint(      kInf,      kInf),
    BLPoint(     -kInf,       0.0),
    BLPoint(       0.0,     -kInf),
    BLPoint(     -kInf,     -kInf),
    BLPoint(      kNaN,       0.0),
    BLPoint(       0.0,      kNaN),
    BLPoint(      kNaN,      kNaN)
  };

  BLSizeI sizei_data[] = {
    BLSizeI(       sw,       sh),
    BLSizeI(   sw / 2,   sh / 2),
    BLSizeI(        1,        1),
    BLSizeI(        0,        0),
    BLSizeI(        0,INT32_MIN),
    BLSizeI(INT32_MIN,        0),
    BLSizeI(INT32_MIN,INT32_MIN),
    BLSizeI(        0,INT32_MAX),
    BLSizeI(INT32_MAX,        0),
    BLSizeI(INT32_MAX,INT32_MAX)
  };

  BLSize sized_data[] = {
    BLSize(       sw,       sh),
    BLSize(   sw / 2,   sh / 2),
    BLSize(      1.0,      1.0),
    BLSize(      0.0,      0.0),
    BLSize(      0.0,       sh),
    BLSize(       sw,      0.0),
    BLSize(      0.1,      0.1),
    BLSize(  0.00001,       sh),
    BLSize(       sw,  0.00001),
    BLSize(  0.00001,  0.00001),
    BLSize(0.0000001,0.0000001),
    BLSize( -0.00001,       sh),
    BLSize(       sw, -0.00001),
    BLSize( -0.00001, -0.00001),
    BLSize(       sw,     1e40),
    BLSize(       sw,     1e80),
    BLSize(       sw,    1e120),
    BLSize(       sw,    1e160),
    BLSize(       sw,    1e200),
    BLSize(     1e40,       sh),
    BLSize(     1e80,       sh),
    BLSize(    1e120,       sh),
    BLSize(    1e160,       sh),
    BLSize(    1e200,       sh),
    BLSize(     kInf,       sh),
    BLSize(     kInf,       sh),
    BLSize(     kInf,     kInf),
    BLSize(       sw,    -kInf),
    BLSize(    -kInf,       sh),
    BLSize(    -kInf,    -kInf),
    BLSize(       sw,     kNaN),
    BLSize(     kNaN,       sh),
    BLSize(     kNaN,     kNaN)
  };

  TEST_LOG("Testing fill clipping");

  ctx.clear_all();

  for (size_t i = 0; i < BL_ARRAY_SIZE(matrix_data); i++) {
    ctx.set_transform(matrix_data[i]);
    ctx.set_fill_style(BLRgba32(0xFFFFFFFF));

    for (size_t j = 0; j < BL_ARRAY_SIZE(pointi_data); j++) {
      for (size_t k = 0; k < BL_ARRAY_SIZE(sizei_data); k++) {
        ctx.fill_rect(BLRectI(pointi_data[j].x, pointi_data[j].y, sizei_data[k].w, sizei_data[k].h));
      }
    }

    for (size_t j = 0; j < BL_ARRAY_SIZE(pointd_data); j++) {
      for (size_t k = 0; k < BL_ARRAY_SIZE(sized_data); k++) {
        ctx.fill_rect(BLRect(pointd_data[j].x, pointd_data[j].y, sized_data[k].w, sized_data[k].h));
      }
    }
  }

  TEST_LOG("Testing blit clipping");

  ctx.clear_all();

  for (size_t i = 0; i < BL_ARRAY_SIZE(matrix_data); i++) {
    ctx.set_transform(matrix_data[i]);

    for (size_t j = 0; j < BL_ARRAY_SIZE(pointi_data); j++) {
      ctx.blit_image(pointi_data[j], sprite);
      for (size_t k = 0; k < BL_ARRAY_SIZE(sizei_data); k++) {
        ctx.blit_image(BLRectI(pointi_data[j].x, pointi_data[j].y, sizei_data[k].w, sizei_data[k].h), sprite);
      }
    }

    for (size_t j = 0; j < BL_ARRAY_SIZE(pointd_data); j++) {
      ctx.blit_image(pointd_data[j], sprite);
      for (size_t k = 0; k < BL_ARRAY_SIZE(sized_data); k++) {
        ctx.blit_image(BLRect(pointd_data[j].x, pointd_data[j].y, sized_data[k].w, sized_data[k].h), sprite);
      }
    }
  }
}

TEST_CASE(core_context) {
  BLImage img(256, 256, BL_FORMAT_PRGB32);
  BLContext ctx(img);

  test_context_state(ctx);
  test_context_blit_fill_clip(ctx);
}

TEST_CASE(core_context_mask_lifetime) {
  uint8_t pixels[64];
  memset(pixels, 0xFF, sizeof(pixels));

  auto destroy_func = []([[maybe_unused]] void* impl_ptr, [[maybe_unused]] void* external_data, void* user_data) noexcept {
    ++*static_cast<uint32_t*>(user_data);
  };

  BLGradient gradient;
  EXPECT_OK(gradient.create(BLLinearGradientValues(0.0, 0.0, 8.0, 8.0)));
  EXPECT_OK(gradient.add_stop(0.0, BLRgba32(0xFFFF0000u)));
  EXPECT_OK(gradient.add_stop(1.0, BLRgba32(0xFF0000FFu)));

  for (uint32_t thread_count = 0; thread_count <= 2; thread_count++) {
    BLContextCreateInfo create_info{};
    create_info.thread_count = thread_count;
    create_info.command_queue_limit = 1024;

    for (uint32_t read_only = 0; read_only <= 1; read_only++) {
      BLDataAccessMode access_mode = read_only ? BL_DATA_ACCESS_MODE_RO : BL_DATA_ACCESS_MODE_RW;

      for (uint32_t use_gradient = 0; use_gradient <= 1; use_gradient++) {
        TEST_LOG("Testing mask lifetime (thread_count=%u, read_only=%u, gradient=%u)", thread_count, read_only, use_gradient);

        // More than 256 commands exercise both multiple cleanup bit words and linked command queues.
        {
          uint32_t destroy_count = 0;
          BLImage target(16, 16, BL_FORMAT_PRGB32);
          BLImage mask;
          EXPECT_OK(mask.create_from_data(8, 8, BL_FORMAT_A8, pixels, 8, access_mode, destroy_func, &destroy_count));
          size_t initial_ref_count = ObjectLayer::get_impl_ref_count(ObjectLayer::get_impl(&mask));

          BLContext ctx;
          EXPECT_OK(ctx.begin(target, create_info));
          if (use_gradient)
            EXPECT_OK(ctx.set_fill_style(gradient));
          else
            EXPECT_OK(ctx.set_fill_style(BLRgba32(0xFFFFFFFFu)));

          for (uint32_t batch = 0; batch < 2; batch++) {
            for (uint32_t i = 0; i < 257; i++) {
              if (i & 1)
                EXPECT_OK(ctx.fill_mask(BLPoint(0.0, 0.0), mask));
              else
                EXPECT_OK(ctx.fill_mask(BLPointI(0, 0), mask));
            }
            EXPECT_OK(ctx.flush(BL_CONTEXT_FLUSH_SYNC));
            EXPECT_EQ(ObjectLayer::get_impl_ref_count(ObjectLayer::get_impl(&mask)), initial_ref_count);
            EXPECT_EQ(destroy_count, 0u);
          }

          EXPECT_OK(ctx.end());
          EXPECT_EQ(ObjectLayer::get_impl_ref_count(ObjectLayer::get_impl(&mask)), initial_ref_count);
          EXPECT_OK(mask.reset());
          EXPECT_EQ(destroy_count, 1u);
        }

        // The caller can release the mask before an explicit flush, end(), or context destruction.
        for (uint32_t finish_mode = 0; finish_mode < 3; finish_mode++) {
          uint32_t destroy_count = 0;
          BLImage target(16, 16, BL_FORMAT_PRGB32);
          {
            BLImage mask;
            EXPECT_OK(mask.create_from_data(8, 8, BL_FORMAT_A8, pixels, 8, access_mode, destroy_func, &destroy_count));
            BLContext ctx;
            EXPECT_OK(ctx.begin(target, create_info));
            if (use_gradient)
              EXPECT_OK(ctx.set_fill_style(gradient));
            else
              EXPECT_OK(ctx.set_fill_style(BLRgba32(0xFFFFFFFFu)));

            for (uint32_t i = 0; i < 3; i++)
              EXPECT_OK(ctx.fill_mask(BLPointI(0, 0), mask));
            EXPECT_OK(mask.reset());
            EXPECT_EQ(destroy_count, thread_count ? 0u : 1u);

            if (finish_mode == 0)
              EXPECT_OK(ctx.flush(BL_CONTEXT_FLUSH_SYNC));
            else if (finish_mode == 1)
              EXPECT_OK(ctx.end());

            if (finish_mode != 2)
              EXPECT_EQ(destroy_count, 1u);
          }
          EXPECT_EQ(destroy_count, 1u);

          BLImageData target_data;
          EXPECT_OK(target.get_data(&target_data));
          EXPECT_EQ(*static_cast<const uint32_t*>(target_data.pixel_data) >> 24, 0xFFu);
        }
      }
    }
  }

  // Releasing the last mask reference must defer destruction while another context is still writing to it.
  for (uint32_t thread_count = 1; thread_count <= 2; thread_count++) {
    uint32_t destroy_count = 0;
    BLImage target(16, 16, BL_FORMAT_PRGB32);
    BLImage mask;
    EXPECT_OK(mask.create_from_data_mut(8, 8, BL_FORMAT_A8, pixels, 8, destroy_func, &destroy_count));
    BLContext mask_writer(mask);
    EXPECT_OK(mask_writer.fill_all(BLRgba32(0xFFFFFFFFu)));

    BLContextCreateInfo create_info{};
    create_info.thread_count = thread_count;
    BLContext ctx;
    EXPECT_OK(ctx.begin(target, create_info));
    EXPECT_OK(ctx.fill_mask(BLPointI(0, 0), mask, BLRgba32(0xFFFFFFFFu)));
    EXPECT_OK(mask.reset());
    EXPECT_EQ(destroy_count, 0u);
    EXPECT_OK(ctx.end());
    EXPECT_EQ(destroy_count, 0u);
    EXPECT_OK(mask_writer.end());
    EXPECT_EQ(destroy_count, 1u);
  }
}

} // {bl::ContextTests}

#endif // BL_TEST
