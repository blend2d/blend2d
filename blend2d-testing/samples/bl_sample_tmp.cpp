#include <blend2d/blend2d.h>
#include <blend2d/blend2d-debug.h>

int main() {
  BLImage img(512, 512, BL_FORMAT_PRGB32);
  BLContext ctx(img);

  BLPath p1;
  double r = 100.0;
  double PI=3.1415;
  double r1=r/5*4;

  p1.add_segments(
    BLPath::MoveTo{1, 2},
    BLPath::LineTo{3, 4},
    BLPath::QuadTo{5, 6, 7, 8},
    BLPath::ConicTo{9, 10, 11, 12, 0.5},
    BLPath::CubicTo{13, 14, 15, 16, 17, 18},
    BLPath::Close{}
  );

  BLArc xa = BLArc(r,0,r1,r1,PI*2/3,PI*2/3);
  p1.add_arc(xa);

  BLStrokeOptions opt;
  opt.set_width(40.0f);

  BLPath p2;
  p2.add_stroked_path(p1, opt, bl_default_approximation_options);
  bl_debug_path(&p2);

  // ctx.clear_all();
  // ctx.set_transform({1,0,0,1,100,100});
  // ctx.stroke_path(BLPoint(100, 150), p2, BLRgba32(0xFFFFFF00));
  // img.write_to_file("bl_sample_tmp.png");
  return 0;
}
