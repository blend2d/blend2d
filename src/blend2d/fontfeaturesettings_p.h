// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONTFEATURESETTINGS_P_H_INCLUDED
#define BLEND2D_FONTFEATURESETTINGS_P_H_INCLUDED

#include "api-internal_p.h"
#include "fontfeaturesettings.h"
#include "fonttags_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace BLFontFeatureSettingsPrivate {

BLResult freeImpl(BLFontFeatureSettingsImpl* impl, BLObjectInfo info) noexcept;

} // {BLFontFeatureSettingsPrivate}

//! \}
//! \endcond

#endif // BLEND2D_FONTFEATURESETTINGS_P_H_INCLUDED
