// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "filesystem.h"
#include "format_p.h"
#include "image_p.h"
#include "imagecodec.h"
#include "imagedecoder.h"
#include "imageencoder.h"
#include "imagescale_p.h"
#include "object_p.h"
#include "pixelconverter_p.h"
#include "runtime_p.h"
#include "support/intops_p.h"
#include "support/memops_p.h"

namespace bl {
namespace ImageInternal {

// bl::Image - Globals
// ===================

static BLObjectEternalImpl<BLImagePrivateImpl> defaultImage;

// bl::Image - Constants
// =====================

static constexpr uint32_t kLargeDataAlignment = 64;
static constexpr uint32_t kLargeDataThreshold = 1024;
static constexpr uint32_t kMaxAddressableOffset = 0x7FFFFFFFu;

// bl::Image - Utilities
// =====================

static BL_INLINE uint32_t strideForWidth(uint32_t width, uint32_t depth) noexcept {
  return (uint32_t(width) * depth + 7u) / 8u;
}

static BL_INLINE bool checkSizeAndFormat(int w, int h, BLFormat format) noexcept {
  return !(unsigned(w - 1) >= BL_RUNTIME_MAX_IMAGE_SIZE ||
           unsigned(h - 1) >= BL_RUNTIME_MAX_IMAGE_SIZE ||
           unsigned(format - 1) >= BL_FORMAT_MAX_VALUE);
}

static BL_INLINE BLResultT<intptr_t> calcStrideFromCreateParams(int w, int h, BLFormat format) noexcept {
  if (BL_UNLIKELY(!checkSizeAndFormat(w, h, format))) {
    BLResult result =
      w <= 0 || h <= 0 || unsigned(format - 1) >= BL_FORMAT_MAX_VALUE
        ? BL_ERROR_INVALID_VALUE
        : BL_ERROR_IMAGE_TOO_LARGE;
    return BLResultT<intptr_t>{result, 0};
  }
  else {
    uint32_t bytesPerLine = strideForWidth(w, blFormatInfo[format].depth);
    uint64_t bytesPerImage = uint64_t(bytesPerLine) * unsigned(h);

    // NOTE: Align the stride to 16 bytes if bytesPerLine is not too small. The reason is that when multi-threaded
    // rendering is used and bytesPerLine not aligned, some bands could share a cache line, which would potentially
    // affect the performance in a very negative way.
    if (bytesPerLine > 256u)
      bytesPerLine = IntOps::alignUp(bytesPerLine, 16);

    BLResult result = bytesPerImage <= kMaxAddressableOffset ? BL_SUCCESS : BL_ERROR_IMAGE_TOO_LARGE;
    return BLResultT<intptr_t>{result, intptr_t(bytesPerLine)};
  }
}

// Make sure that the external image won't cause any kind of overflow in rasterization and texture fetching.
static BL_INLINE BLResult checkCreateFromDataParams(int w, int h, BLFormat format, intptr_t stride) noexcept {
  uint32_t minimumStride = strideForWidth(w, blFormatInfo[format].depth);
  uintptr_t bytesPerLine = uintptr_t(blAbs(stride));

  if (BL_UNLIKELY(!checkSizeAndFormat(w, h, format) || bytesPerLine < minimumStride))
    return BL_ERROR_INVALID_VALUE;

  // Make sure that the image height multiplied by stride is not greater than 2^31 - this makes sure that we
  // can handle also negative strides properly and that we can guarantee that all pixels are addressable via
  // 32-bit offsets, which is required by some SIMD fetchers.
  //
  // NOTE: BytesPerImage also considers all parent images in case this image is indeed a sub-image. The reason
  // is that we have to address all pixels in this sub-image too, so basically we include the parent in the
  // computation as well. Since a sub-image may be managed by the user (outside of Blend2D) we have to check
  // this every time an external pixel data is used.
  uint64_t bytesPerImage = uint64_t(bytesPerLine) * uint64_t(uint32_t(h));
  return bytesPerLine > uintptr_t(kMaxAddressableOffset) || bytesPerImage > kMaxAddressableOffset ? BL_ERROR_IMAGE_TOO_LARGE : BL_SUCCESS;
}

static void copyImageData(uint8_t* dstData, intptr_t dstStride, const uint8_t* srcData, intptr_t srcStride, int w, int h, BLFormat format) noexcept {
  size_t bytesPerLine = (size_t(unsigned(w)) * blFormatInfo[format].depth + 7u) / 8u;

  if (intptr_t(bytesPerLine) == dstStride && intptr_t(bytesPerLine) == srcStride) {
    // Special case that happens often - stride equals bytes-per-line (no gaps).
    memcpy(dstData, srcData, bytesPerLine * unsigned(h));
    return;
  }
  else {
    // Generic case - there are either gaps or source/destination is a sub-image.
    size_t gap = dstStride > 0 ? size_t(dstStride) - bytesPerLine : size_t(0);
    for (unsigned y = unsigned(h); y; y--) {
      memcpy(dstData, srcData, bytesPerLine);
      bl::MemOps::fillSmallT(dstData + bytesPerLine, uint8_t(0), gap);

      dstData += dstStride;
      srcData += srcStride;
    }
  }
}

// bl::Image - Alloc & Free Impl
// =============================

static BL_INLINE void initImplData(BLImagePrivateImpl* impl, int w, int h, BLFormat format, void* pixelData, intptr_t stride) noexcept {
  impl->pixelData = pixelData;
  impl->size.reset(w, h);
  impl->stride = stride;
  impl->format = uint8_t(format);
  impl->flags = uint8_t(0);
  impl->depth = uint16_t(blFormatInfo[format].depth);
  memset(impl->reserved, 0, sizeof(impl->reserved));
}

static BL_NOINLINE BLResult allocImpl(BLImageCore* self, int w, int h, BLFormat format, intptr_t stride) noexcept {
  BL_ASSERT(w > 0);
  BL_ASSERT(h > 0);
  BL_ASSERT(format != BL_FORMAT_NONE);
  BL_ASSERT(format <= BL_FORMAT_MAX_VALUE);
  BL_ASSERT(stride > 0);

  size_t kBaseImplSize = IntOps::alignUp(sizeof(BLImagePrivateImpl), BL_OBJECT_IMPL_ALIGNMENT);
  size_t pixelDataSize = size_t(h) * size_t(stride);

  BLObjectImplSize implSize(kBaseImplSize + pixelDataSize);
  if (pixelDataSize >= kLargeDataThreshold)
    implSize += kLargeDataAlignment - BL_OBJECT_IMPL_ALIGNMENT;

  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_IMAGE);
  BL_PROPAGATE(ObjectInternal::allocImplT<BLImagePrivateImpl>(self, info, implSize));

  BLImagePrivateImpl* impl = getImpl(self);
  uint8_t* pixelData = reinterpret_cast<uint8_t*>(impl) + kBaseImplSize;

  if (pixelDataSize >= kLargeDataThreshold)
    pixelData = bl::IntOps::alignUp(pixelData, kLargeDataAlignment);

  initImplData(impl, w, h, format, pixelData, stride);
  impl->writerCount = 0;
  return BL_SUCCESS;
}

static BL_NOINLINE BLResult allocExternal(BLImageCore* self, int w, int h, BLFormat format, void* pixelData, intptr_t stride, bool immutable, BLDestroyExternalDataFunc destroyFunc, void* userData) noexcept {
  BL_ASSERT(w > 0);
  BL_ASSERT(h > 0);
  BL_ASSERT(format != BL_FORMAT_NONE);
  BL_ASSERT(format <= BL_FORMAT_MAX_VALUE);

  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_IMAGE);
  BL_PROPAGATE(ObjectInternal::allocImplExternalT<BLImagePrivateImpl>(self, info, immutable, destroyFunc, userData));

  BLImagePrivateImpl* impl = getImpl(self);
  initImplData(impl, w, h, format, pixelData, stride);
  impl->writerCount = 0;
  return BL_SUCCESS;
}

// Must be available outside of BLImage implementation.
BLResult freeImpl(BLImagePrivateImpl* impl) noexcept {
  // Postpone the deletion in case that the image still has writers attached. This is required as the rendering
  // context doesn't manipulate the reference count of `BLImage` (otherwise it would not be possible to attach
  // multiple rendering contexts, for example).
  if (impl->writerCount != 0)
    return BL_SUCCESS;

  if (ObjectInternal::isImplExternal(impl))
    ObjectInternal::callExternalDestroyFunc(impl, impl->pixelData);

  return ObjectInternal::freeImpl(impl);
}

} // {ImageInternal}
} // {bl}

// bl::Image - API - Init & Destroy
// ================================

BL_API_IMPL BLResult blImageInit(BLImageCore* self) noexcept {
  using namespace bl::ImageInternal;

  self->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE]._d;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blImageInitMove(BLImageCore* self, BLImageCore* other) noexcept {
  using namespace bl::ImageInternal;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isImage());

  self->_d = other->_d;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE]._d;

  return BL_SUCCESS;
}

BL_API_IMPL BLResult blImageInitWeak(BLImageCore* self, const BLImageCore* other) noexcept {
  using namespace bl::ImageInternal;

  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isImage());

  self->_d = other->_d;
  return retainInstance(self);
}

BL_API_IMPL BLResult blImageInitAs(BLImageCore* self, int w, int h, BLFormat format) noexcept {
  using namespace bl::ImageInternal;

  self->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE]._d;
  return blImageCreate(self, w, h, format);
}

BL_API_IMPL BLResult blImageInitAsFromData(
    BLImageCore* self, int w, int h, BLFormat format,
    void* pixelData, intptr_t stride,
    BLDataAccessFlags accessFlags,
    BLDestroyExternalDataFunc destroyFunc, void* userData) noexcept {

  using namespace bl::ImageInternal;

  self->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE]._d;
  return blImageCreateFromData(self, w, h, format, pixelData, stride, accessFlags, destroyFunc, userData);
}

BL_API_IMPL BLResult blImageDestroy(BLImageCore* self) noexcept {
  using namespace bl::ImageInternal;

  BL_ASSERT(self->_d.isImage());
  return releaseInstance(self);
}

// bl::Image - API - Reset
// =======================

BL_API_IMPL BLResult blImageReset(BLImageCore* self) noexcept {
  using namespace bl::ImageInternal;

  BL_ASSERT(self->_d.isImage());
  return replaceInstance(self, static_cast<BLImageCore*>(&blObjectDefaults[BL_OBJECT_TYPE_IMAGE]));
}

// bl::Image - API - Assign
// ========================

BL_API_IMPL BLResult blImageAssignMove(BLImageCore* self, BLImageCore* other) noexcept {
  using namespace bl::ImageInternal;

  BL_ASSERT(self->_d.isImage());
  BL_ASSERT(other->_d.isImage());

  BLImageCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_IMAGE]._d;
  return replaceInstance(self, &tmp);
}

BL_API_IMPL BLResult blImageAssignWeak(BLImageCore* self, const BLImageCore* other) noexcept {
  using namespace bl::ImageInternal;

  BL_ASSERT(self->_d.isImage());
  BL_ASSERT(other->_d.isImage());

  retainInstance(other);
  return replaceInstance(self, other);
}

BL_API_IMPL BLResult blImageAssignDeep(BLImageCore* self, const BLImageCore* other) noexcept {
  using namespace bl::ImageInternal;

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

// bl::Image - API - Create
// ========================

BL_API_IMPL BLResult blImageCreate(BLImageCore* self, int w, int h, BLFormat format) noexcept {
  using namespace bl::ImageInternal;

  BL_ASSERT(self->_d.isImage());

  BLResultT<intptr_t> result = calcStrideFromCreateParams(w, h, format);
  if (BL_UNLIKELY(result.code != BL_SUCCESS)) {
    if ((w | h) == 0 && format == BL_FORMAT_NONE)
      return blImageReset(self);
    else
      return blTraceError(result.code);
  }

  BLImagePrivateImpl* selfI = getImpl(self);
  if (selfI->size == BLSizeI(w, h) && selfI->format == format)
    if (bl::ObjectInternal::isImplMutable(selfI) && !bl::ObjectInternal::isImplExternal(selfI))
      return BL_SUCCESS;

  BLImageCore newO;
  BL_PROPAGATE(allocImpl(&newO, w, h, format, result.value));

  return replaceInstance(self, &newO);
}

BL_API_IMPL BLResult blImageCreateFromData(
    BLImageCore* self, int w, int h, BLFormat format,
    void* pixelData, intptr_t stride,
    BLDataAccessFlags accessFlags,
    BLDestroyExternalDataFunc destroyFunc, void* userData) noexcept {

  using namespace bl::ImageInternal;

  BL_ASSERT(self->_d.isImage());

  BLResult result = checkCreateFromDataParams(w, h, format, stride);
  if (BL_UNLIKELY(result != BL_SUCCESS))
    return blTraceError(result);

  BLImagePrivateImpl* selfI = getImpl(self);
  bool immutable = !(accessFlags & BL_DATA_ACCESS_WRITE);

  if (bl::ObjectInternal::isImplExternal(selfI) && bl::ObjectInternal::isImplRefCountEqualToBase(selfI) && selfI->writerCount == 0) {
    // OPTIMIZATION: If the user code calls BLImage::createFromData() for every frame, use the same Impl
    // if the `refCount == 1` and the Impl is external to avoid a malloc()/free() roundtrip for each call.
    bl::ObjectInternal::callExternalDestroyFunc(selfI, selfI->pixelData);
    bl::ObjectInternal::initExternalDestroyFunc(selfI, destroyFunc, userData);
    bl::ObjectInternal::initRefCountToBase(selfI, immutable);

    initImplData(selfI, w, h, format, pixelData, stride);
    return BL_SUCCESS;
  }
  else {
    BLImageCore newO;
    BL_PROPAGATE(allocExternal(&newO, w, h, format, pixelData, stride, immutable, destroyFunc, userData));

    return replaceInstance(self, &newO);
  }
}

// bl::Image - API - Accessors
// ===========================

BL_API_IMPL BLResult blImageGetData(const BLImageCore* self, BLImageData* dataOut) noexcept {
  using namespace bl::ImageInternal;

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
  using namespace bl::ImageInternal;

  BL_ASSERT(self->_d.isImage());
  BLImagePrivateImpl* selfI = getImpl(self);

  BLSizeI size = selfI->size;
  BLFormat format = (BLFormat)selfI->format;

  if (format != BL_FORMAT_NONE && !isImplMutable(selfI)) {
    BLImageCore newO;
    BL_PROPAGATE(allocImpl(&newO, size.w, size.h, format, strideForWidth(size.w, blFormatInfo[format].depth)));

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

// bl::Image - API - Convert
// =========================

BL_API_IMPL BLResult blImageConvert(BLImageCore* self, BLFormat format) noexcept {
  using namespace bl::ImageInternal;

  BL_ASSERT(self->_d.isImage());
  BLImagePrivateImpl* selfI = getImpl(self);

  bl::FormatExt srcFormat = bl::FormatExt(selfI->format);
  bl::FormatExt dstFormat = bl::FormatExt(format);

  if (dstFormat == srcFormat)
    return BL_SUCCESS;

  if (dstFormat == bl::FormatExt::kXRGB32)
    dstFormat = bl::FormatExt::kFRGB32;

  if (srcFormat == bl::FormatExt::kNone)
    return blTraceError(BL_ERROR_NOT_INITIALIZED);

  BLResult result = BL_SUCCESS;
  BLPixelConverterCore pc {};

  BLSizeI size = selfI->size;
  const BLFormatInfo& di = blFormatInfo[size_t(dstFormat)];
  const BLFormatInfo& si = blFormatInfo[size_t(srcFormat)];

  // Save some cycles by calling `blPixelConverterInitInternal` as we don't need to sanitize the destination and
  // source formats in this case.
  if (blPixelConverterInitInternal(&pc, di, si, BL_PIXEL_CONVERTER_CREATE_NO_FLAGS) != BL_SUCCESS) {
    // Built-in formats should always have a built-in converter, so report a different error if the initialization
    // failed. This is pretty critical.
    return blTraceError(BL_ERROR_INVALID_STATE);
  }

  if (di.depth == si.depth && isImplMutable(selfI)) {
    // Prefer in-place conversion if the depths are equal and the image mutable.
    pc.convertFunc(&pc, static_cast<uint8_t*>(selfI->pixelData), selfI->stride,
                        static_cast<uint8_t*>(selfI->pixelData), selfI->stride, uint32_t(size.w), uint32_t(size.h), nullptr);
    selfI->format = uint8_t(format);
  }
  else {
    BLImageCore dstImage;
    result = blImageInitAs(&dstImage, size.w, size.h, format);

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

// bl::Image - API - Equality & Comparison
// =======================================

BL_API_IMPL bool blImageEquals(const BLImageCore* a, const BLImageCore* b) noexcept {
  using namespace bl::ImageInternal;

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

// bl::Image - API - Scale
// =======================

BL_API_IMPL BLResult blImageScale(BLImageCore* dst, const BLImageCore* src, const BLSizeI* size, BLImageScaleFilter filter) noexcept {
  using namespace bl::ImageInternal;

  BL_ASSERT(dst->_d.isImage());
  BL_ASSERT(src->_d.isImage());

  BLImagePrivateImpl* srcI = getImpl(src);
  if (srcI->format == BL_FORMAT_NONE)
    return blImageReset(dst);

  bl::ImageScaleContext scaleCtx;
  BL_PROPAGATE(scaleCtx.create(*size, srcI->size, filter));

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

// bl::Image - API - Read File
// ===========================

BL_API_IMPL BLResult blImageReadFromFile(BLImageCore* self, const char* fileName, const BLArrayCore* codecs) noexcept {
  using namespace bl::ImageInternal;

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

// bl::Image - API - Read Data
// ===========================

BL_API_IMPL BLResult blImageReadFromData(BLImageCore* self, const void* data, size_t size, const BLArrayCore* codecs) noexcept {
  using namespace bl::ImageInternal;

  BL_ASSERT(self->_d.isImage());

  BLImageCodec codec;
  BL_PROPAGATE(blImageCodecFindByData(&codec, data, size, codecs));

  if (BL_UNLIKELY(!(codec.features() & BL_IMAGE_CODEC_FEATURE_READ)))
    return blTraceError(BL_ERROR_IMAGE_DECODER_NOT_PROVIDED);

  BLImageDecoder decoder;
  BL_PROPAGATE(codec.createDecoder(&decoder));
  return decoder.readFrame(*self, data, size);
}

// bl::Image - API - Write File
// ============================

namespace bl {
namespace ImageInternal {

static BLResult writeToFileInternal(const BLImageCore* self, const char* fileName, const BLImageCodecCore* codec) noexcept {
  BL_ASSERT(self->_d.isImage());
  BL_ASSERT(codec->_d.isImageCodec());

  BLArray<uint8_t> buffer;
  BL_PROPAGATE(blImageWriteToData(self, &buffer, codec));
  return BLFileSystem::writeFile(fileName, buffer);
}

} // {ImageInternal}
} // {bl}

BL_API_IMPL BLResult blImageWriteToFile(const BLImageCore* self, const char* fileName, const BLImageCodecCore* codec) noexcept {
  using namespace bl::ImageInternal;

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

// bl::Image - API - Write Data
// ============================

BL_API_IMPL BLResult blImageWriteToData(const BLImageCore* self, BLArrayCore* dst, const BLImageCodecCore* codec) noexcept {
  using namespace bl::ImageInternal;

  BL_ASSERT(self->_d.isImage());
  BL_ASSERT(codec->_d.isImageCodec());

  if (BL_UNLIKELY(!(codec->dcast().features() & BL_IMAGE_CODEC_FEATURE_WRITE)))
    return blTraceError(BL_ERROR_IMAGE_ENCODER_NOT_PROVIDED);

  BLImageEncoder encoder;
  BL_PROPAGATE(codec->dcast().createEncoder(&encoder));
  return encoder.writeFrame(dst->dcast<BLArray<uint8_t>>(), self->dcast());
}

// bl::Image - Runtime Registration
// ================================

void blImageRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  auto& defaultImage = bl::ImageInternal::defaultImage;
  blObjectDefaults[BL_OBJECT_TYPE_IMAGE]._d.initDynamic(
    BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_IMAGE), &defaultImage.impl);
}
