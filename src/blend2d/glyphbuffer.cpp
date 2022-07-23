// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "font_p.h"
#include "glyphbuffer_p.h"
#include "runtime_p.h"
#include "string_p.h"
#include "unicode_p.h"
#include "support/intops_p.h"
#include "support/ptrops_p.h"
#include "support/stringops_p.h"

// BLGlyphBuffer - Internals
// =========================

static const constexpr BLGlyphBufferPrivateImpl blGlyphBufferInternalImplNone {};

static BL_INLINE BLResult blGlyphBufferEnsureData(BLGlyphBufferCore* self, BLGlyphBufferPrivateImpl** impl) noexcept {
  *impl = blGlyphBufferGetImpl(self);
  if (*impl != &blGlyphBufferInternalImplNone)
    return BL_SUCCESS;

  *impl = BLGlyphBufferPrivateImpl::create();
  if (BL_UNLIKELY(!*impl))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  self->impl = *impl;
  return BL_SUCCESS;
}

// BLGlyphBuffer - Private API
// ===========================

BLResult BLGlyphBufferPrivateImpl::ensureBuffer(size_t bufferId, size_t copySize, size_t minCapacity) noexcept {
  size_t oldCapacity = capacity[bufferId];
  BL_ASSERT(copySize <= oldCapacity);

  if (minCapacity <= oldCapacity)
    return BL_SUCCESS;

  size_t newCapacity = minCapacity;
  if (newCapacity < BL_GLYPH_BUFFER_INITIAL_CAPACITY)
    newCapacity = BL_GLYPH_BUFFER_INITIAL_CAPACITY;
  else if (newCapacity < SIZE_MAX - 256)
    newCapacity = BLIntOps::alignUp(minCapacity, 64);

  BLOverflowFlag of = 0;
  size_t dataSize = BLIntOps::mulOverflow<size_t>(newCapacity, BL_GLYPH_BUFFER_ANY_ITEM_SIZE, &of);

  if (BL_UNLIKELY(of))
    return BL_ERROR_OUT_OF_MEMORY;

  uint8_t* newData = static_cast<uint8_t*>(malloc(dataSize));
  if (BL_UNLIKELY(!newData))
    return BL_ERROR_OUT_OF_MEMORY;

  uint8_t* oldData = static_cast<uint8_t*>(buffer[bufferId]);
  if (copySize) {
    memcpy(newData,
           oldData,
           copySize * sizeof(uint32_t));

    memcpy(newData + newCapacity * sizeof(uint32_t),
           oldData + oldCapacity * sizeof(uint32_t),
           copySize * sizeof(BLGlyphInfo));
  }

  free(oldData);
  buffer[bufferId] = newData;
  capacity[bufferId] = newCapacity;

  if (bufferId == 0)
    getGlyphDataPtrs(0, &content, &infoData);

  return BL_SUCCESS;
}

template<typename T>
static BL_INLINE BLGlyphInfo blGlyphInfoFromCluster(const T& cluster) noexcept {
  return BLGlyphInfo { uint32_t(cluster), { 0, 0 } };
}

template<typename T>
static BL_INLINE BLResult blInternalGlyphBufferData_setGlyphIds(BLGlyphBufferPrivateImpl* d, const T* src, size_t size, intptr_t advance) noexcept {
  uint32_t* glyphData = d->content;
  BLGlyphInfo* infoData = d->infoData;

  for (size_t i = 0; i < size; i++) {
    glyphData[i] = uint32_t(src[0]);
    infoData[i] = blGlyphInfoFromCluster(i);
    src = BLPtrOps::offset(src, advance);
  }

  d->size = size;
  d->flags = 0;
  return BL_SUCCESS;
}

static BL_INLINE BLResult blInternalGlyphBufferData_setLatin1Text(BLGlyphBufferPrivateImpl* d, const char* src, size_t size) noexcept {
  uint32_t* textData = d->content;
  BLGlyphInfo* infoData = d->infoData;

  for (size_t i = 0; i < size; i++) {
    textData[i] = uint8_t(src[i]);
    infoData[i] = blGlyphInfoFromCluster(i);
  }

  d->size = size;
  d->flags = 0;

  if (d->size)
    d->flags |= BL_GLYPH_RUN_FLAG_UCS4_CONTENT;

  return BL_SUCCESS;
}

template<typename Reader, typename CharType>
static BL_INLINE BLResult blInternalGlyphBufferData_setUnicodeText(BLGlyphBufferPrivateImpl* d, const CharType* src, size_t size) noexcept {
  Reader reader(src, size);

  uint32_t* textData = d->content;
  BLGlyphInfo* infoData = d->infoData;

  while (reader.hasNext()) {
    uint32_t uc;
    uint32_t cluster = uint32_t(reader.nativeIndex(src));
    BLResult result = reader.next(uc);

    *textData++ = uc;
    *infoData++ = blGlyphInfoFromCluster(cluster);

    if (BL_LIKELY(result == BL_SUCCESS))
      continue;

    textData[-1] = BL_CHAR_REPLACEMENT;
    d->flags |= BL_GLYPH_RUN_FLAG_INVALID_TEXT;
    reader.skipOneUnit();
  }

  d->size = (size_t)(textData - d->content);
  d->flags = 0;

  if (d->size)
    d->flags |= BL_GLYPH_RUN_FLAG_UCS4_CONTENT;

  return BL_SUCCESS;
}

// BLGlyphBuffer - Init & Destroy
// ==============================

BL_API_IMPL BLResult blGlyphBufferInit(BLGlyphBufferCore* self) noexcept {
  self->impl = const_cast<BLGlyphBufferPrivateImpl*>(&blGlyphBufferInternalImplNone);
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blGlyphBufferInitMove(BLGlyphBufferCore* self, BLGlyphBufferCore* other) noexcept {
  BLGlyphBufferPrivateImpl* impl = blGlyphBufferGetImpl(other);
  other->impl = const_cast<BLGlyphBufferPrivateImpl*>(&blGlyphBufferInternalImplNone);
  self->impl = impl;
  return BL_SUCCESS;
}

BL_API_IMPL BLResult blGlyphBufferDestroy(BLGlyphBufferCore* self) noexcept {
  BLGlyphBufferPrivateImpl* impl = blGlyphBufferGetImpl(self);
  self->impl = nullptr;

  if (impl != &blGlyphBufferInternalImplNone)
    impl->destroy();
  return BL_SUCCESS;
}

// BLGlyphBuffer - Reset
// =====================

BL_API_IMPL BLResult blGlyphBufferReset(BLGlyphBufferCore* self) noexcept {
  BLGlyphBufferPrivateImpl* impl = blGlyphBufferGetImpl(self);
  self->impl = const_cast<BLGlyphBufferPrivateImpl*>(&blGlyphBufferInternalImplNone);

  if (impl != &blGlyphBufferInternalImplNone)
    impl->destroy();
  return BL_SUCCESS;
}

// BLGlyphBuffer - Content
// =======================

BL_API_IMPL BLResult blGlyphBufferClear(BLGlyphBufferCore* self) noexcept {
  BLGlyphBufferPrivateImpl* selfI = blGlyphBufferGetImpl(self);

  // Would be true if the glyph-buffer is built-in 'none' instance or the data
  // is allocated, but empty.
  if (selfI->size == 0)
    return BL_SUCCESS;

  selfI->clear();
  return BL_SUCCESS;
}

BL_API_IMPL size_t blGlyphBufferGetSize(const BLGlyphBufferCore* self) noexcept {
  BLGlyphBufferPrivateImpl* selfI = blGlyphBufferGetImpl(self);
  return selfI->size;
}

BL_API_IMPL uint32_t blGlyphBufferGetFlags(const BLGlyphBufferCore* self) noexcept {
  BLGlyphBufferPrivateImpl* selfI = blGlyphBufferGetImpl(self);
  return selfI->flags;
}

BL_API_IMPL const BLGlyphRun* blGlyphBufferGetGlyphRun(const BLGlyphBufferCore* self) noexcept {
  BLGlyphBufferPrivateImpl* selfI = blGlyphBufferGetImpl(self);
  return &selfI->glyphRun;
}

BL_API_IMPL const uint32_t* blGlyphBufferGetContent(const BLGlyphBufferCore* self) noexcept {
  BLGlyphBufferPrivateImpl* selfI = blGlyphBufferGetImpl(self);
  return selfI->content;
}

BL_API_IMPL const BLGlyphInfo* blGlyphBufferGetInfoData(const BLGlyphBufferCore* self) noexcept {
  BLGlyphBufferPrivateImpl* selfI = blGlyphBufferGetImpl(self);
  return selfI->infoData;
}

BL_API_IMPL const BLGlyphPlacement* blGlyphBufferGetPlacementData(const BLGlyphBufferCore* self) noexcept {
  BLGlyphBufferPrivateImpl* selfI = blGlyphBufferGetImpl(self);
  return selfI->placementData;
}

BL_API_IMPL BLResult blGlyphBufferSetText(BLGlyphBufferCore* self, const void* textData, size_t size, BLTextEncoding encoding) noexcept {
  if (BL_UNLIKELY(uint32_t(encoding) > BL_TEXT_ENCODING_MAX_VALUE))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLGlyphBufferPrivateImpl* d;
  BL_PROPAGATE(blGlyphBufferEnsureData(self, &d));

  switch (encoding) {
    case BL_TEXT_ENCODING_LATIN1:
      if (size == SIZE_MAX)
        size = strlen(static_cast<const char*>(textData));

      BL_PROPAGATE(d->ensureBuffer(0, 0, size));
      return blInternalGlyphBufferData_setLatin1Text(d, static_cast<const char*>(textData), size);

    case BL_TEXT_ENCODING_UTF8:
      if (size == SIZE_MAX)
        size = strlen(static_cast<const char*>(textData));

      BL_PROPAGATE(d->ensureBuffer(0, 0, size));
      return blInternalGlyphBufferData_setUnicodeText<BLUtf8Reader>(d, static_cast<const uint8_t*>(textData), size);

    case BL_TEXT_ENCODING_UTF16:
      if (size == SIZE_MAX)
        size = blStrLen(static_cast<const uint16_t*>(textData));

      BL_PROPAGATE(d->ensureBuffer(0, 0, size));
      return blInternalGlyphBufferData_setUnicodeText<BLUtf16Reader>(d, static_cast<const uint16_t*>(textData), size * 2u);

    case BL_TEXT_ENCODING_UTF32:
      if (size == SIZE_MAX)
        size = blStrLen(static_cast<const uint32_t*>(textData));

      BL_PROPAGATE(d->ensureBuffer(0, 0, size));
      return blInternalGlyphBufferData_setUnicodeText<BLUtf32Reader>(d, static_cast<const uint32_t*>(textData), size * 4u);

    default:
      // Avoids a compile-time warning, should never be reached.
      return blTraceError(BL_ERROR_INVALID_VALUE);
  }
}

BL_API_IMPL BLResult blGlyphBufferSetGlyphs(BLGlyphBufferCore* self, const uint32_t* glyphData, size_t size) noexcept {
  if (BL_UNLIKELY(sizeof(size_t) > 4 && size > 0xFFFFFFFFu))
    return blTraceError(BL_ERROR_DATA_TOO_LARGE);

  BLGlyphBufferPrivateImpl* d;

  BL_PROPAGATE(blGlyphBufferEnsureData(self, &d));
  BL_PROPAGATE(d->ensureBuffer(0, 0, size));

  return blInternalGlyphBufferData_setGlyphIds(d, glyphData, size, intptr_t(sizeof(uint16_t)));
}

BL_API_IMPL BLResult blGlyphBufferSetGlyphsFromStruct(BLGlyphBufferCore* self, const void* glyphData, size_t size, size_t glyphIdSize, intptr_t glyphIdAdvance) noexcept {
  if (BL_UNLIKELY(glyphIdSize != 2 && glyphIdSize != 4))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  if (BL_UNLIKELY(sizeof(size_t) > 4 && size > 0xFFFFFFFFu))
    return blTraceError(BL_ERROR_DATA_TOO_LARGE);

  BLGlyphBufferPrivateImpl* d;

  BL_PROPAGATE(blGlyphBufferEnsureData(self, &d));
  BL_PROPAGATE(d->ensureBuffer(0, 0, size));

  if (glyphIdSize == 2)
    return blInternalGlyphBufferData_setGlyphIds(d, static_cast<const uint16_t*>(glyphData), size, glyphIdAdvance);
  else
    return blInternalGlyphBufferData_setGlyphIds(d, static_cast<const uint32_t*>(glyphData), size, glyphIdAdvance);
}
