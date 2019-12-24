// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_FONTMANAGER_P_H
#define BLEND2D_FONTMANAGER_P_H

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
  class FamiliesMapNode : public BLZoneHashNode {
  public:
    BL_INLINE FamiliesMapNode(uint32_t hashCode, const BLString& familyName) noexcept
      : BLZoneHashNode(hashCode),
        familyName(familyName),
        faces() {}

    BLString familyName;
    BLArray<BLFontFace> faces;
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
  };

  BLSharedMutex mutex;
  BLZoneAllocator zone;
  BLZoneHashMap<FamiliesMapNode> familiesMap;
  BLZoneHashMap<SubstitutionMapNode> substitutionMap;

  BL_INLINE BLInternalFontManagerImpl(const BLFontManagerVirt* virt_) noexcept
    : mutex(),
      zone(4096),
      familiesMap(),
      substitutionMap() {
    virt = virt_;
  }
};

template<>
struct BLInternalCastImpl<BLFontManagerImpl> { typedef BLInternalFontManagerImpl Type; };

//! \}
//! \endcond

#endif // BLEND2D_FONTMANAGER_P_H
