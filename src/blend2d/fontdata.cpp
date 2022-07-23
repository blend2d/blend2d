// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "filesystem.h"
#include "fontdata_p.h"
#include "fonttags_p.h"
#include "object_p.h"
#include "runtime_p.h"
#include "opentype/otcore_p.h"
#include "support/intops_p.h"
#include "support/ptrops_p.h"

// BLFontData - Globals
// ====================

static BLObjectEthernalVirtualImpl<BLFontDataPrivateImpl, BLFontDataVirt> blFontDataDefaultImpl;

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

struct BLMemFontDataImpl : public BLFontDataPrivateImpl {
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
    if (BLFontTagsPrivate::isOpenTypeVersionTag(sfnt->versionTag())) {
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
      if (BLFontTagsPrivate::isOpenTypeVersionTag(sfnt->versionTag())) {
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

// BLFontData - API - Init & Destroy
// =================================

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

// BLFontData - API - Reset
// ========================

BLResult blFontDataReset(BLFontDataCore* self) noexcept {
  BL_ASSERT(self->_d.isFontData());

  return blObjectPrivateReplaceVirtual(self, static_cast<BLFontDataCore*>(&blObjectDefaults[BL_OBJECT_TYPE_FONT_DATA]));
}

// BLFontData - API - Assign
// =========================

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

// BLFontData - API - Equality & Comparison
// ========================================

bool blFontDataEquals(const BLFontDataCore* a, const BLFontDataCore* b) noexcept {
  BL_ASSERT(a->_d.isFontData());
  BL_ASSERT(b->_d.isFontData());

  return a->_d.impl == b->_d.impl;
}

// BLFontData - API - Create
// =========================

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

  if (BLFontTagsPrivate::isOpenTypeCollectionTag(headerTag)) {
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
    if (!BLFontTagsPrivate::isOpenTypeVersionTag(headerTag))
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

// BLFontData - API - Query
// ========================

BLResult blFontDataListTags(const BLFontDataCore* self, uint32_t faceIndex, BLArrayCore* dst) noexcept {
  BL_ASSERT(self->_d.isFontData());

  BLFontDataPrivateImpl* selfI = blFontDataGetImpl(self);
  return selfI->virt->listTags(selfI, faceIndex, dst);
}

size_t blFontDataQueryTables(const BLFontDataCore* self, uint32_t faceIndex, BLFontTable* dst, const BLTag* tags, size_t count) noexcept {
  BL_ASSERT(self->_d.isFontData());

  BLFontDataPrivateImpl* selfI = blFontDataGetImpl(self);
  return selfI->virt->queryTables(selfI, faceIndex, dst, tags, count);
}

// BLFontData - Runtime Registration
// =================================

void blFontDataRtInit(BLRuntimeContext* rt) noexcept {
  blUnused(rt);

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
}
