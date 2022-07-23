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

// BLFontManager - Globals
// =======================

static BLObjectEthernalVirtualImpl<BLFontManagerPrivateImpl, BLFontManagerVirt> blFontManagerDefaultImpl;

// BLFontManager - Constants
// =========================

static constexpr uint32_t BL_FONT_QUERY_INVALID_DIFF = 0xFFFFFFFFu;

enum BLFontPrecedenceBits : uint32_t {
  BL_FONT_QUERY_DIFF_FAMILY_NAME_SHIFT   = 24, // 0xFF000000 [8 bits].
  BL_FONT_QUERY_DIFF_STYLE_VALUE_SHIFT   = 22, // 0x00C00000 [2 bits].
  BL_FONT_QUERY_DIFF_STYLE_SIGN_SHIFT    = 21, // 0x00200000 [1 bit].
  BL_FONT_QUERY_DIFF_WEIGHT_VALUE_SHIFT  = 10, // 0x001FFC00 [11 bits].
  BL_FONT_QUERY_DIFF_WEIGHT_SIGN_SHIFT   =  9, // 0x00000200 [1 bit].
  BL_FONT_QUERY_DIFF_STRETCH_VALUE_SHIFT =  5, // 0x000001E0 [4 bits].
  BL_FONT_QUERY_DIFF_STRETCH_SIGN_SHIFT  =  4  // 0x00000010 [1 bit].
};

// BLFontManager - Aloc & Free Impl
// ================================

static BLResult blFontManagerImplInit(BLFontManagerCore* self) noexcept {
  BLFontManagerPrivateImpl* impl = blObjectDetailAllocImplT<BLFontManagerPrivateImpl>(self, BLObjectInfo::packType(BL_OBJECT_TYPE_FONT_MANAGER));
  if (BL_UNLIKELY(!impl))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  blCallCtor(*impl, &blFontManagerDefaultImpl.virt);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blFontManagerImplDestroy(BLObjectImpl* impl, uint32_t info) noexcept {
  blCallDtor(*static_cast<BLFontManagerPrivateImpl*>(impl));
  return blObjectDetailFreeImpl(impl, info);
}

// BLFontManager - API - Init & Destroy
// ====================================

BLResult blFontManagerInit(BLFontManagerCore* self) noexcept {
  self->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_MANAGER]._d;
  return BL_SUCCESS;
}

BLResult blFontManagerInitMove(BLFontManagerCore* self, BLFontManagerCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFontManager());

  self->_d = other->_d;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_MANAGER]._d;

  return BL_SUCCESS;

}

BLResult blFontManagerInitWeak(BLFontManagerCore* self, const BLFontManagerCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFontManager());

  return blObjectPrivateInitWeakTagged(self, other);
}

BLResult blFontManagerInitNew(BLFontManagerCore* self) noexcept {
  self->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_MANAGER]._d;
  return blFontManagerImplInit(self);
}

BLResult blFontManagerDestroy(BLFontManagerCore* self) noexcept {
  BL_ASSERT(self->_d.isFontManager());

  return blObjectPrivateReleaseVirtual(self);
}

// BLFontManager - API - Reset
// ===========================

BLResult blFontManagerReset(BLFontManagerCore* self) noexcept {
  BL_ASSERT(self->_d.isFontManager());

  return blObjectPrivateReplaceVirtual(self, static_cast<BLFontManagerCore*>(&blObjectDefaults[BL_OBJECT_TYPE_FONT_MANAGER]));
}

// BLFontManager - API - Assign
// ============================

BLResult blFontManagerAssignMove(BLFontManagerCore* self, BLFontManagerCore* other) noexcept {
  BL_ASSERT(self->_d.isFontManager());
  BL_ASSERT(other->_d.isFontManager());

  BLFontManagerCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_MANAGER]._d;
  return blObjectPrivateReplaceVirtual(self, &tmp);
}

BLResult blFontManagerAssignWeak(BLFontManagerCore* self, const BLFontManagerCore* other) noexcept {
  BL_ASSERT(self->_d.isFontManager());
  BL_ASSERT(other->_d.isFontManager());

  return blObjectPrivateAssignWeakVirtual(self, other);
}

// BLFontManager - API - Equals
// ============================

bool blFontManagerEquals(const BLFontManagerCore* a, const BLFontManagerCore* b) noexcept {
  BL_ASSERT(a->_d.isFontManager());
  BL_ASSERT(b->_d.isFontManager());

  return a->_d.impl == b->_d.impl;
}

// BLFontManager - API - Create
// ============================

BLResult blFontManagerCreate(BLFontManagerCore* self) noexcept {
  BL_ASSERT(self->_d.isFontManager());

  BLFontManagerCore newO;
  BL_PROPAGATE(blFontManagerImplInit(&newO));

  return blObjectPrivateReplaceVirtual(self, &newO);
}

// BLFontManager - API - Accessors
// ===============================

size_t blFontManagerGetFaceCount(const BLFontManagerCore* self) noexcept {
  BL_ASSERT(self->_d.isFontManager());

  BLFontManagerPrivateImpl* selfI = blFontManagerGetImpl(self);
  BLSharedLockGuard<BLSharedMutex> guard(selfI->mutex);

  return selfI->faceCount;
}

size_t blFontManagerGetFamilyCount(const BLFontManagerCore* self) noexcept {
  BL_ASSERT(self->_d.isFontManager());

  BLFontManagerPrivateImpl* selfI = blFontManagerGetImpl(self);
  BLSharedLockGuard<BLSharedMutex> guard(selfI->mutex);

  return selfI->familiesMap.size();
}

// BLFontManager - Internal Utilities
// ==================================

static BL_INLINE BLResult blFontManagerMakeMutable(BLFontManagerCore* self) noexcept {
  BL_ASSERT(self->_d.isFontManager());

  if (!self->dcast().isValid())
    return blFontManagerCreate(self);

  return BL_SUCCESS;
}

// BLFontManager - API - Font Face Management
// ==========================================

static BL_INLINE size_t blFontManagerIndexOfFace(const BLFontFace* array, size_t size, BLFontFaceImpl* faceI) noexcept {
  for (size_t i = 0; i < size; i++)
    if (array[i]._d.impl == faceI)
      return i;
  return SIZE_MAX;
}

static BL_INLINE uint32_t blFontManagerCalcFaceOrder(const BLFontFaceImpl* faceI) noexcept {
  uint32_t style = faceI->style;
  uint32_t weight = faceI->weight;

  return (style << BL_FONT_QUERY_DIFF_STYLE_VALUE_SHIFT) |
         (weight << BL_FONT_QUERY_DIFF_WEIGHT_VALUE_SHIFT);
}

static BL_INLINE size_t blFontManagerIndexForInsertion(const BLFontFace* array, size_t size, BLFontFaceImpl* faceI) noexcept {
  uint32_t faceOrder = blFontManagerCalcFaceOrder(faceI);
  size_t i;

  for (i = 0; i < size; i++) {
    BLFontFacePrivateImpl* storedFaceI = blFontFaceGetImpl(&array[i]);
    uint32_t storedFaceOrder = blFontManagerCalcFaceOrder(storedFaceI);
    if (storedFaceOrder >= faceOrder) {
      if (storedFaceOrder == faceOrder)
        return SIZE_MAX;
      break;
    }
  }

  return i;
}

bool blFontManagerHasFace(const BLFontManagerCore* self, const BLFontFaceCore* face) noexcept {
  BL_ASSERT(self->_d.isFontManager());

  BLFontManagerPrivateImpl* selfI = blFontManagerGetImpl(self);
  BLFontFacePrivateImpl* faceI = blFontFaceGetImpl(face);

  uint32_t nameHash = BLHashOps::hashStringCI(faceI->familyName.dcast().view());

  BLSharedLockGuard<BLSharedMutex> guard(selfI->mutex);
  BLFontManagerPrivateImpl::FamiliesMapNode* familiesNode =
    selfI->familiesMap.get(BLFontManagerPrivateImpl::FamilyMatcher{faceI->familyName.dcast().view(), nameHash});

  if (!familiesNode)
    return false;

  size_t index = blFontManagerIndexOfFace(familiesNode->faces.data(), familiesNode->faces.size(), faceI);
  return index != SIZE_MAX;
}

BLResult blFontManagerAddFace(BLFontManagerCore* self, const BLFontFaceCore* face) noexcept {
  if (!face->dcast().isValid())
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  BL_PROPAGATE(blFontManagerMakeMutable(self));

  BLFontManagerPrivateImpl* selfI = blFontManagerGetImpl(self);
  BLFontFacePrivateImpl* faceI = blFontFaceGetImpl(face);

  uint32_t nameHash = BLHashOps::hashStringCI(faceI->familyName.dcast().view());

  BLLockGuard<BLSharedMutex> guard(selfI->mutex);
  BLArenaAllocator::StatePtr allocatorState = selfI->allocator.saveState();

  BLFontManagerPrivateImpl::FamiliesMapNode* familiesNode =
    selfI->familiesMap.get(BLFontManagerPrivateImpl::FamilyMatcher{faceI->familyName.dcast().view(), nameHash});

  if (!familiesNode) {
    familiesNode = selfI->allocator.newT<BLFontManagerPrivateImpl::FamiliesMapNode>(nameHash, faceI->familyName.dcast());
    if (!familiesNode)
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    // Reserve for only one item at the beginning. This helps to decrease
    // memory footprint when loading a lot of font-faces that don't share
    // family names.
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
    size_t index = blFontManagerIndexForInsertion(familiesNode->faces.data(), familiesNode->faces.size(), faceI);
    if (index == SIZE_MAX)
      return BL_SUCCESS;
    BL_PROPAGATE(familiesNode->faces.insert(index, face->dcast()));
  }

  selfI->faceCount++;
  return BL_SUCCESS;
}

// BLFontManager - Query - Utilities
// =================================

static const BLFontQueryProperties blFontFaceDefaultQueryProperties = {
  BL_FONT_STYLE_NORMAL,
  BL_FONT_WEIGHT_NORMAL,
  BL_FONT_STRETCH_NORMAL
};

static bool blFontQuerySanitizeQueryProperties(BLFontQueryProperties& dst, const BLFontQueryProperties& src) noexcept {
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

// BLFontManager - Query - Prepared Query
// ======================================

struct BLFontPreparedQuery {
  BLStringView _name;
  uint32_t _hashCode;

  typedef BLFontManagerPrivateImpl::FamiliesMapNode FamiliesMapNode;

  BL_INLINE const BLStringView& name() const noexcept { return _name; };
  BL_INLINE uint32_t hashCode() const noexcept { return _hashCode; }

  BL_INLINE bool matches(const FamiliesMapNode* node) const noexcept { return node->familyName.equals(_name); }
};

static bool blFontManagerPrepareQuery(const BLFontManagerPrivateImpl* impl, const char* name, size_t nameSize, BLFontPreparedQuery* out) noexcept {
  blUnused(impl);

  if (nameSize == SIZE_MAX)
    nameSize = strlen(name);

  out->_name.reset(name, nameSize);
  out->_hashCode = BLHashOps::hashStringCI(name, nameSize);
  return nameSize != 0;
}

// BLFontManager - Query - Diff Calculation
// ========================================

static uint32_t blFontQueryCalcFamilyNameDiff(BLStringView aStr, BLStringView bStr) noexcept {
  if (aStr.size != bStr.size)
    return BL_FONT_QUERY_INVALID_DIFF;

  uint32_t diff = 0;

  for (size_t i = 0; i < aStr.size; i++) {
    uint32_t a = uint8_t(aStr.data[i]);
    uint32_t b = uint8_t(bStr.data[i]);

    if (a == b)
      continue;

    a = blAsciiToLower(a);
    b = blAsciiToLower(b);

    if (a != b)
      return BL_FONT_QUERY_INVALID_DIFF;

    diff++;
  }

  if (diff > 255)
    diff = 255;

  return diff << BL_FONT_QUERY_DIFF_FAMILY_NAME_SHIFT;
}

static uint32_t blFontQueryCalcPropertyDiff(const BLFontFaceImpl* faceI, const BLFontQueryProperties* properties) noexcept {
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

  diff |= styleDiff << BL_FONT_QUERY_DIFF_STYLE_VALUE_SHIFT;
  diff |= uint32_t(pStyle < fStyle) << BL_FONT_QUERY_DIFF_STYLE_SIGN_SHIFT;

  diff |= weightDiff << BL_FONT_QUERY_DIFF_WEIGHT_VALUE_SHIFT;
  diff |= uint32_t(pWeight < fWeight) << BL_FONT_QUERY_DIFF_WEIGHT_SIGN_SHIFT;

  diff |= stretchDiff << BL_FONT_QUERY_DIFF_STRETCH_VALUE_SHIFT;
  diff |= uint32_t(pStretch < fStretch) << BL_FONT_QUERY_DIFF_STRETCH_SIGN_SHIFT;

  return diff;
}

// BLFontManager - Query - Match
// =============================

class BLFontQueryBestMatch {
public:
  const BLFontQueryProperties* properties;
  const BLFontFace* face;
  uint32_t diff;

  BL_INLINE BLFontQueryBestMatch(const BLFontQueryProperties* properties) noexcept
    : properties(properties),
      face(nullptr),
      diff(0xFFFFFFFFu) {}

  BL_INLINE bool hasFace() const noexcept { return face != nullptr; }

  void match(const BLFontFace& faceIn, uint32_t baseDiff = 0) noexcept {
    uint32_t localDiff = baseDiff + blFontQueryCalcPropertyDiff(faceIn._impl(), properties);
    if (diff > localDiff) {
      face = &faceIn;
      diff = localDiff;
    }
  }
};

// BLFontManager - Query - API
// ===========================

BLResult blFontManagerQueryFacesByFamilyName(const BLFontManagerCore* self, const char* name, size_t nameSize, BLArrayCore* out) noexcept {
  if (BL_UNLIKELY(out->_d.rawType() != BL_OBJECT_TYPE_ARRAY_OBJECT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  {
    BLFontManagerPrivateImpl* selfI = blFontManagerGetImpl(self);
    BLSharedLockGuard<BLSharedMutex> guard(selfI->mutex);

    BLFontPreparedQuery query;
    uint32_t candidateDiff = 0xFFFFFFFF;
    BLFontManagerPrivateImpl::FamiliesMapNode* candidate = nullptr;

    if (blFontManagerPrepareQuery(selfI, name, nameSize, &query)) {
      BLFontManagerPrivateImpl::FamiliesMapNode* node = selfI->familiesMap.get(query);
      while (node) {
        uint32_t familyDiff = blFontQueryCalcFamilyNameDiff(node->familyName.view(), query.name());
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

BLResult blFontManagerQueryFace(
  const BLFontManagerCore* self,
  const char* name, size_t nameSize,
  const BLFontQueryProperties* properties,
  BLFontFaceCore* out) noexcept {

  if (!properties)
    properties = &blFontFaceDefaultQueryProperties;

  BLFontQueryProperties sanitizedProperties;
  if (!blFontQuerySanitizeQueryProperties(sanitizedProperties, *properties))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  {
    BLFontManagerPrivateImpl* selfI = blFontManagerGetImpl(self);
    BLSharedLockGuard<BLSharedMutex> guard(selfI->mutex);

    BLFontPreparedQuery query;
    BLFontQueryBestMatch bestMatch(&sanitizedProperties);

    if (blFontManagerPrepareQuery(selfI, name, nameSize, &query)) {
      BLFontManagerPrivateImpl::FamiliesMapNode* node = selfI->familiesMap.nodesByHashCode(query.hashCode());
      while (node) {
        uint32_t familyDiff = blFontQueryCalcFamilyNameDiff(node->familyName.view(), query.name());
        if (familyDiff != BL_FONT_QUERY_INVALID_DIFF) {
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

// BLFontManager - Runtime Registration
// ====================================

void blFontManagerRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  blFontManagerDefaultImpl.virt.base.destroy = blFontManagerImplDestroy;
  blFontManagerDefaultImpl.virt.base.getProperty = blObjectImplGetProperty;
  blFontManagerDefaultImpl.virt.base.setProperty = blObjectImplSetProperty;
  blFontManagerDefaultImpl.impl.init(&blFontManagerDefaultImpl.virt);

  blObjectDefaults[BL_OBJECT_TYPE_FONT_MANAGER]._d.initDynamic(
    BL_OBJECT_TYPE_FONT_MANAGER,
    BLObjectInfo{BL_OBJECT_INFO_IMMUTABLE_FLAG},
    &blFontManagerDefaultImpl.impl);
}
