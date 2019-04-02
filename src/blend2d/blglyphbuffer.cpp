// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blarray_p.h"
#include "./blfont_p.h"
#include "./blglyphbuffer_p.h"
#include "./blruntime_p.h"
#include "./blstring_p.h"
#include "./blsupport_p.h"
#include "./blunicode_p.h"

// ============================================================================
// [BLGlyphBuffer - Internals]
// ============================================================================

static const constexpr BLInternalGlyphBufferData blGlyphBufferInternalDataNone {};

template<typename T>
static BL_INLINE size_t strlenT(const T* str) noexcept {
  const T* p = str;
  while (*p)
    p++;
  return (size_t)(p - str);
}

static BL_INLINE BLResult blGlyphBufferEnsureData(BLGlyphBufferCore* self, BLInternalGlyphBufferData** d) noexcept {
  *d = blInternalCast(self->data);
  if (*d != &blGlyphBufferInternalDataNone)
    return BL_SUCCESS;

  *d = BLInternalGlyphBufferData::create();
  if (BL_UNLIKELY(!*d))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  self->data = *d;
  return BL_SUCCESS;
}

// ============================================================================
// [BLGlyphBuffer - Private API]
// ============================================================================

BLResult BLInternalGlyphBufferData::ensureBuffer(size_t bufferId, size_t copySize, size_t minCapacity) noexcept {
  size_t oldCapacity = capacity[bufferId];
  BL_ASSERT(copySize <= oldCapacity);

  if (minCapacity <= oldCapacity)
    return BL_SUCCESS;

  size_t newCapacity = minCapacity;
  if (newCapacity < BL_GLYPH_BUFFER_INITIAL_CAPACITY)
    newCapacity = BL_GLYPH_BUFFER_INITIAL_CAPACITY;
  else if (newCapacity < SIZE_MAX - 256)
    newCapacity = blAlignUp(minCapacity, 64);

  BLOverflowFlag of = 0;
  size_t dataSize = blMulOverflow<size_t>(newCapacity, BL_GLYPH_BUFFER_ANY_ITEM_SIZE, &of);

  if (BL_UNLIKELY(of))
    return BL_ERROR_OUT_OF_MEMORY;

  uint8_t* newData = static_cast<uint8_t*>(malloc(dataSize));
  if (BL_UNLIKELY(!newData))
    return BL_ERROR_OUT_OF_MEMORY;

  uint8_t* oldData = static_cast<uint8_t*>(buffer[bufferId]);
  if (copySize) {
    memcpy(newData,
           oldData,
           copySize * sizeof(BLGlyphItem));

    memcpy(newData + newCapacity * sizeof(BLGlyphItem),
           oldData + oldCapacity * sizeof(BLGlyphItem),
           copySize * sizeof(BLGlyphInfo));
  }

  if (oldData)
    free(oldData);

  buffer[bufferId] = newData;
  capacity[bufferId] = newCapacity;

  if (bufferId == 0)
    getGlyphDataPtrs(0, &glyphItemData, &glyphInfoData);

  return BL_SUCCESS;
}

static BL_INLINE BLResult blInternalGlyphBufferData_setGlyphIds(BLInternalGlyphBufferData* d, const uint16_t* ids, intptr_t advance, size_t size) noexcept {
  BLGlyphItem* itemData = d->glyphItemData;
  BLGlyphInfo* infoData = d->glyphInfoData;

  for (size_t i = 0; i < size; i++) {
    itemData[i].value = *ids;
    infoData[i].cluster = uint32_t(i);
    infoData[i].reserved[0] = 0;
    infoData[i].reserved[1] = 0;
    ids = blOffsetPtr(ids, advance);
  }

  d->size = size;
  d->flags = 0;

  return BL_SUCCESS;
}

static BL_INLINE BLResult blInternalGlyphBufferData_setLatin1Text(BLInternalGlyphBufferData* d, const char* input, size_t size) noexcept {
  BLGlyphItem* itemData = d->glyphItemData;
  BLGlyphInfo* infoData = d->glyphInfoData;

  for (size_t i = 0; i < size; i++) {
    itemData[i].value = uint8_t(input[i]);
    infoData[i].cluster = uint32_t(i);
    infoData[i].reserved[0] = 0;
    infoData[i].reserved[1] = 0;
  }

  d->size = size;
  d->flags = 0;

  if (d->size)
    d->flags |= BL_GLYPH_RUN_FLAG_UCS4_CONTENT;

  return BL_SUCCESS;
}

template<typename Reader, typename CharType>
static BL_INLINE BLResult blInternalGlyphBufferData_setUnicodeText(BLInternalGlyphBufferData* d, const CharType* input, size_t size) noexcept {
  Reader reader(input, size);

  BLGlyphItem* itemData = d->glyphItemData;
  BLGlyphInfo* infoData = d->glyphInfoData;

  while (reader.hasNext()) {
    uint32_t uc;
    uint32_t cluster = uint32_t(reader.nativeIndex(input));
    BLResult result = reader.next(uc);

    itemData->value = uc;
    infoData->cluster = cluster;
    infoData->reserved[0] = 0;
    infoData->reserved[1] = 0;

    itemData++;
    infoData++;

    if (BL_LIKELY(result == BL_SUCCESS))
      continue;

    itemData[-1].value = BL_CHAR_REPLACEMENT;
    d->flags |= BL_GLYPH_RUN_FLAG_INVALID_TEXT;
    reader.skipOneUnit();
  }

  d->size = (size_t)(itemData - d->glyphItemData);
  d->flags = BL_GLYPH_RUN_FLAG_UCS4_CONTENT;

  if (d->size)
    d->flags |= BL_GLYPH_RUN_FLAG_UCS4_CONTENT;

  return BL_SUCCESS;
}

// ============================================================================
// [BLGlyphBuffer - Init / Reset]
// ============================================================================

BLResult blGlyphBufferInit(BLGlyphBufferCore* self) noexcept {
  self->data = const_cast<BLInternalGlyphBufferData*>(&blGlyphBufferInternalDataNone);
  return BL_SUCCESS;
}

BLResult blGlyphBufferReset(BLGlyphBufferCore* self) noexcept {
  BLInternalGlyphBufferData* d = blInternalCast(self->data);
  if (d == &blGlyphBufferInternalDataNone)
    return BL_SUCCESS;

  d->destroy();
  self->data = const_cast<BLInternalGlyphBufferData*>(&blGlyphBufferInternalDataNone);
  return BL_SUCCESS;
}

// ============================================================================
// [BLGlyphBuffer - Content]
// ============================================================================

BLResult blGlyphBufferClear(BLGlyphBufferCore* self) noexcept {
  BLInternalGlyphBufferData* d = blInternalCast(self->data);

  // Would be true if the glyph-buffer is built-in 'none' instance or the data
  // is allocated, but empty.
  if (d->size == 0)
    return BL_SUCCESS;

  d->clear();
  return BL_SUCCESS;
}

BLResult blGlyphBufferSetText(BLGlyphBufferCore* self, const void* data, size_t size, uint32_t encoding) noexcept {
  if (BL_UNLIKELY(encoding >= BL_TEXT_ENCODING_COUNT))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLInternalGlyphBufferData* d;
  BL_PROPAGATE(blGlyphBufferEnsureData(self, &d));

  switch (encoding) {
    case BL_TEXT_ENCODING_LATIN1:
      if (size == SIZE_MAX)
        size = strlen(static_cast<const char*>(data));

      BL_PROPAGATE(d->ensureBuffer(0, 0, size));
      return blInternalGlyphBufferData_setLatin1Text(d, static_cast<const char*>(data), size);

    case BL_TEXT_ENCODING_UTF8:
      if (size == SIZE_MAX)
        size = strlen(static_cast<const char*>(data));

      BL_PROPAGATE(d->ensureBuffer(0, 0, size));
      return blInternalGlyphBufferData_setUnicodeText<BLUtf8Reader>(d, static_cast<const uint8_t*>(data), size);

    case BL_TEXT_ENCODING_UTF16:
      if (size == SIZE_MAX)
        size = strlenT(static_cast<const uint16_t*>(data));

      BL_PROPAGATE(d->ensureBuffer(0, 0, size));
      return blInternalGlyphBufferData_setUnicodeText<BLUtf16Reader>(d, static_cast<const uint16_t*>(data), size * 2u);

    case BL_TEXT_ENCODING_UTF32:
      if (size == SIZE_MAX)
        size = strlenT(static_cast<const uint32_t*>(data));

      BL_PROPAGATE(d->ensureBuffer(0, 0, size));
      return blInternalGlyphBufferData_setUnicodeText<BLUtf32Reader>(d, static_cast<const uint32_t*>(data), size * 4u);

    default:
      BL_NOT_REACHED();
  }
}

BLResult blGlyphBufferSetGlyphIds(BLGlyphBufferCore* self, const void* data, intptr_t advance, size_t size) noexcept {
  BLInternalGlyphBufferData* d;

  BL_PROPAGATE(blGlyphBufferEnsureData(self, &d));
  BL_PROPAGATE(d->ensureBuffer(0, 0, size));

  return blInternalGlyphBufferData_setGlyphIds(d, static_cast<const uint16_t*>(data), advance, size);
}
