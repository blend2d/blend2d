// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "./api-build_p.h"
#include "./array_p.h"
#include "./image_p.h"
#include "./imagecodec.h"
#include "./runtime_p.h"
#include "./string_p.h"
#include "./support_p.h"
#include "./codec/bmpcodec_p.h"
#include "./codec/jpegcodec_p.h"
#include "./codec/pngcodec_p.h"
#include "./threading/mutex_p.h"

// ============================================================================
// [Global Variables]
// ============================================================================

static BLImageCodecVirt blNullImageCodecVirt;
static BLImageDecoderVirt blNullImageDecoderVirt;
static BLImageEncoderVirt blNullImageEncoderVirt;

static BLWrap<BLImageCodecImpl> blNullImageCodecImpl;
static BLWrap<BLImageEncoderImpl> blNullImageEncoderImpl;
static BLWrap<BLImageDecoderImpl> blNullImageDecoderImpl;

static BLWrap<BLArray<BLImageCodec>> blImageCodecs;
static BLWrap<BLSharedMutex> blImageCodecsMutex;

static const char blEmptyCString[] = "";

// ============================================================================
// [BLImageCodec - Init / Destroy]
// ============================================================================

BLResult blImageCodecInit(BLImageCodecCore* self) noexcept {
  self->impl = &blNullImageCodecImpl;
  return BL_SUCCESS;
}

BLResult blImageCodecDestroy(BLImageCodecCore* self) noexcept {
  BLImageCodecImpl* selfI = self->impl;
  self->impl = nullptr;
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLImageCodec - Reset]
// ============================================================================

BLResult blImageCodecReset(BLImageCodecCore* self) noexcept {
  BLImageCodecImpl* selfI = self->impl;
  self->impl = &blNullImageCodecImpl;
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLImageCodec - Assign]
// ============================================================================

BLResult blImageCodecAssignWeak(BLImageCodecCore* self, const BLImageCodecCore* other) noexcept {
  BLImageCodecImpl* selfI = self->impl;
  BLImageCodecImpl* otherI = other->impl;

  self->impl = blImplIncRef(otherI);
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLImageCodec - Find Internal]
// ============================================================================

static bool blImageCodecMatchExtension(const char* extensions, const char* match, size_t matchSize) noexcept {
  while (*extensions) {
    // Match each extension in `extensions` string list delimited by '|'.
    const char* p = extensions;
    while (*p && *p != '|')
      p++;

    size_t extSize = (size_t)(p - extensions);
    if (extSize == matchSize && blMemEqI(extensions, match, matchSize))
      return true;

    if (*p == '|')
      p++;
    extensions = p;
  }
  return false;
}

// Returns possibly advanced `match` and fixes the `size`.
static const char* blImageCodecKeepOnlyExtensionInMatch(const char* match, size_t& size) noexcept {
  const char* end = match + size;
  const char* p = end;

  while (p != match && p[-1] != '.')
    p--;

  size = (size_t)(end - p);
  return p;
}

static BLResult blImageCodecFindByNameInternal(BLImageCodecCore* self, const char* name, size_t size, const BLArrayCore* codecs) noexcept {
  for (const auto& codec : codecs->dcast<BLArray<BLImageCodec>>().view())
    if (blStrEqI(codec.name(), name, size))
      return blImageCodecAssignWeak(self, &codec);
  return blTraceError(BL_ERROR_IMAGE_NO_MATCHING_CODEC);
}

static BLResult blImageCodecFindByExtensionInternal(BLImageCodecCore* self, const char* name, size_t size, const BLArrayCore* codecs) noexcept {
  for (const auto& codec : codecs->dcast<BLArray<BLImageCodec>>().view())
    if (blImageCodecMatchExtension(codec.extensions(), name, size))
      return blImageCodecAssignWeak(self, &codec);
  return blTraceError(BL_ERROR_IMAGE_NO_MATCHING_CODEC);
}

static BLResult blImageCodecFindByDataInternal(BLImageCodecCore* self, const void* data, size_t size, const BLArrayCore* codecs) noexcept {
  uint32_t bestScore = 0;
  const BLImageCodec* candidate = nullptr;

  for (const auto& codec : codecs->dcast<BLArray<BLImageCodec>>().view()) {
    uint32_t score = codec.inspectData(data, size);
    if (bestScore < score) {
      bestScore = score;
      candidate = &codec;
    }
  }

  if (candidate)
    return blImageCodecAssignWeak(self, candidate);

  return blTraceError(BL_ERROR_IMAGE_NO_MATCHING_CODEC);
}

// ============================================================================
// [BLImageCodec - Interface]
// ============================================================================

uint32_t blImageCodecInspectData(const BLImageCodecCore* self, const void* data, size_t size) noexcept {
  BLImageCodecImpl* selfI = self->impl;
  return selfI->virt->inspectData(selfI, static_cast<const uint8_t*>(data), size);
}

BLResult blImageCodecFindByName(BLImageCodecCore* self, const char* name, size_t size, const BLArrayCore* codecs) noexcept {
  if (size == SIZE_MAX)
    size = strlen(name);

  if (!size)
    return blTraceError(BL_ERROR_IMAGE_NO_MATCHING_CODEC);

  if (codecs)
    return blImageCodecFindByNameInternal(self, name, size, codecs);
  else
    return blImageCodecsMutex->protectShared([&] { return blImageCodecFindByNameInternal(self, name, size, &blImageCodecs); });
}

BLResult blImageCodecFindByExtension(BLImageCodecCore* self, const char* name, size_t size, const BLArrayCore* codecs) noexcept {
  if (size == SIZE_MAX)
    size = strlen(name);

  name = blImageCodecKeepOnlyExtensionInMatch(name, size);
  if (codecs)
    return blImageCodecFindByExtensionInternal(self, name, size, codecs);
  else
    return blImageCodecsMutex->protectShared([&] { return blImageCodecFindByExtensionInternal(self, name, size, &blImageCodecs); });
}

BLResult blImageCodecFindByData(BLImageCodecCore* self, const void* data, size_t size, const BLArrayCore* codecs) noexcept {
  if (codecs)
    return blImageCodecFindByDataInternal(self, data, size, codecs);
  else
    return blImageCodecsMutex->protectShared([&] { return blImageCodecFindByDataInternal(self, data, size, &blImageCodecs); });
}

BLResult blImageCodecCreateDecoder(const BLImageCodecCore* self, BLImageDecoderCore* dst) noexcept {
  BLImageCodecImpl* selfI = self->impl;
  return selfI->virt->createDecoder(selfI, dst);
}

BLResult blImageCodecCreateEncoder(const BLImageCodecCore* self, BLImageEncoderCore* dst) noexcept {
  BLImageCodecImpl* selfI = self->impl;
  return selfI->virt->createEncoder(selfI, dst);
}

// ============================================================================
// [BLImageCodec - BuiltIn]
// ============================================================================

BLResult blImageCodecArrayInitBuiltInCodecs(BLArrayCore* self) noexcept {
  self->impl = blImageCodecsMutex->protectShared([&] { return blImplIncRef(blImageCodecs->impl); });
  return BL_SUCCESS;
}

BLResult blImageCodecArrayAssignBuiltInCodecs(BLArrayCore* self) noexcept {
  BLArrayImpl* oldI = self->impl;
  self->impl = blImageCodecsMutex->protectShared([&] { return blImplIncRef(blImageCodecs->impl); });
  return blArrayImplRelease(oldI);
}

BLResult blImageCodecAddToBuiltIn(const BLImageCodecCore* codec) noexcept {
  return blImageCodecsMutex->protect([&] {
    size_t i = blImageCodecs->indexOf(*blDownCast(codec));
    if (i != SIZE_MAX)
      return blTraceError(BL_ERROR_ALREADY_EXISTS);
    return blImageCodecs->append(blDownCast(*codec));
  });
}

BLResult blImageCodecRemoveFromBuiltIn(const BLImageCodecCore* codec) noexcept {
  return blImageCodecsMutex->protect([&] {
    size_t i = blImageCodecs->indexOf(*blDownCast(codec));
    if (i == SIZE_MAX)
      return blTraceError(BL_ERROR_NO_ENTRY);
    return blImageCodecs->remove(i);
  });
}

// ============================================================================
// [BLImageCodec - Virtual Functions]
// ============================================================================

BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)

static BLResult BL_CDECL blImageCodecImplDestroy(BLImageCodecImpl* impl) noexcept { return BL_SUCCESS; }
static uint32_t BL_CDECL blImageCodecImplInspectData(const BLImageCodecImpl* impl, const uint8_t* data, size_t size) noexcept { return 0; }
static BLResult BL_CDECL blImageCodecImplCreateDecoder(const BLImageCodecImpl* impl, BLImageDecoderCore* dst) noexcept { return BL_ERROR_IMAGE_DECODER_NOT_PROVIDED; }
static BLResult BL_CDECL blImageCodecImplCreateEncoder(const BLImageCodecImpl* impl, BLImageEncoderCore* dst) noexcept { return BL_ERROR_IMAGE_ENCODER_NOT_PROVIDED; }

BL_DIAGNOSTIC_POP

// ============================================================================
// [BLImageDecoder - Init / Destroy]
// ============================================================================

BLResult blImageDecoderInit(BLImageDecoderCore* self) noexcept {
  self->impl = &blNullImageDecoderImpl;
  return BL_SUCCESS;
}

BLResult blImageDecoderDestroy(BLImageDecoderCore* self) noexcept {
  BLImageDecoderImpl* selfI = self->impl;
  self->impl = nullptr;
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLImageDecoder - Reset]
// ============================================================================

BLResult blImageDecoderReset(BLImageDecoderCore* self) noexcept {
  BLImageDecoderImpl* selfI = self->impl;
  self->impl = &blNullImageDecoderImpl;
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLImageDecoder - Assign]
// ============================================================================

BLResult blImageDecoderAssignMove(BLImageDecoderCore* self, BLImageDecoderCore* other) noexcept {
  BLImageDecoderImpl* selfI = self->impl;
  BLImageDecoderImpl* otherI = other->impl;

  self->impl = otherI;
  other->impl = &blNullImageDecoderImpl;

  return blImplReleaseVirt(selfI);
}

BLResult blImageDecoderAssignWeak(BLImageDecoderCore* self, const BLImageDecoderCore* other) noexcept {
  BLImageDecoderImpl* selfI = self->impl;
  BLImageDecoderImpl* otherI = other->impl;

  self->impl = blImplIncRef(otherI);
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLImageDecoder - Interface]
// ============================================================================

BLResult blImageDecoderRestart(BLImageDecoderCore* self) noexcept {
  BLImageDecoderImpl* impl = self->impl;
  return impl->virt->restart(impl);
}

BLResult blImageDecoderReadInfo(BLImageDecoderCore* self, BLImageInfo* infoOut, const uint8_t* data, size_t size) noexcept {
  BLImageDecoderImpl* impl = self->impl;
  return impl->virt->readInfo(impl, infoOut, data, size);
}

BLResult blImageDecoderReadFrame(BLImageDecoderCore* self, BLImageCore* imageOut, const uint8_t* data, size_t size) noexcept {
  BLImageDecoderImpl* impl = self->impl;
  return impl->virt->readFrame(impl, imageOut, data, size);
}

// ============================================================================
// [BLImageDecoder - Virtual Functions]
// ============================================================================

BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)

static BLResult BL_CDECL blImageDecoderImplDestroy(BLImageDecoderImpl* impl) noexcept { return BL_SUCCESS; }
static uint32_t BL_CDECL blImageDecoderImplRestart(BLImageDecoderImpl* impl) noexcept { return BL_ERROR_INVALID_STATE; }
static BLResult BL_CDECL blImageDecoderImplReadInfo(BLImageDecoderImpl* impl, BLImageInfo* infoOut, const uint8_t* data, size_t size) noexcept { return BL_ERROR_INVALID_STATE; }
static BLResult BL_CDECL blImageDecoderImplReadFrame(BLImageDecoderImpl* impl, BLImageCore* imageOut, const uint8_t* data, size_t size) noexcept { return BL_ERROR_INVALID_STATE; }

BL_DIAGNOSTIC_POP

// ============================================================================
// [BLImageEncoder - Init / Destroy]
// ============================================================================

BLResult blImageEncoderInit(BLImageEncoderCore* self) noexcept {
  self->impl = &blNullImageEncoderImpl;
  return BL_SUCCESS;
}

BLResult blImageEncoderDestroy(BLImageEncoderCore* self) noexcept {
  BLImageEncoderImpl* selfI = self->impl;
  self->impl = nullptr;
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLImageEncoder - Reset]
// ============================================================================

BLResult blImageEncoderReset(BLImageEncoderCore* self) noexcept {
  BLImageEncoderImpl* selfI = self->impl;
  self->impl = &blNullImageEncoderImpl;
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLImageEncoder - Assign]
// ============================================================================

BLResult blImageEncoderAssignMove(BLImageEncoderCore* self, BLImageEncoderCore* other) noexcept {
  BLImageEncoderImpl* selfI = self->impl;
  BLImageEncoderImpl* otherI = other->impl;

  self->impl = otherI;
  other->impl = &blNullImageEncoderImpl;

  return blImplReleaseVirt(selfI);
}

BLResult blImageEncoderAssignWeak(BLImageEncoderCore* self, const BLImageEncoderCore* other) noexcept {
  BLImageEncoderImpl* selfI = self->impl;
  BLImageEncoderImpl* otherI = other->impl;

  self->impl = blImplIncRef(otherI);
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLImageEncoder - Interface]
// ============================================================================

BLResult blImageEncoderRestart(BLImageEncoderCore* self) noexcept {
  BLImageEncoderImpl* impl = self->impl;
  return impl->virt->restart(impl);
}

BLResult blImageEncoderWriteFrame(BLImageEncoderCore* self, BLArrayCore* dst, const BLImageCore* src) noexcept {
  BLImageEncoderImpl* impl = self->impl;
  return impl->virt->writeFrame(impl, dst, src);
}

// ============================================================================
// [BLImageEncoder - Virtual Functions]
// ============================================================================

BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)

static BLResult BL_CDECL blImageEncoderImplDestroy(BLImageEncoderImpl* impl) noexcept { return BL_SUCCESS; }
static uint32_t BL_CDECL blImageEncoderImplRestart(BLImageEncoderImpl* impl) noexcept { return BL_ERROR_INVALID_STATE; }
static BLResult BL_CDECL blImageEncoderImplWriteFrame(BLImageEncoderImpl* impl, BLArrayCore* dst, const BLImageCore* image) noexcept { return BL_ERROR_INVALID_STATE; }

BL_DIAGNOSTIC_POP

// ============================================================================
// [BLImageCodec - Runtime Init]
// ============================================================================

static void BL_CDECL blImageCodecRtShutdown(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);
  blImageCodecs.destroy();
  blImageCodecsMutex.destroy();
}

void blImageCodecRtInit(BLRuntimeContext* rt) noexcept {
  blImageCodecsMutex.init();

  BLImageCodecVirt* codecV = &blNullImageCodecVirt;
  codecV->destroy = blImageCodecImplDestroy;
  codecV->inspectData = blImageCodecImplInspectData;
  codecV->createDecoder = blImageCodecImplCreateDecoder;
  codecV->createEncoder = blImageCodecImplCreateEncoder;

  BLImageCodecImpl* codecI = &blNullImageCodecImpl;
  blInitBuiltInNull(codecI, BL_IMPL_TYPE_IMAGE_CODEC, BL_IMPL_TRAIT_VIRT);
  codecI->virt = codecV;
  codecI->name = blEmptyCString;
  codecI->vendor = blEmptyCString;
  codecI->mimeType = blEmptyCString;
  codecI->extensions = blEmptyCString;
  blAssignBuiltInNull(codecI);

  BLImageDecoderVirt* decoderV = &blNullImageDecoderVirt;
  decoderV->destroy = blImageDecoderImplDestroy;
  decoderV->restart = blImageDecoderImplRestart;
  decoderV->readInfo = blImageDecoderImplReadInfo;
  decoderV->readFrame = blImageDecoderImplReadFrame;

  BLImageDecoderImpl* decoderI = &blNullImageDecoderImpl;
  blInitBuiltInNull(decoderI, BL_IMPL_TYPE_IMAGE_DECODER, BL_IMPL_TRAIT_VIRT);
  decoderI->virt = decoderV;
  decoderI->lastResult = BL_ERROR_INVALID_STATE;
  blAssignBuiltInNull(decoderI);

  BLImageEncoderVirt* encoderV = &blNullImageEncoderVirt;
  encoderV->destroy = blImageEncoderImplDestroy;
  encoderV->restart = blImageEncoderImplRestart;
  encoderV->writeFrame = blImageEncoderImplWriteFrame;

  BLImageEncoderImpl* encoderI = &blNullImageEncoderImpl;
  blInitBuiltInNull(encoderI, BL_IMPL_TYPE_IMAGE_ENCODER, BL_IMPL_TRAIT_VIRT);
  encoderI->virt = encoderV;
  encoderI->lastResult = BL_ERROR_INVALID_STATE;
  blAssignBuiltInNull(encoderI);

  // Register built-in codecs.
  BLArray<BLImageCodec>* codecs = blImageCodecs.init();
  BLImageCodecCore bmpCodec { blBmpCodecRtInit(rt) };
  BLImageCodecCore jpegCodec { blJpegCodecRtInit(rt) };
  BLImageCodecCore pngCodec { blPngCodecRtInit(rt) };

  codecs->append(blDownCast(bmpCodec));
  codecs->append(blDownCast(jpegCodec));
  codecs->append(blDownCast(pngCodec));

  rt->shutdownHandlers.add(blImageCodecRtShutdown);
}

// ============================================================================
// [BLImageCodec - Unit Tests]
// ============================================================================

#ifdef BL_TEST
UNIT(image_codecs) {
  INFO("Testing BLImageCodec::findByName() and BLImageCodec::findByData()");
  {
    static const uint8_t bmpSignature[2] = { 'B', 'M' };
    static const uint8_t pngSignature[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    static const uint8_t jpgSignature[3] = { 0xFF, 0xD8, 0xFF };

    BLImageCodec codec;
    BLImageCodec bmp;
    BLImageCodec png;
    BLImageCodec jpg;

    EXPECT(bmp.findByName("BMP") == BL_SUCCESS);
    EXPECT(png.findByName("PNG") == BL_SUCCESS);
    EXPECT(jpg.findByName("JPEG") == BL_SUCCESS);

    EXPECT(codec.findByExtension("bmp") == BL_SUCCESS);
    EXPECT(codec == bmp);

    EXPECT(codec.findByExtension(".bmp") == BL_SUCCESS);
    EXPECT(codec == bmp);

    EXPECT(codec.findByExtension("SomeFile.BMp") == BL_SUCCESS);
    EXPECT(codec == bmp);

    EXPECT(codec.findByExtension("png") == BL_SUCCESS);
    EXPECT(codec == png);

    EXPECT(codec.findByExtension(".png") == BL_SUCCESS);
    EXPECT(codec == png);

    EXPECT(codec.findByExtension(".jpg") == BL_SUCCESS);
    EXPECT(codec == jpg);

    EXPECT(codec.findByExtension(".jpeg") == BL_SUCCESS);
    EXPECT(codec == jpg);

    EXPECT(codec.findByData(bmpSignature, BL_ARRAY_SIZE(bmpSignature)) == BL_SUCCESS);
    EXPECT(codec == bmp);

    EXPECT(codec.findByData(pngSignature, BL_ARRAY_SIZE(pngSignature)) == BL_SUCCESS);
    EXPECT(codec == png);

    EXPECT(codec.findByData(jpgSignature, BL_ARRAY_SIZE(jpgSignature)) == BL_SUCCESS);
    EXPECT(codec == jpg);
  }
}
#endif
