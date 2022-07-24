// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "image_p.h"
#include "imagecodec.h"
#include "runtime_p.h"
#include "string_p.h"
#include "codec/bmpcodec_p.h"
#include "codec/jpegcodec_p.h"
#include "codec/pngcodec_p.h"
#include "support/stringops_p.h"
#include "support/wrap_p.h"
#include "threading/mutex_p.h"

namespace BLImageCodecPrivate {

// BLImageCodec - Globals
// ======================

static BLObjectEthernalVirtualImpl<BLImageCodecImpl, BLImageCodecVirt> defaultCodec;
static BLWrap<BLArray<BLImageCodec>> imageCodecsArray;
static BLWrap<BLSharedMutex> imageCodecsArrayMutex;

} // {BLImageCodecPrivate}

// BLImageCodec - API - Init & Destroy
// ===================================

BL_API_IMPL BLResult blImageCodecInit(BLImageCodecCore* self) noexcept {
  self->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE_CODEC]._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blImageCodecInitMove(BLImageCodecCore* self, BLImageCodecCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isImageCodec());

  self->_d = other->_d;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE_CODEC]._d;

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blImageCodecInitWeak(BLImageCodecCore* self, const BLImageCodecCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isImageCodec());

  return blObjectPrivateInitWeakTagged(self, other);
}

BL_API_IMPL BLResult blImageCodecInitByName(BLImageCodecCore* self, const char* name, size_t size, const BLArrayCore* codecs) noexcept {
  self->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE_CODEC]._d;
  return blImageCodecFindByName(self, name, size, codecs);
}

BL_API_IMPL BLResult blImageCodecDestroy(BLImageCodecCore* self) noexcept {
  return blObjectPrivateReleaseVirtual(self);
}

// BLImageCodec - API - Reset
// ==========================

BL_API_IMPL BLResult blImageCodecReset(BLImageCodecCore* self) noexcept {
  BL_ASSERT(self->_d.isImageCodec());

  return blObjectPrivateReplaceVirtual(self, static_cast<BLImageCodecCore*>(&blObjectDefaults[BL_OBJECT_TYPE_IMAGE_CODEC]));
}

// BLImageCodec - API - Assign
// ===========================

BL_API_IMPL BLResult blImageCodecAssignMove(BLImageCodecCore* self, BLImageCodecCore* other) noexcept {
  BL_ASSERT(self->_d.isImageCodec());
  BL_ASSERT(other->_d.isImageCodec());

  BLImageCodecCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE_CODEC]._d;
  return blObjectPrivateReplaceVirtual(self, &tmp);
}

BL_API_IMPL BLResult blImageCodecAssignWeak(BLImageCodecCore* self, const BLImageCodecCore* other) noexcept {
  BL_ASSERT(self->_d.isImageCodec());
  BL_ASSERT(other->_d.isImageCodec());

  return blObjectPrivateAssignWeakVirtual(self, other);
}

// BLImageCodec - API - Inspect Data
// =================================

BL_API_IMPL uint32_t blImageCodecInspectData(const BLImageCodecCore* self, const void* data, size_t size) noexcept {
  BL_ASSERT(self->_d.isImageCodec());
  BLImageCodecImpl* selfI = self->dcast()._impl();

  return selfI->virt->inspectData(selfI, static_cast<const uint8_t*>(data), size);
}

// BLImageCodec - API - Find By Name & Extension & Data
// ====================================================

namespace BLImageCodecPrivate {

static bool matchExtension(const char* extensions, const char* match, size_t matchSize) noexcept {
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
static const char* keepOnlyExtensionInMatch(const char* match, size_t& size) noexcept {
  const char* end = match + size;
  const char* p = end;

  while (p != match && p[-1] != '.')
    p--;

  size = (size_t)(end - p);
  return p;
}

static BLResult findCodecByName(BLImageCodecCore* self, const char* name, size_t size, const BLArrayCore* codecs) noexcept {
  for (const BLImageCodec& codec : codecs->dcast<BLArray<BLImageCodec>>().view())
    if (codec.name() == BLStringView{name, size})
      return blImageCodecAssignWeak(self, &codec);

  return blTraceError(BL_ERROR_IMAGE_NO_MATCHING_CODEC);
}

static BLResult findCodecByExtension(BLImageCodecCore* self, const char* name, size_t size, const BLArrayCore* codecs) noexcept {
  for (const BLImageCodec& codec : codecs->dcast<BLArray<BLImageCodec>>().view())
    if (matchExtension(codec.extensions().data(), name, size))
      return blImageCodecAssignWeak(self, &codec);

  return blTraceError(BL_ERROR_IMAGE_NO_MATCHING_CODEC);
}

static BLResult findCodecByData(BLImageCodecCore* self, const void* data, size_t size, const BLArrayCore* codecs) noexcept {
  uint32_t bestScore = 0;
  const BLImageCodec* candidate = nullptr;

  for (const BLImageCodec& codec : codecs->dcast<BLArray<BLImageCodec>>().view()) {
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

} // {BLImageCodecPrivate}

BL_API_IMPL BLResult blImageCodecFindByName(BLImageCodecCore* self, const char* name, size_t size, const BLArrayCore* codecs) noexcept {
  using namespace BLImageCodecPrivate;
  BL_ASSERT(self->_d.isImageCodec());

  if (size == SIZE_MAX)
    size = strlen(name);

  if (!size)
    return blTraceError(BL_ERROR_IMAGE_NO_MATCHING_CODEC);

  if (codecs)
    return findCodecByName(self, name, size, codecs);
  else
    return imageCodecsArrayMutex->protectShared([&] { return findCodecByName(self, name, size, &imageCodecsArray); });
}

BL_API_IMPL BLResult blImageCodecFindByExtension(BLImageCodecCore* self, const char* name, size_t size, const BLArrayCore* codecs) noexcept {
  using namespace BLImageCodecPrivate;
  BL_ASSERT(self->_d.isImageCodec());

  if (size == SIZE_MAX)
    size = strlen(name);

  name = keepOnlyExtensionInMatch(name, size);
  if (codecs)
    return findCodecByExtension(self, name, size, codecs);
  else
    return imageCodecsArrayMutex->protectShared([&] { return findCodecByExtension(self, name, size, &imageCodecsArray); });
}

BL_API_IMPL BLResult blImageCodecFindByData(BLImageCodecCore* self, const void* data, size_t size, const BLArrayCore* codecs) noexcept {
  using namespace BLImageCodecPrivate;
  BL_ASSERT(self->_d.isImageCodec());

  if (codecs)
    return findCodecByData(self, data, size, codecs);
  else
    return imageCodecsArrayMutex->protectShared([&] { return findCodecByData(self, data, size, &imageCodecsArray); });
}

BL_API_IMPL BLResult blImageCodecCreateDecoder(const BLImageCodecCore* self, BLImageDecoderCore* dst) noexcept {
  BL_ASSERT(self->_d.isImageCodec());
  BLImageCodecImpl* selfI = self->dcast()._impl();

  return selfI->virt->createDecoder(selfI, dst);
}

BL_API_IMPL BLResult blImageCodecCreateEncoder(const BLImageCodecCore* self, BLImageEncoderCore* dst) noexcept {
  BL_ASSERT(self->_d.isImageCodec());
  BLImageCodecImpl* selfI = self->dcast()._impl();

  return selfI->virt->createEncoder(selfI, dst);
}

// BLImageCodec - API - Built-In Codecs (Global)
// =============================================

BL_API_IMPL BLResult blImageCodecArrayInitBuiltInCodecs(BLArrayCore* self) noexcept {
  using namespace BLImageCodecPrivate;

  *self = imageCodecsArrayMutex->protectShared([&] {
    BLArrayCore tmp = imageCodecsArray();
    blObjectPrivateAddRefTagged(&tmp);
    return tmp;
  });
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blImageCodecArrayAssignBuiltInCodecs(BLArrayCore* self) noexcept {
  blArrayDestroy(self);
  return blImageCodecArrayInitBuiltInCodecs(self);
}

BL_API_IMPL BLResult blImageCodecAddToBuiltIn(const BLImageCodecCore* codec) noexcept {
  using namespace BLImageCodecPrivate;
  BL_ASSERT(codec->_d.isImageCodec());

  return imageCodecsArrayMutex->protect([&] {
    size_t i = imageCodecsArray->indexOf(codec->dcast());
    if (i != SIZE_MAX)
      return blTraceError(BL_ERROR_ALREADY_EXISTS);
    return imageCodecsArray->append(codec->dcast());
  });
}

BL_API_IMPL BLResult blImageCodecRemoveFromBuiltIn(const BLImageCodecCore* codec) noexcept {
  using namespace BLImageCodecPrivate;
  BL_ASSERT(codec->_d.isImageCodec());

  return imageCodecsArrayMutex->protect([&] {
    size_t i = imageCodecsArray->indexOf(codec->dcast());
    if (i == SIZE_MAX)
      return blTraceError(BL_ERROR_NO_ENTRY);
    return imageCodecsArray->remove(i);
  });
}

// BLImageCodec - Virtual Functions (Null)
// =======================================

BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)

static BLResult BL_CDECL blImageCodecImplDestroy(BLObjectImpl* impl, uint32_t info) noexcept { return BL_SUCCESS; }
static uint32_t BL_CDECL blImageCodecImplInspectData(const BLImageCodecImpl* impl, const uint8_t* data, size_t size) noexcept { return 0; }
static BLResult BL_CDECL blImageCodecImplCreateDecoder(const BLImageCodecImpl* impl, BLImageDecoderCore* dst) noexcept { return BL_ERROR_IMAGE_DECODER_NOT_PROVIDED; }
static BLResult BL_CDECL blImageCodecImplCreateEncoder(const BLImageCodecImpl* impl, BLImageEncoderCore* dst) noexcept { return BL_ERROR_IMAGE_ENCODER_NOT_PROVIDED; }

BL_DIAGNOSTIC_POP

// BLImageCodec - Runtime Registration
// ===================================

static void BL_CDECL blImageCodecRtShutdown(BLRuntimeContext* rt) noexcept {
  using namespace BLImageCodecPrivate;
  blUnused(rt);

  imageCodecsArray.destroy();
  imageCodecsArrayMutex.destroy();
}

void blImageCodecRtInit(BLRuntimeContext* rt) noexcept {
  using namespace BLImageCodecPrivate;

  imageCodecsArrayMutex.init();
  imageCodecsArray.init();

  // Initialize default BLImageCodec.
  defaultCodec.virt.base.destroy = blImageCodecImplDestroy;
  defaultCodec.virt.base.getProperty = blObjectImplGetProperty;
  defaultCodec.virt.base.setProperty = blObjectImplSetProperty;
  defaultCodec.virt.inspectData = blImageCodecImplInspectData;
  defaultCodec.virt.createDecoder = blImageCodecImplCreateDecoder;
  defaultCodec.virt.createEncoder = blImageCodecImplCreateEncoder;
  defaultCodec.impl->ctor(&defaultCodec.virt);

  blObjectDefaults[BL_OBJECT_TYPE_IMAGE_CODEC]._d.initDynamic(
    BL_OBJECT_TYPE_IMAGE_CODEC,
    BLObjectInfo{BL_OBJECT_INFO_IMMUTABLE_FLAG},
    &defaultCodec.impl);

  rt->shutdownHandlers.add(blImageCodecRtShutdown);
}

void blRegisterBuiltInCodecs(BLRuntimeContext* rt) noexcept {
  using namespace BLImageCodecPrivate;

  BLArray<BLImageCodec>* codecs = &imageCodecsArray;
  blBmpCodecOnInit(rt, codecs);
  blJpegCodecOnInit(rt, codecs);
  blPngCodecOnInit(rt, codecs);
}

// BLImageCodec - Tests
// ====================

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

    EXPECT_SUCCESS(bmp.findByName("BMP"));
    EXPECT_SUCCESS(png.findByName("PNG"));
    EXPECT_SUCCESS(jpg.findByName("JPEG"));

    EXPECT_SUCCESS(codec.findByExtension("bmp"));
    EXPECT_EQ(codec, bmp);

    EXPECT_SUCCESS(codec.findByExtension(".bmp"));
    EXPECT_EQ(codec, bmp);

    EXPECT_SUCCESS(codec.findByExtension("SomeFile.BMp"));
    EXPECT_EQ(codec, bmp);

    EXPECT_SUCCESS(codec.findByExtension("png"));
    EXPECT_EQ(codec, png);

    EXPECT_SUCCESS(codec.findByExtension(".png"));
    EXPECT_EQ(codec, png);

    EXPECT_SUCCESS(codec.findByExtension(".jpg"));
    EXPECT_EQ(codec, jpg);

    EXPECT_SUCCESS(codec.findByExtension(".jpeg"));
    EXPECT_EQ(codec, jpg);

    EXPECT_SUCCESS(codec.findByData(bmpSignature, BL_ARRAY_SIZE(bmpSignature)));
    EXPECT_EQ(codec, bmp);

    EXPECT_SUCCESS(codec.findByData(pngSignature, BL_ARRAY_SIZE(pngSignature)));
    EXPECT_EQ(codec, png);

    EXPECT_SUCCESS(codec.findByData(jpgSignature, BL_ARRAY_SIZE(jpgSignature)));
    EXPECT_EQ(codec, jpg);
  }
}
#endif
