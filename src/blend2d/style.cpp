// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "./api-build_p.h"
#include "./gradient.h"
#include "./pattern.h"
#include "./style_p.h"
#include "./variant_p.h"

// ============================================================================
// [BLStyle - Init / Destroy]
// ============================================================================

BLResult blStyleInit(BLStyleCore* self) noexcept {
  return blStyleInitNoneInline(self);
}

BLResult blStyleInitMove(BLStyleCore* self, BLStyleCore* other) noexcept {
  uint64_t q0 = other->u64Data[0];
  uint64_t q1 = other->u64Data[1];

  blStyleInitNoneInline(other);
  self->u64Data[0] = q0;
  self->u64Data[1] = q1;
  return BL_SUCCESS;
}

BLResult blStyleInitWeak(BLStyleCore* self, const BLStyleCore* other) noexcept {
  if (blDownCast(other)->isObject())
    return blStyleInitObjectInline(self, blImplIncRef(other->variant.impl), other->data.type);

  memcpy(self, other, sizeof(*self));
  return BL_SUCCESS;
}

BLResult blStyleInitRgba(BLStyleCore* self, const BLRgba* rgba) noexcept {
  if (blStyleIsValidRgba(*rgba))
    self->rgba = blClamp(*rgba, BLRgba(0.0f, 0.0f, 0.0f, 0.0f),
                                BLRgba(1.0f, 1.0f, 1.0f, 1.0f));
  else
    blStyleInitNoneInline(self);
  return BL_SUCCESS;
}

BLResult blStyleInitRgba32(BLStyleCore* self, uint32_t rgba32) noexcept {
  self->rgba.reset(BLRgba32(rgba32));
  return BL_SUCCESS;
}

BLResult blStyleInitRgba64(BLStyleCore* self, uint64_t rgba64) noexcept {
  self->rgba.reset(BLRgba64(rgba64));
  return BL_SUCCESS;
}

BLResult blStyleInitObject(BLStyleCore* self, const void* object) noexcept {
  BLVariantImpl* impl = static_cast<const BLVariant*>(object)->impl;
  uint32_t styleType = blStyleTypeFromImplType(impl->implType);

  if (BL_UNLIKELY(styleType == BL_STYLE_TYPE_NONE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  return blStyleInitObjectInline(self, blImplIncRef(impl), styleType);
}

BLResult blStyleDestroy(BLStyleCore* self) noexcept {
  blStyleDestroyInline(self);
  return BL_SUCCESS;
}

// ============================================================================
// [BLStyle - Reset]
// ============================================================================

BLResult blStyleReset(BLStyleCore* self) noexcept {
  blStyleDestroyInline(self);
  blStyleInitNoneInline(self);
  return BL_SUCCESS;
}

// ============================================================================
// [BLStyle - Common Functionality]
// ============================================================================

BLResult blStyleAssignMove(BLStyleCore* self, BLStyleCore* other) noexcept {
  BLStyleCore copy = *other;
  blStyleInitNoneInline(other);

  if (blDownCast(self)->isObject())
    blVariantImplRelease(self->variant.impl);

  *self = copy;
  return BL_SUCCESS;
}

BLResult blStyleAssignWeak(BLStyleCore* self, const BLStyleCore* other) noexcept {
  BLStyleCore copy = *self;

  if (blDownCast(other)->isObject())
    blStyleInitObjectInline(self, blImplIncRef(other->variant.impl), other->data.type);
  else
    *self = *other;

  if (blDownCast(&copy)->isObject())
    return blVariantImplRelease(copy.variant.impl);

  return BL_SUCCESS;
}

BLResult blStyleAssignRgba(BLStyleCore* self, const BLRgba* rgba) noexcept {
  if (blDownCast(self)->isObject())
    blVariantImplRelease(self->variant.impl);

  if (blStyleIsValidRgba(*rgba))
    self->rgba = blClamp(*rgba, BLRgba(0.0f, 0.0f, 0.0f, 0.0f),
                                BLRgba(1.0f, 1.0f, 1.0f, 1.0f));
  else
    blStyleInitNoneInline(self);

  return BL_SUCCESS;
}

BLResult blStyleAssignRgba32(BLStyleCore* self, uint32_t rgba32) noexcept {
  if (blDownCast(self)->isObject())
    blVariantImplRelease(self->variant.impl);

  self->rgba.reset(BLRgba32(rgba32));
  return BL_SUCCESS;
}

BLResult blStyleAssignRgba64(BLStyleCore* self, uint64_t rgba64) noexcept {
  if (blDownCast(self)->isObject())
    blVariantImplRelease(self->variant.impl);

  self->rgba.reset(BLRgba64(rgba64));
  return BL_SUCCESS;
}

BLResult blStyleAssignObject(BLStyleCore* self, const void* object) noexcept {
  BLVariantImpl* impl = static_cast<const BLVariant*>(object)->impl;
  uint32_t styleType = blStyleTypeFromImplType(impl->implType);

  if (BL_UNLIKELY(styleType == BL_STYLE_TYPE_NONE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  return blStyleInitObjectInline(self, blImplIncRef(impl), styleType);
}

// ============================================================================
// [BLStyle - Accessors]
// ============================================================================

BLResult blStyleGetType(const BLStyleCore* self) noexcept {
  uint32_t kNaN = blBitCast<uint32_t>(std::numeric_limits<float>::quiet_NaN());
  uint32_t result = self->data.type;
  if (self->data.tag == kNaN)
    result = BL_STYLE_TYPE_SOLID;
  return result;
}

BLResult blStyleGetRgba(const BLStyleCore* self, BLRgba* rgbaOut) noexcept {
  if (blDownCast(self)->isSolid()) {
    *rgbaOut = self->rgba;
    return BL_SUCCESS;
  }
  else {
    rgbaOut->reset();
    return blTraceError(BL_ERROR_INVALID_STATE);
  }
}

BLResult blStyleGetRgba32(const BLStyleCore* self, uint32_t* rgba32Out) noexcept {
  if (blDownCast(self)->isSolid()) {
    BLRgba32 c = BLRgba32(uint32_t(blRoundToInt(self->rgba.r * 255.0f)),
                          uint32_t(blRoundToInt(self->rgba.g * 255.0f)),
                          uint32_t(blRoundToInt(self->rgba.b * 255.0f)),
                          uint32_t(blRoundToInt(self->rgba.a * 255.0f)));
    *rgba32Out = c.value;
    return BL_SUCCESS;
  }
  else {
    *rgba32Out = 0;
    return blTraceError(BL_ERROR_INVALID_STATE);
  }
}

BLResult blStyleGetRgba64(const BLStyleCore* self, uint64_t* rgba64Out) noexcept {
  if (blDownCast(self)->isSolid()) {
    BLRgba64 c = BLRgba64(uint32_t(blRoundToInt(self->rgba.r * 65535.0f)),
                          uint32_t(blRoundToInt(self->rgba.g * 65535.0f)),
                          uint32_t(blRoundToInt(self->rgba.b * 65535.0f)),
                          uint32_t(blRoundToInt(self->rgba.a * 65535.0f)));
    *rgba64Out = c.value;
    return BL_SUCCESS;
  }
  else {
    *rgba64Out = 0;
    return blTraceError(BL_ERROR_INVALID_STATE);
  }
}

BLResult blStyleGetObject(const BLStyleCore* self, void* object) noexcept {
  if (!blDownCast(self)->isObject())
    return blTraceError(BL_ERROR_INVALID_STATE);

  uint32_t styleImplType = self->variant.impl->implType;
  uint32_t objectImplType = static_cast<BLVariant*>(object)->impl->implType;

  if (styleImplType != objectImplType)
    return blTraceError(BL_ERROR_INVALID_STATE);

  return blVariantAssignWeak(object, &self->variant);
}

// ============================================================================
// [BLStyle - Equality / Comparison]
// ============================================================================

bool blStyleEquals(const BLStyleCore* self, const BLStyleCore* other) noexcept {
  // Either Blue/Alpha part of RGBA color or style type and tag must match.
  if (self->u64Data[1] != other->u64Data[1])
    return false;

  // Either Red/Green part of RGBA color matches or both variants share the same impl.
  if (self->u64Data[0] == other->u64Data[0])
    return true;

  if (!blDownCast(self)->isObject())
    return false;

  // We know the `other` must be variant too, as we have already compared it.
  BL_ASSERT(blDownCast(other)->isObject());
  return blVariantEquals(&self->variant, &other->variant);
}

// ============================================================================
// [BLStyle - Unit Tests]
// ============================================================================

#if defined(BL_TEST)
UNIT(style) {
  INFO("Testing basic BLStyle operations");

  BLStyle a(BLRgba32(0xFFFFFFFFu));
  BLStyle b(BLRgba32(0xFFFFFFFFu));
  BLStyle none;

  BLGradient gradient;
  gradient.addStop(0.0, BLRgba32(0xFF000000u));
  gradient.addStop(1.0, BLRgba32(0xFFFFFFFFu));

  EXPECT(a.type() == BL_STYLE_TYPE_SOLID);
  EXPECT(a.isSolid() == true);

  EXPECT(b.type() == BL_STYLE_TYPE_SOLID);
  EXPECT(b.isSolid() == true);

  EXPECT(none.type() == BL_STYLE_TYPE_NONE);
  EXPECT(none.isNone() == true);

  EXPECT(a == b);
  EXPECT(a != none);

  EXPECT(a.assign(gradient) == BL_SUCCESS);
  EXPECT(a.type() == BL_STYLE_TYPE_GRADIENT);
  EXPECT(a.isGradient() == true);
  EXPECT(a != b);

  EXPECT(b.assign(gradient) == BL_SUCCESS);
  EXPECT(a == b);
}
#endif
