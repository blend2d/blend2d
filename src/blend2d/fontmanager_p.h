// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
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

#ifndef BLEND2D_FONTMANAGER_P_H_INCLUDED
#define BLEND2D_FONTMANAGER_P_H_INCLUDED

#include "./api-internal_p.h"
#include "./fontmanager.h"
#include "./zoneallocator_p.h"
#include "./zonehash_p.h"
#include "./threading/mutex_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

// ============================================================================
// [BLFontManager - Internal]
// ============================================================================

class BLInternalFontManagerImpl : public BLFontManagerImpl {
public:
  BL_NONCOPYABLE(BLInternalFontManagerImpl)

  class FamiliesMapNode : public BLZoneHashNode {
  public:
    BL_NONCOPYABLE(FamiliesMapNode)

    BLString familyName;
    BLArray<BLFontFace> faces;

    BL_INLINE FamiliesMapNode(uint32_t hashCode, const BLString& familyName) noexcept
      : BLZoneHashNode(hashCode),
        familyName(familyName),
        faces() {}
    BL_INLINE ~FamiliesMapNode() noexcept {}

    BL_INLINE FamiliesMapNode* next() const noexcept { return static_cast<FamiliesMapNode*>(_hashNext); }
  };

  struct FamilyMatcher {
    BLStringView _family;
    uint32_t _hashCode;

    BL_INLINE uint32_t hashCode() const noexcept { return _hashCode; }
    BL_INLINE bool matches(const FamiliesMapNode* node) const noexcept { return node->familyName.equals(_family); }
  };

  class SubstitutionMapNode : public BLZoneHashNode {
  public:
    BL_NONCOPYABLE(SubstitutionMapNode)

    BLString from;
    BLString to;

    BL_INLINE SubstitutionMapNode(uint32_t hashCode, const BLString& from, const BLString& to) noexcept
      : BLZoneHashNode(hashCode),
        from(from),
        to(to) {}
    BL_INLINE ~SubstitutionMapNode() noexcept {}

    BL_INLINE SubstitutionMapNode* next() const noexcept { return static_cast<SubstitutionMapNode*>(_hashNext); }
  };

  BLSharedMutex mutex;
  BLZoneAllocator zone;
  BLZoneHashMap<FamiliesMapNode> familiesMap;
  BLZoneHashMap<SubstitutionMapNode> substitutionMap;
  size_t faceCount;

  BL_INLINE BLInternalFontManagerImpl(const BLFontManagerVirt* virt_, uint16_t memPoolData_) noexcept
    : mutex(),
      zone(8192 - BLZoneAllocator::kBlockOverhead),
      familiesMap(),
      substitutionMap() {
    virt = virt_;
    refCount = 1;
    implType = uint8_t(BL_IMPL_TYPE_FONT_MANAGER);
    implTraits = uint8_t(BL_IMPL_TRAIT_MUTABLE | BL_IMPL_TRAIT_VIRT);
    memPoolData = memPoolData_;
    memset(reserved, 0, sizeof(reserved));
    faceCount = 0;
  }

  BL_INLINE ~BLInternalFontManagerImpl() noexcept {}
};

template<>
struct BLInternalCastImpl<BLFontManagerImpl> { typedef BLInternalFontManagerImpl Type; };

//! \}
//! \endcond

#endif // BLEND2D_FONTMANAGER_P_H_INCLUDED
