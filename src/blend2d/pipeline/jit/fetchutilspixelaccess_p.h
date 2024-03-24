// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_FETCHUTILSPIXELACCESS_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_FETCHUTILSPIXELACCESS_P_H_INCLUDED

#include "../../pipeline/jit/pipecompiler_p.h"
#include "../../pipeline/jit/pipeprimitives_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace bl {
namespace Pipeline {
namespace JIT {
namespace FetchUtils {

static BL_INLINE void shiftOrRotate(PipeCompiler* pc, const Vec& dst, const Vec& src, uint32_t n) noexcept {
#if defined(BL_JIT_ARCH_X86)
  pc->v_srlb_u128(dst, src, n);
#else
  // This doesn't rely on a zero constant on AArch64, which is okay as we don't care what's shifted in.
  pc->v_alignr_u128(dst, src, src, n);
#endif
}

// bl::Pipeline::Jit::FetchUtils - Fetch Mask
// ==========================================

void fetchMaskA8AndAdvance(PipeCompiler* pc, VecArray& vm, PixelCount n, PixelType pixelType, PixelCoverageFormat coverageFormat, const Gp& mPtr, const Vec& globalAlpha) noexcept;

// bl::Pipeline::Jit::FetchUtils - Satisfy Pixel(s)
// ================================================

//! Makes sure that the given pixel `p` has all the requrements as specified by `flags`.
void x_satisfy_pixel(PipeCompiler* pc, Pixel& p, PixelFlags flags) noexcept;

//! Makes sure that the given pixel `p` has all the requrements as specified by `flags` (solid source only).
void x_satisfy_solid(PipeCompiler* pc, Pixel& p, PixelFlags flags) noexcept;

// bl::Pipeline::Jit::FetchUtils - Fetch Pixel(s)
// ==============================================

//! Fetches `n` pixels to vector register(s) in `p` from memory location `src_`.
void x_fetch_pixel(PipeCompiler* pc, Pixel& p, PixelCount n, PixelFlags flags, FormatExt format, const Mem& src_, Alignment alignment) noexcept;
void x_fetch_pixel(PipeCompiler* pc, Pixel& p, PixelCount n, PixelFlags flags, FormatExt format, const Mem& src_, Alignment alignment, PixelPredicate& predicate) noexcept;

// bl::Pipeline::Jit::FetchUtils - Miscellaneous
// =============================================

void _x_pack_pixel(PipeCompiler* pc, VecArray& px, VecArray& ux, uint32_t n, const char* prefix, const char* pxName) noexcept;
void _x_unpack_pixel(PipeCompiler* pc, VecArray& ux, VecArray& px, uint32_t n, const char* prefix, const char* uxName) noexcept;

void x_fetch_unpacked_a8_2x(PipeCompiler* pc, const Vec& dst, FormatExt format, const Mem& src1, const Mem& src0) noexcept;

void x_assign_unpacked_alpha_values(PipeCompiler* pc, Pixel& p, PixelFlags flags, const Vec& vec) noexcept;

//! Fills alpha channel with 1.
void x_fill_pixel_alpha(PipeCompiler* pc, Pixel& p) noexcept;

void x_store_pixel_advance(PipeCompiler* pc, const Gp& dPtr, Pixel& p, PixelCount n, uint32_t bpp, Alignment alignment, PixelPredicate& predicate) noexcept;

} // {FetchUtils}
} // {JIT}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FETCHUTILSPIXELACCESS_P_H_INCLUDED
