// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "api-build_p.h"
#include "array_p.h"
#include "filesystem.h"
#include "fontdata_p.h"
#include "fonttagdata_p.h"
#include "object_p.h"
#include "runtime_p.h"
#include "opentype/otcore_p.h"
#include "support/intops_p.h"
#include "support/ptrops_p.h"

namespace bl {
namespace FontDataInternal {

// bl::FontData - Globals
// ======================

static BLObjectEternalVirtualImpl<BLFontDataPrivateImpl, BLFontDataVirt> defaultImpl;

static BLFontDataVirt memFontDataVirt;

// bl::FontData - Null Impl
// ========================

static BLResult BL_CDECL nullDestroyImpl(BLObjectImpl* impl) noexcept {
  blUnused(impl);
  return BL_SUCCESS;
}

static BLResult BL_CDECL nullGetTableTagsImpl(const BLFontDataImpl* impl, uint32_t faceIndex, BLArrayCore* out) noexcept {
  blUnused(impl, faceIndex);
  return blArrayClear(out);
}

static size_t BL_CDECL nullGetTablesImpl(const BLFontDataImpl* impl, uint32_t faceIndex, BLFontTable* dst, const BLTag* tags, size_t n) noexcept {
  blUnused(impl, faceIndex, tags);
  for (size_t i = 0; i < n; i++)
    dst[i].reset();
  return 0;
}

// bl::FontData - Memory Impl
// ==========================

struct MemFontDataImpl : public BLFontDataPrivateImpl {
  //! Pointer to the start of font data.
  void* data;
  //! Size of `data` [in bytes].
  uint32_t dataSize;
  //! Offset to an array that contains offsets for each font face.
  uint32_t offsetArrayIndex;

  //! If the `data` is not external it's held by this array.
  BLArray<uint8_t> dataArray;
};

// Destroys `MemFontDataImpl` - this is a real destructor.
static BLResult memRealDestroy(MemFontDataImpl* impl) noexcept {
  if (ObjectInternal::isImplExternal(impl))
    ObjectInternal::callExternalDestroyFunc(impl, impl->data);

  blCallDtor(impl->faceCache);
  blCallDtor(impl->dataArray);

  return ObjectInternal::freeImpl(impl);
}

static BLResult BL_CDECL memDestroyImpl(BLObjectImpl* impl) noexcept {
  return memRealDestroy(static_cast<MemFontDataImpl*>(impl));
}

static BLResult BL_CDECL memGetTableTagsImpl(const BLFontDataImpl* impl_, uint32_t faceIndex, BLArrayCore* out) noexcept {
  using namespace OpenType;

  const MemFontDataImpl* impl = static_cast<const MemFontDataImpl*>(impl_);
  const void* fontData = impl->data;
  size_t dataSize = impl->dataSize;

  if (BL_UNLIKELY(faceIndex >= impl->faceCount)) {
    blArrayClear(out);
    return blTraceError(BL_ERROR_INVALID_VALUE);
  }

  uint32_t headerOffset = 0;
  if (impl->offsetArrayIndex)
    headerOffset = PtrOps::offset<UInt32>(fontData, impl->offsetArrayIndex)[faceIndex].value();

  if (BL_LIKELY(headerOffset <= dataSize - sizeof(SFNTHeader))) {
    const SFNTHeader* sfnt = PtrOps::offset<SFNTHeader>(fontData, headerOffset);
    if (FontTagData::isOpenTypeVersionTag(sfnt->versionTag())) {
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

static size_t BL_CDECL memGetTablesImpl(const BLFontDataImpl* impl_, uint32_t faceIndex, BLFontTable* dst, const BLTag* tags, size_t n) noexcept {
  using namespace bl::OpenType;

  const MemFontDataImpl* impl = static_cast<const MemFontDataImpl*>(impl_);
  const void* fontData = impl->data;
  size_t dataSize = impl->dataSize;

  memset(dst, 0, n * sizeof(BLFontTable));

  if (BL_LIKELY(faceIndex < impl->faceCount)) {
    uint32_t headerOffset = 0;
    if (impl->offsetArrayIndex)
      headerOffset = PtrOps::offset<UInt32>(fontData, impl->offsetArrayIndex)[faceIndex].value();

    if (BL_LIKELY(headerOffset <= dataSize - sizeof(SFNTHeader))) {
      const SFNTHeader* sfnt = PtrOps::offset<SFNTHeader>(fontData, headerOffset);
      if (FontTagData::isOpenTypeVersionTag(sfnt->versionTag())) {
        uint32_t tableCount = sfnt->numTables();

        // We can safely multiply `tableCount` as SFNTHeader::numTables is `UInt16`.
        uint32_t minDataSize = uint32_t(sizeof(SFNTHeader)) + tableCount * uint32_t(sizeof(SFNTHeader::TableRecord));
        if (BL_LIKELY(dataSize - headerOffset >= minDataSize)) {
          // Iterate over all tables and try to find all tables as specified by `tags`.
          const SFNTHeader::TableRecord* tables = sfnt->tableRecords();
          size_t matchCount = 0;

          // If all tags are known (convertible to tableId) we can just build a small index and then try
          // to match all tags by doing a linear scan. If there is one or more unknown tags, we would go
          // with linear scan.
          //
          // In general, we do this, because Blend2D's OpenType engine requests all tables in one go, and
          // then inspects them on load - this means that this table lookup is in general optimized and
          // only does a single iteration of 'sfnt' tables.
          if (n >= 3u && n < 255u) {
            constexpr uint32_t kTableIdCountAligned = IntOps::alignUp(FontTagData::kTableIdCount, 16);

            uint8_t tableIdToIndex[kTableIdCountAligned];
            memset(tableIdToIndex, 0xFF, kTableIdCountAligned);

            size_t i = 0;
            while (i < n) {
              uint32_t tableId = FontTagData::tableTagToId(tags[i]);
              if (tableId == FontTagData::kInvalidId)
                break;
              tableIdToIndex[tableId] = uint8_t(i);
              i++;
            }

            if (i == n) {
              // All requested tags known, table id to dst index lookup built successfully.
              for (uint32_t tableIndex = 0; tableIndex < tableCount; tableIndex++) {
                const SFNTHeader::TableRecord& table = tables[tableIndex];

                BLTag tableTag = table.tag();
                uint32_t tableId = FontTagData::tableTagToId(tableTag);

                // Since we know that all requested tags have their unique IDs, we can refuse any tag that doesn't.
                if (tableId == FontTagData::kInvalidId)
                  continue;

                i = tableIdToIndex[tableId];
                if (i != 0xFF) {
                  uint32_t tableOffset = table.offset();
                  uint32_t tableSize = table.length();

                  if (tableOffset < dataSize && tableSize && tableSize <= dataSize - tableOffset) {
                    dst[i].reset(PtrOps::offset<uint8_t>(fontData, tableOffset), tableSize);
                    matchCount++;
                  }
                }
              }
            }

            return matchCount;
          }

          // Linear search.
          for (size_t tagIndex = 0; tagIndex < n; tagIndex++) {
            uint32_t tag = IntOps::byteSwap32BE(tags[tagIndex]);
            for (uint32_t tableIndex = 0; tableIndex < tableCount; tableIndex++) {
              const SFNTHeader::TableRecord& table = tables[tableIndex];

              if (table.tag.rawValue() == tag) {
                uint32_t tableOffset = table.offset();
                uint32_t tableSize = table.length();

                if (tableOffset < dataSize && tableSize && tableSize <= dataSize - tableOffset) {
                  dst[tagIndex].reset(PtrOps::offset<uint8_t>(fontData, tableOffset), tableSize);
                  matchCount++;
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

  return 0;
}

} // {FontDataInternal}
} // {bl}

// bl::FontData - API - Init & Destroy
// ===================================

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

  return bl::ObjectInternal::releaseVirtualInstance(self);
}

// bl::FontData - API - Reset
// ==========================

BLResult blFontDataReset(BLFontDataCore* self) noexcept {
  BL_ASSERT(self->_d.isFontData());

  return bl::ObjectInternal::replaceVirtualInstance(self, static_cast<BLFontDataCore*>(&blObjectDefaults[BL_OBJECT_TYPE_FONT_DATA]));
}

// bl::FontData - API - Assign
// ===========================

BLResult blFontDataAssignMove(BLFontDataCore* self, BLFontDataCore* other) noexcept {
  BL_ASSERT(self->_d.isFontData());
  BL_ASSERT(other->_d.isFontData());

  BLFontDataCore tmp = *other;
  other->_d = blObjectDefaults[BL_OBJECT_TYPE_FONT_DATA]._d;
  return bl::ObjectInternal::replaceVirtualInstance(self, &tmp);
}

BLResult blFontDataAssignWeak(BLFontDataCore* self, const BLFontDataCore* other) noexcept {
  BL_ASSERT(self->_d.isFontData());
  BL_ASSERT(other->_d.isFontData());

  return bl::ObjectInternal::assignVirtualInstance(self, other);
}

// bl::FontData - API - Equality & Comparison
// ==========================================

bool blFontDataEquals(const BLFontDataCore* a, const BLFontDataCore* b) noexcept {
  BL_ASSERT(a->_d.isFontData());
  BL_ASSERT(b->_d.isFontData());

  return a->_d.impl == b->_d.impl;
}

// bl::FontData - API - Create
// ===========================

BLResult blFontDataCreateFromFile(BLFontDataCore* self, const char* fileName, BLFileReadFlags readFlags) noexcept {
  BL_ASSERT(self->_d.isFontData());

  BLArray<uint8_t> buffer;
  BL_PROPAGATE(BLFileSystem::readFile(fileName, buffer, 0, readFlags));

  if (buffer.empty())
    return blTraceError(BL_ERROR_FILE_EMPTY);

  return blFontDataCreateFromDataArray(self, &buffer);
}

static BLResult blFontDataCreateFromDataInternal(BLFontDataCore* self, const void* data, size_t dataSize, BLDestroyExternalDataFunc destroyFunc, void* userData, const BLArray<uint8_t>* array) noexcept {
  using namespace bl::FontDataInternal;
  using namespace bl::OpenType;

  constexpr uint32_t kBaseSize = blMin<uint32_t>(SFNTHeader::kBaseSize, TTCFHeader::kBaseSize);
  if (BL_UNLIKELY(dataSize < kBaseSize))
    return blTraceError(BL_ERROR_INVALID_DATA);

  if (BL_UNLIKELY(sizeof(size_t) > 4 && dataSize > 0xFFFFFFFFu))
    return blTraceError(BL_ERROR_DATA_TOO_LARGE);

  uint32_t headerTag = bl::PtrOps::offset<const UInt32>(data, 0)->value();
  uint32_t faceCount = 1;
  uint32_t dataFlags = 0;

  uint32_t offsetArrayIndex = 0;
  const UInt32* offsetArray = nullptr;

  if (bl::FontTagData::isOpenTypeCollectionTag(headerTag)) {
    if (BL_UNLIKELY(dataSize < TTCFHeader::kBaseSize))
      return blTraceError(BL_ERROR_INVALID_DATA);

    const TTCFHeader* header = bl::PtrOps::offset<const TTCFHeader>(data, 0);

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
    if (!bl::FontTagData::isOpenTypeVersionTag(headerTag))
      return blTraceError(BL_ERROR_INVALID_SIGNATURE);
  }

  BLArray<BLFontFaceImpl*> faceCache;
  BL_PROPAGATE(faceCache.resize(faceCount, nullptr));

  BLFontDataCore newO;
  BLObjectInfo info = BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_FONT_DATA);

  if (!destroyFunc)
    BL_PROPAGATE(bl::ObjectInternal::allocImplT<MemFontDataImpl>(&newO, info));
  else
    BL_PROPAGATE(bl::ObjectInternal::allocImplExternalT<MemFontDataImpl>(&newO, info, true, destroyFunc, userData));

  MemFontDataImpl* newI = static_cast<MemFontDataImpl*>(newO._d.impl);
  initImpl(newI, &memFontDataVirt);

  newI->faceType = uint8_t(BL_FONT_FACE_TYPE_OPENTYPE);
  newI->faceCount = faceCount;
  newI->flags = dataFlags;

  blCallCtor(newI->faceCache, BLInternal::move(faceCache));
  blCallCtor(newI->dataArray);

  if (array) {
    newI->dataArray = *array;
    data = newI->dataArray.data();
  }

  newI->data = const_cast<void*>(data);
  newI->dataSize = uint32_t(dataSize);
  newI->offsetArrayIndex = offsetArrayIndex;

  return bl::ObjectInternal::replaceVirtualInstance(self, &newO);
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

// bl::FontData - API - Accessors
// ==============================

uint32_t blFontDataGetFaceCount(const BLFontDataCore* self) noexcept {
  using namespace bl::FontDataInternal;
  BL_ASSERT(self->_d.isFontData());

  const BLFontDataPrivateImpl* selfI = getImpl(self);
  return selfI->faceCount;
}

BLFontFaceType blFontDataGetFaceType(const BLFontDataCore* self) noexcept {
  using namespace bl::FontDataInternal;
  BL_ASSERT(self->_d.isFontData());

  const BLFontDataPrivateImpl* selfI = getImpl(self);
  return BLFontFaceType(selfI->faceType);
}

BLFontDataFlags blFontDataGetFlags(const BLFontDataCore* self) noexcept {
  using namespace bl::FontDataInternal;
  BL_ASSERT(self->_d.isFontData());

  const BLFontDataPrivateImpl* selfI = getImpl(self);
  return BLFontDataFlags(selfI->flags);
}

BLResult blFontDataGetTableTags(const BLFontDataCore* self, uint32_t faceIndex, BLArrayCore* dst) noexcept {
  using namespace bl::FontDataInternal;
  BL_ASSERT(self->_d.isFontData());

  const BLFontDataPrivateImpl* selfI = getImpl(self);
  return selfI->virt->getTableTags(selfI, faceIndex, dst);
}

size_t blFontDataGetTables(const BLFontDataCore* self, uint32_t faceIndex, BLFontTable* dst, const BLTag* tags, size_t count) noexcept {
  using namespace bl::FontDataInternal;
  BL_ASSERT(self->_d.isFontData());

  const BLFontDataPrivateImpl* selfI = getImpl(self);
  return selfI->virt->getTables(selfI, faceIndex, dst, tags, count);
}

// bl::FontData - Runtime Registration
// ===================================

void blFontDataRtInit(BLRuntimeContext* rt) noexcept {
  using namespace bl::FontDataInternal;

  blUnused(rt);

  defaultImpl.virt.base.destroy = nullDestroyImpl;
  defaultImpl.virt.base.getProperty = blObjectImplGetProperty;
  defaultImpl.virt.base.setProperty = blObjectImplSetProperty;
  defaultImpl.virt.getTableTags = nullGetTableTagsImpl;
  defaultImpl.virt.getTables = nullGetTablesImpl;
  initImpl(&defaultImpl.impl, &defaultImpl.virt);

  memFontDataVirt.base.destroy = memDestroyImpl;
  memFontDataVirt.base.getProperty = blObjectImplGetProperty;
  memFontDataVirt.base.setProperty = blObjectImplSetProperty;
  memFontDataVirt.getTableTags = memGetTableTagsImpl;
  memFontDataVirt.getTables = memGetTablesImpl;

  blObjectDefaults[BL_OBJECT_TYPE_FONT_DATA]._d.initDynamic(BLObjectInfo::fromTypeWithMarker(BL_OBJECT_TYPE_FONT_DATA), &defaultImpl.impl);
}
