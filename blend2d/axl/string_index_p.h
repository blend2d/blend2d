// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_AXL_STRING_INDEX_P_H_INCLUDED
#define BLEND2D_AXL_STRING_INDEX_P_H_INCLUDED

#include <blend2d/axl/array_p.h>
#include <blend2d/axl/commons.h>

namespace bl::axl {

//! \cond INTERNAL
//! \addtogroup blend2d_support
//! \{

template<size_t IndexN, size_t StrN, typename IndexT = std_uint_t<(StrN < 256) ? 1 : (StrN < 65535) ? 2 : 4>>
static consteval Array<IndexT, IndexN> make_string_index(const char(&str)[StrN]) noexcept {
  Array<IndexT, IndexN> out;
  for (size_t idx_i = 0, str_i = 0; idx_i < IndexN; idx_i++) {
    out[idx_i] = IndexT(str_i);
    while (str[str_i++] != '\0') {
      continue;
    }
  }
  return out;
}

//! \}
//! \endcond

} // {bl::axl}

#endif // BLEND2D_AXL_STRING_INDEX_P_H_INCLUDED