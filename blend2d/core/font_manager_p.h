// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_CORE_FONT_MANAGER_P_H_INCLUDED
#define BLEND2D_CORE_FONT_MANAGER_P_H_INCLUDED

#include <blend2d/core/api_internal_p.h>
#include <blend2d/core/font_manager.h>
#include <blend2d/core/object_layer_p.h>
#include <blend2d/axl/arena_allocator_p.h>
#include <blend2d/axl/arena_hash_map_p.h>
#include <blend2d/threading/mutex_p.h>

//! \cond INTERNAL
//! \addtogroup blend2d_internal
//! \{

namespace bl {

class FontManagerPrivateImpl : public BLFontManagerImpl {
public:
  BL_NONCOPYABLE(FontManagerPrivateImpl)

  class FamiliesMapNode : public ArenaHashMapNode {
  public:
    BL_NONCOPYABLE(FamiliesMapNode)

    BLString family_name;
    BLArray<BLFontFace> faces;

    BL_INLINE FamiliesMapNode(uint32_t hash_code, const BLString& family_name) noexcept
      : ArenaHashMapNode(hash_code),
        family_name(family_name),
        faces() {}
    BL_INLINE ~FamiliesMapNode() noexcept {}

    BL_INLINE FamiliesMapNode* next() const noexcept { return static_cast<FamiliesMapNode*>(_hash_next); }
  };

  struct FamilyMatcher {
    BLStringView _family;
    uint32_t _hash_code;

    BL_INLINE uint32_t hash_code() const noexcept { return _hash_code; }
    BL_INLINE bool matches(const FamiliesMapNode* node) const noexcept { return node->family_name.equals(_family); }
  };

  class SubstitutionMapNode : public ArenaHashMapNode {
  public:
    BL_NONCOPYABLE(SubstitutionMapNode)

    BLString from;
    BLString to;

    BL_INLINE SubstitutionMapNode(uint32_t hash_code, const BLString& from, const BLString& to) noexcept
      : ArenaHashMapNode(hash_code),
        from(from),
        to(to) {}
    BL_INLINE ~SubstitutionMapNode() noexcept {}

    BL_INLINE SubstitutionMapNode* next() const noexcept { return static_cast<SubstitutionMapNode*>(_hash_next); }
  };

  SharedMutex mutex;
  ArenaAllocator allocator;
  ArenaHashMap<FamiliesMapNode> families_map;
  ArenaHashMap<SubstitutionMapNode> substitution_map;
  size_t face_count = 0;

  BL_INLINE FontManagerPrivateImpl() noexcept
    : mutex(),
      allocator(8192),
      families_map(&allocator),
      substitution_map(&allocator) {}

  BL_INLINE ~FontManagerPrivateImpl() noexcept {}
};

template<>
struct ObjectDispatchInfo<BLFontManagerCore> {
  using ImplType = FontManagerPrivateImpl;

  static constexpr bool kHasInitSSO = false;

  static BL_INLINE BLResult free_object(BLFontManagerCore* self, ImplType* impl) noexcept {
    axl::maybe_unused(impl);
    return self->_virt()->base.destroy(self);
  }
};

} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_CORE_FONT_MANAGER_P_H_INCLUDED
