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

namespace bl::Pipeline::JIT::FetchUtils {

// bl::Pipeline::Jit::FetchUtils - Fetch & Store
// =============================================

void fetchVec8(PipeCompiler* pc, const VecArray& d_vec, Gp s_ptr, uint32_t n, AdvanceMode advance_mode) noexcept;
void fetchVec32(PipeCompiler* pc, const VecArray& d_vec, Gp s_ptr, uint32_t n, AdvanceMode advance_mode) noexcept;

void storeVec8(PipeCompiler* pc, const Gp& d_ptr, const VecArray& s_vec, uint32_t n, AdvanceMode advance_mode) noexcept;
void storeVec32(PipeCompiler* pc, const Gp& d_ptr, const VecArray& s_vec, uint32_t n, AdvanceMode advance_mode) noexcept;

void fetchVec8(PipeCompiler* pc, const VecArray& d_vec, Gp s_ptr, uint32_t n, AdvanceMode advance_mode, PixelPredicate& predicate) noexcept;
void fetchVec32(PipeCompiler* pc, const VecArray& d_vec, Gp s_ptr, uint32_t n, AdvanceMode advance_mode, PixelPredicate& predicate) noexcept;

void storeVec8(PipeCompiler* pc, const Gp& d_ptr, const VecArray& s_vec, uint32_t n, AdvanceMode advance_mode, PixelPredicate& predicate) noexcept;
void storeVec32(PipeCompiler* pc, const Gp& d_ptr, const VecArray& s_vec, uint32_t n, AdvanceMode advance_mode, PixelPredicate& predicate) noexcept;

// bl::Pipeline::Jit::FetchUtils - Fetch Miscellaneous
// ===================================================

void fetchSecond32BitElement(PipeCompiler* pc, const Vec& vec, const Mem& src) noexcept;
void fetchThird32BitElement(PipeCompiler* pc, const Vec& vec, const Mem& src) noexcept;

// bl::Pipeline::Jit::FetchUtils - Predicated Fetch & Store
// ========================================================

void fetchPredicatedVec8(PipeCompiler* pc, const VecArray& d_vec, Gp s_ptr, uint32_t n, AdvanceMode advance_mode, PixelPredicate& predicate) noexcept;
void fetchPredicatedVec32(PipeCompiler* pc, const VecArray& d_vec, Gp s_ptr, uint32_t n, AdvanceMode advance_mode, PixelPredicate& predicate) noexcept;

void storePredicatedVec8(PipeCompiler* pc, const Gp& d_ptr, const VecArray& s_vec, uint32_t n, AdvanceMode advance_mode, PixelPredicate& predicate) noexcept;
void storePredicatedVec32(PipeCompiler* pc, const Gp& d_ptr, const VecArray& s_vec, uint32_t n, AdvanceMode advance_mode, PixelPredicate& predicate) noexcept;

// bl::Pipeline::Jit::FetchUtils - Fetch Mask
// ==========================================

void fetchMaskA8(PipeCompiler* pc, VecArray& vm, const Gp& mPtr, PixelCount n, PixelType pixel_type, PixelCoverageFormat coverage_format, AdvanceMode advance, PixelPredicate& predicate, GlobalAlpha* ga = nullptr) noexcept;

// bl::Pipeline::Jit::FetchUtils - Fetch Pixel(s)
// ==============================================

// Functions with `Info` suffix fetch into an already preallocated destination.
void fetchMaskA8IntoPA(PipeCompiler* pc, VecArray& d_vec, const Gp& s_ptr, PixelCount n, AdvanceMode advance_mode, PixelPredicate& predicate, GlobalAlpha* ga = nullptr) noexcept;
void fetchMaskA8IntoUA(PipeCompiler* pc, VecArray& d_vec, const Gp& s_ptr, PixelCount n, AdvanceMode advance_mode, PixelPredicate& predicate, GlobalAlpha* ga = nullptr) noexcept;
void fetchMaskA8IntoPC(PipeCompiler* pc, VecArray d_vec, const Gp& s_ptr, PixelCount n, AdvanceMode advance_mode, PixelPredicate& predicate, GlobalAlpha* ga = nullptr) noexcept;
void fetchMaskA8IntoUC(PipeCompiler* pc, VecArray& d_vec, const Gp& s_ptr, PixelCount n, AdvanceMode advance_mode, PixelPredicate& predicate, GlobalAlpha* ga = nullptr) noexcept;
void fetchPRGB32IntoPA(PipeCompiler* pc, VecArray& d_vec, const Gp& s_ptr, PixelCount n, AdvanceMode advance_mode, PixelPredicate& predicate) noexcept;
void fetchPRGB32IntoUA(PipeCompiler* pc, VecArray& d_vec, const Gp& s_ptr, PixelCount n, AdvanceMode advance_mode, PixelPredicate& predicate) noexcept;

//! Fetches `1` pixel to a vector or scalar register in `p` from memory location `s_mem`.
void fetch_pixel(PipeCompiler* pc, Pixel& p, PixelFlags flags, PixelFetchInfo fInfo, Mem s_mem) noexcept;

//! Fetches `n` pixels to vector register(s) in `p` from memory location `s_ptr`.
void fetch_pixels(PipeCompiler* pc, Pixel& p, PixelCount n, PixelFlags flags, PixelFetchInfo fInfo, const Gp& s_ptr, Alignment alignment, AdvanceMode advance_mode) noexcept;
void fetch_pixels(PipeCompiler* pc, Pixel& p, PixelCount n, PixelFlags flags, PixelFetchInfo fInfo, const Gp& s_ptr, Alignment alignment, AdvanceMode advance_mode, PixelPredicate& predicate) noexcept;

// bl::Pipeline::Jit::FetchUtils - Satisfy Pixel(s)
// ================================================

//! Makes sure that the given pixel `p` has all the requirements as specified by `flags`.
void satisfy_pixels(PipeCompiler* pc, Pixel& p, PixelFlags flags) noexcept;

//! Makes sure that the given pixel `p` has all the requirements as specified by `flags` (solid source only).
void satisfy_solid_pixels(PipeCompiler* pc, Pixel& p, PixelFlags flags) noexcept;

// bl::Pipeline::Jit::FetchUtils - Miscellaneous
// =============================================

void _x_pack_pixel(PipeCompiler* pc, VecArray& px, VecArray& ux, uint32_t n, const char* prefix, const char* px_name) noexcept;
void _x_unpack_pixel(PipeCompiler* pc, VecArray& ux, VecArray& px, uint32_t n, const char* prefix, const char* ux_name) noexcept;

void x_fetch_unpacked_a8_2x(PipeCompiler* pc, const Vec& dst, PixelFetchInfo fInfo, const Mem& src1, const Mem& src0) noexcept;

void x_assign_unpacked_alpha_values(PipeCompiler* pc, Pixel& p, PixelFlags flags, const Vec& vec) noexcept;

//! Fills alpha channel with 1.
void fill_alpha_channel(PipeCompiler* pc, Pixel& p) noexcept;

void store_pixels_and_advance(PipeCompiler* pc, const Gp& d_ptr, Pixel& p, PixelCount n, uint32_t bpp, Alignment alignment, PixelPredicate& predicate) noexcept;

} // {bl::Pipeline::JIT::FetchUtils}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_FETCHUTILSPIXELACCESS_P_H_INCLUDED
