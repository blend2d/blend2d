// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "filesystem.h"
#include "font_p.h"
#include "fontface_p.h"
#include "fontmanager_p.h"
#include "object_p.h"
#include "runtime_p.h"
#include "string_p.h"
#include "support/hashops_p.h"

namespace bl {
namespace FontManagerInternal {

// bl::FontManager - Internals - Globals
// =====================================

static BLObjectEternalVirtualImpl<BLFontManagerPrivateImpl, BLFontManagerVirt> defaultImpl;

// bl::FontManager - Internals - Constants
// =======================================

enum QueryPrecedenceBits : uint32_t {
  kQueryDiffFamilyNameShift   = 24, // 0xFF000000 [8 bits].
  kQueryDiffStyleValueShift   = 22, // 0x00C00000 [2 bits].
  kQueryDiffStyleSignShift    = 21, // 0x00200000 [1 bit].
  kQueryDiffWeightValueShift  = 10, // 0x001FFC00 [11 bits].
  kQueryDiffWeightSignShift   =  9, // 0x00000200 [1 bit].
  kQueryDiffStretchValueShift =  5, // 0x000001E0 [4 bits].
  kQueryDiffStretchSignShift  =  4  // 0x00000010 [1 bit].
};

static constexpr uint32_t kQueryInvalidDiff = 0xFFFFFFFFu;

// bl::FontManager - Internals - Alloc & Free Impl
// ===============================================

static BLResult allocImpl(BLFontManagerCore* self) noexcept {
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_FONT_MANAGER);
  BL_PROPAGATE(ObjectInternal::allocImplT<BLFontManagerPrivateImpl>(self, info));

  BLFontManagerPrivateImpl* impl = getImpl(self);
  blCallCtor(*impl, &defaultImpl.virt);
  return BL_SUCCESS;
}

static BLResult BL_CDECL destroyImpl(BLObjectImpl* impl) noexcept {
  blCallDtor(*static_cast<BLFontManagerPrivateImpl*>(impl));
  return blObjectFreeImpl(impl);
}

// bl::FontManager - Internals - Faces
// ===================================

static BL_INLINE size_t indexOfFace(const BLFontFace* array, size_t size, BLFontFaceImpl* faceI) noexcept {
  for (size_t i = 0; i < size; i++)
    if (array[i]._d.impl == faceI)
      return i;
  return SIZE_MAX;
}

static BL_INLINE uint32_t calcFaceOrder(const BLFontFaceImpl* faceI) noexcept {
  uint32_t style = faceI->style;
  uint32_t weight = faceI->weight;

  return (style << kQueryDiffStyleValueShift) |
         (weight << kQueryDiffWeightValueShift);
}

static BL_INLINE size_t indexForInsertion(const BLFontFace* array, size_t size, BLFontFaceImpl* faceI) noexcept {
  uint32_t faceOrder = calcFaceOrder(faceI);
  size_t i;

  for (i = 0; i < size; i++) {
    BLFontFacePrivateImpl* storedFaceI = FontFaceInternal::getImpl(&array[i]);
    uint32_t storedFaceOrder = calcFaceOrder(storedFaceI);
    if (storedFaceOrder >= faceOrder) {
      if (storedFaceOrder == faceOrder)
        return SIZE_MAX;
      break;
    }
  }

  return i;
}

// bl::FontManager - Query - Utilities
// ===================================

static const BLFontQueryProperties defaultQueryProperties = {
  BL_FONT_STYLE_NORMAL,
  BL_FONT_WEIGHT_NORMAL,
  BL_FONT_STRETCH_NORMAL
};

static bool sanitizeQueryProperties(BLFontQueryProperties& dst, const BLFontQueryProperties& src) noexcept {
  bool valid = src.weight <= 1000u &&
               src.style <= BL_FONT_STYLE_MAX_VALUE &&
               src.stretch <= BL_FONT_STRETCH_ULTRA_EXPANDED;
  if (!valid)
    return false;

  dst.style = src.style;
  dst.weight = src.weight ? src.weight : BL_FONT_WEIGHT_NORMAL;
  dst.stretch = src.stretch ? src.stretch : BL_FONT_STRETCH_NORMAL;

  return true;
}

// bl::FontManager - Query - Prepared Query
// ========================================

struct PreparedQuery {
  BLStringView _name;
  uint32_t _hashCode;

  typedef BLFontManagerPrivateImpl::FamiliesMapNode FamiliesMapNode;

  BL_INLINE const BLStringView& name() const noexcept { return _name; };
  BL_INLINE uint32_t hashCode() const noexcept { return _hashCode; }

  BL_INLINE bool matches(const FamiliesMapNode* node) const noexcept { return node->familyName.equals(_name); }
};

static bool prepareQuery(const BLFontManagerPrivateImpl* impl, const char* name, size_t nameSize, PreparedQuery* out) noexcept {
  blUnused(impl);

  if (nameSize == SIZE_MAX)
    nameSize = strlen(name);

  out->_name.reset(name, nameSize);
  out->_hashCode = HashOps::hashStringCI(name, nameSize);
  return nameSize != 0;
}

// bl::FontManager - Query - Diff Calculation
// ==========================================

static BL_INLINE uint32_t calcFamilyNameDiff(BLStringView aStr, BLStringView bStr) noexcept {
  if (aStr.size != bStr.size)
    return kQueryInvalidDiff;

  uint32_t diff = 0;

  for (size_t i = 0; i < aStr.size; i++) {
    uint32_t a = uint8_t(aStr.data[i]);
    uint32_t b = uint8_t(bStr.data[i]);

    if (a == b)
      continue;

    a = Unicode::asciiToLower(a);
    b = Unicode::asciiToLower(b);

    if (a != b)
      return kQueryInvalidDiff;

    diff++;
  }

  if (diff > 255)
    diff = 255;

  return diff << kQueryDiffFamilyNameShift;
}

static BL_INLINE uint32_t calcPropertyDiff(const BLFontFaceImpl* faceI, const BLFontQueryProperties* properties) noexcept {
  uint32_t diff = 0;

  uint32_t fStyle = faceI->style;
  uint32_t fWeight = faceI->weight;
  uint32_t fStretch = faceI->stretch;

  uint32_t pStyle = properties->style;
  uint32_t pWeight = properties->weight;
  uint32_t pStretch = properties->stretch;

  uint32_t styleDiff = uint32_t(blAbs(int(pStyle) - int(fStyle)));
  uint32_t weightDiff = uint32_t(blAbs(int(pWeight) - int(fWeight)));
  uint32_t stretchDiff = uint32_t(blAbs(int(pStretch) - int(fStretch)));

  diff |= styleDiff << kQueryDiffStyleValueShift;
  diff |= uint32_t(pStyle < fStyle) << kQueryDiffStyleSignShift;

  diff |= weightDiff << kQueryDiffWeightValueShift;
  diff |= uint32_t(pWeight < fWeight) << kQueryDiffWeightSignShift;

  diff |= stretchDiff << kQueryDiffStretchValueShift;
  diff |= uint32_t(pStretch < fStretch) << kQueryDiffStretchSignShift;

  return diff;
}

// bl::FontManager - Query - Match
// ===============================

class QueryBestMatch {
public:
  const BLFontQueryProperties* properties;
  const BLFontFace* face;
  uint32_t diff;

  BL_INLINE QueryBestMatch(const BLFontQueryProperties* properties) noexcept
    : properties(properties),
      face(nullptr),
      diff(0xFFFFFFFFu) {}

  BL_INLINE bool hasFace() const noexcept { return face != nullptr; }

  void match(const BLFontFace& faceIn, uint32_t baseDiff = 0) noexcept {
    uint32_t localDiff = baseDiff + calcPropertyDiff(faceIn._impl(), properties);
    if (diff > localDiff) {
      face = &faceIn;
      diff = localDiff;
    }
  }
};

} // {FontManagerInternal}
} // {bl}

// bl::FontManager - API - Init & Destroy
// ======================================

BL_API_IMPL BLResult blFontManagerInit(BLFontManagerCore* self) noexcept {
  self->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_MANAGER]._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blFontManagerInitMove(BLFontManagerCore* self, BLFontManagerCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFontManager());

  self->_d = other->_d;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_MANAGER]._d;

  return BL_SUCCESS;

}

BL_API_IMPL BLResult blFontManagerInitWeak(BLFontManagerCore* self, const BLFontManagerCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFontManager());

  return blObjectPrivateInitWeakTagged(self, other);
}

BL_API_IMPL BLResult blFontManagerInitNew(BLFontManagerCore* self) noexcept {
  using namespace bl::FontManagerInternal;

  self->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_MANAGER]._d;
  return allocImpl(self);
}

BL_API_IMPL BLResult blFontManagerDestroy(BLFontManagerCore* self) noexcept {
  BL_ASSERT(self->_d.isFontManager());

  return bl::ObjectInternal::releaseVirtualInstance(self);
}

// bl::FontManager - API - Reset
// =============================

BL_API_IMPL BLResult blFontManagerReset(BLFontManagerCore* self) noexcept {
  BL_ASSERT(self->_d.isFontManager());

  return bl::ObjectInternal::replaceVirtualInstance(self, static_cast<BLFontManagerCore*>(&blObjectDefaults[BL_OBJECT_TYPE_FONT_MANAGER]));
}

// bl::FontManager - API - Assign
// ==============================

BL_API_IMPL BLResult blFontManagerAssignMove(BLFontManagerCore* self, BLFontManagerCore* other) noexcept {
  BL_ASSERT(self->_d.isFontManager());
  BL_ASSERT(other->_d.isFontManager());

  BLFontManagerCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_MANAGER]._d;
  return bl::ObjectInternal::replaceVirtualInstance(self, &tmp);
}

BL_API_IMPL BLResult blFontManagerAssignWeak(BLFontManagerCore* self, const BLFontManagerCore* other) noexcept {
  BL_ASSERT(self->_d.isFontManager());
  BL_ASSERT(other->_d.isFontManager());

  return bl::ObjectInternal::assignVirtualInstance(self, other);
}

// bl::FontManager - API - Equals
// ==============================

bool blFontManagerEquals(const BLFontManagerCore* a, const BLFontManagerCore* b) noexcept {
  BL_ASSERT(a->_d.isFontManager());
  BL_ASSERT(b->_d.isFontManager());

  return a->_d.impl == b->_d.impl;
}

// bl::FontManager - API - Create
// ==============================

BL_API_IMPL BLResult blFontManagerCreate(BLFontManagerCore* self) noexcept {
  using namespace bl::FontManagerInternal;
  BL_ASSERT(self->_d.isFontManager());

  BLFontManagerCore newO;
  BL_PROPAGATE(allocImpl(&newO));

  return bl::ObjectInternal::replaceVirtualInstance(self, &newO);
}

// bl::FontManager - API - Accessors
// =================================

BL_API_IMPL size_t blFontManagerGetFaceCount(const BLFontManagerCore* self) noexcept {
  using namespace bl::FontManagerInternal;
  BL_ASSERT(self->_d.isFontManager());

  BLFontManagerPrivateImpl* selfI = getImpl(self);
  BLSharedLockGuard<BLSharedMutex> guard(selfI->mutex);

  return selfI->faceCount;
}

BL_API_IMPL size_t blFontManagerGetFamilyCount(const BLFontManagerCore* self) noexcept {
  using namespace bl::FontManagerInternal;
  BL_ASSERT(self->_d.isFontManager());

  BLFontManagerPrivateImpl* selfI = getImpl(self);
  BLSharedLockGuard<BLSharedMutex> guard(selfI->mutex);

  return selfI->familiesMap.size();
}

// bl::FontManager - Internal Utilities
// ====================================

static BL_INLINE BLResult blFontManagerMakeMutable(BLFontManagerCore* self) noexcept {
  BL_ASSERT(self->_d.isFontManager());

  if (!self->dcast().isValid())
    return blFontManagerCreate(self);

  return BL_SUCCESS;
}

// bl::FontManager - API - Font Face Management
// ============================================

BL_API_IMPL bool blFontManagerHasFace(const BLFontManagerCore* self, const BLFontFaceCore* face) noexcept {
  using namespace bl::FontManagerInternal;

  BL_ASSERT(self->_d.isFontManager());
  BL_ASSERT(self->_d.isFontFace());

  BLFontManagerPrivateImpl* selfI = getImpl(self);
  BLFontFacePrivateImpl* faceI = bl::FontFaceInternal::getImpl(face);

  uint32_t nameHash = bl::HashOps::hashStringCI(faceI->familyName.dcast().view());

  BLSharedLockGuard<BLSharedMutex> guard(selfI->mutex);
  BLFontManagerPrivateImpl::FamiliesMapNode* familiesNode =
    selfI->familiesMap.get(BLFontManagerPrivateImpl::FamilyMatcher{faceI->familyName.dcast().view(), nameHash});

  if (!familiesNode)
    return false;

  size_t index = indexOfFace(familiesNode->faces.data(), familiesNode->faces.size(), faceI);
  return index != SIZE_MAX;
}

BL_API_IMPL BLResult blFontManagerAddFace(BLFontManagerCore* self, const BLFontFaceCore* face) noexcept {
  using namespace bl::FontManagerInternal;

  BL_ASSERT(self->_d.isFontManager());
  BL_ASSERT(self->_d.isFontFace());

  if (!face->dcast().isValid())
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  BL_PROPAGATE(blFontManagerMakeMutable(self));

  BLFontManagerPrivateImpl* selfI = getImpl(self);
  BLFontFacePrivateImpl* faceI = bl::FontFaceInternal::getImpl(face);

  uint32_t nameHash = bl::HashOps::hashStringCI(faceI->familyName.dcast().view());

  BLLockGuard<BLSharedMutex> guard(selfI->mutex);
  bl::ArenaAllocator::StatePtr allocatorState = selfI->allocator.saveState();

  BLFontManagerPrivateImpl::FamiliesMapNode* familiesNode =
    selfI->familiesMap.get(BLFontManagerPrivateImpl::FamilyMatcher{faceI->familyName.dcast().view(), nameHash});

  if (!familiesNode) {
    familiesNode = selfI->allocator.newT<BLFontManagerPrivateImpl::FamiliesMapNode>(nameHash, faceI->familyName.dcast());
    if (!familiesNode)
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    // Reserve for only one item at the beginning. This helps to decrease memory footprint when loading a lot of font
    // faces that don't share family names.
    BLResult result = familiesNode->faces.reserve(1u);
    if (BL_UNLIKELY(result != BL_SUCCESS)) {
      blCallDtor(*familiesNode);
      selfI->allocator.restoreState(allocatorState);
      return result;
    }

    familiesNode->faces.append(face->dcast());
    selfI->familiesMap.insert(familiesNode);
  }
  else {
    size_t index = indexForInsertion(familiesNode->faces.data(), familiesNode->faces.size(), faceI);
    if (index == SIZE_MAX)
      return BL_SUCCESS;
    BL_PROPAGATE(familiesNode->faces.insert(index, face->dcast()));
  }

  selfI->faceCount++;
  return BL_SUCCESS;
}

// bl::FontManager - Query - API
// =============================

BL_API_IMPL BLResult blFontManagerQueryFacesByFamilyName(const BLFontManagerCore* self, const char* name, size_t nameSize, BLArrayCore* out) noexcept {
  using namespace bl::FontManagerInternal;
  BL_ASSERT(self->_d.isFontManager());

  if (BL_UNLIKELY(out->_d.rawType() != BL_OBJECT_TYPE_ARRAY_OBJECT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  {
    BLFontManagerPrivateImpl* selfI = getImpl(self);
    BLSharedLockGuard<BLSharedMutex> guard(selfI->mutex);

    PreparedQuery query;
    uint32_t candidateDiff = 0xFFFFFFFF;
    BLFontManagerPrivateImpl::FamiliesMapNode* candidate = nullptr;

    if (prepareQuery(selfI, name, nameSize, &query)) {
      BLFontManagerPrivateImpl::FamiliesMapNode* node = selfI->familiesMap.get(query);
      while (node) {
        uint32_t familyDiff = calcFamilyNameDiff(node->familyName.view(), query.name());
        if (candidateDiff > familyDiff) {
          candidateDiff = familyDiff;
          candidate = node;
        }
        node = node->next();
      }
    }

    if (candidate)
      return out->dcast<BLArray<BLFontFace>>().assign(candidate->faces);
  }

  // This is not considered to be an error, thus don't use blTraceError().
  out->dcast<BLArray<BLFontFace>>().clear();
  return BL_ERROR_FONT_NO_MATCH;
}

BL_API_IMPL BLResult blFontManagerQueryFace(
  const BLFontManagerCore* self,
  const char* name, size_t nameSize,
  const BLFontQueryProperties* properties,
  BLFontFaceCore* out) noexcept {

  using namespace bl::FontManagerInternal;
  BL_ASSERT(self->_d.isFontManager());

  if (!properties)
    properties = &defaultQueryProperties;

  BLFontQueryProperties sanitizedProperties;
  if (!sanitizeQueryProperties(sanitizedProperties, *properties))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  {
    BLFontManagerPrivateImpl* selfI = getImpl(self);
    BLSharedLockGuard<BLSharedMutex> guard(selfI->mutex);

    PreparedQuery query;
    QueryBestMatch bestMatch(&sanitizedProperties);

    if (prepareQuery(selfI, name, nameSize, &query)) {
      BLFontManagerPrivateImpl::FamiliesMapNode* node = selfI->familiesMap.nodesByHashCode(query.hashCode());
      while (node) {
        uint32_t familyDiff = calcFamilyNameDiff(node->familyName.view(), query.name());
        if (familyDiff != kQueryInvalidDiff) {
          for (const BLFontFace& face : node->faces.dcast<BLArray<BLFontFace>>())
            bestMatch.match(face, familyDiff);
        }
        node = node->next();
      }
    }

    if (bestMatch.hasFace())
      return out->dcast().assign(*bestMatch.face);
  }

  // This is not considered to be an error, thus don't use blTraceError().
  out->dcast().reset();
  return BL_ERROR_FONT_NO_MATCH;
}

// bl::FontManager - Runtime Registration
// ======================================

void blFontManagerRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  auto& defaultImpl = bl::FontManagerInternal::defaultImpl;

  defaultImpl.virt.base.destroy = bl::FontManagerInternal::destroyImpl;
  defaultImpl.virt.base.getProperty = blObjectImplGetProperty;
  defaultImpl.virt.base.setProperty = blObjectImplSetProperty;
  defaultImpl.impl.init(&defaultImpl.virt);

  blObjectDefaults[BL_OBJECT_TYPE_FONT_MANAGER]._d.initDynamic(
    BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_FONT_MANAGER) | BLObjectInfo::fromAbcp(1), &defaultImpl.impl);
}
