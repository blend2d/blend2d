// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

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
// [BLFontManager - Impl]
// ============================================================================

static BLResult BL_CDECL blFontManagerImplDestroy(BLFontManagerImpl* impl) noexcept {
  return BL_SUCCESS;
}

static BLFontManagerImpl* blFontManagerImplNew() noexcept {
  // TODO: FontManager.
  return nullptr;
}

// ============================================================================
// [BLFontManager - Init / Reset]
// ============================================================================

BLResult blFontManagerInit(BLFontManagerCore* self) noexcept {
  self->impl = &blNullFontManagerImpl;
  return BL_SUCCESS;
}

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

// TODO: FontManager.

// ============================================================================
// [Runtime Init]
// ============================================================================

void blFontManagerRtInit(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);

  // Initialize BLFontManager built-in null instance.
  BLFontManagerImpl* fontManagerI = new (&blNullFontManagerImpl) BLInternalFontManagerImpl(&blFontManagerVirt);
  blInitBuiltInNull(fontManagerI, BL_IMPL_TYPE_FONT_MANAGER, BL_IMPL_TRAIT_VIRT);
  blAssignBuiltInNull(fontManagerI);

  // Initialize BLFontManager virtual functions.
  blFontManagerVirt.destroy = blFontManagerImplDestroy;
}
