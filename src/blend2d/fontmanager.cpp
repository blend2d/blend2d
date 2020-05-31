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

#include "./api-build_p.h"
#include "./filesystem.h"
#include "./font_p.h"
#include "./fontmanager_p.h"
#include "./runtime_p.h"
#include "./string_p.h"
#include "./support_p.h"

// ============================================================================
// [Global Variables]
// ============================================================================

static BLWrap<BLInternalFontManagerImpl> blNullFontManagerImpl;
static BLFontManagerVirt blFontManagerVirt;

// ============================================================================
// [Constants]
// ============================================================================

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

// ============================================================================
// [BLFontManager - Impl]
// ============================================================================

static BLResult BL_CDECL blFontManagerImplDestroy(BLFontManagerImpl* impl_) noexcept {
  BLInternalFontManagerImpl* impl = static_cast<BLInternalFontManagerImpl*>(impl_);
  impl->~BLInternalFontManagerImpl();
  return blRuntimeFreeImpl(impl, sizeof(BLInternalFontManagerImpl), impl->memPoolData);
}

static BLFontManagerImpl* blFontManagerImplNew() noexcept {
  uint16_t memPoolData;
  void* p = blRuntimeAllocImpl(sizeof(BLInternalFontManagerImpl), &memPoolData);

  if (!p)
    return nullptr;

  return new(p) BLInternalFontManagerImpl(&blFontManagerVirt, memPoolData);
}

// ============================================================================
// [BLFontManager - Init / Destroy]
// ============================================================================

BLResult blFontManagerInit(BLFontManagerCore* self) noexcept {
  self->impl = &blNullFontManagerImpl;
  return BL_SUCCESS;
}

BLResult blFontManagerInitNew(BLFontManagerCore* self) noexcept {
  BLResult result = BL_SUCCESS;
  BLFontManagerImpl* impl = blFontManagerImplNew();

  if (BL_UNLIKELY(!impl)) {
    impl = &blNullFontManagerImpl;
    result = blTraceError(BL_ERROR_OUT_OF_MEMORY);
  }

  self->impl = impl;
  return result;
}

BLResult blFontManagerDestroy(BLFontManagerCore* self) noexcept {
  BLFontManagerImpl* selfI = self->impl;
  self->impl = nullptr;
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLFontManager - Reset]
// ============================================================================

BLResult blFontManagerReset(BLFontManagerCore* self) noexcept {
  BLFontManagerImpl* selfI = self->impl;
  self->impl = &blNullFontManagerImpl;
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLFontManager - Assign]
// ============================================================================

BLResult blFontManagerAssignMove(BLFontManagerCore* self, BLFontManagerCore* other) noexcept {
  BLFontManagerImpl* selfI = self->impl;
  BLFontManagerImpl* otherI = other->impl;

  self->impl = otherI;
  other->impl = &blNullFontManagerImpl;

  return blImplReleaseVirt(selfI);
}

BLResult blFontManagerAssignWeak(BLFontManagerCore* self, const BLFontManagerCore* other) noexcept {
  BLFontManagerImpl* selfI = self->impl;
  BLFontManagerImpl* otherI = other->impl;

  self->impl = blImplIncRef(otherI);
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLFontManager - Equals]
// ============================================================================

bool blFontManagerEquals(const BLFontManagerCore* a, const BLFontManagerCore* b) noexcept {
  return a->impl == b->impl;
}

// ============================================================================
// [BLFontManager - Create]
// ============================================================================

BLResult blFontManagerCreate(BLFontManagerCore* self) noexcept {
  BLFontManagerImpl* oldI = blInternalCast(self->impl);
  BLFontManagerImpl* newI = blFontManagerImplNew();

  if (BL_UNLIKELY(!newI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  self->impl = newI;
  return blImplReleaseVirt(oldI);
}

// ============================================================================
// [BLFontManager - Accessors]
// ============================================================================

size_t blFontManagerGetFaceCount(const BLFontManagerCore* self) noexcept {
  BLInternalFontManagerImpl* selfI = blInternalCast(self->impl);
  BLSharedLockGuard<BLSharedMutex> guard(selfI->mutex);

  return selfI->faceCount;
}

size_t blFontManagerGetFamilyCount(const BLFontManagerCore* self) noexcept {
  BLInternalFontManagerImpl* selfI = blInternalCast(self->impl);
  BLSharedLockGuard<BLSharedMutex> guard(selfI->mutex);

  return selfI->familiesMap.size();
}

// ============================================================================
// [BLFontManager - Internal Utilities]
// ============================================================================

static BL_INLINE BLResult blFontManagerMakeMutable(BLFontManagerCore* self) noexcept {
  if (self->impl == &blNullFontManagerImpl)
    return blFontManagerCreate(self);
  return BL_SUCCESS;
}

// ============================================================================
// [BLFontManager - Face Management]
// ============================================================================

static BL_INLINE size_t blFontManagerIndexOfFace(const BLFontFace* array, size_t size, BLFontFaceImpl* faceI) noexcept {
  for (size_t i = 0; i < size; i++)
    if (array[i].impl == faceI)
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
    BLFontFaceImpl* storedFaceI = array[i].impl;
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
  BLInternalFontManagerImpl* selfI = blInternalCast(self->impl);

  BLInternalFontFaceImpl* faceI = blInternalCast(face->impl);
  uint32_t nameHash = blHashStringCI(faceI->familyName.view());

  BLSharedLockGuard<BLSharedMutex> guard(selfI->mutex);
  BLInternalFontManagerImpl::FamiliesMapNode* familiesNode =
    selfI->familiesMap.get(BLInternalFontManagerImpl::FamilyMatcher{faceI->familyName.view(), nameHash});

  if (!familiesNode)
    return false;

  size_t index = blFontManagerIndexOfFace(familiesNode->faces.data(), familiesNode->faces.size(), faceI);
  return index != SIZE_MAX;
}

BLResult blFontManagerAddFace(BLFontManagerCore* self, const BLFontFaceCore* face) noexcept {
  if (blDownCast(face)->isNone())
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  BL_PROPAGATE(blFontManagerMakeMutable(self));

  BLInternalFontFaceImpl* faceI = blInternalCast(face->impl);
  uint32_t nameHash = blHashStringCI(faceI->familyName.view());

  BLInternalFontManagerImpl* selfI = blInternalCast(self->impl);
  BLLockGuard<BLSharedMutex> guard(selfI->mutex);

  BLZoneAllocator::StatePtr zoneState = selfI->zone.saveState();

  BLInternalFontManagerImpl::FamiliesMapNode* familiesNode =
    selfI->familiesMap.get(BLInternalFontManagerImpl::FamilyMatcher{faceI->familyName.view(), nameHash});

  if (!familiesNode) {
    familiesNode = selfI->zone.newT<BLInternalFontManagerImpl::FamiliesMapNode>(nameHash, faceI->familyName);
    if (!familiesNode)
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    // Reserve for only one item at the beginning. This helps to decrease
    // memory footprint when loading a lot of font-faces that don't share
    // family names.
    BLResult result = familiesNode->faces.reserve(1u);
    if (BL_UNLIKELY(result != BL_SUCCESS)) {
      blCallDtor(*familiesNode);
      selfI->zone.restoreState(zoneState);
      return result;
    }

    familiesNode->faces.append(*blDownCast(face));
    selfI->familiesMap.insert(familiesNode);
  }
  else {
    size_t index = blFontManagerIndexForInsertion(familiesNode->faces.data(), familiesNode->faces.size(), faceI);
    if (index == SIZE_MAX)
      return BL_SUCCESS;
    BL_PROPAGATE(familiesNode->faces.insert(index, *blDownCast(face)));
  }

  selfI->faceCount++;
  return BL_SUCCESS;
}

// ============================================================================
// [BLFontManager - Query - Utilities]
// ============================================================================

static const BLFontQueryProperties blFontFaceDefaultQueryProperties = {
  BL_FONT_STYLE_NORMAL,
  BL_FONT_WEIGHT_NORMAL,
  BL_FONT_STRETCH_NORMAL
};

static bool blFontQuerySanitizeQueryProperties(BLFontQueryProperties& dst, const BLFontQueryProperties& src) noexcept {
  bool valid = src.weight <= 1000u &&
               src.style < BL_FONT_STYLE_COUNT &&
               src.stretch <= BL_FONT_STRETCH_ULTRA_EXPANDED;
  if (!valid)
    return false;

  dst.style = src.style;
  dst.weight = src.weight ? src.weight : BL_FONT_WEIGHT_NORMAL;
  dst.stretch = src.stretch ? src.stretch : BL_FONT_STRETCH_NORMAL;

  return true;
}

// ============================================================================
// [BLFontManager - Query - Prepared Query]
// ============================================================================

struct BLFontPreparedQuery {
  BLStringView _name;
  uint32_t _hashCode;

  typedef BLInternalFontManagerImpl::FamiliesMapNode FamiliesMapNode;

  BL_INLINE const BLStringView& name() const noexcept { return _name; };
  BL_INLINE uint32_t hashCode() const noexcept { return _hashCode; }

  BL_INLINE bool matches(const FamiliesMapNode* node) const noexcept { return node->familyName.equals(_name); }
};

static bool blFontManagerPrepareQuery(const BLInternalFontManagerImpl* impl, const char* name, size_t nameSize, BLFontPreparedQuery* out) noexcept {
  blUnused(impl);

  if (nameSize == SIZE_MAX)
    nameSize = strlen(name);

  out->_name.reset(name, nameSize);
  out->_hashCode = blHashStringCI(name, nameSize);
  return nameSize != 0;
}

// ============================================================================
// [BLFontManager - Query - Diff Calculation]
// ============================================================================

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

// ============================================================================
// [BLFontManager - Query - Match]
// ============================================================================

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
    uint32_t localDiff = baseDiff + blFontQueryCalcPropertyDiff(faceIn.impl, properties);
    if (diff > localDiff) {
      face = &faceIn;
      diff = localDiff;
    }
  }
};

// ============================================================================
// [BLFontManager - Query - API]
// ============================================================================

BLResult blFontManagerQueryFacesByFamilyName(const BLFontManagerCore* self, const char* name, size_t nameSize, BLArrayCore* out) noexcept {
  if (BL_UNLIKELY(out->impl->implType != BL_IMPL_TYPE_ARRAY_VAR))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLInternalFontManagerImpl* selfI = blInternalCast(self->impl);
  BLSharedLockGuard<BLSharedMutex> guard(selfI->mutex);

  BLFontPreparedQuery query;

  uint32_t candidateDiff = 0xFFFFFFFF;
  BLInternalFontManagerImpl::FamiliesMapNode* candidate = nullptr;

  if (blFontManagerPrepareQuery(selfI, name, nameSize, &query)) {
    BLInternalFontManagerImpl::FamiliesMapNode* node = selfI->familiesMap.get(query);
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

  BLInternalFontManagerImpl* selfI = blInternalCast(self->impl);
  BLSharedLockGuard<BLSharedMutex> guard(selfI->mutex);

  BLFontPreparedQuery query;
  BLFontQueryBestMatch bestMatch(&sanitizedProperties);

  if (blFontManagerPrepareQuery(selfI, name, nameSize, &query)) {
    BLInternalFontManagerImpl::FamiliesMapNode* node = selfI->familiesMap.nodesByHashCode(query.hashCode());
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
    return blDownCast(out)->assign(*bestMatch.face);

  // This is not considered to be an error, thus don't use blTraceError().
  blDownCast(out)->reset();
  return BL_ERROR_FONT_NO_MATCH;
}

// ============================================================================
// [BLFontManager - Runtime]
// ============================================================================

void blFontManagerOnInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  // Initialize BLFontManager virtual functions.
  blFontManagerVirt.destroy = blFontManagerImplDestroy;

  // Initialize BLFontManager built-in null instance.
  BLFontManagerImpl* fontManagerI = new (&blNullFontManagerImpl) BLInternalFontManagerImpl(&blFontManagerVirt, 0);
  blInitBuiltInNull(fontManagerI, BL_IMPL_TYPE_FONT_MANAGER, BL_IMPL_TRAIT_VIRT);
  blAssignBuiltInNull(fontManagerI);
}
