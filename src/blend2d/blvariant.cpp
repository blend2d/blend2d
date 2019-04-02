// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blarray_p.h"
#include "./blfont_p.h"
#include "./blgradient_p.h"
#include "./blimage_p.h"
#include "./blpath_p.h"
#include "./blpattern_p.h"
#include "./blpixelconverter_p.h"
#include "./blregion_p.h"
#include "./blruntime_p.h"
#include "./blstring_p.h"
#include "./blvariant_p.h"

// ============================================================================
// [BLVariant - None]
// ============================================================================

BLVariantCore blNone[BL_IMPL_TYPE_COUNT];

// ============================================================================
// [BLVariant - Internal]
// ============================================================================

struct BLVariantVirt {
  BLResult (BL_CDECL* destroy)(void* impl) BL_NOEXCEPT;
};

BLResult blVariantImplDelete(BLVariantImpl* impl) noexcept {
  uint32_t implType = impl->implType;
  switch (implType) {
    case BL_IMPL_TYPE_NULL:
      return BL_SUCCESS;

    case BL_IMPL_TYPE_STRING:
      return blStringImplDelete(reinterpret_cast<BLStringImpl*>(impl));

    case BL_IMPL_TYPE_ARRAY_I8:
    case BL_IMPL_TYPE_ARRAY_U8:
    case BL_IMPL_TYPE_ARRAY_I16:
    case BL_IMPL_TYPE_ARRAY_U16:
    case BL_IMPL_TYPE_ARRAY_I32:
    case BL_IMPL_TYPE_ARRAY_U32:
    case BL_IMPL_TYPE_ARRAY_I64:
    case BL_IMPL_TYPE_ARRAY_U64:
    case BL_IMPL_TYPE_ARRAY_F32:
    case BL_IMPL_TYPE_ARRAY_F64:
    case BL_IMPL_TYPE_ARRAY_VAR:
      return blArrayImplDelete(reinterpret_cast<BLArrayImpl*>(impl));

    case BL_IMPL_TYPE_PATH2D:
      return blPathImplDelete(reinterpret_cast<BLPathImpl*>(impl));

    case BL_IMPL_TYPE_REGION:
      return blRegionImplDelete(reinterpret_cast<BLRegionImpl*>(impl));

    case BL_IMPL_TYPE_IMAGE:
      return blImageImplDelete(reinterpret_cast<BLImageImpl*>(impl));

    case BL_IMPL_TYPE_GRADIENT:
      return blGradientImplDelete(reinterpret_cast<BLGradientImpl*>(impl));

    case BL_IMPL_TYPE_PATTERN:
      return blPatternImplDelete(reinterpret_cast<BLPatternImpl*>(impl));

    case BL_IMPL_TYPE_FONT:
      return blFontImplDelete(reinterpret_cast<BLFontImpl*>(impl));

    default: {
      uint32_t implTraits = impl->implTraits;
      if (implTraits & BL_IMPL_TRAIT_VIRT)
        return static_cast<const BLVariantVirt*>(impl->virt)->destroy(impl);

      // FATAL ERROR: Either a new impl-type was introduced or memory corrupted.
      blRuntimeFailure("blVariantImplDelete(): Cannot delete Impl of impl-type #<%u>", implType);
    }
  }
}

// ============================================================================
// [BLVariant - Init / Reset]
// ============================================================================

BLResult blVariantInit(void* self) noexcept {
  static_cast<BLVariant*>(self)->impl = BLVariant::none().impl;
  return BL_SUCCESS;
}

BLResult blVariantInitMove(void* self, void* other) noexcept {
  BLVariantImpl* otherI = static_cast<BLVariant*>(other)->impl;

  static_cast<BLVariant*>(other)->impl = blNone[otherI->implType].impl;
  static_cast<BLVariant*>(self)->impl = otherI;

  return BL_SUCCESS;
}

BLResult blVariantInitWeak(void* self, const void* other) noexcept {
  static_cast<BLVariant*>(self)->impl = blImplIncRef(static_cast<const BLVariant*>(other)->impl);
  return BL_SUCCESS;
}

BLResult blVariantReset(void* self) noexcept {
  BLVariantImpl* selfI = static_cast<BLVariant*>(self)->impl;
  static_cast<BLVariant*>(self)->impl = blNone[selfI->implType].impl;

  if (blImplDecRefAndTest(selfI))
    return blVariantImplDelete(selfI);
  return BL_SUCCESS;
}

// ============================================================================
// [BLVariant - Introspection]
// ============================================================================

uint32_t blVariantGetImplType(const void* self) noexcept {
  BLVariantImpl* selfI = static_cast<const BLVariant*>(self)->impl;
  return selfI->implType;
}

// ============================================================================
// [BLVariant - Assign]
// ============================================================================

BLResult blVariantAssignMove(void* self, void* other) noexcept {
  BLVariantImpl* selfI = static_cast<BLVariant*>(self)->impl;
  BLVariantImpl* otherI = static_cast<BLVariant*>(other)->impl;

  static_cast<BLVariant*>(other)->impl = blNone[otherI->implType].impl;
  static_cast<BLVariant*>(self)->impl = otherI;

  if (blImplDecRefAndTest(selfI))
    return blVariantImplDelete(selfI);
  return BL_SUCCESS;
}

BLResult blVariantAssignWeak(void* self, const void* other) noexcept {
  BLVariantImpl* selfI = static_cast<BLVariant*>(self)->impl;
  BLVariantImpl* otherI = blImplIncRef(static_cast<const BLVariant*>(other)->impl);

  static_cast<BLVariant*>(self)->impl = otherI;

  if (blImplDecRefAndTest(selfI))
    return blVariantImplDelete(selfI);
  return BL_SUCCESS;
}

// ============================================================================
// [BLVariant - Equals]
// ============================================================================

bool blVariantEquals(const void* a, const void* b) noexcept {
  const BLVariantImpl* aI = static_cast<const BLVariant*>(a)->impl;
  const BLVariantImpl* bI = static_cast<const BLVariant*>(b)->impl;

  uint32_t implType = aI->implType;
  if (implType != bI->implType)
    return false;

  switch (implType) {
    case BL_IMPL_TYPE_NULL:
      return true;

    case BL_IMPL_TYPE_STRING:
      return blStringEquals(reinterpret_cast<const BLStringCore*>(a), reinterpret_cast<const BLStringCore*>(b));

    case BL_IMPL_TYPE_ARRAY_I8:
    case BL_IMPL_TYPE_ARRAY_U8:
    case BL_IMPL_TYPE_ARRAY_I16:
    case BL_IMPL_TYPE_ARRAY_U16:
    case BL_IMPL_TYPE_ARRAY_I32:
    case BL_IMPL_TYPE_ARRAY_U32:
    case BL_IMPL_TYPE_ARRAY_I64:
    case BL_IMPL_TYPE_ARRAY_U64:
    case BL_IMPL_TYPE_ARRAY_F32:
    case BL_IMPL_TYPE_ARRAY_F64:
    case BL_IMPL_TYPE_ARRAY_VAR:
      return blArrayEquals(reinterpret_cast<const BLArrayCore*>(a), reinterpret_cast<const BLArrayCore*>(b));

    case BL_IMPL_TYPE_PATH2D:
      return blPathEquals(reinterpret_cast<const BLPathCore*>(a), reinterpret_cast<const BLPathCore*>(b));

    case BL_IMPL_TYPE_REGION:
      return blRegionEquals(reinterpret_cast<const BLRegionCore*>(a), reinterpret_cast<const BLRegionCore*>(b));

    case BL_IMPL_TYPE_IMAGE:
      return blImageEquals(reinterpret_cast<const BLImageCore*>(a), reinterpret_cast<const BLImageCore*>(b));

    case BL_IMPL_TYPE_GRADIENT:
      return blGradientEquals(reinterpret_cast<const BLGradientCore*>(a), reinterpret_cast<const BLGradientCore*>(b));

    case BL_IMPL_TYPE_PATTERN:
      return blPatternEquals(reinterpret_cast<const BLPatternCore*>(a), reinterpret_cast<const BLPatternCore*>(b));

    default:
      return aI == bI;
  }
}
