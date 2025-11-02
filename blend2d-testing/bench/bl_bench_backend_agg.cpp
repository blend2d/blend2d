// This file is part of Blend2D project <https://blend2d.com>
//
// See LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifdef BL_BENCH_ENABLE_AGG

#include "bl_bench_app.h"
#include "bl_bench_backend.h"

#include <algorithm>
#include <cmath>

#include "agg_basics.h"
#include "agg_bezier_arc.h"
#include "agg_conv_curve.h"
#include "agg_conv_stroke.h"
#include "agg_conv_transform.h"
#include "agg_font_cache_manager.h"
#include "agg_gamma_functions.h"
#include "agg_image_accessors.h"
#include "agg_path_storage.h"
#include "agg_pixfmt_rgba.h"
#include "agg_rasterizer_scanline_aa.h"
#include "agg_rasterizer_scanline_aa_nogamma.h"
#include "agg_renderer_base.h"
#include "agg_renderer_scanline.h"
#include "agg_rendering_buffer.h"
#include "agg_rounded_rect.h"
#include "agg_scanline_u.h"
#include "agg_span_allocator.h"
#include "agg_span_converter.h"
#include "agg_span_gradient.h"
#include "agg_span_image_filter_rgba.h"
#include "agg_span_interpolator_linear.h"
#include "agg_trans_affine.h"
#include "agg_trans_viewport.h"

// AGG Benchmarking - Agg2D Abstraction
// ====================================

// Agg2D - Version 1.0 (with modifications)
// Based on Anti-Grain Geometry
// Copyright (C) 2005 Maxim Shemanarev (http://www.antigrain.com)
//
// Permission to copy, use, modify, sell and distribute this software
// is granted provided this copyright notice appears in all copies.
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.
//
// Modifications:
//   25 Jan 2007 - Ported to AGG 2.4 Jerry Evans (jerry@novadsp.com)
//   21 June 2023 - Modified to only provide the required API for benchmarks
static const double g_approxScale = 2.0;

class Agg2D {
  typedef agg::order_bgra ComponentOrder;

  typedef agg::rgba8 ColorType;
  typedef agg::blender_rgba_pre<ColorType, ComponentOrder> BlenderPre;
  typedef agg::comp_op_adaptor_rgba_pre<ColorType, ComponentOrder> BlenderCompPre;

  typedef agg::pixfmt_bgra32_pre PixFormatPre;
  typedef agg::pixfmt_custom_blend_rgba<BlenderCompPre,agg::rendering_buffer> PixFormatCompPre;

  typedef agg::renderer_base<PixFormatPre> RendererBasePre;
  typedef agg::renderer_base<PixFormatCompPre> RendererBaseCompPre;

  typedef agg::renderer_scanline_aa_solid<RendererBasePre> RendererSolid;
  typedef agg::renderer_scanline_aa_solid<RendererBaseCompPre> RendererSolidComp;

  typedef agg::span_allocator<ColorType> SpanAllocator;
  typedef agg::pod_auto_array<ColorType, 256> GradientArray;

  typedef agg::span_gradient<ColorType, agg::span_interpolator_linear<>, agg::gradient_x,      GradientArray> LinearGradientSpan;
  typedef agg::span_gradient<ColorType, agg::span_interpolator_linear<>, agg::gradient_circle, GradientArray> RadialGradientSpan;
  typedef agg::span_gradient<ColorType, agg::span_interpolator_linear<>, agg::gradient_conic , GradientArray> ConicGradientSpan;

  typedef agg::conv_curve<agg::path_storage> ConvCurve;
  typedef agg::conv_stroke<ConvCurve> ConvStroke;
  typedef agg::conv_transform<ConvCurve> PathTransform;
  typedef agg::conv_transform<ConvStroke> StrokeTransform;

  enum StyleFlag {
    None,
    Solid,
    Linear,
    Radial
  };

public:
  friend class Agg2DRenderer;

  typedef ColorType         Color;
  typedef agg::rect_i       Rect;
  typedef agg::rect_d       RectD;
  typedef agg::trans_affine Affine;

  enum StyleSlot {
    FillSlot,
    StrokeSlot
  };

  enum LineJoin {
    JoinMiter = agg::miter_join,
    JoinRound = agg::round_join,
    JoinBevel = agg::bevel_join
  };

  enum LineCap {
    CapButt   = agg::butt_cap,
    CapSquare = agg::square_cap,
    CapRound  = agg::round_cap
  };

  enum DrawPathFlag {
    FillOnly,
    StrokeOnly,
    FillAndStroke,
    FillWithLineColor
  };

  enum ImageFilter {
    NoFilter,
    Bilinear
  };

  enum ImageResample {
    NoResample,
    ResampleAlways,
    ResampleOnZoomOut
  };

  enum BlendMode {
    BlendAlpha      = agg::end_of_comp_op_e,
    BlendClear      = agg::comp_op_clear,
    BlendSrc        = agg::comp_op_src,
    BlendDst        = agg::comp_op_dst,
    BlendSrcOver    = agg::comp_op_src_over,
    BlendDstOver    = agg::comp_op_dst_over,
    BlendSrcIn      = agg::comp_op_src_in,
    BlendDstIn      = agg::comp_op_dst_in,
    BlendSrcOut     = agg::comp_op_src_out,
    BlendDstOut     = agg::comp_op_dst_out,
    BlendSrcAtop    = agg::comp_op_src_atop,
    BlendDstAtop    = agg::comp_op_dst_atop,
    BlendXor        = agg::comp_op_xor,
    BlendAdd        = agg::comp_op_plus,
    BlendMultiply   = agg::comp_op_multiply,
    BlendScreen     = agg::comp_op_screen,
    BlendOverlay    = agg::comp_op_overlay,
    BlendDarken     = agg::comp_op_darken,
    BlendLighten    = agg::comp_op_lighten,
    BlendColorDodge = agg::comp_op_color_dodge,
    BlendColorBurn  = agg::comp_op_color_burn,
    BlendHardLight  = agg::comp_op_hard_light,
    BlendSoftLight  = agg::comp_op_soft_light,
    BlendDifference = agg::comp_op_difference,
    BlendExclusion  = agg::comp_op_exclusion
  };

  enum Direction {
    CW, CCW
  };

  struct Transformations {
    double affineMatrix[6];
  };

  struct Image {
    agg::rendering_buffer renBuf;

    Image() {}
    Image(unsigned char* buf, unsigned width, unsigned height, int stride) :
        renBuf(buf, width, height, stride) {}

    void attach(unsigned char* buf, unsigned width, unsigned height, int stride) {
      renBuf.attach(buf, width, height, stride);
    }

    int width() const { return renBuf.width(); }
    int height() const { return renBuf.height(); }
  };

private:
  agg::rendering_buffer           m_rbuf;
  PixFormatPre                    m_pixFormatPre;
  PixFormatCompPre                m_pixFormatCompPre;
  RendererBasePre                 m_renBasePre;
  RendererBaseCompPre             m_renBaseCompPre;
  RendererSolid                   m_renSolid;
  RendererSolidComp               m_renSolidComp;

  SpanAllocator                   m_allocator;
  RectD                           m_clipBox;

  BlendMode                       m_blendMode;
  BlendMode                       m_imageBlendMode;
  Color                           m_imageBlendColor;

  agg::scanline_u8                m_scanline;
  agg::rasterizer_scanline_aa_nogamma<> m_rasterizer;
  double                          m_masterAlpha;

  Color                           m_color[2];
  GradientArray                   m_gradient[2];

  LineCap                         m_lineCap;
  LineJoin                        m_lineJoin;

  StyleFlag                       m_styleFlag[2];
  agg::trans_affine               m_gradientMatrix[2];
  double                          m_gradientD1[2];
  double                          m_gradientD2[2];

  ImageFilter                     m_imageFilter;
  ImageResample                   m_imageResample;
  agg::image_filter_lut           m_imageFilterLut;

  agg::span_interpolator_linear<> m_gradientInterpolator[2];

  agg::gradient_x                 m_linearGradientFunction;
  agg::gradient_circle            m_radialGradientFunction;

  double                          m_lineWidth;
  bool                            m_evenOddFlag;

  agg::path_storage               m_path;
  agg::trans_affine               m_transform;

  ConvCurve                       m_convCurve;
  ConvStroke                      m_convStroke;

  PathTransform                   m_pathTransform;
  StrokeTransform                 m_strokeTransform;

public:
  Agg2D() :
    m_rbuf(),
    m_pixFormatPre(m_rbuf),
    m_pixFormatCompPre(m_rbuf),
    m_renBasePre(m_pixFormatPre),
    m_renBaseCompPre(m_pixFormatCompPre),
    m_renSolid(m_renBasePre),
    m_renSolidComp(m_renBaseCompPre),

    m_allocator(),
    m_clipBox(0,0,0,0),

    m_blendMode(BlendSrcOver),
    m_imageBlendMode(BlendDst),
    m_imageBlendColor(0,0,0),

    m_scanline(),
    m_rasterizer(),

    m_masterAlpha(1.0),

    m_color{},
    m_gradient{},

    m_lineCap(CapRound),
    m_lineJoin(JoinRound),

    m_styleFlag{Solid, Solid},
    m_gradientMatrix{},
    m_gradientD1{0.0, 0.0},
    m_gradientD2{100.0, 100.0},

    m_imageFilter(Bilinear),
    m_imageResample(NoResample),
    m_imageFilterLut(agg::image_filter_bilinear(), true),

    m_gradientInterpolator{m_gradientMatrix[0], m_gradientMatrix[1]},
    m_linearGradientFunction(),
    m_radialGradientFunction(),

    m_lineWidth(1),
    m_evenOddFlag(false),

    m_path(),
    m_transform(),

    m_convCurve(m_path),
    m_convStroke(m_convCurve),

    m_pathTransform(m_convCurve, m_transform),
    m_strokeTransform(m_convStroke, m_transform)
  {
    lineCap(m_lineCap);
    lineJoin(m_lineJoin);
  }

  ~Agg2D() {}

  // Setup
  // -----

  void attach(unsigned char* buf, unsigned width, unsigned height, int stride) {
    m_rbuf.attach(buf, width, height, stride);
    m_renBasePre.reset_clipping(true);
    m_renBaseCompPre.reset_clipping(true);

    resetTransformations();
    lineWidth(1.0),
    lineColor(0,0,0);
    fillColor(255,255,255);
    clipBox(0, 0, width, height);
    lineCap(CapRound);
    lineJoin(JoinRound);
    imageFilter(Bilinear);
    imageResample(NoResample);
    m_masterAlpha = 1.0;
    m_blendMode = BlendSrcOver;
  }

  void attach(Image& img) {
    attach(img.renBuf.buf(), img.renBuf.width(), img.renBuf.height(), img.renBuf.stride());
  }

  void clipBox(double x1, double y1, double x2, double y2) {
    m_clipBox = RectD(x1, y1, x2, y2);
    int rx1 = int(x1);
    int ry1 = int(y1);
    int rx2 = int(x2);
    int ry2 = int(y2);

    m_renBasePre.clip_box(rx1, ry1, rx2, ry2);
    m_renBaseCompPre.clip_box(rx1, ry1, rx2, ry2);

    m_rasterizer.clip_box(x1, y1, x2, y2);
  }

  RectD clipBox() const { return m_clipBox; }

  void clearAll(Color c) {
    c.premultiply();
    m_renBasePre.fill(c);
  }

  void clearAll(unsigned r, unsigned g, unsigned b, unsigned a) {
    clearAll(Color(r, g, b, a));
  }

  // Conversions
  // -----------

  void worldToScreen(double& x, double& y) const { m_transform.transform(&x, &y); }
  void screenToWorld(double& x, double& y) const { m_transform.inverse_transform(&x, &y); }

  double worldToScreen(double scalar) const {
    double x1 = 0, x2 = scalar;
    double y1 = 0, y2 = scalar;
    worldToScreen(x1, y1);
    worldToScreen(x2, y2);
    return std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)) * 0.7071068;
  }

  double screenToWorld(double scalar) const {
    double x1 = 0, x2 = scalar;
    double y1 = 0, y2 = scalar;
    screenToWorld(x1, y1);
    screenToWorld(x2, y2);
    return std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)) * 0.7071068;
  }

  void alignPoint(double& x, double& y) const {
    worldToScreen(x, y);
    x = std::floor(x) + 0.5;
    y = std::floor(y) + 0.5;
    screenToWorld(x, y);
  }

  bool inBox(double worldX, double worldY) const {
    worldToScreen(worldX, worldY);
    return m_renBasePre.inbox(int(worldX), int(worldY));
  }

  // General Attributes
  // ------------------

  void blendMode(BlendMode m) {
    m_blendMode = m;
    m_pixFormatCompPre.comp_op(m);
  }
  BlendMode blendMode() const { return m_blendMode; }

  void imageBlendMode(BlendMode m) { m_imageBlendMode = m; }
  BlendMode imageBlendMode() const { return m_imageBlendMode; }

  void imageBlendColor(Color c) { m_imageBlendColor = c; }
  void imageBlendColor(unsigned r, unsigned g, unsigned b, unsigned a) { imageBlendColor(Color(r, g, b, a)); }
  Color imageBlendColor() const { return m_imageBlendColor; }

  void masterAlpha(double a) { m_masterAlpha = a; }
  double masterAlpha() const { return m_masterAlpha; }

  void fillColor(Color c) {
    m_color[FillSlot] = c;
    m_color[FillSlot].premultiply();
    m_styleFlag[FillSlot] = Solid;
  }

  void lineColor(Color c) {
    m_color[StrokeSlot] = c;
    m_color[StrokeSlot].premultiply();
    m_styleFlag[StrokeSlot] = Solid;
  }

  void fillColor(unsigned r, unsigned g, unsigned b, unsigned a = 0xFFu) { fillColor(Color(r, g, b, a)); }
  void lineColor(unsigned r, unsigned g, unsigned b, unsigned a = 0xFFu) { lineColor(Color(r, g, b, a)); }

  void noFill() {
    m_color[FillSlot] = Color(0, 0, 0, 0);
    m_styleFlag[FillSlot] = None;
  }

  void noLine() {
    m_color[StrokeSlot] = Color(0, 0, 0, 0);
    m_styleFlag[StrokeSlot] = None;
  }

  void setLinearGradient(StyleSlot slot, double x1, double y1, double x2, double y2, Color c1, Color c2, Color c3) {
    int i;

    for (i = 0; i < 128; i++) {
      m_gradient[slot][i] = c1.gradient(c2, double(i) / 127.0);
      m_gradient[slot][i].premultiply();
    }

    for (; i < 256; i++) {
      m_gradient[slot][i] = c2.gradient(c3, double(i - 128) / 127.0);
      m_gradient[slot][i].premultiply();
    }

    double angle = std::atan2(y2-y1, x2-x1);
    m_gradientMatrix[slot].reset();
    m_gradientMatrix[slot] *= agg::trans_affine_rotation(angle);
    m_gradientMatrix[slot] *= agg::trans_affine_translation(x1, y1);
    m_gradientMatrix[slot] *= m_transform;
    m_gradientMatrix[slot].invert();
    m_gradientD1[slot] = 0.0;
    m_gradientD2[slot] = std::sqrt((x2-x1) * (x2-x1) + (y2-y1) * (y2-y1));
    m_styleFlag[slot] = Linear;
    m_color[slot] = Color(0,0,0);  // Set some real color
  }

  void fillLinearGradient(double x1, double y1, double x2, double y2, Color c1, Color c2, Color c3) {
    setLinearGradient(FillSlot, x1, y1, x2, y2, c1, c2, c3);
  }

  void lineLinearGradient(double x1, double y1, double x2, double y2, Color c1, Color c2, Color c3) {
    setLinearGradient(StrokeSlot, x1, y1, x2, y2, c1, c2, c3);
  }

  void setRadialGradient(StyleSlot slot, double x, double y, double r, Color c1, Color c2, Color c3) {
    int i;

    for (i = 0; i < 128; i++) {
      m_gradient[slot][i] = c1.gradient(c2, double(i) / 127.0);
      m_gradient[slot][i].premultiply();
    }

    for (; i < 256; i++) {
      m_gradient[slot][i] = c2.gradient(c3, double(i - 128) / 127.0);
      m_gradient[slot][i].premultiply();
    }

    m_gradientD2[slot] = worldToScreen(r);
    worldToScreen(x, y);
    m_gradientMatrix[slot].reset();
    m_gradientMatrix[slot] *= agg::trans_affine_translation(x, y);
    m_gradientMatrix[slot].invert();
    m_gradientD1[slot] = 0;
    m_styleFlag[slot] = Radial;
    m_color[slot] = Color(0,0,0);  // Set some real color
  }

  void fillRadialGradient(double x, double y, double r, Color c1, Color c2, Color c3) {
    setRadialGradient(FillSlot, x, y, r, c1, c2, c3);
  }

  void lineRadialGradient(double x, double y, double r, Color c1, Color c2, Color c3) {
    setRadialGradient(StrokeSlot, x, y, r, c1, c2, c3);
  }

  void lineWidth(double w) {
      m_lineWidth = w;
      m_convStroke.width(w);
  }

  void fillEvenOdd(bool evenOddFlag) {
    m_evenOddFlag = evenOddFlag;
    m_rasterizer.filling_rule(evenOddFlag ? agg::fill_even_odd : agg::fill_non_zero);
  }

  void lineCap(LineCap cap) {
      m_lineCap = cap;
      m_convStroke.line_cap((agg::line_cap_e)cap);
  }

  void lineJoin(LineJoin join) {
    m_lineJoin = join;
    m_convStroke.line_join((agg::line_join_e)join);
  }

  double lineWidth(double w) const { return m_lineWidth; }
  bool fillEvenOdd() const { return m_evenOddFlag; }
  LineCap lineCap() const { return m_lineCap; }
  LineJoin lineJoin() const { return m_lineJoin; }

  // Transformations
  // ---------------

  Transformations transformations() const {
    Transformations tr;
    m_transform.store_to(tr.affineMatrix);
    return tr;
  }

  void transformations(const Transformations& tr) {
    m_transform.load_from(tr.affineMatrix);
    m_convCurve.approximation_scale(worldToScreen(1.0) * g_approxScale);
    m_convStroke.approximation_scale(worldToScreen(1.0) * g_approxScale);
  }

  void resetTransformations() {
    m_transform.reset();
  }

  void rotate(double angle) { m_transform *= agg::trans_affine_rotation(angle); }
  void skew(double sx, double sy) { m_transform *= agg::trans_affine_skewing(sx, sy); }
  void translate(double x, double y) { m_transform *= agg::trans_affine_translation(x, y); }

  void affine(const Affine& tr) {
    m_transform *= tr;
    m_convCurve.approximation_scale(worldToScreen(1.0) * g_approxScale);
    m_convStroke.approximation_scale(worldToScreen(1.0) * g_approxScale);
  }

  void affine(const Transformations& tr) {
    affine(agg::trans_affine(tr.affineMatrix[0], tr.affineMatrix[1], tr.affineMatrix[2],
                            tr.affineMatrix[3], tr.affineMatrix[4], tr.affineMatrix[5]));
  }

  void scale(double sx, double sy) {
    m_transform *= agg::trans_affine_scaling(sx, sy);
    m_convCurve.approximation_scale(worldToScreen(1.0) * g_approxScale);
    m_convStroke.approximation_scale(worldToScreen(1.0) * g_approxScale);
  }

  // Basic Shapes
  // ------------

  void fillRectangleI(int x1, int y1, int x2, int y2, Color color);

  void line(double x1, double y1, double x2, double y2);
  void triangle(double x1, double y1, double x2, double y2, double x3, double y3);
  void rectangle(double x1, double y1, double x2, double y2);
  void roundedRect(double x1, double y1, double x2, double y2, double r);
  void roundedRect(double x1, double y1, double x2, double y2, double rx, double ry);
  void roundedRect(double x1, double y1, double x2, double y2,
                    double rxBottom, double ryBottom,
                    double rxTop,    double ryTop);
  void ellipse(double cx, double cy, double rx, double ry);
  void arc(double cx, double cy, double rx, double ry, double start, double sweep);
  void star(double cx, double cy, double r1, double r2, double startAngle, int numRays);
  void curve(double x1, double y1, double x2, double y2, double x3, double y3);
  void curve(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4);
  void polygon(double* xy, int numPoints);
  void polyline(double* xy, int numPoints);


  // Path commands
  // -------------

  void resetPath() { m_path.remove_all(); }
  void moveTo(double x, double y) { m_path.move_to(x, y); }
  void moveRel(double dx, double dy) { m_path.move_rel(dx, dy); }
  void lineTo(double x, double y) { m_path.line_to(x, y); }
  void lineRel(double dx, double dy) { m_path.line_rel(dx, dy); }
  void horLineTo(double x) { m_path.hline_to(x); }
  void horLineRel(double dx) { m_path.hline_rel(dx); }
  void verLineTo(double y) { m_path.vline_to(y); }
  void verLineRel(double dy) { m_path.vline_rel(dy); }
  void arcTo(double rx, double ry, double angle, bool laFlag, bool sweepFlag, double x, double y) { m_path.arc_to(rx, ry, angle, laFlag, sweepFlag, x, y); }
  void arcRel(double rx, double ry, double angle, bool laFlag, bool sweepFlag, double dx, double dy) { m_path.arc_rel(rx, ry, angle, laFlag, sweepFlag, dx, dy); }
  void quadricCurveTo(double x0, double y0, double x1, double y1) { m_path.curve3(x0, y0, x1, y1); }
  void quadricCurveRel(double x0, double y0, double x1, double y1) { m_path.curve3_rel(x0, y0, x1, y1); }
  void quadricCurveTo(double x1, double y1) { m_path.curve3(x1, y1); }
  void quadricCurveRel(double x1, double y1) { m_path.curve3_rel(x1, y1); }
  void cubicCurveTo(double x0, double y0, double x1, double y1, double x2, double y2) { m_path.curve4(x0, y0, x1, y1, x2, y2); }
  void cubicCurveRel(double x0, double y0, double x1, double y1, double x2, double y2) { m_path.curve4_rel(x0, y0, x1, y1, x2, y2); }
  void cubicCurveTo(double x1, double y1, double x2, double y2) { m_path.curve4(x1, y1, x2, y2); }
  void cubicCurveRel(double x1, double y1, double x2, double y2) { m_path.curve4_rel(x1, y1, x2, y2); }

  void addEllipse(double cx, double cy, double rx, double ry, Direction dir) {
    agg::bezier_arc arc(cx, cy, rx, ry, 0, (dir == CCW) ? 2*pi() : -2*pi());
    m_path.concat_path(arc,0);
    m_path.close_polygon();
  }

  void closePolygon() { m_path.close_polygon(); }

  void drawPath(DrawPathFlag flag = FillAndStroke);
  void drawPathNoTransform(DrawPathFlag flag = FillAndStroke);


  // Image Transformations
  // ---------------------

  void imageFilter(ImageFilter f);
  ImageFilter imageFilter() const { return m_imageFilter; }

  void imageResample(ImageResample f) { m_imageResample = f; }
  ImageResample imageResample() const { return m_imageResample; }

  void transformImage(const Image& img,
    int imgX1, int imgY1, int imgX2, int imgY2,
    double dstX1, double dstY1, double dstX2, double dstY2);

  void transformImage(const Image& img,
    double dstX1, double dstY1, double dstX2, double dstY2);

  void transformImage(const Image& img,
    int imgX1, int imgY1, int imgX2, int imgY2,
    const double* parallelogram);

  void transformImage(const Image& img, const double* parallelogram);

  void transformImagePath(const Image& img,
    int imgX1,    int imgY1,    int imgX2,    int imgY2,
    double dstX1, double dstY1, double dstX2, double dstY2);

  void transformImagePath(const Image& img,
    double dstX1, double dstY1, double dstX2, double dstY2);

  void transformImagePath(const Image& img,
    int imgX1, int imgY1, int imgX2, int imgY2,
    const double* parallelogram);

  void transformImagePath(const Image& img, const double* parallelogram);

  // Image Blending (no transformations available)
  void blendImage(Image& img,
    int imgX1, int imgY1, int imgX2, int imgY2,
    double dstX, double dstY, unsigned alpha=255);
  void blendImage(Image& img, double dstX, double dstY, unsigned alpha=255);

  // Copy image directly, together with alpha-channel
  void copyImage(Image& img,
    int imgX1, int imgY1, int imgX2, int imgY2,
    double dstX, double dstY);
  void copyImage(Image& img, double dstX, double dstY);

  // Auxiliary
  // ---------

  static inline double pi() { return agg::pi; }
  static inline double deg2Rad(double v) { return v * agg::pi / 180.0; }
  static inline double rad2Deg(double v) { return v * 180.0 / agg::pi; }

private:
  void render(StyleSlot slot);
  void addLine(double x1, double y1, double x2, double y2);
  void renderImage(const Image& img, int x1, int y1, int x2, int y2, const double* parl);
};

inline bool operator==(const Agg2D::Color& c1, const Agg2D::Color& c2) { return c1.r == c2.r && c1.g == c2.g && c1.b == c2.b && c1.a == c2.a; }
inline bool operator!=(const Agg2D::Color& c1, const Agg2D::Color& c2) { return !(c1 == c2); }

void Agg2D::addLine(double x1, double y1, double x2, double y2) {
  m_path.move_to(x1, y1);
  m_path.line_to(x2, y2);
}

void Agg2D::fillRectangleI(int x1, int y1, int x2, int y2, Color color) {
  color.premultiply();

  if (m_blendMode == BlendSrc) {
    m_renBasePre.copy_bar(x1, y1, x2, y2, color);
  }
  else if (m_blendMode == BlendSrcOver) {
    m_renBasePre.blend_bar(x1, y1, x2, y2, color, 0xFF);
  }
  else {
    m_renBaseCompPre.blend_bar(x1, y1, x2, y2, color, 0xFF);
  }
}

void Agg2D::line(double x1, double y1, double x2, double y2) {
  m_path.remove_all();
  addLine(x1, y1, x2, y2);
  drawPath(StrokeOnly);
}

void Agg2D::triangle(double x1, double y1, double x2, double y2, double x3, double y3) {
  m_path.remove_all();
  m_path.move_to(x1, y1);
  m_path.line_to(x2, y2);
  m_path.line_to(x3, y3);
  m_path.close_polygon();
  drawPath(FillAndStroke);
}

void Agg2D::rectangle(double x1, double y1, double x2, double y2) {
  m_path.remove_all();
  m_path.move_to(x1, y1);
  m_path.line_to(x2, y1);
  m_path.line_to(x2, y2);
  m_path.line_to(x1, y2);
  m_path.close_polygon();
  drawPath(FillAndStroke);
}

void Agg2D::roundedRect(double x1, double y1, double x2, double y2, double r) {
  m_path.remove_all();
  agg::rounded_rect rc(x1, y1, x2, y2, r);
  rc.normalize_radius();
  rc.approximation_scale(worldToScreen(1.0) * g_approxScale);
  m_path.concat_path(rc,0);
  drawPath(FillAndStroke);
}

void Agg2D::roundedRect(double x1, double y1, double x2, double y2, double rx, double ry) {
  m_path.remove_all();
  agg::rounded_rect rc;
  rc.rect(x1, y1, x2, y2);
  rc.radius(rx, ry);
  rc.normalize_radius();
  m_path.concat_path(rc,0);
  drawPath(FillAndStroke);
}

void Agg2D::roundedRect(double x1, double y1, double x2, double y2, double rx_bottom, double ry_bottom, double rx_top, double ry_top) {
  m_path.remove_all();
  agg::rounded_rect rc;
  rc.rect(x1, y1, x2, y2);
  rc.radius(rx_bottom, ry_bottom, rx_top, ry_top);
  rc.normalize_radius();
  rc.approximation_scale(worldToScreen(1.0) * g_approxScale);
  m_path.concat_path(rc,0);
  drawPath(FillAndStroke);
}

void Agg2D::ellipse(double cx, double cy, double rx, double ry) {
  m_path.remove_all();
  agg::bezier_arc arc(cx, cy, rx, ry, 0, 2*pi());
  m_path.concat_path(arc,0);
  m_path.close_polygon();
  drawPath(FillAndStroke);
}

void Agg2D::arc(double cx, double cy, double rx, double ry, double start, double sweep) {
  m_path.remove_all();
  agg::bezier_arc arc(cx, cy, rx, ry, start, sweep);
  m_path.concat_path(arc,0);
  drawPath(StrokeOnly);
}

void Agg2D::star(double cx, double cy, double r1, double r2, double startAngle, int numRays) {
  m_path.remove_all();
  double da = agg::pi / double(numRays);
  double a = startAngle;
  for (int i = 0; i < numRays; i++) {
    double x = cos(a) * r2 + cx;
    double y = sin(a) * r2 + cy;
    if (i) m_path.line_to(x, y);
    else   m_path.move_to(x, y);
    a += da;
    m_path.line_to(cos(a) * r1 + cx, sin(a) * r1 + cy);
    a += da;
  }
  closePolygon();
  drawPath(FillAndStroke);
}

void Agg2D::curve(double x1, double y1, double x2, double y2, double x3, double y3) {
  m_path.remove_all();
  m_path.move_to(x1, y1);
  m_path.curve3(x2, y2, x3, y3);
  drawPath(StrokeOnly);
}

void Agg2D::curve(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4) {
  m_path.remove_all();
  m_path.move_to(x1, y1);
  m_path.curve4(x2, y2, x3, y3, x4, y4);
  drawPath(StrokeOnly);
}

void Agg2D::polygon(double* xy, int numPoints) {
  m_path.remove_all();
  m_path.concat_poly(xy,0,true);
  closePolygon();
  drawPath(FillAndStroke);
}

void Agg2D::polyline(double* xy, int numPoints) {
  m_path.remove_all();
  m_path.concat_poly(xy,0,true);
  drawPath(StrokeOnly);
}

void Agg2D::imageFilter(ImageFilter f) {
  m_imageFilter = f;
  switch(f) {
    case NoFilter: break;
    case Bilinear: m_imageFilterLut.calculate(agg::image_filter_bilinear(), true); break;
  }
}

void Agg2D::transformImage(const Image& img, int imgX1, int imgY1, int imgX2, int imgY2, double dstX1, double dstY1, double dstX2, double dstY2) {
  resetPath();
  moveTo(dstX1, dstY1);
  lineTo(dstX2, dstY1);
  lineTo(dstX2, dstY2);
  lineTo(dstX1, dstY2);
  closePolygon();
  double parallelogram[6] = { dstX1, dstY1, dstX2, dstY1, dstX2, dstY2 };
  renderImage(img, imgX1, imgY1, imgX2, imgY2, parallelogram);
}

void Agg2D::transformImage(const Image& img, double dstX1, double dstY1, double dstX2, double dstY2) {
  resetPath();
  moveTo(dstX1, dstY1);
  lineTo(dstX2, dstY1);
  lineTo(dstX2, dstY2);
  lineTo(dstX1, dstY2);
  closePolygon();
  double parallelogram[6] = { dstX1, dstY1, dstX2, dstY1, dstX2, dstY2 };
  renderImage(img, 0, 0, img.renBuf.width(), img.renBuf.height(), parallelogram);
}

void Agg2D::transformImage(const Image& img, int imgX1, int imgY1, int imgX2, int imgY2, const double* parallelogram) {
  resetPath();
  moveTo(parallelogram[0], parallelogram[1]);
  lineTo(parallelogram[2], parallelogram[3]);
  lineTo(parallelogram[4], parallelogram[5]);
  lineTo(parallelogram[0] + parallelogram[4] - parallelogram[2], parallelogram[1] + parallelogram[5] - parallelogram[3]);
  closePolygon();
  renderImage(img, imgX1, imgY1, imgX2, imgY2, parallelogram);
}


void Agg2D::transformImage(const Image& img, const double* parallelogram) {
  resetPath();
  moveTo(parallelogram[0], parallelogram[1]);
  lineTo(parallelogram[2], parallelogram[3]);
  lineTo(parallelogram[4], parallelogram[5]);
  lineTo(parallelogram[0] + parallelogram[4] - parallelogram[2], parallelogram[1] + parallelogram[5] - parallelogram[3]);
  closePolygon();
  renderImage(img, 0, 0, img.renBuf.width(), img.renBuf.height(), parallelogram);
}

void Agg2D::transformImagePath(const Image& img, int imgX1, int imgY1, int imgX2, int imgY2, double dstX1, double dstY1, double dstX2, double dstY2) {
  double parallelogram[6] = { dstX1, dstY1, dstX2, dstY1, dstX2, dstY2 };
  renderImage(img, imgX1, imgY1, imgX2, imgY2, parallelogram);
}

void Agg2D::transformImagePath(const Image& img, double dstX1, double dstY1, double dstX2, double dstY2) {
  double parallelogram[6] = { dstX1, dstY1, dstX2, dstY1, dstX2, dstY2 };
  renderImage(img, 0, 0, img.renBuf.width(), img.renBuf.height(), parallelogram);
}

void Agg2D::transformImagePath(const Image& img, int imgX1, int imgY1, int imgX2, int imgY2, const double* parallelogram) {
  renderImage(img, imgX1, imgY1, imgX2, imgY2, parallelogram);
}

void Agg2D::transformImagePath(const Image& img, const double* parallelogram) {
  renderImage(img, 0, 0, img.renBuf.width(), img.renBuf.height(), parallelogram);
}

void Agg2D::drawPath(DrawPathFlag flag) {
  m_rasterizer.reset();
  switch(flag) {
    case FillOnly:
      if (m_styleFlag[FillSlot] != None) {
        m_rasterizer.add_path(m_pathTransform);
        render(FillSlot);
      }
      break;

    case StrokeOnly:
      if (m_styleFlag[StrokeSlot] != None && m_lineWidth > 0.0) {
        m_rasterizer.add_path(m_strokeTransform);
        render(StrokeSlot);
      }
      break;

    case FillAndStroke:
      if (m_styleFlag[FillSlot] != None) {
        m_rasterizer.add_path(m_pathTransform);
        render(FillSlot);
      }

      if (m_styleFlag[StrokeSlot] != None && m_lineWidth > 0.0) {
        m_rasterizer.add_path(m_strokeTransform);
        render(StrokeSlot);
      }
      break;

    case FillWithLineColor:
      if (m_styleFlag[StrokeSlot] != None) {
        m_rasterizer.add_path(m_pathTransform);
        render(StrokeSlot);
      }
      break;
  }
}

class Agg2DRenderer {
public:
  template<class BaseRenderer, class SolidRenderer>
  void static render(Agg2D& gr, BaseRenderer& renBase, SolidRenderer& renSolid, Agg2D::StyleSlot slot) {
    typedef agg::span_allocator<agg::rgba8> span_allocator_type;
    typedef agg::renderer_scanline_aa<BaseRenderer, span_allocator_type, Agg2D::LinearGradientSpan> RendererLinearGradient;
    typedef agg::renderer_scanline_aa<BaseRenderer, span_allocator_type, Agg2D::RadialGradientSpan> RendererRadialGradient;

    switch (gr.m_styleFlag[slot]) {
      case Agg2D::None: {
        return;
      }

      case Agg2D::Solid: {
        renSolid.color(gr.m_color[slot]);
        agg::render_scanlines(gr.m_rasterizer, gr.m_scanline, renSolid);
        return;
      }

      case Agg2D::Linear: {
        Agg2D::LinearGradientSpan span(/*gr.m_allocator, */
                                       gr.m_gradientInterpolator[slot],
                                       gr.m_linearGradientFunction,
                                       gr.m_gradient[slot],
                                       gr.m_gradientD1[slot],
                                       gr.m_gradientD2[slot]);
        RendererLinearGradient ren(renBase,gr.m_allocator,span);
        agg::render_scanlines(gr.m_rasterizer, gr.m_scanline, ren);
        return;
      }

      case Agg2D::Radial: {
        Agg2D::RadialGradientSpan span(/*gr.m_allocator, */
                                       gr.m_gradientInterpolator[slot],
                                       gr.m_radialGradientFunction,
                                       gr.m_gradient[slot],
                                       gr.m_gradientD1[slot],
                                       gr.m_gradientD2[slot]);
        RendererRadialGradient ren(renBase,gr.m_allocator,span);
        agg::render_scanlines(gr.m_rasterizer, gr.m_scanline, ren);
        return;
      }
    }
  }

  class SpanConvImageBlend {
  private:
    Agg2D::BlendMode m_mode;
    Agg2D::Color m_color;

  public:
    SpanConvImageBlend(Agg2D::BlendMode m, Agg2D::Color c) :
        m_mode(m), m_color(c) {}

    void convert(Agg2D::Color* span, int x, int y, unsigned len) const {
      unsigned l2;
      Agg2D::Color* s2;
      if (m_mode != Agg2D::BlendDst) {
        l2 = len;
        s2 = span;
        typedef agg::comp_op_adaptor_clip_to_dst_rgba_pre<Agg2D::Color, agg::order_rgba> OpType;
        do {
          OpType::blend_pix(m_mode, (Agg2D::Color::value_type*)s2, m_color.r, m_color.g, m_color.b, Agg2D::Color::base_mask, agg::cover_full);
          ++s2;
        }
        while(--l2);
      }
      if (m_color.a < Agg2D::Color::base_mask) {
        l2 = len;
        s2 = span;
        unsigned a = m_color.a;
        do {
          s2->r = (s2->r * a) >> Agg2D::Color::base_shift;
          s2->g = (s2->g * a) >> Agg2D::Color::base_shift;
          s2->b = (s2->b * a) >> Agg2D::Color::base_shift;
          s2->a = (s2->a * a) >> Agg2D::Color::base_shift;
          ++s2;
        }
        while(--l2);
      }
    }
  };

  template<class BaseRenderer, class SolidRenderer, class Rasterizer, class Scanline>
  void static render(Agg2D& gr, BaseRenderer& renBase, SolidRenderer& renSolid, Rasterizer& ras, Scanline& sl) {
    typedef agg::span_allocator<agg::rgba8> span_allocator_type;
    typedef agg::renderer_scanline_aa<BaseRenderer,span_allocator_type,Agg2D::LinearGradientSpan> RendererLinearGradient;
    typedef agg::renderer_scanline_aa<BaseRenderer,span_allocator_type,Agg2D::RadialGradientSpan> RendererRadialGradient;

    int slot = 0;

    switch (gr.m_styleFlag[slot]) {
      case Agg2D::None: {
        return;
      }

      case Agg2D::Solid: {
        renSolid.color(gr.m_color[slot]);
        agg::render_scanlines(ras, sl, renSolid);
        return;
      }

      case Agg2D::Linear: {
        Agg2D::LinearGradientSpan span(gr.m_gradientInterpolator[slot],
                                       gr.m_linearGradientFunction,
                                       gr.m_gradient[slot],
                                       gr.m_gradientD1[slot],
                                       gr.m_gradientD2[slot]);
        RendererLinearGradient ren(renBase,gr.m_allocator,span);
        agg::render_scanlines(ras, sl, ren);
        return;
      }

      case Agg2D::Radial: {
        Agg2D::RadialGradientSpan span(gr.m_gradientInterpolator[slot],
                                       gr.m_radialGradientFunction,
                                       gr.m_gradient[slot],
                                       gr.m_gradientD1[slot],
                                       gr.m_gradientD2[slot]);
        RendererRadialGradient ren(renBase,gr.m_allocator,span);
        agg::render_scanlines(ras, sl, ren);
        return;
      }
    }
  }

  template<class BaseRenderer, class Interpolator>
  static void renderImage(Agg2D& gr, const Agg2D::Image& img, BaseRenderer& renBase, Interpolator& interpolator) {
    Agg2D::Image& imgc = const_cast<Agg2D::Image&>(img);
    Agg2D::PixFormatPre img_pixf(imgc.renBuf);
    typedef agg::image_accessor_clone<Agg2D::PixFormatPre> img_source_type;
    img_source_type source(img_pixf);

    SpanConvImageBlend blend(gr.m_imageBlendMode, gr.m_imageBlendColor);
    if (gr.m_imageFilter == Agg2D::NoFilter) {
      typedef agg::span_image_filter_rgba_nn<img_source_type,Interpolator> SpanGenType;
      typedef agg::span_converter<SpanGenType,SpanConvImageBlend> SpanConvType;
      typedef agg::renderer_scanline_aa<BaseRenderer,Agg2D::SpanAllocator,SpanGenType> RendererType;

      SpanGenType sg(source,interpolator);
      SpanConvType sc(sg, blend);
      RendererType ri(renBase,gr.m_allocator,sg);
      agg::render_scanlines(gr.m_rasterizer, gr.m_scanline, ri);
    }
    else {
      bool resample = (gr.m_imageResample == Agg2D::ResampleAlways);
      if (gr.m_imageResample == Agg2D::ResampleOnZoomOut) {
        double sx, sy;
        interpolator.transformer().scaling_abs(&sx,&sy);
        if (sx > 1.125 || sy > 1.125) {
          resample = true;
        }
      }

      if (resample) {
        typedef agg::span_image_resample_rgba_affine<img_source_type> SpanGenType;
        typedef agg::span_converter<SpanGenType,SpanConvImageBlend> SpanConvType;
        typedef agg::renderer_scanline_aa<BaseRenderer,Agg2D::SpanAllocator,SpanGenType> RendererType;

        SpanGenType sg(source,interpolator,gr.m_imageFilterLut);
        SpanConvType sc(sg, blend);
        RendererType ri(renBase,gr.m_allocator,sg);
        agg::render_scanlines(gr.m_rasterizer, gr.m_scanline, ri);
      }
      else {
        typedef agg::span_image_filter_rgba_bilinear<img_source_type,Interpolator> SpanGenType;
        typedef agg::span_converter<SpanGenType,SpanConvImageBlend> SpanConvType;
        typedef agg::renderer_scanline_aa<BaseRenderer,Agg2D::SpanAllocator,SpanGenType> RendererType;

        SpanGenType sg(source,interpolator);
        SpanConvType sc(sg, blend);
        RendererType ri(renBase,gr.m_allocator,sg);
        agg::render_scanlines(gr.m_rasterizer, gr.m_scanline, ri);
      }
    }
  }
};

void Agg2D::render(StyleSlot slot) {
  if (m_blendMode == BlendSrcOver)
    Agg2DRenderer::render(*this, m_renBasePre, m_renSolidComp, slot);
  else
    Agg2DRenderer::render(*this, m_renBaseCompPre, m_renSolidComp, slot);
}

void Agg2D::renderImage(const Image& img, int x1, int y1, int x2, int y2, const double* parl) {
  agg::trans_affine mtx((double)x1, (double)y1, (double)x2, (double)y2, parl);
  mtx *= m_transform;
  mtx.invert();

  m_rasterizer.reset();
  m_rasterizer.add_path(m_pathTransform);

  typedef agg::span_interpolator_linear<agg::trans_affine> Interpolator;
  Interpolator interpolator(mtx);

  Agg2DRenderer::renderImage(*this,img, m_renBaseCompPre, interpolator);
}

void Agg2D::blendImage(Image& img, int imgX1, int imgY1, int imgX2, int imgY2, double dstX, double dstY, unsigned alpha) {
  worldToScreen(dstX, dstY);
  PixFormatPre pixF(img.renBuf);
  Rect r(imgX1, imgY1, imgX2, imgY2);
  m_renBaseCompPre.blend_from(pixF, &r, int(dstX)-imgX1, int(dstY)-imgY1, alpha);
}

void Agg2D::blendImage(Image& img, double dstX, double dstY, unsigned alpha) {
  worldToScreen(dstX, dstY);
  PixFormatPre pixF(img.renBuf);
  m_renBasePre.blend_from(pixF, 0, int(dstX), int(dstY), alpha);
  m_renBaseCompPre.blend_from(pixF, 0, int(dstX), int(dstY), alpha);
}

void Agg2D::copyImage(Image& img, int imgX1, int imgY1, int imgX2, int imgY2, double dstX, double dstY) {
  worldToScreen(dstX, dstY);
  Rect r(imgX1, imgY1, imgX2, imgY2);
  m_renBasePre.copy_from(img.renBuf, &r, int(dstX)-imgX1, int(dstY)-imgY1);
}

void Agg2D::copyImage(Image& img, double dstX, double dstY) {
  worldToScreen(dstX, dstY);
  m_renBasePre.copy_from(img.renBuf, 0, int(dstX), int(dstY));
}

// AGG Benchmarking - Backend Implementation
// =========================================

namespace blbench {

static inline uint32_t to_agg2d_blend_mode(BLCompOp comp_op) {
  switch (comp_op) {
    case BL_COMP_OP_CLEAR      : return uint32_t(Agg2D::BlendClear); break;
    case BL_COMP_OP_SRC_COPY   : return uint32_t(Agg2D::BlendSrc); break;
    case BL_COMP_OP_DST_COPY   : return uint32_t(Agg2D::BlendDst); break;
    case BL_COMP_OP_SRC_OVER   : return uint32_t(Agg2D::BlendSrcOver); break;
    case BL_COMP_OP_DST_OVER   : return uint32_t(Agg2D::BlendDstOver); break;
    case BL_COMP_OP_SRC_IN     : return uint32_t(Agg2D::BlendSrcIn); break;
    case BL_COMP_OP_DST_IN     : return uint32_t(Agg2D::BlendDstIn); break;
    case BL_COMP_OP_SRC_OUT    : return uint32_t(Agg2D::BlendSrcOut); break;
    case BL_COMP_OP_DST_OUT    : return uint32_t(Agg2D::BlendDstOut); break;
    case BL_COMP_OP_SRC_ATOP   : return uint32_t(Agg2D::BlendSrcAtop); break;
    case BL_COMP_OP_DST_ATOP   : return uint32_t(Agg2D::BlendDstAtop); break;
    case BL_COMP_OP_XOR        : return uint32_t(Agg2D::BlendXor); break;
    case BL_COMP_OP_PLUS       : return uint32_t(Agg2D::BlendAdd); break;
    case BL_COMP_OP_MULTIPLY   : return uint32_t(Agg2D::BlendMultiply); break;
    case BL_COMP_OP_SCREEN     : return uint32_t(Agg2D::BlendScreen); break;
    case BL_COMP_OP_OVERLAY    : return uint32_t(Agg2D::BlendOverlay); break;
    case BL_COMP_OP_DARKEN     : return uint32_t(Agg2D::BlendDarken); break;
    case BL_COMP_OP_LIGHTEN    : return uint32_t(Agg2D::BlendLighten); break;
    case BL_COMP_OP_COLOR_DODGE: return uint32_t(Agg2D::BlendColorDodge); break;
    case BL_COMP_OP_COLOR_BURN : return uint32_t(Agg2D::BlendColorBurn); break;
    case BL_COMP_OP_HARD_LIGHT : return uint32_t(Agg2D::BlendHardLight); break;
    case BL_COMP_OP_SOFT_LIGHT : return uint32_t(Agg2D::BlendSoftLight); break;
    case BL_COMP_OP_DIFFERENCE : return uint32_t(Agg2D::BlendDifference); break;
    case BL_COMP_OP_EXCLUSION  : return uint32_t(Agg2D::BlendExclusion); break;

    default:
      return 0xFFFFFFFFu;
  }
}

static inline Agg2D::Color to_agg2d_color(BLRgba32 rgba32) {
  return Agg2D::Color(rgba32.r(), rgba32.g(), rgba32.b(), rgba32.a());
}

struct AggModule : public Backend {
  Agg2D _ctx;

  AggModule();
  ~AggModule() override;

  bool supports_comp_op(BLCompOp comp_op) const override;
  bool supports_style(StyleKind style) const override;

  void before_run() override;
  void flush() override;
  void after_run() override;

  void prepare_fill_stroke_option(RenderOp op);

  template<typename RectT>
  void setup_style(RenderOp op, const RectT& rect);

  void render_rect_a(RenderOp op) override;
  void render_rect_f(RenderOp op) override;
  void render_rect_rotated(RenderOp op) override;
  void render_round_f(RenderOp op) override;
  void render_round_rotated(RenderOp op) override;
  void render_polygon(RenderOp op, uint32_t complexity) override;
  void render_shape(RenderOp op, ShapeData shape) override;
};

AggModule::AggModule() {
  strcpy(_name, "AGG");
}
AggModule::~AggModule() {}

bool AggModule::supports_comp_op(BLCompOp comp_op) const {
  return to_agg2d_blend_mode(comp_op) != 0xFFFFFFFFu;
}

bool AggModule::supports_style(StyleKind style) const {
  return style == StyleKind::kSolid         ||
         style == StyleKind::kLinearPad     ||
         style == StyleKind::kLinearRepeat  ||
         style == StyleKind::kLinearReflect ||
         style == StyleKind::kRadialPad     ||
         style == StyleKind::kRadialRepeat  ||
         style == StyleKind::kRadialReflect ;
}

void AggModule::before_run() {
  int w = int(_params.screen_w);
  int h = int(_params.screen_h);

  BLImageData surface_data;
  _surface.create(w, h, BL_FORMAT_PRGB32);
  _surface.make_mutable(&surface_data);

  _ctx.attach(
    static_cast<unsigned char*>(surface_data.pixel_data),
    unsigned(surface_data.size.w),
    unsigned(surface_data.size.h),
    int(surface_data.stride));

  _ctx.fillEvenOdd(false);
  _ctx.noLine();
  _ctx.blendMode(Agg2D::BlendSrc);
  _ctx.clearAll(Agg2D::Color(0, 0, 0, 0));
  _ctx.blendMode(Agg2D::BlendMode(to_agg2d_blend_mode(_params.comp_op)));
}

void AggModule::flush() {
  // Nothing...
}

void AggModule::after_run() {
  _ctx.attach(nullptr, 0, 0, 0);
}

void AggModule::prepare_fill_stroke_option(RenderOp op) {
  if (op == RenderOp::kStroke)
    _ctx.noFill();
  else
    _ctx.noLine();
}

template<typename RectT>
void AggModule::setup_style(RenderOp op, const RectT& rect) {
  switch (_params.style) {
    case StyleKind::kSolid: {
      BLRgba32 color = _rnd_color.next_rgba32();
      if (op == RenderOp::kStroke)
        _ctx.lineColor(to_agg2d_color(color));
      else
        _ctx.fillColor(to_agg2d_color(color));
      break;
    }

    case StyleKind::kLinearPad:
    case StyleKind::kLinearRepeat:
    case StyleKind::kLinearReflect: {
      double x1 = rect.x + rect.w * 0.2;
      double y1 = rect.y + rect.h * 0.2;
      double x2 = rect.x + rect.w * 0.8;
      double y2 = rect.y + rect.h * 0.8;

      BLRgba32 c1 = _rnd_color.next_rgba32();
      BLRgba32 c2 = _rnd_color.next_rgba32();
      BLRgba32 c3 = _rnd_color.next_rgba32();

      if (op == RenderOp::kStroke)
        _ctx.lineLinearGradient(x1, y1, x2, y2, to_agg2d_color(c1), to_agg2d_color(c2), to_agg2d_color(c3));
      else
        _ctx.fillLinearGradient(x1, y1, x2, y2, to_agg2d_color(c1), to_agg2d_color(c2), to_agg2d_color(c3));
      break;
    }

    case StyleKind::kRadialPad:
    case StyleKind::kRadialRepeat:
    case StyleKind::kRadialReflect: {
      double cx = rect.x + rect.w / 2.0;
      double cy = rect.y + rect.h / 2.0;
      double cr = (rect.w + rect.h) / 4.0;

      BLRgba32 c1 = _rnd_color.next_rgba32();
      BLRgba32 c2 = _rnd_color.next_rgba32();
      BLRgba32 c3 = _rnd_color.next_rgba32();

      if (op == RenderOp::kStroke)
        _ctx.lineRadialGradient(cx, cy, cr, to_agg2d_color(c1), to_agg2d_color(c2), to_agg2d_color(c3));
      else
        _ctx.fillRadialGradient(cx, cy, cr, to_agg2d_color(c1), to_agg2d_color(c2), to_agg2d_color(c3));
      break;
    }

    default:
      break;
  }
}

void AggModule::render_rect_a(RenderOp op) {
  BLSizeI bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;
  int wh = _params.shape_size;

  prepare_fill_stroke_option(op);

  if (_params.style == StyleKind::kSolid && op != RenderOp::kStroke) {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRectI rect = _rnd_coord.next_rect_i(bounds, wh, wh);
      _ctx.fillRectangleI(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h, to_agg2d_color(_rnd_color.next_rgba32()));
    }
  }
  else {
    for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
      BLRectI rect = _rnd_coord.next_rect_i(bounds, wh, wh);
      setup_style(op, rect);
      _ctx.rectangle(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h);
    }
  }
}

void AggModule::render_rect_f(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;
  double wh = _params.shape_size;

  prepare_fill_stroke_option(op);

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLRect rect = _rnd_coord.next_rect(bounds, wh, wh);
    setup_style(op, rect);
    _ctx.rectangle(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h);
  }
}

void AggModule::render_rect_rotated(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;

  double cx = double(_params.screen_w) * 0.5;
  double cy = double(_params.screen_h) * 0.5;
  double wh = _params.shape_size;
  double angle = 0.0;

  prepare_fill_stroke_option(op);

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++, angle += 0.01) {
    BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));

    agg::trans_affine affine;
    affine.translate(-cx, -cy);
    affine.rotate(angle);
    affine.translate(cx, cy);

    _ctx.affine(affine);
    setup_style(op, rect);
    _ctx.rectangle(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h);
    _ctx.resetTransformations();
  }
}

void AggModule::render_round_f(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;
  double wh = _params.shape_size;

  prepare_fill_stroke_option(op);

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLRect rect = _rnd_coord.next_rect(bounds, wh, wh);
    double radius = _rnd_extra.next_double(4.0, 40.0);

    setup_style(op, rect);
    _ctx.roundedRect(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h, radius);
  }
}

void AggModule::render_round_rotated(RenderOp op) {
  BLSize bounds(_params.screen_w, _params.screen_h);
  StyleKind style = _params.style;

  double cx = double(_params.screen_w) * 0.5;
  double cy = double(_params.screen_h) * 0.5;
  double wh = _params.shape_size;
  double angle = 0.0;

  prepare_fill_stroke_option(op);

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++, angle += 0.01) {
    BLRect rect(_rnd_coord.next_rect(bounds, wh, wh));
    double radius = _rnd_extra.next_double(4.0, 40.0);

    agg::trans_affine affine;
    affine.translate(-cx, -cy);
    affine.rotate(angle);
    affine.translate(cx, cy);
    _ctx.affine(affine);

    setup_style(op, rect);
    _ctx.roundedRect(rect.x, rect.y, rect.x + rect.w, rect.y + rect.h, radius);
    _ctx.resetTransformations();
  }
}

void AggModule::render_polygon(RenderOp op, uint32_t complexity) {
  BLSizeI bounds(_params.screen_w - _params.shape_size,
                 _params.screen_h - _params.shape_size);

  StyleKind style = _params.style;
  double wh = _params.shape_size;

  prepare_fill_stroke_option(op);
  _ctx.fillEvenOdd(op == RenderOp::kFillEvenOdd);

  Agg2D::DrawPathFlag draw_path_flag = op == RenderOp::kStroke ? Agg2D::StrokeOnly : Agg2D::FillOnly;

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLPoint base(_rnd_coord.nextPoint(bounds));

    double x = _rnd_coord.next_double(base.x, base.x + wh);
    double y = _rnd_coord.next_double(base.y, base.y + wh);

    _ctx.resetPath();
    _ctx.moveTo(x, y);

    for (uint32_t p = 1; p < complexity; p++) {
      x = _rnd_coord.next_double(base.x, base.x + wh);
      y = _rnd_coord.next_double(base.y, base.y + wh);
      _ctx.lineTo(x, y);
    }

    setup_style(op, BLRect(base.x, base.y, wh, wh));
    _ctx.drawPath(draw_path_flag);
  }
}

void AggModule::render_shape(RenderOp op, ShapeData shape) {
  BLSizeI bounds(_params.screen_w - _params.shape_size,
                 _params.screen_h - _params.shape_size);

  StyleKind style = _params.style;
  double wh = double(_params.shape_size);

  prepare_fill_stroke_option(op);
  _ctx.fillEvenOdd(op == RenderOp::kFillEvenOdd);

  Agg2D::DrawPathFlag draw_path_flag = op == RenderOp::kStroke ? Agg2D::StrokeOnly : Agg2D::FillOnly;

  for (uint32_t i = 0, quantity = _params.quantity; i < quantity; i++) {
    BLPoint base(_rnd_coord.nextPoint(bounds));
    ShapeIterator it(shape);

    _ctx.resetPath();
    while (it.has_command()) {
      if (it.is_move_to()) {
        _ctx.moveTo(base.x + it.x(0) * wh, base.y + it.y(0) * wh);
      }
      else if (it.is_line_to()) {
        _ctx.lineTo(base.x + it.x(0) * wh, base.y + it.y(0) * wh);
      }
      else if (it.is_quad_to()) {
        _ctx.quadricCurveTo(
          base.x + it.x(0) * wh, base.y + it.y(0) * wh,
          base.x + it.x(1) * wh, base.y + it.y(1) * wh
        );
      }
      else if (it.is_cubic_to()) {
        _ctx.cubicCurveTo(
          base.x + it.x(0) * wh, base.y + it.y(0) * wh,
          base.x + it.x(1) * wh, base.y + it.y(1) * wh,
          base.x + it.x(2) * wh, base.y + it.y(2) * wh
        );
      }
      else {
        _ctx.closePolygon();
      }
      it.next();
    }

    setup_style(op, BLRect(base.x, base.y, wh, wh));
    _ctx.drawPath(draw_path_flag);
  }
}

Backend* create_agg_backend() {
  return new AggModule();
}

} // {blbench}

#endif // BL_BENCH_ENABLE_AGG
