// This file is part of Blend2D project <https://blend2d.com>
//
// SPDX-License-Identifier: Zlib
// Official GitHub Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2008-2021 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

// ----------------------------------------------------------------------------
// This is a public header file designed to be used by Blend2D users. It
// includes all the necessary files required to use Blend2D library from both
// C and C++ and it's the only header that is guaranteed to always be provided.
//
// Never include directly header files placed in "blend2d" directory. Headers
// that end with "_p" suffix are private and should never be  included as they
// are not part of public API and they are not part of blend2d-dev packages.
// ----------------------------------------------------------------------------

#ifndef BLEND2D_H_INCLUDED
#define BLEND2D_H_INCLUDED

#if defined(_MSC_VER)
  #pragma warning(push)
  #pragma warning(disable: 4201) // Nameless struct/union.
  #pragma warning(disable: 4324) // Structure was padded due to alignment specifier.
  #pragma warning(disable: 4458) // declaration of 'X' hides class member.
  #pragma warning(disable: 4582) // constructor is not implicitly called.
  #pragma warning(disable: 4583) // destructor is not implicitly called.
#endif

#include "blend2d/api.h"
#include "blend2d/array.h"
#include "blend2d/bitset.h"
#include "blend2d/context.h"
#include "blend2d/filesystem.h"
#include "blend2d/font.h"
#include "blend2d/fontdefs.h"
#include "blend2d/fontmanager.h"
#include "blend2d/format.h"
#include "blend2d/geometry.h"
#include "blend2d/glyphbuffer.h"
#include "blend2d/gradient.h"
#include "blend2d/image.h"
#include "blend2d/imagecodec.h"
#include "blend2d/matrix.h"
#include "blend2d/object.h"
#include "blend2d/path.h"
#include "blend2d/pattern.h"
#include "blend2d/pixelconverter.h"
#include "blend2d/random.h"
#include "blend2d/rgba.h"
#include "blend2d/runtime.h"
#include "blend2d/string.h"
#include "blend2d/var.h"

#if defined(_MSC_VER)
  #pragma warning(pop)
#endif

#endif // BLEND2D_H_INCLUDED
