// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_FONTMANAGER_P_H_INCLUDED
#define BLEND2D_FONTMANAGER_P_H_INCLUDED

#include "api-internal_p.h"
#include "fontmanager.h"
#include "support/arenaallocator_p.h"
#include "support/arenahashmap_p.h"
#include "threading/mutex_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

//! \name BLFontManager - Internals - Memory Management
//! \{

class BLFontManagerPrivateImpl : public BLFontManagerImpl {
public:
  BL_NONCOPYABLE(BLFontManagerPrivateImpl)

  class FamiliesMapNode : public bl::ArenaHashMapNode {
  public:
    BL_NONCOPYABLE(FamiliesMapNode)

    BLString familyName;
    BLArray<BLFontFace> faces;

    BL_INLINE FamiliesMapNode(uint32_t hashCode, const BLString& familyName) noexcept
      : bl::ArenaHashMapNode(hashCode),
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

  class SubstitutionMapNode : public bl::ArenaHashMapNode {
  public:
    BL_NONCOPYABLE(SubstitutionMapNode)

    BLString from;
    BLString to;

    BL_INLINE SubstitutionMapNode(uint32_t hashCode, const BLString& from, const BLString& to) noexcept
      : bl::ArenaHashMapNode(hashCode),
        from(from),
        to(to) {}
    BL_INLINE ~SubstitutionMapNode() noexcept {}

    BL_INLINE SubstitutionMapNode* next() const noexcept { return static_cast<SubstitutionMapNode*>(_hashNext); }
  };

  BLSharedMutex mutex;
  bl::ArenaAllocator allocator;
  bl::ArenaHashMap<FamiliesMapNode> familiesMap;
  bl::ArenaHashMap<SubstitutionMapNode> substitutionMap;
  size_t faceCount = 0;

  BL_INLINE BLFontManagerPrivateImpl(const BLFontManagerVirt* virt_) noexcept
    : mutex(),
      allocator(8192),
      familiesMap(&allocator),
      substitutionMap(&allocator) { virt = virt_; }

  BL_INLINE ~BLFontManagerPrivateImpl() noexcept {}
};

namespace bl {
namespace FontManagerInternal {

static BL_INLINE BLFontManagerPrivateImpl* getImpl(const BLFontManagerCore* self) noexcept {
  return static_cast<BLFontManagerPrivateImpl*>(self->_d.impl);
}

} // {FontManagerInternal}
} // {bl}

//! \}

//! \}
//! \endcond

#endif // BLEND2D_FONTMANAGER_P_H_INCLUDED
