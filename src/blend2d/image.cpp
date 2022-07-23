// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "filesystem.h"
#include "format.h"
#include "image_p.h"
#include "imagecodec.h"
#include "imagedecoder.h"
#include "imageencoder.h"
#include "imagescale_p.h"
#include "pixelconverter_p.h"
#include "runtime_p.h"
#include "support/intops_p.h"
#include "support/memops_p.h"

namespace BLImagePrivate {

// BLImage - Globals
// =================

static BLObjectEthernalImpl<BLImagePrivateImpl> defaultImage;

// BLImage - Constants
// ===================

static constexpr uint32_t kSmallDataAlignment = 8;
static constexpr uint32_t kLargeDataAlignment = 64;
static constexpr uint32_t kLargeDataThreshold = 1024;

// BLImage - Utilities
// ===================

static BL_INLINE size_t strideForWidth(uint32_t width, uint32_t depth, BLOverflowFlag* of) noexcept {
  if (depth <= 8)
    return (size_t(width) * depth + 7u) / 8u;

  size_t bytesPerPixel = size_t(depth / 8u);
  return BLIntOps::mulOverflow(size_t(width), bytesPerPixel, of);
}

static void copyImageData(uint8_t* dstData, intptr_t dstStride, const uint8_t* srcData, intptr_t srcStride, int w, int h, BLFormat format) noexcept {
  size_t bytesPerLine = (size_t(unsigned(w)) * blFormatInfo[format].depth + 7u) / 8u;

  if (intptr_t(bytesPerLine) == dstStride && intptr_t(bytesPerLine) == srcStride) {
    // Special case that happens offen - stride equals bytes-per-line (no gaps).
    memcpy(dstData, srcData, bytesPerLine * unsigned(h));
    return;
  }
  else {
    // Generic case - there are either gaps or source/destination is a subimage.
    size_t gap = dstStride > 0 ? size_t(dstStride) - bytesPerLine : size_t(0);
    for (unsigned y = unsigned(h); y; y--) {
      memcpy(dstData, srcData, bytesPerLine);
      BLMemOps::fillSmallT(dstData + bytesPerLine, uint8_t(0), gap);

      dstData += dstStride;
      srcData += srcStride;
    }
  }
}

// BLImage - Alloc & Free Impl
// ===========================

static BL_NOINLINE BLResult allocImpl(BLImageCore* self, int w, int h, BLFormat format) noexcept {
  BL_ASSERT(w > 0 && h > 0);
  BL_ASSERT(format <= BL_FORMAT_MAX_VALUE);

  BLOverflowFlag of = 0;
  uint32_t depth = blFormatInfo[format].depth;
  size_t stride = strideForWidth(unsigned(w), depth, &of);

  BLObjectImplSize baseSize(sizeof(BLImagePrivateImpl));
  BLObjectImplSize implSize(BLIntOps::mulOverflow<size_t>(size_t(h), stride, &of));

  size_t dataAlignment = implSize <= kLargeDataThreshold ? kSmallDataAlignment : kLargeDataAlignment;
  baseSize = BLIntOps::alignUp(baseSize.value(), dataAlignment);
  implSize = BLIntOps::addOverflow<size_t>(baseSize.value(), implSize.value(), &of);

  if (BL_UNLIKELY(of))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BLImagePrivateImpl* impl = blObjectDetailAllocImplT<BLImagePrivateImpl>(self, BLObjectInfo::packType(BL_OBJECT_TYPE_IMAGE), implSize, &implSize);
  if (BL_UNLIKELY(!impl))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  uint8_t* pixelData = reinterpret_cast<uint8_t*>(impl) + baseSize.value();
  impl->pixelData = pixelData;
  impl->size.reset(w, h);
  impl->stride = intptr_t(stride);
  impl->format = uint8_t(format);
  impl->flags = uint8_t(0);
  impl->depth = uint16_t(depth);
  memset(impl->reserved, 0, sizeof(impl->reserved));
  impl->writerCount = 0;

  return BL_SUCCESS;
}

static BL_NOINLINE BLResult allocExternal(BLImageCore* self, int w, int h, BLFormat format, void* pixelData, intptr_t stride, BLDestroyExternalDataFunc destroyFunc, void* userData) noexcept {
  BL_ASSERT(w > 0 && h > 0);
  BL_ASSERT(format <= BL_FORMAT_MAX_VALUE);

  void* externalOptData;
  BLObjectExternalInfo* externalInfo;

  BLObjectImplSize implSize(sizeof(BLImagePrivateImpl));
  BLImagePrivateImpl* impl = blObjectDetailAllocImplExternalT<BLImagePrivateImpl>(self,
    BLObjectInfo::packType(BL_OBJECT_TYPE_IMAGE),
    implSize,
    &externalInfo,
    &externalOptData);

  if (BL_UNLIKELY(!impl))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  externalInfo->destroyFunc = destroyFunc ? destroyFunc : blObjectDestroyExternalDataDummy;
  externalInfo->userData = userData;

  impl->pixelData = pixelData;
  impl->size.reset(w, h);
  impl->stride = stride;
  impl->format = uint8_t(format);
  impl->flags = uint8_t(0);
  impl->depth = uint16_t(blFormatInfo[format].depth);
  memset(impl->reserved, 0, sizeof(impl->reserved));
  impl->writerCount = 0;

  return BL_SUCCESS;
}

// Must be available outside of BLImage implementation.
BLResult freeImpl(BLImagePrivateImpl* impl, BLObjectInfo info) noexcept {
  // Postpone the deletion in case that the image still has writers attached. This is required as the rendering
  // context doesn't manipulate the reference count of `BLImage` (otherwise it would not be possible to attach
  // multiple rendering contexts, for example).
  if (impl->writerCount != 0)
    return BL_SUCCESS;

  if (info.xFlag())
    blObjectDetailCallExternalDestroyFunc(impl, info, BLObjectImplSize(sizeof(BLImagePrivateImpl)), impl->pixelData);

  return blObjectImplFreeInline(impl, info);
}

} // {BLImagePrivate}

// BLImage - API - Init & Destroy
// ==============================

BL_API_IMPL BLResult blImageInit(BLImageCore* self) noexcept {
  using namespace BLImagePrivate;

  self->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE]._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blImageInitMove(BLImageCore* self, BLImageCore* other) noexcept {
  using namespace BLImagePrivate;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isImage());

  self->_d = other->_d;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE]._d;

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blImageInitWeak(BLImageCore* self, const BLImageCore* other) noexcept {
  using namespace BLImagePrivate;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isImage());

  return blObjectPrivateInitWeakTagged(self, other);
}

BL_API_IMPL BLResult blImageInitAs(BLImageCore* self, int w, int h, BLFormat format) noexcept {
  using namespace BLImagePrivate;

  self->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE]._d;
  return blImageCreate(self, w, h, format);
}

BL_API_IMPL BLResult blImageInitAsFromData(BLImageCore* self, int w, int h, BLFormat format, void* pixelData, intptr_t stride, BLDestroyExternalDataFunc destroyFunc, void* userData) noexcept {
  using namespace BLImagePrivate;

  self->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE]._d;
  return blImageCreateFromData(self, w, h, format, pixelData, stride, destroyFunc, userData);
}

BL_API_IMPL BLResult blImageDestroy(BLImageCore* self) noexcept {
  using namespace BLImagePrivate;

  BL_ASSERT(self->_d.isImage());
  return releaseInstance(self);
}

// BLImage - API - Reset
// =====================

BL_API_IMPL BLResult blImageReset(BLImageCore* self) noexcept {
  using namespace BLImagePrivate;

  BL_ASSERT(self->_d.isImage());
  return replaceInstance(self, static_cast<BLImageCore*>(&blObjectDefaults[BL_OBJECT_TYPE_IMAGE]));
}

// BLImage - API - Assign
// ======================

BL_API_IMPL BLResult blImageAssignMove(BLImageCore* self, BLImageCore* other) noexcept {
  using namespace BLImagePrivate;

  BL_ASSERT(self->_d.isImage());
  BL_ASSERT(other->_d.isImage());

  BLImageCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE]._d;
  return replaceInstance(self, &tmp);
}

BL_API_IMPL BLResult blImageAssignWeak(BLImageCore* self, const BLImageCore* other) noexcept {
  using namespace BLImagePrivate;

  BL_ASSERT(self->_d.isImage());
  BL_ASSERT(other->_d.isImage());

  blObjectPrivateAddRefTagged(other);
  return replaceInstance(self, other);
}

BL_API_IMPL BLResult blImageAssignDeep(BLImageCore* self, const BLImageCore* other) noexcept {
  using namespace BLImagePrivate;

  BL_ASSERT(self->_d.isImage());
  BL_ASSERT(other->_d.isImage());

  BLImagePrivateImpl* selfI = getImpl(self);
  BLImagePrivateImpl* otherI = getImpl(other);

  BLSizeI size = otherI->size;
  BLFormat format = BLFormat(otherI->format);

  if (format == BL_FORMAT_NONE)
    return blImageReset(self);

  BLImageData dummyImageData;
  if (selfI == otherI)
    return blImageMakeMutable(self, &dummyImageData);

  BL_PROPAGATE(blImageCreate(self, size.w, size.h, format));
  selfI = getImpl(self);

  copyImageData(static_cast<uint8_t*>(selfI->pixelData), selfI->stride,
              static_cast<uint8_t*>(otherI->pixelData), otherI->stride, size.w, size.h, format);
  return BL_SUCCESS;
}

// BLImage - API - Create
// ======================

BL_API_IMPL BLResult blImageCreate(BLImageCore* self, int w, int h, BLFormat format) noexcept {
  using namespace BLImagePrivate;

  BL_ASSERT(self->_d.isImage());

  if (BL_UNLIKELY(w <= 0 || h <= 0 || format == BL_FORMAT_NONE || format > BL_FORMAT_MAX_VALUE)) {
    if (w == 0 && h == 0 && format == BL_FORMAT_NONE)
      return blImageReset(self);
    else
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  if (BL_UNLIKELY(unsigned(w) >= BL_RUNTIME_MAX_IMAGE_SIZE ||
                  unsigned(h) >= BL_RUNTIME_MAX_IMAGE_SIZE))
    return blTraceError(BL_ERROR_IMAGE_TOO_LARGE);

  BLImagePrivateImpl* selfI = getImpl(self);
  if (selfI->size == BLSizeI(w, h) && selfI->format == format)
    if (isMutable(self) && !self->_d.xFlag())
      return BL_SUCCESS;

  BLImageCore newO;
  BL_PROPAGATE(allocImpl(&newO, w, h, format));

  return replaceInstance(self, &newO);
}

BL_API_IMPL BLResult blImageCreateFromData(BLImageCore* self, int w, int h, BLFormat format, void* pixelData, intptr_t stride, BLDestroyExternalDataFunc destroyFunc, void* userData) noexcept {
  using namespace BLImagePrivate;

  BL_ASSERT(self->_d.isImage());

  if (BL_UNLIKELY(w <= 0 || h <= 0 || format == BL_FORMAT_NONE || format > BL_FORMAT_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (BL_UNLIKELY(unsigned(w) >= BL_RUNTIME_MAX_IMAGE_SIZE || unsigned(h) >= BL_RUNTIME_MAX_IMAGE_SIZE))
    return blTraceError(BL_ERROR_IMAGE_TOO_LARGE);

  BLImageCore newO;
  BL_PROPAGATE(allocExternal(&newO, w, h, format, pixelData, stride, destroyFunc, userData));

  return replaceInstance(self, &newO);
}

// BLImage - API - Accessors
// =========================

BL_API_IMPL BLResult blImageGetData(const BLImageCore* self, BLImageData* dataOut) noexcept {
  using namespace BLImagePrivate;

  BL_ASSERT(self->_d.isImage());
  BLImagePrivateImpl* selfI = getImpl(self);

  dataOut->pixelData = selfI->pixelData;
  dataOut->stride = selfI->stride;
  dataOut->size = selfI->size;
  dataOut->format = selfI->format;
  dataOut->flags = 0;

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blImageMakeMutable(BLImageCore* self, BLImageData* dataOut) noexcept {
  using namespace BLImagePrivate;

  BL_ASSERT(self->_d.isImage());
  BLImagePrivateImpl* selfI = getImpl(self);

  BLSizeI size = selfI->size;
  BLFormat format = (BLFormat)selfI->format;

  if (format != BL_FORMAT_NONE && !isMutable(self)) {
    BLImageCore newO;
    BL_PROPAGATE(allocImpl(&newO, size.w, size.h, format));

    BLImagePrivateImpl* newI = getImpl(&newO);
    dataOut->pixelData = newI->pixelData;
    dataOut->stride = newI->stride;
    dataOut->size = size;
    dataOut->format = format;
    dataOut->flags = 0;

    copyImageData(static_cast<uint8_t*>(newI->pixelData), newI->stride,
                static_cast<uint8_t*>(selfI->pixelData), selfI->stride, size.w, size.h, format);

    return replaceInstance(self, &newO);
  }
  else {
    dataOut->pixelData = selfI->pixelData;
    dataOut->stride = selfI->stride;
    dataOut->size = size;
    dataOut->format = format;
    dataOut->flags = 0;
    return BL_SUCCESS;
  }
}

// BLImage - API - Convert
// =======================

BL_API_IMPL BLResult blImageConvert(BLImageCore* self, BLFormat format) noexcept {
  using namespace BLImagePrivate;

  BL_ASSERT(self->_d.isImage());
  BLImagePrivateImpl* selfI = getImpl(self);

  BLFormat srcFormat = BLFormat(selfI->format);
  BLFormat dstFormat = format;

  if (dstFormat == srcFormat)
    return BL_SUCCESS;

  if (srcFormat == BL_FORMAT_NONE)
    return blTraceError(BL_ERROR_NOT_INITIALIZED);

  BLResult result = BL_SUCCESS;
  BLPixelConverterCore pc {};

  BLSizeI size = selfI->size;
  const BLFormatInfo& di = blFormatInfo[dstFormat];
  const BLFormatInfo& si = blFormatInfo[srcFormat];

  // Save some cycles by calling `blPixelConverterInitInternal` as we don't need to sanitize the destination and
  // source formats in this case.
  if (blPixelConverterInitInternal(&pc, di, si, BL_PIXEL_CONVERTER_CREATE_NO_FLAGS) != BL_SUCCESS) {
    // Built-in formats should always have a built-in converter, so report a different error if the initialization
    // failed. This is pretty critical.
    return blTraceError(BL_ERROR_INVALID_STATE);
  }

  if (di.depth == si.depth && isMutable(self)) {
    // Prefer in-place conversion if the depths are equal and the image mutable.
    pc.convertFunc(&pc, static_cast<uint8_t*>(selfI->pixelData), selfI->stride,
                        static_cast<uint8_t*>(selfI->pixelData), selfI->stride, uint32_t(size.w), uint32_t(size.h), nullptr);
  }
  else {
    BLImageCore dstImage;
    result = blImageInitAs(&dstImage, size.w, size.h, dstFormat);

    if (result == BL_SUCCESS) {
      BLImagePrivateImpl* dstI = getImpl(&dstImage);
      BLPixelConverterOptions opt {};

      opt.gap = uintptr_t(blAbs(dstI->stride)) - uintptr_t(uint32_t(size.w)) * (dstI->depth / 8u);
      pc.convertFunc(&pc, static_cast<uint8_t*>(dstI->pixelData), dstI->stride,
                          static_cast<uint8_t*>(selfI->pixelData), selfI->stride, uint32_t(size.w), uint32_t(size.h), &opt);

      return replaceInstance(self, &dstImage);
    }
  }

  blPixelConverterReset(&pc);
  return result;
}

// BLImage - API - Equality & Comparison
// =====================================

BL_API_IMPL bool blImageEquals(const BLImageCore* a, const BLImageCore* b) noexcept {
  using namespace BLImagePrivate;

  BL_ASSERT(a->_d.isImage());
  BL_ASSERT(b->_d.isImage());

  const BLImagePrivateImpl* aImpl = getImpl(a);
  const BLImagePrivateImpl* bImpl = getImpl(b);

  if (aImpl == bImpl)
    return true;

  if (aImpl->size != bImpl->size || aImpl->format != bImpl->format)
    return false;

  uint32_t w = uint32_t(aImpl->size.w);
  uint32_t h = uint32_t(aImpl->size.h);

  const uint8_t* aData = static_cast<const uint8_t*>(aImpl->pixelData);
  const uint8_t* bData = static_cast<const uint8_t*>(bImpl->pixelData);

  intptr_t aStride = aImpl->stride;
  intptr_t bStride = bImpl->stride;

  size_t bytesPerLine = (w * blFormatInfo[aImpl->format].depth + 7u) / 8u;
  for (uint32_t y = 0; y < h; y++) {
    if (memcmp(aData, bData, bytesPerLine) != 0)
      return false;
    aData += aStride;
    bData += bStride;
  }

  return true;
}

// BLImage - API - Scale
// =====================

BL_API_IMPL BLResult blImageScale(BLImageCore* dst, const BLImageCore* src, const BLSizeI* size, uint32_t filter, const BLImageScaleOptions* options) noexcept {
  using namespace BLImagePrivate;

  BL_ASSERT(dst->_d.isImage());
  BL_ASSERT(src->_d.isImage());

  BLImagePrivateImpl* srcI = getImpl(src);
  if (srcI->format == BL_FORMAT_NONE)
    return blImageReset(dst);

  BLImageScaleContext scaleCtx;
  BL_PROPAGATE(scaleCtx.create(*size, srcI->size, filter, options));

  BLFormat format = BLFormat(srcI->format);
  int tw = scaleCtx.dstWidth();
  int th = scaleCtx.srcHeight();

  BLImage tmp;
  BLImageData buf;

  if (th == scaleCtx.dstHeight() || tw == scaleCtx.srcWidth()) {
    // Only horizontal or vertical scale.

    // Move to `tmp` so it's not destroyed by `dst->create()`.
    if (dst == src)
      tmp = src->dcast();

    BL_PROPAGATE(blImageCreate(dst, scaleCtx.dstWidth(), scaleCtx.dstHeight(), format));
    BL_PROPAGATE(blImageMakeMutable(dst, &buf));

    if (th == scaleCtx.dstHeight())
      scaleCtx.processHorzData(static_cast<uint8_t*>(buf.pixelData), buf.stride, static_cast<const uint8_t*>(srcI->pixelData), srcI->stride, format);
    else
      scaleCtx.processVertData(static_cast<uint8_t*>(buf.pixelData), buf.stride, static_cast<const uint8_t*>(srcI->pixelData), srcI->stride, format);
  }
  else {
    // Both horizontal and vertical scale.
    BL_PROPAGATE(tmp.create(tw, th, format));
    BL_PROPAGATE(tmp.makeMutable(&buf));
    scaleCtx.processHorzData(static_cast<uint8_t*>(buf.pixelData), buf.stride, static_cast<const uint8_t*>(srcI->pixelData), srcI->stride, format);

    srcI = getImpl(&tmp);
    BL_PROPAGATE(blImageCreate(dst, scaleCtx.dstWidth(), scaleCtx.dstHeight(), format));
    BL_PROPAGATE(blImageMakeMutable(dst, &buf));

    scaleCtx.processVertData(static_cast<uint8_t*>(buf.pixelData), buf.stride, static_cast<const uint8_t*>(srcI->pixelData), srcI->stride, format);
  }

  return BL_SUCCESS;
}

// BLImage - API - Read File
// =========================

BL_API_IMPL BLResult blImageReadFromFile(BLImageCore* self, const char* fileName, const BLArrayCore* codecs) noexcept {
  using namespace BLImagePrivate;

  BL_ASSERT(self->_d.isImage());

  BLArray<uint8_t> buffer;
  BL_PROPAGATE(BLFileSystem::readFile(fileName, buffer));

  if (buffer.empty())
    return blTraceError(BL_ERROR_FILE_EMPTY);

  BLImageCodec codec;
  BL_PROPAGATE(blImageCodecFindByData(&codec, buffer.data(), buffer.size(), codecs));

  if (BL_UNLIKELY(!(codec.features() & BL_IMAGE_CODEC_FEATURE_READ)))
    return blTraceError(BL_ERROR_IMAGE_DECODER_NOT_PROVIDED);

  BLImageDecoder decoder;
  BL_PROPAGATE(codec.createDecoder(&decoder));
  return decoder.readFrame(*self, buffer);
}

// BLImage - API - Read Data
// =========================

BL_API_IMPL BLResult blImageReadFromData(BLImageCore* self, const void* data, size_t size, const BLArrayCore* codecs) noexcept {
  using namespace BLImagePrivate;

  BL_ASSERT(self->_d.isImage());

  BLImageCodec codec;
  BL_PROPAGATE(blImageCodecFindByData(&codec, data, size, codecs));

  if (BL_UNLIKELY(!(codec.features() & BL_IMAGE_CODEC_FEATURE_READ)))
    return blTraceError(BL_ERROR_IMAGE_DECODER_NOT_PROVIDED);

  BLImageDecoder decoder;
  BL_PROPAGATE(codec.createDecoder(&decoder));
  return decoder.readFrame(*self, data, size);
}

// BLImage - API - Write File
// ==========================

namespace BLImagePrivate {

static BLResult writeToFileInternal(const BLImageCore* self, const char* fileName, const BLImageCodecCore* codec) noexcept {
  using namespace BLImagePrivate;

  BL_ASSERT(self->_d.isImage());
  BL_ASSERT(codec->_d.isImageCodec());

  BLArray<uint8_t> buffer;
  BL_PROPAGATE(blImageWriteToData(self, &buffer, codec));
  return BLFileSystem::writeFile(fileName, buffer);
}

} // {BLImagePrivate}

BL_API_IMPL BLResult blImageWriteToFile(const BLImageCore* self, const char* fileName, const BLImageCodecCore* codec) noexcept {
  using namespace BLImagePrivate;

  BL_ASSERT(self->_d.isImage());

  if (!codec) {
    BLImageCodec localCodec;
    BL_PROPAGATE(localCodec.findByExtension(fileName));
    return writeToFileInternal(self, fileName, &localCodec);
  }
  else {
    BL_ASSERT(codec->_d.isImageCodec());
    return writeToFileInternal(self, fileName, codec);
  }
}

// BLImage - API - Write Data
// ==========================

BL_API_IMPL BLResult blImageWriteToData(const BLImageCore* self, BLArrayCore* dst, const BLImageCodecCore* codec) noexcept {
  using namespace BLImagePrivate;

  BL_ASSERT(self->_d.isImage());
  BL_ASSERT(codec->_d.isImageCodec());

  if (BL_UNLIKELY(!(codec->dcast().features() & BL_IMAGE_CODEC_FEATURE_WRITE)))
    return blTraceError(BL_ERROR_IMAGE_ENCODER_NOT_PROVIDED);

  BLImageEncoder encoder;
  BL_PROPAGATE(codec->dcast().createEncoder(&encoder));
  return encoder.writeFrame(dst->dcast<BLArray<uint8_t>>(), self->dcast());
}

// BLImage - Runtime Registration
// ==============================

void blImageRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  auto& defaultImage = BLImagePrivate::defaultImage;
  blObjectDefaults[BL_OBJECT_TYPE_IMAGE]._d.initDynamic(
    BL_OBJECT_TYPE_IMAGE,
    BLObjectInfo{BL_OBJECT_INFO_IMMUTABLE_FLAG},
    &defaultImage.impl);
}

// BLImage - Tests
// ===============

#if defined(BL_TEST)
UNIT(image) {
  INFO("Image data");
  {
    constexpr uint32_t kSize = 256;

    BLImage img0;
    BLImage img1;

    EXPECT_SUCCESS(img0.create(kSize, kSize, BL_FORMAT_PRGB32));
    EXPECT_SUCCESS(img1.create(kSize, kSize, BL_FORMAT_PRGB32));

    EXPECT_EQ(img0.width(), 256);
    EXPECT_EQ(img0.height(), 256);
    EXPECT_EQ(img0.format(), BL_FORMAT_PRGB32);

    EXPECT_EQ(img1.width(), 256);
    EXPECT_EQ(img1.height(), 256);
    EXPECT_EQ(img1.format(), BL_FORMAT_PRGB32);

    BLImageData imgData0;
    BLImageData imgData1;

    EXPECT_SUCCESS(img0.getData(&imgData0));
    EXPECT_SUCCESS(img1.getData(&imgData1));

    // Direct memory manipulation.
    for (uint32_t i = 0; i < kSize; i++) {
      memset(static_cast<uint8_t*>(imgData0.pixelData) + i * imgData0.stride, 0xFF, kSize * 4);
      memset(static_cast<uint8_t*>(imgData1.pixelData) + i * imgData1.stride, 0xFF, kSize * 4);
    }

    EXPECT_TRUE(img0.equals(img1));
  }
}
#endif
