// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include <blend2d/core/api_build_p.h>
#if !defined(BL_BUILD_NO_CODECS) && defined(BL_TARGET_OPT_ASIMD)

#include <blend2d/codec/png_ops_simd_impl_p.h>
#include <blend2d/codec/png_encoder_fast_simd_p.h>

namespace bl::Png::Ops {

void init_func_table_asimd(FunctionTable& ft) noexcept {
  init_simd_functions(ft);
  ft.encode_rows_fast = FastEncoder::encode_rows_simd;
  ft.write_header_fast = FastEncoder::write_header_simd;
}

} // {bl::Png::Ops}

#endif // !BL_BUILD_NO_CODECS && BL_TARGET_OPT_ASIMD
