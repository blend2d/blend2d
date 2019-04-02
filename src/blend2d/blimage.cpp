// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blfilesystem.h"
#include "./blformat.h"
#include "./blimage_p.h"
#include "./blimagescale_p.h"
#include "./blruntime_p.h"
#include "./blsupport_p.h"
#include "./codec/blbmpcodec_p.h"
#include "./codec/bljpegcodec_p.h"
#include "./codec/blpngcodec_p.h"

// ============================================================================
// [Global Variables]
// ============================================================================

static BLImageCodecVirt blNullImageCodecVirt;
static BLImageDecoderVirt blNullImageDecoderVirt;
static BLImageEncoderVirt blNullImageEncoderVirt;

static BLWrap<BLInternalImageImpl> blNullImageImpl;
static BLWrap<BLImageCodecImpl> blNullImageCodecImpl;
static BLWrap<BLImageEncoderImpl> blNullImageEncoderImpl;
static BLWrap<BLImageDecoderImpl> blNullImageDecoderImpl;

static BLWrap<BLArray<BLImageCodec>> blImageBuildInCodecs;

static const char blEmptyCString[] = "";

// ============================================================================
// [BLImage - Utilities]
// ============================================================================

static BL_INLINE void blZeroMemoryInline(uint8_t* dst, size_t n) noexcept {
  for (size_t i = 0; i < n; i++)
    dst[i] = 0;
}

static void blImageCopy(uint8_t* dstData, intptr_t dstStride, const uint8_t* srcData, intptr_t srcStride, int w, int h, uint32_t format) noexcept {
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
      blZeroMemoryInline(dstData + bytesPerLine, gap);

      dstData += dstStride;
      srcData += srcStride;
    }
  }
}

// ============================================================================
// [BLImage - Internals]
// ============================================================================

static BLInternalImageImpl* blImageImplNewInternal(int w, int h, uint32_t format) noexcept {
  BL_ASSERT(w > 0 && h > 0);
  BL_ASSERT(format < BL_FORMAT_COUNT);

  uint32_t depth = blFormatInfo[format].depth;
  size_t stride = blImageStrideForWidth(w, depth);

  BL_ASSERT(stride != 0);

  size_t baseSize = sizeof(BLInternalImageImpl);
  size_t implSize;

  if (BL_INTERNAL_IMAGE_DATA_ALIGNMENT > sizeof(void*))
    baseSize += BL_INTERNAL_IMAGE_DATA_ALIGNMENT - sizeof(void*);

  BLOverflowFlag of = 0;
  implSize = blMulOverflow<size_t>(size_t(h), stride, &of);
  implSize = blAddOverflow<size_t>(baseSize, implSize, &of);

  if (BL_UNLIKELY(of))
    return nullptr;

  uint16_t memPoolData;
  BLInternalImageImpl* impl = blRuntimeAllocImplT<BLInternalImageImpl>(implSize, &memPoolData);

  if (BL_UNLIKELY(!impl))
    return impl;

  uint8_t* pixelData = reinterpret_cast<uint8_t*>(impl) + sizeof(BLInternalImageImpl);
  if (BL_INTERNAL_IMAGE_DATA_ALIGNMENT > sizeof(void*))
    pixelData = blAlignUp(pixelData, BL_INTERNAL_IMAGE_DATA_ALIGNMENT);

  blImplInit(impl, BL_IMPL_TYPE_IMAGE, 0, memPoolData);
  impl->pixelData = pixelData;
  impl->stride = intptr_t(stride);
  impl->writer = nullptr;
  impl->format = uint8_t(format);
  impl->flags = uint8_t(0);
  impl->depth = uint16_t(depth);
  impl->size.reset(w, h);
  impl->writerCount = 0;

  return impl;
}

static void BL_CDECL blImageImplDestroyExternalDummyFunc(void* impl, void* destroyData) noexcept {}

static BLInternalImageImpl* blImageImplNewExternal(int w, int h, uint32_t format, void* pixelData, intptr_t stride, BLDestroyImplFunc destroyFunc, void* destroyData) noexcept {
  BL_ASSERT(w > 0 && h > 0);
  BL_ASSERT(format < BL_FORMAT_COUNT);

  size_t implSize = sizeof(BLExternalImplPreface) + sizeof(BLInternalImageImpl);
  uint16_t memPoolData;

  void* p = blRuntimeAllocImpl(implSize, &memPoolData);
  if (BL_UNLIKELY(!p))
    return nullptr;

  BLExternalImplPreface* preface = static_cast<BLExternalImplPreface*>(p);
  BLInternalImageImpl* impl = blOffsetPtr<BLInternalImageImpl>(p, sizeof(BLExternalImplPreface));

  preface->destroyFunc = destroyFunc ? destroyFunc : blImageImplDestroyExternalDummyFunc;
  preface->destroyData = destroyData;

  blImplInit(impl, BL_IMPL_TYPE_IMAGE, BL_IMPL_TRAIT_EXTERNAL, memPoolData);
  impl->pixelData = pixelData;
  impl->stride = stride;
  impl->writer = nullptr;
  impl->format = uint8_t(format);
  impl->flags = uint8_t(0);
  impl->depth = uint16_t(blFormatInfo[format].depth);
  impl->size.reset(w, h);
  impl->writerCount = 0;

  return impl;
}

// Cannot be static, called by `BLVariant` implementation.
BLResult blImageImplDelete(BLImageImpl* impl_) noexcept {
  BLInternalImageImpl* impl = blInternalCast(impl_);

  if (impl->writerCount != 0)
    return BL_SUCCESS;

  uint8_t* implBase = reinterpret_cast<uint8_t*>(impl);
  size_t implSize = 0;
  uint32_t implTraits = impl->implTraits;
  uint32_t memPoolData = impl->memPoolData;

  if (implTraits & BL_IMPL_TRAIT_EXTERNAL) {
    // External does never allocate the image-data past `BLInternalImageImpl`.
    implSize = sizeof(BLInternalImageImpl) + sizeof(BLExternalImplPreface);
    implBase -= sizeof(BLExternalImplPreface);
    blImplDestroyExternal(impl);
  }
  else {
    implSize = sizeof(BLInternalImageImpl) +
               BL_INTERNAL_IMAGE_DATA_ALIGNMENT +
               impl->size.h * size_t(blAbs(impl->stride));
  }

  if (implTraits & BL_IMPL_TRAIT_FOREIGN)
    return BL_SUCCESS;
  else
    return blRuntimeFreeImpl(implBase, implSize, memPoolData);
}

static BL_INLINE BLResult blImageImplRelease(BLInternalImageImpl* impl) noexcept {
  if (blAtomicFetchDecRef(&impl->refCount) != 1)
    return BL_SUCCESS;
  return blImageImplDelete(impl);
}

// ============================================================================
// [BLImage - Init / Reset]
// ============================================================================

BLResult blImageInit(BLImageCore* self) noexcept {
  self->impl = &blNullImageImpl;
  return BL_SUCCESS;
}

BLResult blImageInitAs(BLImageCore* self, int w, int h, uint32_t format) noexcept {
  self->impl = &blNullImageImpl;
  return blImageCreate(self, w, h, format);
}

BLResult blImageReset(BLImageCore* self) noexcept {
  BLInternalImageImpl* selfI = blInternalCast(self->impl);
  self->impl = &blNullImageImpl;
  return blImageImplRelease(selfI);
}

// ============================================================================
// [BLImage - Assign]
// ============================================================================

BLResult blImageAssignMove(BLImageCore* self, BLImageCore* other) noexcept {
  BLInternalImageImpl* selfI = blInternalCast(self->impl);
  BLInternalImageImpl* otherI = blInternalCast(other->impl);

  self->impl = otherI;
  other->impl = &blNullImageImpl;

  return blImageImplRelease(selfI);
}

BLResult blImageAssignWeak(BLImageCore* self, const BLImageCore* other) noexcept {
  BLInternalImageImpl* selfI = blInternalCast(self->impl);
  BLInternalImageImpl* otherI = blInternalCast(other->impl);

  self->impl = blImplIncRef(otherI);
  return blImageImplRelease(selfI);
}

BLResult blImageAssignDeep(BLImageCore* self, const BLImageCore* other) noexcept {
  BLInternalImageImpl* selfI = blInternalCast(self->impl);
  BLInternalImageImpl* otherI = blInternalCast(other->impl);

  int w = otherI->size.w;
  int h = otherI->size.h;
  uint32_t format = otherI->format;

  BLImageData dummyImageData;
  if (selfI == otherI)
    return blImageMakeMutable(self, &dummyImageData);

  BL_PROPAGATE(blImageCreate(self, w, h, format));
  selfI = blInternalCast(self->impl);

  blImageCopy(static_cast<uint8_t*>(selfI->pixelData), selfI->stride,
              static_cast<uint8_t*>(otherI->pixelData), otherI->stride, w, h, format);
  return BL_SUCCESS;
}

// ============================================================================
// [BLImage - Create]
// ============================================================================

BLResult blImageCreate(BLImageCore* self, int w, int h, uint32_t format) noexcept {
  if (BL_UNLIKELY(w <= 0 || h <= 0 ||
                  format == BL_FORMAT_NONE ||
                  format >= BL_FORMAT_COUNT)) {
    if (w == 0 && h == 0 && format == BL_FORMAT_NONE)
      return blImageReset(self);
    else
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  if (BL_UNLIKELY(unsigned(w) >= BL_RUNTIME_MAX_IMAGE_SIZE ||
                  unsigned(h) >= BL_RUNTIME_MAX_IMAGE_SIZE))
    return blTraceError(BL_ERROR_IMAGE_TOO_LARGE);

  BLInternalImageImpl* selfI = blInternalCast(self->impl);
  if (selfI->size.w == w && selfI->size.h == h && selfI->format == format && !(selfI->implTraits & BL_IMPL_TRAIT_EXTERNAL) && blImplIsMutable(selfI))
    return BL_SUCCESS;

  BLInternalImageImpl* newI = blImageImplNewInternal(w, h, format);
  if (newI == nullptr)
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  self->impl = newI;
  return blImageImplRelease(selfI);
}

BLResult blImageCreateFromData(BLImageCore* self, int w, int h, uint32_t format, void* pixelData, intptr_t stride, BLDestroyImplFunc destroyFunc, void* destroyData) noexcept {
  if (BL_UNLIKELY(w <= 0 || h <= 0 ||
                  format == BL_FORMAT_NONE ||
                  format >= BL_FORMAT_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (BL_UNLIKELY(unsigned(w) >= BL_RUNTIME_MAX_IMAGE_SIZE ||
                  unsigned(h) >= BL_RUNTIME_MAX_IMAGE_SIZE))
    return blTraceError(BL_ERROR_IMAGE_TOO_LARGE);

  BLInternalImageImpl* newI = blImageImplNewExternal(w, h, format, pixelData, stride, destroyFunc, destroyData);
  if (newI == nullptr)
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BLInternalImageImpl* selfI = blInternalCast(self->impl);
  self->impl = newI;
  return blImageImplRelease(selfI);
}

// ============================================================================
// [BLImage [GetData / MakeMutable]
// ============================================================================

BLResult blImageGetData(const BLImageCore* self, BLImageData* dataOut) noexcept {
  BLInternalImageImpl* selfI = blInternalCast(self->impl);

  dataOut->pixelData = selfI->pixelData;
  dataOut->stride = selfI->stride;
  dataOut->size = selfI->size;
  dataOut->format = selfI->format;
  dataOut->flags = 0;

  return BL_SUCCESS;
}

BLResult blImageMakeMutable(BLImageCore* self, BLImageData* dataOut) noexcept {
  BLInternalImageImpl* selfI = blInternalCast(self->impl);
  int w = selfI->size.w;
  int h = selfI->size.h;
  uint32_t format = selfI->format;

  if (format != BL_FORMAT_NONE && !blImplIsMutable(selfI)) {
    BLInternalImageImpl* newI = blImageImplNewInternal(w, h, format);
    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    dataOut->pixelData = newI->pixelData;
    dataOut->stride = newI->stride;
    dataOut->size.reset(w, h);
    dataOut->format = format;
    dataOut->flags = 0;

    blImageCopy(static_cast<uint8_t*>(newI->pixelData), newI->stride,
                static_cast<uint8_t*>(selfI->pixelData), selfI->stride, w, h, format);
    self->impl = newI;
    return blImageImplRelease(selfI);
  }
  else {
    dataOut->pixelData = selfI->pixelData;
    dataOut->stride = selfI->stride;
    dataOut->size.reset(w, h);
    dataOut->format = format;
    dataOut->flags = 0;
    return BL_SUCCESS;
  }
}

// ============================================================================
// [BLImage - Equals]
// ============================================================================

bool blImageEquals(const BLImageCore* a, const BLImageCore* b) noexcept {
  const BLInternalImageImpl* aImpl = blInternalCast(a->impl);
  const BLInternalImageImpl* bImpl = blInternalCast(b->impl);

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

// ============================================================================
// [BLImage - Scale]
// ============================================================================

BLResult blImageScale(BLImageCore* dst, const BLImageCore* src, const BLSizeI* size, uint32_t filter, const BLImageScaleOptions* options) noexcept {
  BLImageImpl* srcI = src->impl;
  if (srcI->format == BL_FORMAT_NONE)
    return blImageReset(dst);

  BLImageScaleContext scaleCtx;
  BL_PROPAGATE(scaleCtx.create(*size, srcI->size, filter, options));

  uint32_t format = srcI->format;
  int tw = scaleCtx.dstWidth();
  int th = scaleCtx.srcHeight();

  BLImage tmp;
  BLImageData buf;

  if (th == scaleCtx.dstHeight() || tw == scaleCtx.srcWidth()) {
    // Only horizontal or vertical scale.

    // Move to `tmp` so it's not destroyed by `dst->create()`.
    if (dst == src) tmp = *blDownCast(src);

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

    srcI = tmp.impl;
    BL_PROPAGATE(blImageCreate(dst, scaleCtx.dstWidth(), scaleCtx.dstHeight(), format));
    BL_PROPAGATE(blImageMakeMutable(dst, &buf));

    scaleCtx.processVertData(static_cast<uint8_t*>(buf.pixelData), buf.stride, static_cast<const uint8_t*>(srcI->pixelData), srcI->stride, format);
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLImage - Read / Write]
// ============================================================================

BLResult blImageReadFromFile(BLImageCore* self, const char* fileName, const BLArrayCore* codecs) noexcept {
  BLArray<uint8_t> buf;
  BL_PROPAGATE(BLFileSystem::readFile(fileName, buf));

  BLImageCodec codec;
  BL_PROPAGATE(blImageCodecFindByData(&codec, codecs, buf.data(), buf.size()));

  if (BL_UNLIKELY(!(codec.features() & BL_IMAGE_CODEC_FEATURE_READ)))
    return blTraceError(BL_ERROR_IMAGE_DECODER_NOT_PROVIDED);

  BLImageDecoder decoder;
  BL_PROPAGATE(codec.createDecoder(&decoder));
  return decoder.readFrame(*blDownCast(self), buf);
}

BLResult blImageReadFromData(BLImageCore* self, const void* data, size_t size, const BLArrayCore* codecs) noexcept {
  BLImageCodec codec;
  BL_PROPAGATE(blImageCodecFindByData(&codec, codecs, data, size));

  if (BL_UNLIKELY(!(codec.features() & BL_IMAGE_CODEC_FEATURE_READ)))
    return blTraceError(BL_ERROR_IMAGE_DECODER_NOT_PROVIDED);

  BLImageDecoder decoder;
  BL_PROPAGATE(codec.createDecoder(&decoder));
  return decoder.readFrame(*blDownCast(self), data, size);
}

BLResult blImageWriteToFile(const BLImageCore* self, const char* fileName, const BLImageCodecCore* codec) noexcept {
  BLArray<uint8_t> buf;
  BL_PROPAGATE(blImageWriteToData(self, &buf, codec));
  return BLFileSystem::writeFile(fileName, buf);
}

BLResult blImageWriteToData(const BLImageCore* self, BLArrayCore* dst, const BLImageCodecCore* codec) noexcept {
  if (BL_UNLIKELY(!(blDownCast(codec)->features() & BL_IMAGE_CODEC_FEATURE_WRITE)))
    return blTraceError(BL_ERROR_IMAGE_ENCODER_NOT_PROVIDED);

  BLImageEncoder encoder;
  BL_PROPAGATE(blDownCast(codec)->createEncoder(&encoder));
  return encoder.writeFrame(dst->dcast<BLArray<uint8_t>>(), *blDownCast(self));
}

// ============================================================================
// [BLImageCodec - Init / Reset]
// ============================================================================

BLResult blImageCodecInit(BLImageCodecCore* self) noexcept {
  self->impl = &blNullImageCodecImpl;
  return BL_SUCCESS;
}

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
// [BLImageCodec - Interface]
// ============================================================================

uint32_t blImageCodecInspectData(const BLImageCodecCore* self, const void* data, size_t size) noexcept {
  BLImageCodecImpl* selfI = self->impl;
  return selfI->virt->inspectData(selfI, static_cast<const uint8_t*>(data), size);
}

BLResult blImageCodecFindByName(BLImageCodecCore* self, const BLArrayCore* codecs, const char* name) noexcept {
  for (const auto& codec : codecs->dcast<BLArray<BLImageCodec>>().view()) {
    if (strcmp(codec.name(), name) == 0)
      return blImageCodecAssignWeak(self, &codec);
  }

  return BL_ERROR_IMAGE_NO_MATCHING_CODEC;
}

BLResult blImageCodecFindByData(BLImageCodecCore* self, const BLArrayCore* codecs, const void* data, size_t size) noexcept {
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

  return BL_ERROR_IMAGE_NO_MATCHING_CODEC;
}

BLResult blImageCodecCreateDecoder(const BLImageCodecCore* self, BLImageDecoderCore* dst) noexcept {
  BLImageCodecImpl* selfI = self->impl;
  return selfI->virt->createDecoder(selfI, dst);
}

BLResult blImageCodecCreateEncoder(const BLImageCodecCore* self, BLImageEncoderCore* dst) noexcept {
  BLImageCodecImpl* selfI = self->impl;
  return selfI->virt->createEncoder(selfI, dst);
}

BLArrayCore* blImageCodecBuiltInCodecs(void) noexcept {
  return &blImageBuildInCodecs;
}

// ============================================================================
// [BLImageCodec - Virtual Functions]
// ============================================================================

static BLResult BL_CDECL blImageCodecImplDestroy(BLImageCodecImpl* impl) noexcept { return BL_SUCCESS; }
static uint32_t BL_CDECL blImageCodecImplInspectData(const BLImageCodecImpl* impl, const uint8_t* data, size_t size) noexcept { return 0; }
static BLResult BL_CDECL blImageCodecImplCreateDecoder(const BLImageCodecImpl* impl, BLImageDecoderCore* dst) noexcept { return BL_ERROR_IMAGE_DECODER_NOT_PROVIDED; }
static BLResult BL_CDECL blImageCodecImplCreateEncoder(const BLImageCodecImpl* impl, BLImageEncoderCore* dst) noexcept { return BL_ERROR_IMAGE_ENCODER_NOT_PROVIDED; }

// ============================================================================
// [BLImageDecoder - Init / Reset]
// ============================================================================

BLResult blImageDecoderInit(BLImageDecoderCore* self) noexcept {
  self->impl = &blNullImageDecoderImpl;
  return BL_SUCCESS;
}

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

static BLResult BL_CDECL blImageDecoderImplDestroy(BLImageDecoderImpl* impl) noexcept { return BL_SUCCESS; }
static uint32_t BL_CDECL blImageDecoderImplRestart(BLImageDecoderImpl* impl) noexcept { return BL_ERROR_INVALID_STATE; }
static BLResult BL_CDECL blImageDecoderImplReadInfo(BLImageDecoderImpl* impl, BLImageInfo* infoOut, const uint8_t* data, size_t size) noexcept { return BL_ERROR_INVALID_STATE; }
static BLResult BL_CDECL blImageDecoderImplReadFrame(BLImageDecoderImpl* impl, BLImageCore* imageOut, const uint8_t* data, size_t size) noexcept { return BL_ERROR_INVALID_STATE; }

// ============================================================================
// [BLImageEncoder - Init / Reset]
// ============================================================================

BLResult blImageEncoderInit(BLImageEncoderCore* self) noexcept {
  self->impl = &blNullImageEncoderImpl;
  return BL_SUCCESS;
}

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

static BLResult BL_CDECL blImageEncoderImplDestroy(BLImageEncoderImpl* impl) noexcept { return BL_SUCCESS; }
static uint32_t BL_CDECL blImageEncoderImplRestart(BLImageEncoderImpl* impl) noexcept { return BL_ERROR_INVALID_STATE; }
static BLResult BL_CDECL blImageEncoderImplWriteFrame(BLImageEncoderImpl* impl, BLArrayCore* dst, const BLImageCore* image) noexcept { return BL_ERROR_INVALID_STATE; }

// ============================================================================
// [BLImage - Runtime Init]
// ============================================================================

static void BL_CDECL blImageRtShutdown(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);
  blImageBuildInCodecs.destroy();
}

void blImageRtInit(BLRuntimeContext* rt) noexcept {
  BLInternalImageImpl* imageI = &blNullImageImpl;
  imageI->implType = uint8_t(BL_IMPL_TYPE_IMAGE);
  imageI->implTraits = uint8_t(BL_IMPL_TRAIT_NULL);
  blAssignBuiltInNull(imageI);

  BLImageCodecVirt* codecV = &blNullImageCodecVirt;
  codecV->destroy = blImageCodecImplDestroy;
  codecV->inspectData = blImageCodecImplInspectData;
  codecV->createDecoder = blImageCodecImplCreateDecoder;
  codecV->createEncoder = blImageCodecImplCreateEncoder;

  BLImageCodecImpl* codecI = &blNullImageCodecImpl;
  codecI->virt = codecV;
  codecI->implType = uint8_t(BL_IMPL_TYPE_IMAGE_CODEC);
  codecI->implTraits = uint8_t(BL_IMPL_TRAIT_NULL | BL_IMPL_TRAIT_VIRT);
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
  decoderI->virt = decoderV;
  decoderI->implType = uint8_t(BL_IMPL_TYPE_IMAGE_DECODER);
  decoderI->implTraits = uint8_t(BL_IMPL_TRAIT_NULL | BL_IMPL_TRAIT_VIRT);
  decoderI->lastResult = BL_ERROR_INVALID_STATE;
  blAssignBuiltInNull(decoderI);

  BLImageEncoderVirt* encoderV = &blNullImageEncoderVirt;
  encoderV->destroy = blImageEncoderImplDestroy;
  encoderV->restart = blImageEncoderImplRestart;
  encoderV->writeFrame = blImageEncoderImplWriteFrame;

  BLImageEncoderImpl* encoderI = &blNullImageEncoderImpl;
  encoderI->virt = encoderV;
  encoderI->implType = uint8_t(BL_IMPL_TYPE_IMAGE_ENCODER);
  encoderI->implTraits = uint8_t(BL_IMPL_TRAIT_NULL | BL_IMPL_TRAIT_VIRT);
  encoderI->lastResult = BL_ERROR_INVALID_STATE;
  blAssignBuiltInNull(encoderI);

  // Register built-in codecs.
  BLImageCodecCore bmpCodec  { blBmpCodecRtInit(rt)  };
  BLImageCodecCore jpegCodec { blJpegCodecRtInit(rt) };
  BLImageCodecCore pngCodec  { blPngCodecRtInit(rt)  };

  BLArray<BLImageCodec>* codecs = blImageBuildInCodecs.init();
  codecs->append(blDownCast(bmpCodec));
  codecs->append(blDownCast(jpegCodec));
  codecs->append(blDownCast(pngCodec));

  rt->shutdownHandlers.add(blImageRtShutdown);
}
