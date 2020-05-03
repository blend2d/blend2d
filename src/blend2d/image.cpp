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
#include "./array_p.h"
#include "./filesystem.h"
#include "./format.h"
#include "./image_p.h"
#include "./imagecodec.h"
#include "./imagescale_p.h"
#include "./pixelconverter_p.h"
#include "./runtime_p.h"
#include "./support_p.h"

// ============================================================================
// [BLImage - Globals]
// ============================================================================

static BLWrap<BLInternalImageImpl> blNullImageImpl;

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

  BLOverflowFlag of = 0;
  uint32_t depth = blFormatInfo[format].depth;
  size_t stride = blImageStrideForWidth(unsigned(w), depth, &of);

  size_t baseSize = sizeof(BLInternalImageImpl);
  size_t implSize = blMulOverflow<size_t>(size_t(h), stride, &of);

  size_t dataAlignment = implSize <= BL_INTERNAL_IMAGE_LARGE_DATA_THRESHOLD
    ? BL_INTERNAL_IMAGE_SMALL_DATA_ALIGNMENT
    : BL_INTERNAL_IMAGE_LARGE_DATA_ALIGNMENT;

  if (dataAlignment > BL_ALLOC_ALIGNMENT)
    baseSize += dataAlignment - BL_ALLOC_ALIGNMENT;

  implSize = blAddOverflow<size_t>(baseSize, implSize, &of);
  if (BL_UNLIKELY(of))
    return nullptr;

  uint16_t memPoolData;
  BLInternalImageImpl* impl = blRuntimeAllocImplT<BLInternalImageImpl>(implSize, &memPoolData);

  if (BL_UNLIKELY(!impl))
    return impl;

  uint8_t* pixelData = reinterpret_cast<uint8_t*>(impl) + sizeof(BLInternalImageImpl);
  if (dataAlignment > BL_ALLOC_ALIGNMENT)
    pixelData = blAlignUp(pixelData, dataAlignment);

  blImplInit(impl, BL_IMPL_TYPE_IMAGE, BL_IMPL_TRAIT_MUTABLE, memPoolData);
  impl->pixelData = pixelData;
  impl->stride = intptr_t(stride);
  impl->format = uint8_t(format);
  impl->flags = uint8_t(0);
  impl->depth = uint16_t(depth);
  impl->size.reset(w, h);
  impl->writerCount = 0;

  return impl;
}

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

  preface->destroyFunc = destroyFunc ? destroyFunc : blRuntimeDummyDestroyImplFunc;
  preface->destroyData = destroyFunc ? destroyData : nullptr;

  blImplInit(impl, BL_IMPL_TYPE_IMAGE, BL_IMPL_TRAIT_MUTABLE | BL_IMPL_TRAIT_EXTERNAL, memPoolData);
  impl->pixelData = pixelData;
  impl->stride = stride;
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

  // Postpone the deletion in case that the image still has writers attached.
  // This is required as the rendering context doesn't manipulate the reference
  // count of `BLImage` (otherwise it would not be possible to attach multiple
  // rendering contexts, for example).
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
    size_t dataSize = uintptr_t(blAbs(impl->stride)) * uint32_t(impl->size.h);
    size_t dataAlignment = dataSize <= BL_INTERNAL_IMAGE_LARGE_DATA_THRESHOLD
      ? BL_INTERNAL_IMAGE_SMALL_DATA_ALIGNMENT
      : BL_INTERNAL_IMAGE_LARGE_DATA_ALIGNMENT;

    implSize = sizeof(BLInternalImageImpl) + dataSize;
    if (dataAlignment > BL_ALLOC_ALIGNMENT)
      implSize += dataAlignment - BL_ALLOC_ALIGNMENT;
  }

  if (implTraits & BL_IMPL_TRAIT_FOREIGN)
    return BL_SUCCESS;
  else
    return blRuntimeFreeImpl(implBase, implSize, memPoolData);
}

static BL_INLINE BLResult blImageImplRelease(BLInternalImageImpl* impl) noexcept {
  if (blImplDecRefAndTest(impl))
    return blImageImplDelete(impl);
  return BL_SUCCESS;
}

// ============================================================================
// [BLImage - Init / Destroy]
// ============================================================================

BLResult blImageInit(BLImageCore* self) noexcept {
  self->impl = &blNullImageImpl;
  return BL_SUCCESS;
}

BLResult blImageInitAs(BLImageCore* self, int w, int h, uint32_t format) noexcept {
  self->impl = &blNullImageImpl;
  return blImageCreate(self, w, h, format);
}

BLResult blImageInitAsFromData(BLImageCore* self, int w, int h, uint32_t format, void* pixelData, intptr_t stride, BLDestroyImplFunc destroyFunc, void* destroyData) noexcept {
  self->impl = &blNullImageImpl;
  return blImageCreateFromData(self, w, h, format, pixelData, stride, destroyFunc, destroyData);
}

BLResult blImageDestroy(BLImageCore* self) noexcept {
  BLInternalImageImpl* selfI = blInternalCast(self->impl);
  self->impl = nullptr;
  return blImageImplRelease(selfI);
}

// ============================================================================
// [BLImage - Reset]
// ============================================================================

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
// [BLImage - Convert]
// ============================================================================

BLResult blImageConvert(BLImageCore* self, uint32_t format) noexcept {
  BLInternalImageImpl* selfI = blInternalCast(self->impl);

  uint32_t srcFormat = selfI->format;
  uint32_t dstFormat = format;

  if (dstFormat == srcFormat)
    return BL_SUCCESS;

  if (srcFormat == BL_FORMAT_NONE)
    return blTraceError(BL_ERROR_NOT_INITIALIZED);

  BLResult result = BL_SUCCESS;
  BLPixelConverterCore pc {};

  int w = selfI->size.w;
  int h = selfI->size.h;

  const BLFormatInfo& di = blFormatInfo[dstFormat];
  const BLFormatInfo& si = blFormatInfo[srcFormat];

  // Save some cycles by calling `blPixelConverterInitInternal` as we don't
  // need to sanitize the destination and source formats in this case.
  if (blPixelConverterInitInternal(&pc, di, si, 0) != BL_SUCCESS) {
    // Built-in formats should always have a built-in converter, so report a
    // different error if the initialization failed. This is pretty critical.
    return blTraceError(BL_ERROR_INVALID_STATE);
  }

  if (di.depth == si.depth && blImplIsMutable(selfI)) {
    // Prefer in-place conversion if the depths are equal and the image mutable.
    pc.convertFunc(&pc, static_cast<uint8_t*>(selfI->pixelData), selfI->stride,
                        static_cast<uint8_t*>(selfI->pixelData), selfI->stride, uint32_t(w), uint32_t(h), nullptr);
  }
  else {
    BLImageCore dstImage;
    result = blImageInitAs(&dstImage, w, h, dstFormat);
    if (result == BL_SUCCESS) {
      BLInternalImageImpl* dstI = blInternalCast(dstImage.impl);
      BLPixelConverterOptions opt {};

      opt.gap = uintptr_t(blAbs(dstI->stride)) - uintptr_t(uint32_t(w)) * (dstI->depth / 8u);
      pc.convertFunc(&pc, static_cast<uint8_t*>(dstI->pixelData), dstI->stride,
                          static_cast<uint8_t*>(selfI->pixelData), selfI->stride, uint32_t(w), uint32_t(h), &opt);

      self->impl = dstImage.impl;
      blImageImplRelease(selfI);
    }
  }

  blPixelConverterReset(&pc);
  return result;
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
// [BLImage - Read]
// ============================================================================

BLResult blImageReadFromFile(BLImageCore* self, const char* fileName, const BLArrayCore* codecs) noexcept {
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
  return decoder.readFrame(*blDownCast(self), buffer);
}

BLResult blImageReadFromData(BLImageCore* self, const void* data, size_t size, const BLArrayCore* codecs) noexcept {
  BLImageCodec codec;
  BL_PROPAGATE(blImageCodecFindByData(&codec, data, size, codecs));

  if (BL_UNLIKELY(!(codec.features() & BL_IMAGE_CODEC_FEATURE_READ)))
    return blTraceError(BL_ERROR_IMAGE_DECODER_NOT_PROVIDED);

  BLImageDecoder decoder;
  BL_PROPAGATE(codec.createDecoder(&decoder));
  return decoder.readFrame(*blDownCast(self), data, size);
}

// ============================================================================
// [BLImage - Write]
// ============================================================================

static BLResult blImageWriteToFileInternal(const BLImageCore* self, const char* fileName, const BLImageCodecCore* codec) noexcept {
  BLArray<uint8_t> buffer;
  BL_PROPAGATE(blImageWriteToData(self, &buffer, codec));
  return BLFileSystem::writeFile(fileName, buffer);
}

BLResult blImageWriteToFile(const BLImageCore* self, const char* fileName, const BLImageCodecCore* codec) noexcept {
  if (!codec) {
    BLImageCodec localCodec;
    BL_PROPAGATE(localCodec.findByExtension(fileName));
    return blImageWriteToFileInternal(self, fileName, &localCodec);
  }
  else {
    return blImageWriteToFileInternal(self, fileName, codec);
  }
}

BLResult blImageWriteToData(const BLImageCore* self, BLArrayCore* dst, const BLImageCodecCore* codec) noexcept {
  if (BL_UNLIKELY(!(blDownCast(codec)->features() & BL_IMAGE_CODEC_FEATURE_WRITE)))
    return blTraceError(BL_ERROR_IMAGE_ENCODER_NOT_PROVIDED);

  BLImageEncoder encoder;
  BL_PROPAGATE(blDownCast(codec)->createEncoder(&encoder));
  return encoder.writeFrame(dst->dcast<BLArray<uint8_t>>(), *blDownCast(self));
}

// ============================================================================
// [BLImage - Runtime]
// ============================================================================

void blImageOnInit(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);

  BLInternalImageImpl* imageI = &blNullImageImpl;
  blInitBuiltInNull(imageI, BL_IMPL_TYPE_IMAGE, 0);
  blAssignBuiltInNull(imageI);
}
