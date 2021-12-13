// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "glyphbuffer_p.h"
#include "filesystem.h"
#include "font_p.h"
#include "matrix.h"
#include "object_p.h"
#include "path.h"
#include "runtime_p.h"
#include "string_p.h"
#include "unicode_p.h"
#include "opentype/otcore_p.h"
#include "opentype/otface_p.h"
#include "support/intops_p.h"
#include "support/ptrops_p.h"
#include "support/scopedbuffer_p.h"
#include "threading/uniqueidgenerator_p.h"

// BLFont - Globals
// ================

BLInternalFontFaceFuncs blNullFontFaceFuncs;

static BLObjectEthernalImpl<BLInternalFontImpl> blFontDefaultImpl;
static BLObjectEthernalVirtualImpl<BLInternalFontDataImpl, BLFontDataVirt> blFontDataDefaultImpl;
static BLObjectEthernalVirtualImpl<BLInternalFontFaceImpl, BLFontFaceVirt> blFontFaceDefaultImpl;

// BLFont - Internal Utilities
// ===========================

static BL_INLINE bool blFontIsOpenTypeVersionTag(uint32_t tag) noexcept {
  return tag == BL_MAKE_TAG('O', 'T', 'T', 'O') ||
         tag == BL_MAKE_TAG( 0 ,  1 ,  0 ,  0 ) ||
         tag == BL_MAKE_TAG('t', 'r', 'u', 'e') ;
}

static void blFontCalcProperties(BLInternalFontImpl* fontI, const BLInternalFontFaceImpl* faceI, float size) noexcept {
  const BLFontDesignMetrics& dm = faceI->designMetrics;

  double yScale = dm.unitsPerEm ? double(size) / double(dm.unitsPerEm) : 0.0;
  double xScale = yScale;

  fontI->metrics.size                   = size;
  fontI->metrics.ascent                 = float(double(dm.ascent                ) * yScale);
  fontI->metrics.descent                = float(double(dm.descent               ) * yScale);
  fontI->metrics.lineGap                = float(double(dm.lineGap               ) * yScale);
  fontI->metrics.xHeight                = float(double(dm.xHeight               ) * yScale);
  fontI->metrics.capHeight              = float(double(dm.capHeight             ) * yScale);
  fontI->metrics.vAscent                = float(double(dm.vAscent               ) * yScale);
  fontI->metrics.vDescent               = float(double(dm.vDescent              ) * yScale);
  fontI->metrics.xMin                   = float(double(dm.glyphBoundingBox.x0   ) * xScale);
  fontI->metrics.yMin                   = float(double(dm.glyphBoundingBox.y0   ) * yScale);
  fontI->metrics.xMax                   = float(double(dm.glyphBoundingBox.x1   ) * xScale);
  fontI->metrics.yMax                   = float(double(dm.glyphBoundingBox.y1   ) * yScale);
  fontI->metrics.underlinePosition      = float(double(dm.underlinePosition     ) * yScale);
  fontI->metrics.underlineThickness     = float(double(dm.underlineThickness    ) * yScale);
  fontI->metrics.strikethroughPosition  = float(double(dm.strikethroughPosition ) * yScale);
  fontI->metrics.strikethroughThickness = float(double(dm.strikethroughThickness) * yScale);
  fontI->matrix.reset(xScale, 0.0, 0.0, -yScale);
}

// BLFontData - Default Impl
// =========================

static BLResult BL_CDECL blNullFontDataImplDestroy(BLObjectImpl* impl, uint32_t info) noexcept {
  blUnused(impl, info);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blNullFontDataImplListTags(const BLFontDataImpl* impl, uint32_t faceIndex, BLArrayCore* out) noexcept {
  blUnused(impl, faceIndex);
  return blArrayClear(out);
}

static size_t BL_CDECL blNullFontDataImplQueryTables(const BLFontDataImpl* impl, uint32_t faceIndex, BLFontTable* dst, const BLTag* tags, size_t n) noexcept {
  blUnused(impl, faceIndex, tags);
  for (size_t i = 0; i < n; i++)
    dst[i].reset();
  return 0;
}

// BLFontData - Memory Impl
// ========================

static BLFontDataVirt blMemFontDataVirt;

struct BLMemFontDataImpl : public BLInternalFontDataImpl {
  //! Pointer to the start of font data.
  void* data;
  //! Size of `data` [in bytes].
  uint32_t dataSize;
  //! Offset to an array that contains offsets for each font-face.
  uint32_t offsetArrayIndex;

  //! If the `data` is not external it's held by this array.
  BLArray<uint8_t> dataArray;
};

// Destroys `BLMemFontDataImpl` - this is a real destructor.
static BLResult blMemFontDataImplRealDestroy(BLMemFontDataImpl* impl, uint32_t infoBits) noexcept {
  BLObjectInfo info{infoBits};

  if (info.xFlag())
    blObjectDetailCallExternalDestroyFunc(impl, info, BLObjectImplSize(sizeof(BLMemFontDataImpl)), impl->data);

  blCallDtor(impl->faceCache);
  blCallDtor(impl->dataArray);
  return blObjectImplFreeInline(impl, info);
}

static BLResult BL_CDECL blMemFontDataImplDestroy(BLObjectImpl* impl, uint32_t info) noexcept {
  return blMemFontDataImplRealDestroy(static_cast<BLMemFontDataImpl*>(impl), info);
}

static BLResult BL_CDECL blMemFontDataImplListTags(const BLFontDataImpl* impl_, uint32_t faceIndex, BLArrayCore* out) noexcept {
  using namespace BLOpenType;

  const BLMemFontDataImpl* impl = static_cast<const BLMemFontDataImpl*>(impl_);
  const void* fontData = impl->data;
  size_t dataSize = impl->dataSize;

  if (BL_UNLIKELY(faceIndex >= impl->faceCount)) {
    blArrayClear(out);
    return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  uint32_t headerOffset = 0;
  if (impl->offsetArrayIndex)
    headerOffset = BLPtrOps::offset<UInt32>(fontData, impl->offsetArrayIndex)[faceIndex].value();

  if (BL_LIKELY(headerOffset <= dataSize - sizeof(SFNTHeader))) {
    const SFNTHeader* sfnt = BLPtrOps::offset<SFNTHeader>(fontData, headerOffset);
    if (blFontIsOpenTypeVersionTag(sfnt->versionTag())) {
      // We can safely multiply `tableCount` as SFNTHeader::numTables is `UInt16`.
      uint32_t tableCount = sfnt->numTables();
      uint32_t minDataSize = uint32_t(sizeof(SFNTHeader)) + tableCount * uint32_t(sizeof(SFNTHeader::TableRecord));

      if (BL_LIKELY(dataSize - headerOffset >= minDataSize)) {
        uint32_t* dst;
        BL_PROPAGATE(blArrayModifyOp(out, BL_MODIFY_OP_ASSIGN_FIT, tableCount, (void**)&dst));

        const SFNTHeader::TableRecord* tables = sfnt->tableRecords();
        for (uint32_t tableIndex = 0; tableIndex < tableCount; tableIndex++)
          dst[tableIndex] = tables[tableIndex].tag();
        return BL_SUCCESS;
      }
    }
  }

  blArrayClear(out);
  return blTraceError(BL_ERROR_INVALID_DATA);
}

static size_t BL_CDECL blMemFontDataImplQueryTables(const BLFontDataImpl* impl_, uint32_t faceIndex, BLFontTable* dst, const BLTag* tags, size_t n) noexcept {
  using namespace BLOpenType;

  const BLMemFontDataImpl* impl = static_cast<const BLMemFontDataImpl*>(impl_);
  const void* fontData = impl->data;
  size_t dataSize = impl->dataSize;

  if (BL_LIKELY(faceIndex < impl->faceCount)) {
    uint32_t headerOffset = 0;
    if (impl->offsetArrayIndex)
      headerOffset = BLPtrOps::offset<UInt32>(fontData, impl->offsetArrayIndex)[faceIndex].value();

    if (BL_LIKELY(headerOffset <= dataSize - sizeof(SFNTHeader))) {
      const SFNTHeader* sfnt = BLPtrOps::offset<SFNTHeader>(fontData, headerOffset);
      if (blFontIsOpenTypeVersionTag(sfnt->versionTag())) {
        uint32_t tableCount = sfnt->numTables();

        // We can safely multiply `tableCount` as SFNTHeader::numTables is `UInt16`.
        uint32_t minDataSize = uint32_t(sizeof(SFNTHeader)) + tableCount * uint32_t(sizeof(SFNTHeader::TableRecord));
        if (BL_LIKELY(dataSize - headerOffset >= minDataSize)) {
          const SFNTHeader::TableRecord* tables = sfnt->tableRecords();
          size_t matchCount = 0;

          // Iterate over all tables and try to find all tables as specified by `tags`.
          for (size_t tagIndex = 0; tagIndex < n; tagIndex++) {
            uint32_t tag = BLIntOps::byteSwap32BE(tags[tagIndex]);
            dst[tagIndex].reset();

            for (uint32_t tableIndex = 0; tableIndex < tableCount; tableIndex++) {
              const SFNTHeader::TableRecord& table = tables[tableIndex];

              if (table.tag.rawValue() == tag) {
                uint32_t tableOffset = table.offset();
                uint32_t tableSize = table.length();

                if (tableOffset < dataSize && tableSize && tableSize <= dataSize - tableOffset) {
                  matchCount++;
                  dst[tagIndex].reset(BLPtrOps::offset<uint8_t>(fontData, tableOffset), tableSize);
                }

                break;
              }
            }
          }

          return matchCount;
        }
      }
    }
  }

  memset(dst, 0, n * sizeof(BLFontTable));
  return 0;
}

// BLFontData - Init & Destroy
// ===========================

BLResult blFontDataInit(BLFontDataCore* self) noexcept {
  self->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_DATA]._d;
  return BL_SUCCESS;
}

BLResult blFontDataInitMove(BLFontDataCore* self, BLFontDataCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFontData());

  self->_d = other->_d;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_DATA]._d;

  return BL_SUCCESS;
}

BLResult blFontDataInitWeak(BLFontDataCore* self, const BLFontDataCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFontData());

  return blObjectPrivateInitWeakTagged(self, other);
}

BLResult blFontDataDestroy(BLFontDataCore* self) noexcept {
  BL_ASSERT(self->_d.isFontData());

  return blObjectPrivateReleaseVirtual(self);
}

// BLFontData - Reset
// ==================

BLResult blFontDataReset(BLFontDataCore* self) noexcept {
  BL_ASSERT(self->_d.isFontData());

  return blObjectPrivateReplaceVirtual(self, static_cast<BLFontDataCore*>(&blObjectDefaults[BL_OBJECT_TYPE_FONT_DATA]));
}

// BLFontData - Assign
// ===================

BLResult blFontDataAssignMove(BLFontDataCore* self, BLFontDataCore* other) noexcept {
  BL_ASSERT(self->_d.isFontData());
  BL_ASSERT(other->_d.isFontData());

  BLFontDataCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_DATA]._d;
  return blObjectPrivateReplaceVirtual(self, &tmp);
}

BLResult blFontDataAssignWeak(BLFontDataCore* self, const BLFontDataCore* other) noexcept {
  BL_ASSERT(self->_d.isFontData());
  BL_ASSERT(other->_d.isFontData());

  return blObjectPrivateAssignWeakVirtual(self, other);
}

// BLFontData - Equality & Comparison
// ==================================

bool blFontDataEquals(const BLFontDataCore* a, const BLFontDataCore* b) noexcept {
  BL_ASSERT(a->_d.isFontData());
  BL_ASSERT(b->_d.isFontData());

  return a->_d.impl == b->_d.impl;
}

// BLFontData - Create
// ===================

BLResult blFontDataCreateFromFile(BLFontDataCore* self, const char* fileName, BLFileReadFlags readFlags) noexcept {
  BL_ASSERT(self->_d.isFontData());

  BLArray<uint8_t> buffer;
  BL_PROPAGATE(BLFileSystem::readFile(fileName, buffer, 0, readFlags));

  if (buffer.empty())
    return blTraceError(BL_ERROR_FILE_EMPTY);

  return blFontDataCreateFromDataArray(self, &buffer);
}

static BLResult blFontDataCreateFromDataInternal(BLFontDataCore* self, const void* data, size_t dataSize, BLDestroyExternalDataFunc destroyFunc, void* userData, const BLArray<uint8_t>* array) noexcept {
  using namespace BLOpenType;

  constexpr uint32_t kMinSize = blMin<uint32_t>(SFNTHeader::kMinSize, TTCFHeader::kMinSize);
  if (BL_UNLIKELY(dataSize < kMinSize))
    return blTraceError(BL_ERROR_INVALID_DATA);

  if (BL_UNLIKELY(sizeof(size_t) > 4 && dataSize > 0xFFFFFFFFu))
    return blTraceError(BL_ERROR_DATA_TOO_LARGE);

  uint32_t headerTag = BLPtrOps::offset<const UInt32>(data, 0)->value();
  uint32_t faceCount = 1;
  uint32_t dataFlags = 0;

  uint32_t offsetArrayIndex = 0;
  const UInt32* offsetArray = nullptr;

  if (headerTag == BL_MAKE_TAG('t', 't', 'c', 'f')) {
    if (BL_UNLIKELY(dataSize < TTCFHeader::kMinSize))
      return blTraceError(BL_ERROR_INVALID_DATA);

    const TTCFHeader* header = BLPtrOps::offset<const TTCFHeader>(data, 0);

    faceCount = header->fonts.count();
    if (BL_UNLIKELY(!faceCount || faceCount > BL_FONT_DATA_MAX_FACE_COUNT))
      return blTraceError(BL_ERROR_INVALID_DATA);

    size_t ttcHeaderSize = header->calcSize(faceCount);
    if (BL_UNLIKELY(ttcHeaderSize > dataSize))
      return blTraceError(BL_ERROR_INVALID_DATA);

    offsetArray = header->fonts.array();
    offsetArrayIndex = (uint32_t)((uintptr_t)offsetArray - (uintptr_t)header);

    dataFlags |= BL_FONT_DATA_FLAG_COLLECTION;
  }
  else {
    if (!blFontIsOpenTypeVersionTag(headerTag))
      return blTraceError(BL_ERROR_INVALID_SIGNATURE);
  }

  BLArray<BLFontFaceImpl*> faceCache;
  BL_PROPAGATE(faceCache.resize(faceCount, nullptr));

  BLFontDataCore newO;
  BLMemFontDataImpl* newI;

  void* externalOptData = nullptr;
  BLObjectExternalInfo* externalInfo = nullptr;

  BLObjectImplSize implSize(sizeof(BLMemFontDataImpl));
  BLObjectInfo implInfo = BLObjectInfo::packType(BL_OBJECT_TYPE_FONT_DATA);

  if (!destroyFunc)
    newI = blObjectDetailAllocImplT<BLMemFontDataImpl>(&newO, implInfo, implSize);
  else
    newI = blObjectDetailAllocImplExternalT<BLMemFontDataImpl>(&newO, implInfo, implSize, &externalInfo, &externalOptData);

  if (BL_UNLIKELY(!newI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  blFontDataImplCtor(newI, &blMemFontDataVirt);
  newI->faceType = uint8_t(BL_FONT_FACE_TYPE_OPENTYPE);
  newI->faceCount = faceCount;
  newI->flags = dataFlags;
  newI->backRefCount = 0;

  blCallCtor(newI->faceCache, std::move(faceCache));
  blCallCtor(newI->dataArray);

  if (array) {
    newI->dataArray = *array;
    data = newI->dataArray.data();
  }

  newI->data = const_cast<void*>(data);
  newI->dataSize = uint32_t(dataSize);
  newI->offsetArrayIndex = offsetArrayIndex;

  if (externalInfo) {
    externalInfo->destroyFunc = destroyFunc ? destroyFunc : blObjectDestroyExternalDataDummy;
    externalInfo->userData = userData;
  }

  return blObjectPrivateReplaceVirtual(self, &newO);
}

BLResult blFontDataCreateFromDataArray(BLFontDataCore* self, const BLArrayCore* dataArray) noexcept {
  BL_ASSERT(self->_d.isFontData());

  if (dataArray->_d.rawType() != BL_OBJECT_TYPE_ARRAY_UINT8)
    return blTraceError(BL_ERROR_INVALID_VALUE);

  const BLArray<uint8_t>& array = dataArray->dcast<BLArray<uint8_t>>();
  const void* data = array.data();
  size_t dataSize = array.size();

  return blFontDataCreateFromDataInternal(self, data, dataSize, nullptr, nullptr, &array);
}

BLResult blFontDataCreateFromData(BLFontDataCore* self, const void* data, size_t dataSize, BLDestroyExternalDataFunc destroyFunc, void* userData) noexcept {
  return blFontDataCreateFromDataInternal(self, data, dataSize, destroyFunc, userData, nullptr);
};

// BLFontData - Query
// ==================

BLResult blFontDataListTags(const BLFontDataCore* self, uint32_t faceIndex, BLArrayCore* dst) noexcept {
  BL_ASSERT(self->_d.isFontData());

  BLInternalFontDataImpl* selfI = blFontDataGetImpl(self);
  return selfI->virt->listTags(selfI, faceIndex, dst);
}

size_t blFontDataQueryTables(const BLFontDataCore* self, uint32_t faceIndex, BLFontTable* dst, const BLTag* tags, size_t count) noexcept {
  BL_ASSERT(self->_d.isFontData());

  BLInternalFontDataImpl* selfI = blFontDataGetImpl(self);
  return selfI->virt->queryTables(selfI, faceIndex, dst, tags, count);
}

// BLFontFace - Default Impl
// =========================

BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)

static BLResult BL_CDECL blNullFontFaceImplDestroy(BLObjectImpl* impl, uint32_t info) noexcept {
  return BL_SUCCESS;
}

static BLResult BL_CDECL blNullFontFaceMapTextToGlyphs(
  const BLFontFaceImpl* impl,
  uint32_t* content,
  size_t count,
  BLGlyphMappingState* state) noexcept {

  state->reset();
  return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceGetGlyphBounds(
  const BLFontFaceImpl* impl,
  const uint32_t* glyphData,
  intptr_t glyphAdvance,
  BLBoxI* boxes,
  size_t count) noexcept {

  return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceGetGlyphAdvances(
  const BLFontFaceImpl* impl,
  const uint32_t* glyphData,
  intptr_t glyphAdvance,
  BLGlyphPlacement* placementData,
  size_t count) noexcept {

  return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceGetGlyphOutlines(
  const BLFontFaceImpl* impl,
  uint32_t glyphId,
  const BLMatrix2D* userMatrix,
  BLPath* out,
  size_t* contourCountOut,
  BLScopedBuffer* tmpBuffer) noexcept {

  *contourCountOut = 0;
  return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceApplyKern(
  const BLFontFaceImpl* faceI,
  uint32_t* glyphData,
  BLGlyphPlacement* placementData,
  size_t count) noexcept {

  return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceApplyGSub(
  const BLFontFaceImpl* impl,
  BLGlyphBuffer* gb,
  const BLBitSetCore* lookups) noexcept {

  return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceApplyGPos(
  const BLFontFaceImpl* impl,
  BLGlyphBuffer* gb,
  const BLBitSetCore* lookups) noexcept {

  return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFacePositionGlyphs(
  const BLFontFaceImpl* impl,
  uint32_t* glyphData,
  BLGlyphPlacement* placementData,
  size_t count) noexcept {

  return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);
}

BL_DIAGNOSTIC_POP

// BLFontFace - Init & Destroy
// ===========================

BLResult blFontFaceInit(BLFontFaceCore* self) noexcept {
  self->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_FACE]._d;
  return BL_SUCCESS;
}

BLResult blFontFaceInitMove(BLFontFaceCore* self, BLFontFaceCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFontFace());

  self->_d = other->_d;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_FACE]._d;

  return BL_SUCCESS;
}

BLResult blFontFaceInitWeak(BLFontFaceCore* self, const BLFontFaceCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFontFace());

  return blObjectPrivateInitWeakTagged(self, other);
}

BLResult blFontFaceDestroy(BLFontFaceCore* self) noexcept {
  BL_ASSERT(self->_d.isFontFace());

  return blObjectPrivateReleaseVirtual(self);
}

// BLFontFace - Reset
// ==================

BLResult blFontFaceReset(BLFontFaceCore* self) noexcept {
  BL_ASSERT(self->_d.isFontFace());

  return blObjectPrivateReplaceVirtual(self, static_cast<BLFontFaceCore*>(&blObjectDefaults[BL_OBJECT_TYPE_FONT_FACE]));
}

// BLFontFace - Assign
// ===================

BLResult blFontFaceAssignMove(BLFontFaceCore* self, BLFontFaceCore* other) noexcept {
  BL_ASSERT(self->_d.isFontFace());
  BL_ASSERT(other->_d.isFontFace());

  BLFontFaceCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_FACE]._d;
  return blObjectPrivateReplaceVirtual(self, &tmp);
}

BLResult blFontFaceAssignWeak(BLFontFaceCore* self, const BLFontFaceCore* other) noexcept {
  BL_ASSERT(self->_d.isFontFace());
  BL_ASSERT(other->_d.isFontFace());

  return blObjectPrivateAssignWeakVirtual(self, other);
}

// BLFontFace - Equality & Comparison
// ==================================

bool blFontFaceEquals(const BLFontFaceCore* a, const BLFontFaceCore* b) noexcept {
  BL_ASSERT(a->_d.isFontFace());
  BL_ASSERT(b->_d.isFontFace());

  return a->_d.impl == b->_d.impl;
}

// BLFontFace - Create
// ===================

BLResult blFontFaceCreateFromFile(BLFontFaceCore* self, const char* fileName, BLFileReadFlags readFlags) noexcept {
  BL_ASSERT(self->_d.isFontFace());

  BLFontData fontData;
  BL_PROPAGATE(fontData.createFromFile(fileName, readFlags));
  return blFontFaceCreateFromData(self, &fontData, 0);
}

BLResult blFontFaceCreateFromData(BLFontFaceCore* self, const BLFontDataCore* fontData, uint32_t faceIndex) noexcept {
  BL_ASSERT(self->_d.isFontFace());
  BL_ASSERT(fontData->_d.isFontData());

  if (BL_UNLIKELY(!blDownCast(fontData)->isValid()))
    return blTraceError(BL_ERROR_NOT_INITIALIZED);

  if (faceIndex >= blDownCast(fontData)->faceCount())
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLFontFaceCore newO;
  BL_PROPAGATE(BLOpenType::createOpenTypeFace(&newO, blDownCast(fontData), faceIndex));

  // TODO: Move to OTFace?
  blFontFaceGetImpl<BLOpenType::OTFaceImpl>(&newO)->uniqueId = BLUniqueIdGenerator::generateId(BLUniqueIdGenerator::Domain::kAny);

  return blObjectPrivateReplaceVirtual(self, &newO);
}

// BLFontFace - Accessors
// ======================

BLResult blFontFaceGetFaceInfo(const BLFontFaceCore* self, BLFontFaceInfo* out) noexcept {
  BL_ASSERT(self->_d.isFontFace());

  *out = blDownCast(self)->faceInfo();
  return BL_SUCCESS;
}

BLResult blFontFaceGetDesignMetrics(const BLFontFaceCore* self, BLFontDesignMetrics* out) noexcept {
  BL_ASSERT(self->_d.isFontFace());

  *out = blDownCast(self)->designMetrics();
  return BL_SUCCESS;
}

BLResult blFontFaceGetUnicodeCoverage(const BLFontFaceCore* self, BLFontUnicodeCoverage* out) noexcept {
  BL_ASSERT(self->_d.isFontFace());

  *out = blDownCast(self)->unicodeCoverage();
  return BL_SUCCESS;
}

BLResult blFontFaceGetCharacterCoverage(const BLFontFaceCore* self, BLBitSetCore* out) noexcept {
  BL_ASSERT(self->_d.isFontFace());

  // Don't calculate the `characterCoverage` again if it was already calculated. We don't need atomics here as it
  // is set only once, atomics will be used only if it hasn't been calculated yet or if there is a race (already
  // calculated by another thread, but nullptr at this exact moment here).
  BLInternalFontFaceImpl* selfI = blFontFaceGetImpl(self);
  if (!blObjectAtomicContentTest(&selfI->characterCoverage)) {
    if (selfI->faceInfo.faceType != BL_FONT_FACE_TYPE_OPENTYPE)
      return blTraceError(BL_ERROR_NOT_IMPLEMENTED);

    BLBitSetCore tmpBitSet;
    BL_PROPAGATE(BLOpenType::CMapImpl::populateCharacterCoverage(static_cast<BLOpenType::OTFaceImpl*>(selfI), blDownCast(&tmpBitSet)));

    blBitSetShrink(&tmpBitSet);
    if (!blObjectAtomicContentMove(&selfI->characterCoverage, &tmpBitSet))
      return blBitSetAssignMove(out, &tmpBitSet);
  }

  return blBitSetAssignWeak(out, &selfI->characterCoverage);
}

// BLFont - Alloc & Free Impl
// ==========================

static BL_INLINE BLInternalFontImpl* blFontPrivateAllocImpl(BLFontCore* self, const BLFontFaceCore* face, float size) noexcept {
  BLObjectImplSize implSize(sizeof(BLInternalFontImpl));
  BLInternalFontImpl* impl = blObjectDetailAllocImplT<BLInternalFontImpl>(self, BLObjectInfo::packType(BL_OBJECT_TYPE_FONT), implSize);

  if (BL_UNLIKELY(!impl))
    return impl;

  blCallCtor(impl->face, *blDownCast(face));
  blCallCtor(impl->features);
  blCallCtor(impl->variations);
  impl->weight = 0;
  impl->stretch = 0;
  impl->style = 0;
  blFontCalcProperties(impl, blFontFaceGetImpl(face), size);

  return impl;
}

BLResult blFontImplFree(BLInternalFontImpl* impl, BLObjectInfo info) noexcept {
  blCallDtor(impl->face);
  blCallDtor(impl->features);
  blCallDtor(impl->variations);

  return blObjectImplFreeInline(impl, info);
}

static BL_INLINE BLResult blFontPrivateRelease(BLFontCore* self) noexcept {
  BLInternalFontImpl* impl = blFontGetImpl(self);
  BLObjectInfo info = self->_d.info;

  if (info.refCountedFlag() && blObjectImplDecRefAndTest(impl, info))
    return blFontImplFree(impl, info);

  return BL_SUCCESS;
}

static BL_INLINE BLResult blFontPrivateReplace(BLFontCore* self, const BLFontCore* other) noexcept {
  BLInternalFontImpl* impl = blFontGetImpl(self);
  BLObjectInfo info = self->_d.info;

  self->_d = other->_d;

  if (info.refCountedFlag() && blObjectImplDecRefAndTest(impl, info))
    return blFontImplFree(impl, info);

  return BL_SUCCESS;
}

// BLFont - Init & Destroy
// =======================

BLResult blFontInit(BLFontCore* self) noexcept {
  self->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT]._d;
  return BL_SUCCESS;
}

BLResult blFontInitMove(BLFontCore* self, BLFontCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFont());

  self->_d = other->_d;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT]._d;

  return BL_SUCCESS;
}

BLResult blFontInitWeak(BLFontCore* self, const BLFontCore* other) noexcept {
  BL_ASSERT(self != other);
  BL_ASSERT(other->_d.isFont());

  return blObjectPrivateInitWeakTagged(self, other);
}

BLResult blFontDestroy(BLFontCore* self) noexcept {
  BL_ASSERT(self->_d.isFont());

  return blFontPrivateRelease(self);
}

// BLFont - Reset
// ==============

BLResult blFontReset(BLFontCore* self) noexcept {
  BL_ASSERT(self->_d.isFont());

  return blFontPrivateReplace(self, static_cast<BLFontCore*>(&blObjectDefaults[BL_OBJECT_TYPE_FONT]));
}

// BLFont - Assign
// ===============

BLResult blFontAssignMove(BLFontCore* self, BLFontCore* other) noexcept {
  BL_ASSERT(self->_d.isFont());
  BL_ASSERT(other->_d.isFont());

  BLFontCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT]._d;
  return blFontPrivateReplace(self, &tmp);
}

BLResult blFontAssignWeak(BLFontCore* self, const BLFontCore* other) noexcept {
  BL_ASSERT(self->_d.isFont());
  BL_ASSERT(other->_d.isFont());

  blObjectPrivateAddRefTagged(other);
  return blFontPrivateReplace(self, other);
}

// BLFont - Equality & Comparison
// ==============================

bool blFontEquals(const BLFontCore* a, const BLFontCore* b) noexcept {
  return a->_d.impl == b->_d.impl;
}

// BLFont - Create
// ===============

BLResult blFontCreateFromFace(BLFontCore* self, const BLFontFaceCore* face, float size) noexcept {
  if (!blDownCast(face)->isValid())
    return blTraceError(BL_ERROR_FONT_NOT_INITIALIZED);

  if (blFontPrivateIsMutable(self)) {
    BLInternalFontImpl* selfI = blFontGetImpl(self);
    BLInternalFontFaceImpl* faceI = blFontFaceGetImpl(face);

    selfI->features.clear();
    selfI->variations.clear();
    selfI->weight = 0;
    selfI->stretch = 0;
    selfI->style = 0;
    blFontCalcProperties(selfI, faceI, size);

    return blObjectPrivateAssignWeakVirtual(&selfI->face, blDownCast(face));
  }
  else {
    BLFontCore newO;
    BLInternalFontImpl* newI = blFontPrivateAllocImpl(&newO, face, size);

    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    return blFontPrivateReplace(self, &newO);
  }
}

// BLFont - Accessors
// ==================

BLResult blFontGetMatrix(const BLFontCore* self, BLFontMatrix* out) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLInternalFontImpl* selfI = blFontGetImpl(self);
  *out = selfI->matrix;
  return BL_SUCCESS;
}

BLResult blFontGetMetrics(const BLFontCore* self, BLFontMetrics* out) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLInternalFontImpl* selfI = blFontGetImpl(self);
  *out = selfI->metrics;
  return BL_SUCCESS;
}

BLResult blFontGetDesignMetrics(const BLFontCore* self, BLFontDesignMetrics* out) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLInternalFontImpl* selfI = blFontGetImpl(self);
  BLInternalFontFaceImpl* faceI = blFontFaceGetImpl(&selfI->face);

  *out = faceI->designMetrics;
  return BL_SUCCESS;
}

// BLFont - Shaping
// ================

BLResult blFontShape(const BLFontCore* self, BLGlyphBufferCore* gb) noexcept {
  BL_ASSERT(self->_d.isFont());

  BL_PROPAGATE(blFontMapTextToGlyphs(self, gb, nullptr));
  BL_PROPAGATE(blFontPositionGlyphs(self, gb, 0xFFFFFFFFu));

  return BL_SUCCESS;
}

BLResult blFontMapTextToGlyphs(const BLFontCore* self, BLGlyphBufferCore* gb, BLGlyphMappingState* stateOut) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLInternalFontImpl* selfI = blFontGetImpl(self);
  BLInternalFontFaceImpl* faceI = blFontFaceGetImpl(&selfI->face);
  BLInternalGlyphBufferImpl* gbI = blGlyphBufferGetImpl(gb);

  if (!gbI->size)
    return BL_SUCCESS;

  if (BL_UNLIKELY(!(gbI->flags & BL_GLYPH_RUN_FLAG_UCS4_CONTENT)))
    return blTraceError(BL_ERROR_INVALID_STATE);

  BLGlyphMappingState state;
  if (!stateOut)
    stateOut = &state;

  BL_PROPAGATE(faceI->funcs.mapTextToGlyphs(faceI, gbI->content, gbI->size, stateOut));

  gbI->flags = gbI->flags & ~BL_GLYPH_RUN_FLAG_UCS4_CONTENT;
  if (stateOut->undefinedCount > 0)
    gbI->flags |= BL_GLYPH_RUN_FLAG_UNDEFINED_GLYPHS;

  return BL_SUCCESS;
}

BLResult blFontPositionGlyphs(const BLFontCore* self, BLGlyphBufferCore* gb, uint32_t positioningFlags) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLInternalFontImpl* selfI = blFontGetImpl(self);
  BLInternalFontFaceImpl* faceI = blFontFaceGetImpl(&selfI->face);
  BLInternalGlyphBufferImpl* gbI = blGlyphBufferGetImpl(gb);

  if (!gbI->size)
    return BL_SUCCESS;

  if (BL_UNLIKELY(gbI->flags & BL_GLYPH_RUN_FLAG_UCS4_CONTENT))
    return blTraceError(BL_ERROR_INVALID_STATE);

  if (!(gbI->flags & BL_GLYPH_BUFFER_GLYPH_ADVANCES)) {
    BL_PROPAGATE(gbI->ensurePlacement());
    faceI->funcs.getGlyphAdvances(faceI, gbI->content, sizeof(uint32_t), gbI->placementData, gbI->size);
    gbI->glyphRun.placementType = uint8_t(BL_GLYPH_PLACEMENT_TYPE_ADVANCE_OFFSET);
    gbI->flags |= BL_GLYPH_BUFFER_GLYPH_ADVANCES;
  }

  if (positioningFlags) {
    faceI->funcs.applyKern(faceI, gbI->content, gbI->placementData, gbI->size);
  }

  return BL_SUCCESS;
}

BLResult blFontApplyKerning(const BLFontCore* self, BLGlyphBufferCore* gb) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLInternalFontImpl* selfI = blFontGetImpl(self);
  BLInternalFontFaceImpl* faceI = blFontFaceGetImpl(&selfI->face);
  BLInternalGlyphBufferImpl* gbI = blGlyphBufferGetImpl(gb);

  if (!gbI->size)
    return BL_SUCCESS;

  if (BL_UNLIKELY(!(gbI->placementData)))
    return blTraceError(BL_ERROR_INVALID_STATE);

  return faceI->funcs.applyKern(faceI, gbI->content, gbI->placementData, gbI->size);
}

BLResult blFontApplyGSub(const BLFontCore* self, BLGlyphBufferCore* gb, const BLBitSetCore* lookups) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLInternalFontImpl* selfI = blFontGetImpl(self);
  BLInternalFontFaceImpl* faceI = blFontFaceGetImpl(&selfI->face);

  return faceI->funcs.applyGSub(faceI, static_cast<BLGlyphBuffer*>(gb), lookups);
}

BLResult blFontApplyGPos(const BLFontCore* self, BLGlyphBufferCore* gb, const BLBitSetCore* lookups) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLInternalFontImpl* selfI = blFontGetImpl(self);
  BLInternalFontFaceImpl* faceI = blFontFaceGetImpl(&selfI->face);
  BLInternalGlyphBufferImpl* gbI = blGlyphBufferGetImpl(gb);

  if (!gbI->size)
    return BL_SUCCESS;

  if (BL_UNLIKELY(!(gbI->placementData)))
    return blTraceError(BL_ERROR_INVALID_STATE);

  return faceI->funcs.applyGPos(faceI, static_cast<BLGlyphBuffer*>(gb), lookups);
}

BLResult blFontGetTextMetrics(const BLFontCore* self, BLGlyphBufferCore* gb, BLTextMetrics* out) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLInternalFontImpl* selfI = blFontGetImpl(self);
  BLInternalGlyphBufferImpl* gbI = blGlyphBufferGetImpl(gb);

  out->reset();
  if (!(gbI->flags & BL_GLYPH_BUFFER_GLYPH_ADVANCES)) {
    BL_PROPAGATE(blFontShape(self, gb));
    gbI = blGlyphBufferGetImpl(gb);
  }

  size_t size = gbI->size;
  if (!size)
    return BL_SUCCESS;

  BLPoint advance {};

  const uint32_t* glyphData = gbI->content;
  const BLGlyphPlacement* placementData = gbI->placementData;

  for (size_t i = 0; i < size; i++) {
    advance += BLPoint(placementData[i].advance);
  }

  BLBoxI glyphBounds[2];
  uint32_t borderGlyphs[2] = { glyphData[0], glyphData[size - 1] };

  BL_PROPAGATE(blFontGetGlyphBounds(self, borderGlyphs, intptr_t(sizeof(uint32_t)), glyphBounds, 2));
  out->advance = advance;

  double lsb = glyphBounds[0].x0;
  double rsb = placementData[size - 1].advance.x - glyphBounds[1].x1;

  out->leadingBearing.reset(lsb, 0);
  out->trailingBearing.reset(rsb, 0);
  out->boundingBox.reset(glyphBounds[0].x0, 0.0, advance.x - rsb, 0.0);

  const BLFontMatrix& m = selfI->matrix;
  BLPoint scalePt = BLPoint(m.m00, m.m11);

  out->advance *= scalePt;
  out->leadingBearing *= scalePt;
  out->trailingBearing *= scalePt;
  out->boundingBox *= scalePt;

  return BL_SUCCESS;
}

// BLFont - Low-Level API
// ======================

BLResult blFontGetGlyphBounds(const BLFontCore* self, const uint32_t* glyphData, intptr_t glyphAdvance, BLBoxI* out, size_t count) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLInternalFontImpl* selfI = blFontGetImpl(self);
  BLInternalFontFaceImpl* faceI = blFontFaceGetImpl(&selfI->face);

  return faceI->funcs.getGlyphBounds(faceI, glyphData, glyphAdvance, out, count);
}

BLResult blFontGetGlyphAdvances(const BLFontCore* self, const uint32_t* glyphData, intptr_t glyphAdvance, BLGlyphPlacement* out, size_t count) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLInternalFontImpl* selfI = blFontGetImpl(self);
  BLInternalFontFaceImpl* faceI = blFontFaceGetImpl(&selfI->face);

  return faceI->funcs.getGlyphAdvances(faceI, glyphData, glyphAdvance, out, count);
}

// BLFont - Glyph Outlines
// =======================

static BLResult BL_CDECL blFontDummyPathSink(BLPathCore* path, const void* info, void* closure) noexcept {
  blUnused(path, info, closure);
  return BL_SUCCESS;
}

BLResult blFontGetGlyphOutlines(const BLFontCore* self, uint32_t glyphId, const BLMatrix2D* userMatrix, BLPathCore* out, BLPathSinkFunc sink, void* closure) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLInternalFontImpl* selfI = blFontGetImpl(self);
  BLInternalFontFaceImpl* faceI = blFontFaceGetImpl(&selfI->face);

  BLMatrix2D finalMatrix;
  const BLFontMatrix& fMat = selfI->matrix;

  if (userMatrix)
    blFontMatrixMultiply(&finalMatrix, &fMat, userMatrix);
  else
    finalMatrix.reset(fMat.m00, fMat.m01, fMat.m10, fMat.m11, 0.0, 0.0);

  BLScopedBufferTmp<BL_FONT_GET_GLYPH_OUTLINE_BUFFER_SIZE> tmpBuffer;
  BLGlyphOutlineSinkInfo sinkInfo;
  BL_PROPAGATE(faceI->funcs.getGlyphOutlines(faceI, glyphId, &finalMatrix, static_cast<BLPath*>(out), &sinkInfo.contourCount, &tmpBuffer));

  if (!sink)
    return BL_SUCCESS;

  sinkInfo.glyphIndex = 0;
  return sink(out, &sinkInfo, closure);
}

BLResult blFontGetGlyphRunOutlines(const BLFontCore* self, const BLGlyphRun* glyphRun, const BLMatrix2D* userMatrix, BLPathCore* out, BLPathSinkFunc sink, void* closure) noexcept {
  BL_ASSERT(self->_d.isFont());

  BLInternalFontImpl* selfI = blFontGetImpl(self);
  BLInternalFontFaceImpl* faceI = blFontFaceGetImpl(&selfI->face);

  if (!glyphRun->size)
    return BL_SUCCESS;

  BLMatrix2D finalMatrix;
  const BLFontMatrix& fMat = selfI->matrix;

  if (userMatrix) {
    blFontMatrixMultiply(&finalMatrix, &fMat, userMatrix);
  }
  else {
    userMatrix = &BLTransformPrivate::identityTransform;
    finalMatrix.reset(fMat.m00, fMat.m01, fMat.m10, fMat.m11, 0.0, 0.0);
  }

  if (!sink)
    sink = blFontDummyPathSink;

  BLScopedBufferTmp<BL_FONT_GET_GLYPH_OUTLINE_BUFFER_SIZE> tmpBuffer;
  BLGlyphOutlineSinkInfo sinkInfo;

  uint32_t placementType = glyphRun->placementType;
  BLGlyphRunIterator it(*glyphRun);
  auto getGlyphOutlinesFunc = faceI->funcs.getGlyphOutlines;

  if (it.hasPlacement() && placementType != BL_GLYPH_PLACEMENT_TYPE_NONE) {
    BLMatrix2D offsetMatrix(1.0, 0.0, 0.0, 1.0, finalMatrix.m20, finalMatrix.m21);

    switch (placementType) {
      case BL_GLYPH_PLACEMENT_TYPE_ADVANCE_OFFSET:
      case BL_GLYPH_PLACEMENT_TYPE_DESIGN_UNITS:
        offsetMatrix.m00 = finalMatrix.m00;
        offsetMatrix.m01 = finalMatrix.m01;
        offsetMatrix.m10 = finalMatrix.m10;
        offsetMatrix.m11 = finalMatrix.m11;
        break;

      case BL_GLYPH_PLACEMENT_TYPE_USER_UNITS:
        offsetMatrix.m00 = userMatrix->m00;
        offsetMatrix.m01 = userMatrix->m01;
        offsetMatrix.m10 = userMatrix->m10;
        offsetMatrix.m11 = userMatrix->m11;
        break;
    }

    if (placementType == BL_GLYPH_PLACEMENT_TYPE_ADVANCE_OFFSET) {
      double ox = finalMatrix.m20;
      double oy = finalMatrix.m21;
      double px;
      double py;

      while (!it.atEnd()) {
        const BLGlyphPlacement& pos = it.placement<BLGlyphPlacement>();

        px = pos.placement.x;
        py = pos.placement.y;
        finalMatrix.m20 = px * offsetMatrix.m00 + py * offsetMatrix.m10 + ox;
        finalMatrix.m21 = px * offsetMatrix.m01 + py * offsetMatrix.m11 + oy;

        sinkInfo.glyphIndex = it.index;
        BL_PROPAGATE(getGlyphOutlinesFunc(faceI, it.glyphId(), &finalMatrix, blDownCast(out), &sinkInfo.contourCount, &tmpBuffer));
        BL_PROPAGATE(sink(out, &sinkInfo, closure));
        it.advance();

        px = pos.advance.x;
        py = pos.advance.y;
        ox += px * offsetMatrix.m00 + py * offsetMatrix.m10;
        oy += px * offsetMatrix.m01 + py * offsetMatrix.m11;
      }
    }
    else {
      while (!it.atEnd()) {
        const BLPoint& placement = it.placement<BLPoint>();
        finalMatrix.m20 = placement.x * offsetMatrix.m00 + placement.y * offsetMatrix.m10 + offsetMatrix.m20;
        finalMatrix.m21 = placement.x * offsetMatrix.m01 + placement.y * offsetMatrix.m11 + offsetMatrix.m21;

        sinkInfo.glyphIndex = it.index;
        BL_PROPAGATE(getGlyphOutlinesFunc(faceI, it.glyphId(), &finalMatrix, blDownCast(out), &sinkInfo.contourCount, &tmpBuffer));
        BL_PROPAGATE(sink(out, &sinkInfo, closure));
        it.advance();
      }
    }
  }
  else {
    while (!it.atEnd()) {
      sinkInfo.glyphIndex = it.index;
      BL_PROPAGATE(getGlyphOutlinesFunc(faceI, it.glyphId(), &finalMatrix, blDownCast(out), &sinkInfo.contourCount, &tmpBuffer));
      BL_PROPAGATE(sink(out, &sinkInfo, closure));
      it.advance();
    }
  }

  return BL_SUCCESS;
}

// BLFont - Runtime Registration
// =============================

void blFontRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

  // Initialize BLFontData built-ins.
  blFontDataDefaultImpl.virt.base.destroy = blNullFontDataImplDestroy;
  blFontDataDefaultImpl.virt.base.getProperty = blObjectImplGetProperty;
  blFontDataDefaultImpl.virt.base.setProperty = blObjectImplSetProperty;
  blFontDataDefaultImpl.virt.listTags = blNullFontDataImplListTags;
  blFontDataDefaultImpl.virt.queryTables = blNullFontDataImplQueryTables;
  blFontDataImplCtor(&blFontDataDefaultImpl.impl, &blFontDataDefaultImpl.virt);

  blMemFontDataVirt.base.destroy = blMemFontDataImplDestroy;
  blMemFontDataVirt.base.getProperty = blObjectImplGetProperty;
  blMemFontDataVirt.base.setProperty = blObjectImplSetProperty;
  blMemFontDataVirt.listTags = blMemFontDataImplListTags;
  blMemFontDataVirt.queryTables = blMemFontDataImplQueryTables;

  blObjectDefaults[BL_OBJECT_TYPE_FONT_DATA]._d.initDynamic(
    BL_OBJECT_TYPE_FONT_DATA,
    BLObjectInfo{BL_OBJECT_INFO_IMMUTABLE_FLAG},
    &blFontDataDefaultImpl.impl);

  // Initialize BLFontFace built-ins.
  blNullFontFaceFuncs.mapTextToGlyphs = blNullFontFaceMapTextToGlyphs;
  blNullFontFaceFuncs.getGlyphBounds = blNullFontFaceGetGlyphBounds;
  blNullFontFaceFuncs.getGlyphAdvances = blNullFontFaceGetGlyphAdvances;
  blNullFontFaceFuncs.getGlyphOutlines = blNullFontFaceGetGlyphOutlines;
  blNullFontFaceFuncs.applyKern = blNullFontFaceApplyKern;
  blNullFontFaceFuncs.applyGSub = blNullFontFaceApplyGSub;
  blNullFontFaceFuncs.applyGPos = blNullFontFaceApplyGPos;
  blNullFontFaceFuncs.positionGlyphs = blNullFontFacePositionGlyphs;

  blFontFaceDefaultImpl.virt.base.destroy = blNullFontFaceImplDestroy;
  blFontFaceDefaultImpl.virt.base.getProperty = blObjectImplGetProperty;
  blFontFaceDefaultImpl.virt.base.setProperty = blObjectImplSetProperty;
  blFontFaceImplCtor(&blFontFaceDefaultImpl.impl, &blFontFaceDefaultImpl.virt, blNullFontFaceFuncs);

  blObjectDefaults[BL_OBJECT_TYPE_FONT_FACE]._d.initDynamic(
    BL_OBJECT_TYPE_FONT_FACE,
    BLObjectInfo{BL_OBJECT_INFO_IMMUTABLE_FLAG},
    &blFontFaceDefaultImpl.impl);

  // Initialize BLFont built-ins.
  blFontImplCtor(&blFontDefaultImpl.impl);
  blObjectDefaults[BL_OBJECT_TYPE_FONT]._d.initDynamic(
    BL_OBJECT_TYPE_FONT,
    BLObjectInfo{BL_OBJECT_INFO_IMMUTABLE_FLAG},
    &blFontDefaultImpl.impl);

  // Initialize OpenType implementation.
  blOpenTypeRtInit(rt);
}
