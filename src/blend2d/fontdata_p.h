// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONTDATA_P_H_INCLUDED
#define BLEND2D_FONTDATA_P_H_INCLUDED

#include "api-internal_p.h"
#include "fontdata.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! \name BLFontData - Impl
//! \{

struct BLFontDataPrivateImpl : public BLFontDataImpl {
  BLArray<BLFontFaceImpl*> faceCache;
};

namespace bl {
namespace FontDataInternal {

static BL_INLINE BLFontDataPrivateImpl* getImpl(const BLFontDataCore* self) noexcept {
  return static_cast<BLFontDataPrivateImpl*>(self->_d.impl);
}

static BL_INLINE void initImpl(BLFontDataPrivateImpl* impl, BLFontDataVirt* virt) noexcept {
  impl->virt = virt;
  impl->faceCount = 0;
  impl->faceType = BL_FONT_FACE_TYPE_NONE;
  impl->flags = 0;
  blCallCtor(impl->faceCache);
}

} // {FontDataInternal}
} // {bl}

//! \}

//! \}
//! \endcond

#endif // BLEND2D_FONTDATA_P_H_INCLUDED
