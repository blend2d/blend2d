// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#include "./api-build_p.h"
#include "./array_p.h"
#include "./glyphbuffer_p.h"
#include "./filesystem.h"
#include "./font_p.h"
#include "./matrix.h"
#include "./path.h"
#include "./runtime_p.h"
#include "./string_p.h"
#include "./support_p.h"
#include "./unicode_p.h"
#include "./opentype/otcore_p.h"
#include "./opentype/otface_p.h"
#include "./threading/atomic_p.h"

// ============================================================================
// [Global Variables]
// ============================================================================

BLInternalFontFaceFuncs blNullFontFaceFuncs;

static BLWrap<BLInternalFontImpl> blNullFontImpl;
static BLWrap<BLInternalFontFaceImpl> blNullFontFaceImpl;
static BLWrap<BLFontDataImpl> blNullFontDataImpl;

static BLFontFaceVirt blNullFontFaceVirt;
static BLAtomicUInt64Generator blFontFaceIdGenerator;

// ============================================================================
// [BLFontData - Null]
// ============================================================================

static BLFontDataVirt blNullFontDataVirt;

static BLResult BL_CDECL blNullFontDataImplDestroy(BLFontDataImpl* impl) noexcept {
  BL_UNUSED(impl);
  return BL_SUCCESS;
}

static BLResult BL_CDECL blNullFontDataImplListTags(const BLFontDataImpl* impl, uint32_t faceIndex, BLArrayCore* out) noexcept {
  BL_UNUSED(impl);
  BL_UNUSED(faceIndex);
  return blArrayClear(out);
}

static size_t BL_CDECL blNullFontDataImplQueryTables(const BLFontDataImpl* impl, uint32_t faceIndex, BLFontTable* dst, const BLTag* tags, size_t n) noexcept {
  BL_UNUSED(impl);
  BL_UNUSED(faceIndex);
  BL_UNUSED(tags);
  for (size_t i = 0; i < n; i++)
    dst[i].reset();
  return 0;
}

// ============================================================================
// [BLFontData - Utilities]
// ============================================================================

static BL_INLINE bool blFontIsOpenTypeVersionTag(uint32_t tag) noexcept {
  return tag == BL_MAKE_TAG('O', 'T', 'T', 'O') ||
         tag == BL_MAKE_TAG( 0 ,  1 ,  0 ,  0 ) ||
         tag == BL_MAKE_TAG('t', 'r', 'u', 'e') ;
}

// ============================================================================
// [BLFontData - Memory Impl]
// ============================================================================

static BLFontDataVirt blMemFontDataVirt;

struct BLMemFontDataImpl : public BLInternalFontDataImpl {
  //! Pointer to the start of font data.
  void* data;
  //! Size of `data` [in bytes].
  uint32_t dataSize;
  //! Offset to an array that contains offsets for each font-face.
  uint32_t offsetArrayIndex;
};

// Destroys `BLMemFontDataImpl` - this is a real destructor.
static BLResult blMemFontDataImplRealDestroy(BLMemFontDataImpl* impl) noexcept {
  uint8_t* implBase = reinterpret_cast<uint8_t*>(impl);
  size_t implSize = sizeof(BLMemFontDataImpl);
  uint32_t implTraits = impl->implTraits;
  uint32_t memPoolData = impl->memPoolData;

  if (implTraits & BL_IMPL_TRAIT_EXTERNAL) {
    implSize += sizeof(BLExternalImplPreface);
    implBase -= sizeof(BLExternalImplPreface);
    blImplDestroyExternal(impl);
  }

  return blRuntimeFreeImpl(implBase, implSize, memPoolData);
}

static BLResult BL_CDECL blMemFontDataImplDestroy(BLFontDataImpl* impl_) noexcept {
  BLMemFontDataImpl* impl = static_cast<BLMemFontDataImpl*>(impl_);
  return blMemFontDataImplRealDestroy(impl);
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
    headerOffset = blOffsetPtr<UInt32>(fontData, impl->offsetArrayIndex)[faceIndex].value();

  if (BL_LIKELY(headerOffset <= dataSize - sizeof(SFNTHeader))) {
    const SFNTHeader* sfnt = blOffsetPtr<SFNTHeader>(fontData, headerOffset);
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
      headerOffset = blOffsetPtr<UInt32>(fontData, impl->offsetArrayIndex)[faceIndex].value();

    if (BL_LIKELY(headerOffset <= dataSize - sizeof(SFNTHeader))) {
      const SFNTHeader* sfnt = blOffsetPtr<SFNTHeader>(fontData, headerOffset);
      if (blFontIsOpenTypeVersionTag(sfnt->versionTag())) {
        uint32_t tableCount = sfnt->numTables();

        // We can safely multiply `tableCount` as SFNTHeader::numTables is `UInt16`.
        uint32_t minDataSize = uint32_t(sizeof(SFNTHeader)) + tableCount * uint32_t(sizeof(SFNTHeader::TableRecord));
        if (BL_LIKELY(dataSize - headerOffset >= minDataSize)) {
          const SFNTHeader::TableRecord* tables = sfnt->tableRecords();
          size_t matchCount = 0;

          // Iterate over all tables and try to find all tables as specified by `tags`.
          for (size_t tagIndex = 0; tagIndex < n; tagIndex++) {
            uint32_t tag = blByteSwap32BE(tags[tagIndex]);
            dst[tagIndex].reset();

            for (uint32_t tableIndex = 0; tableIndex < tableCount; tableIndex++) {
              const SFNTHeader::TableRecord& table = tables[tableIndex];

              if (table.tag.rawValue() == tag) {
                uint32_t tableOffset = table.offset();
                uint32_t tableSize = table.length();

                if (tableOffset < dataSize && tableSize && tableSize <= dataSize - tableOffset) {
                  matchCount++;
                  dst[tagIndex].reset(blOffsetPtr<uint8_t>(fontData, tableOffset), tableSize);
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

// ============================================================================
// [BLFontData - Init / Reset]
// ============================================================================

BLResult blFontDataInit(BLFontDataCore* self) noexcept {
  self->impl = &blNullFontDataImpl;
  return BL_SUCCESS;
}

BLResult blFontDataReset(BLFontDataCore* self) noexcept {
  BLFontDataImpl* selfI = self->impl;

  self->impl = &blNullFontDataImpl;
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLFontData - Assign]
// ============================================================================

BLResult blFontDataAssignMove(BLFontDataCore* self, BLFontDataCore* other) noexcept {
  BLFontDataImpl* selfI = self->impl;
  BLFontDataImpl* otherI = other->impl;

  self->impl = otherI;
  other->impl = &blNullFontDataImpl;

  return blImplReleaseVirt(selfI);
}

BLResult blFontDataAssignWeak(BLFontDataCore* self, const BLFontDataCore* other) noexcept {
  BLFontDataImpl* selfI = self->impl;
  BLFontDataImpl* otherI = other->impl;

  self->impl = blImplIncRef(otherI);
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLFontData - Equals]
// ============================================================================

bool blFontDataEquals(const BLFontDataCore* a, const BLFontDataCore* b) noexcept {
  return a->impl == b->impl;
}

// ============================================================================
// [BLFontData - Create]
// ============================================================================

// A callback that we use to destroy an array-impl we keep if `BLMemFontLoaderImpl`
// was created from `BLArray<T>()`.
static void BL_CDECL blDestroyArrayImpl(void* impl, void* arrayI) noexcept {
  BL_UNUSED(impl);
  blArrayImplRelease(static_cast<BLArrayImpl*>(arrayI));
}

BLResult blFontDataCreateFromFile(BLFontDataCore* self, const char* fileName, uint32_t readFlags) noexcept {
  BLArray<uint8_t> buffer;
  BL_PROPAGATE(BLFileSystem::readFile(fileName, buffer, 0, readFlags));

  if (buffer.empty())
    return blTraceError(BL_ERROR_FILE_EMPTY);

  return blFontDataCreateFromDataArray(self, &buffer);
}

BLResult blFontDataCreateFromDataArray(BLFontDataCore* self, const BLArrayCore* dataArray) noexcept {
  BLArrayImpl* arrI = dataArray->impl;
  BLResult result = blFontDataCreateFromData(self, arrI->data, arrI->size * arrI->itemSize, blDestroyArrayImpl, arrI);

  if (result == BL_SUCCESS)
    blImplIncRef(arrI);
  return result;
}

BLResult blFontDataCreateFromData(BLFontDataCore* self, const void* data, size_t dataSize, BLDestroyImplFunc destroyFunc, void* destroyData) noexcept {
  using namespace BLOpenType;

  constexpr uint32_t kMinSize = blMin<uint32_t>(SFNTHeader::kMinSize, TTCFHeader::kMinSize);
  if (BL_UNLIKELY(dataSize < kMinSize))
    return blTraceError(BL_ERROR_INVALID_DATA);

  uint32_t headerTag = blOffsetPtr<const UInt32>(data, 0)->value();
  uint32_t faceCount = 1;
  uint32_t dataFlags = 0;

  uint32_t offsetArrayIndex = 0;
  const UInt32* offsetArray = nullptr;

  if (headerTag == BL_MAKE_TAG('t', 't', 'c', 'f')) {
    if (BL_UNLIKELY(dataSize < TTCFHeader::kMinSize))
      return blTraceError(BL_ERROR_INVALID_DATA);

    const TTCFHeader* header = blOffsetPtr<const TTCFHeader>(data, 0);

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

  size_t implSize = sizeof(BLMemFontDataImpl);
  uint32_t implTraits = BL_IMPL_TRAIT_MUTABLE | BL_IMPL_TRAIT_VIRT;

  if (destroyFunc) {
    implSize += sizeof(BLExternalImplPreface);
    implTraits |= BL_IMPL_TRAIT_EXTERNAL;
  }

  uint16_t memPoolData;
  BLMemFontDataImpl* newI = blRuntimeAllocImplT<BLMemFontDataImpl>(implSize, &memPoolData);

  if (BL_UNLIKELY(!newI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  if (destroyFunc)
    newI = blImplInitExternal(newI, destroyFunc, destroyData);

  blImplInit(newI, BL_IMPL_TYPE_FONT_DATA, implTraits, memPoolData);
  newI->virt = &blMemFontDataVirt;
  newI->faceType = uint8_t(BL_FONT_FACE_TYPE_OPENTYPE);
  newI->faceCount = faceCount;
  newI->flags = dataFlags;

  newI->backRefCount = 0;
  blCallCtor(newI->faceCache, std::move(faceCache));

  newI->data = const_cast<void*>(static_cast<const void*>(data));;
  newI->dataSize = dataSize;
  newI->offsetArrayIndex = offsetArrayIndex;

  BLFontDataImpl* oldI = self->impl;
  self->impl = newI;
  return blImplReleaseVirt(oldI);
};

// ============================================================================
// [BLFontData - Query]
// ============================================================================

BLResult blFontDataListTags(const BLFontDataCore* self, uint32_t faceIndex, BLArrayCore* dst) noexcept {
  BLFontDataImpl* selfI = self->impl;
  return selfI->virt->listTags(selfI, faceIndex, dst);
}

size_t blFontDataQueryTables(const BLFontDataCore* self, uint32_t faceIndex, BLFontTable* dst, const BLTag* tags, size_t count) noexcept {
  BLFontDataImpl* selfI = self->impl;
  return selfI->virt->queryTables(selfI, faceIndex, dst, tags, count);
}

// ============================================================================
// [BLFontFace - Null]
// ============================================================================

BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)

static BLResult BL_CDECL blNullFontFaceImplDestroy(BLFontFaceImpl* impl) noexcept {
  return BL_SUCCESS;
}

static BLResult BL_CDECL blNullFontFaceMapTextToGlyphs(
  const BLFontFaceImpl* impl,
  BLGlyphItem* itemData,
  size_t count,
  BLGlyphMappingState* state) noexcept {

  state->reset();
  return blTraceError(BL_ERROR_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceGetGlyphBounds(
  const BLFontFaceImpl* impl,
  const BLGlyphId* glyphIdData,
  intptr_t glyphIdAdvance,
  BLBoxI* boxes,
  size_t count) noexcept {

  return blTraceError(BL_ERROR_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceGetGlyphAdvances(
  const BLFontFaceImpl* impl,
  const BLGlyphId* glyphIdData,
  intptr_t glyphIdAdvance,
  BLGlyphPlacement* placementData,
  size_t count) noexcept {

  return blTraceError(BL_ERROR_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceGetGlyphOutlines(
  const BLFontFaceImpl* impl,
  uint32_t glyphId,
  const BLMatrix2D* userMatrix,
  BLPath* out,
  size_t* contourCountOut,
  BLMemBuffer* tmpBuffer) noexcept {

  *contourCountOut = 0;
  return blTraceError(BL_ERROR_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceApplyKern(
  const BLFontFaceImpl* faceI,
  BLGlyphItem* itemData,
  BLGlyphPlacement* placementData,
  size_t count) noexcept {

  return blTraceError(BL_ERROR_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceApplyGSub(
  const BLFontFaceImpl* impl,
  BLGlyphBuffer* gb,
  size_t index,
  BLBitWord lookups) noexcept {

  return blTraceError(BL_ERROR_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceApplyGPos(
  const BLFontFaceImpl* impl,
  BLGlyphBuffer* gb,
  size_t index,
  BLBitWord lookups) noexcept {

  return blTraceError(BL_ERROR_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFacePositionGlyphs(
  const BLFontFaceImpl* impl,
  BLGlyphItem* itemData,
  BLGlyphPlacement* placementData,
  size_t count) noexcept {

  return blTraceError(BL_ERROR_NOT_INITIALIZED);
}

BL_DIAGNOSTIC_POP

// ============================================================================
// [BLFontFace - Init / Reset]
// ============================================================================

BLResult blFontFaceInit(BLFontFaceCore* self) noexcept {
  self->impl = &blNullFontFaceImpl;
  return BL_SUCCESS;
}

BLResult blFontFaceReset(BLFontFaceCore* self) noexcept {
  BLInternalFontFaceImpl* selfI = blInternalCast(self->impl);
  self->impl = &blNullFontFaceImpl;
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLFontFace - Assign]
// ============================================================================

BLResult blFontFaceAssignMove(BLFontFaceCore* self, BLFontFaceCore* other) noexcept {
  BLInternalFontFaceImpl* selfI = blInternalCast(self->impl);
  BLInternalFontFaceImpl* otherI = blInternalCast(other->impl);

  self->impl = otherI;
  other->impl = &blNullFontFaceImpl;

  return blImplReleaseVirt(selfI);
}

BLResult blFontFaceAssignWeak(BLFontFaceCore* self, const BLFontFaceCore* other) noexcept {
  BLInternalFontFaceImpl* selfI = blInternalCast(self->impl);
  BLInternalFontFaceImpl* otherI = blInternalCast(other->impl);

  self->impl = blImplIncRef(otherI);
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLFontFace - Equals]
// ============================================================================

bool blFontFaceEquals(const BLFontFaceCore* a, const BLFontFaceCore* b) noexcept {
  return a->impl == b->impl;
}

// ============================================================================
// [BLFontFace - Create]
// ============================================================================

BLResult blFontFaceCreateFromFile(BLFontFaceCore* self, const char* fileName, uint32_t readFlags) noexcept {
  BLFontData fontData;
  BL_PROPAGATE(fontData.createFromFile(fileName, readFlags));
  return blFontFaceCreateFromData(self, &fontData, 0);
}

BLResult blFontFaceCreateFromData(BLFontFaceCore* self, const BLFontDataCore* fontData, uint32_t faceIndex) noexcept {
  if (BL_UNLIKELY(blDownCast(fontData)->isNone()))
    return blTraceError(BL_ERROR_NOT_INITIALIZED);

  if (faceIndex >= blDownCast(fontData)->faceCount())
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLOTFaceImpl* newI;
  BL_PROPAGATE(blOTFaceImplNew(&newI, blDownCast(fontData), faceIndex));
  newI->faceUniqueId = blFontFaceIdGenerator.next();

  BLInternalFontFaceImpl* oldI = blInternalCast(self->impl);
  self->impl = newI;
  return blImplReleaseVirt(oldI);
}

// ============================================================================
// [BLFontFace - Properties]
// ============================================================================

BLResult blFontFaceGetFaceInfo(const BLFontFaceCore* self, BLFontFaceInfo* out) noexcept {
  *out = blDownCast(self)->faceInfo();
  return BL_SUCCESS;
}

BLResult blFontFaceGetDesignMetrics(const BLFontFaceCore* self, BLFontDesignMetrics* out) noexcept {
  *out = blDownCast(self)->designMetrics();
  return BL_SUCCESS;
}

BLResult blFontFaceGetUnicodeCoverage(const BLFontFaceCore* self, BLFontUnicodeCoverage* out) noexcept {
  *out = blDownCast(self)->unicodeCoverage();
  return BL_SUCCESS;
}

// ============================================================================
// [BLFont - Utilities]
// ============================================================================

static void blFontImplCalcProperties(BLFontImpl* fontI, const BLFontFaceImpl* faceI, float size) noexcept {
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
  fontI->metrics.xMin                   = float(double(dm.xMin                  ) * xScale);
  fontI->metrics.yMin                   = float(double(dm.yMin                  ) * yScale);
  fontI->metrics.xMax                   = float(double(dm.xMax                  ) * xScale);
  fontI->metrics.yMax                   = float(double(dm.yMax                  ) * yScale);
  fontI->metrics.underlinePosition      = float(double(dm.underlinePosition     ) * yScale);
  fontI->metrics.underlineThickness     = float(double(dm.underlineThickness    ) * yScale);
  fontI->metrics.strikethroughPosition  = float(double(dm.strikethroughPosition ) * yScale);
  fontI->metrics.strikethroughThickness = float(double(dm.strikethroughThickness) * yScale);
  fontI->matrix.reset(xScale, 0.0, 0.0, -yScale);
}

// ============================================================================
// [BLFont - Internals]
// ============================================================================

static BL_INLINE BLInternalFontImpl* blFontImplNew(BLFontFaceImpl* faceI, float size) noexcept {
  uint16_t memPoolData;
  BLInternalFontImpl* impl = blRuntimeAllocImplT<BLInternalFontImpl>(sizeof(BLInternalFontImpl), &memPoolData);

  if (BL_UNLIKELY(!impl))
    return impl;

  blImplInit(impl, BL_IMPL_TYPE_FONT, BL_IMPL_TRAIT_MUTABLE, memPoolData);
  impl->face.impl = blImplIncRef(faceI);
  impl->features.impl = BLArray<BLFontFeature>::none().impl;
  impl->variations.impl = BLArray<BLFontVariation>::none().impl;
  impl->weight = 0;
  impl->stretch = 0;
  impl->style = 0;
  blFontImplCalcProperties(impl, faceI, size);

  return impl;
}

// Cannot be static, called by `BLVariant` implementation.
BLResult blFontImplDelete(BLFontImpl* impl_) noexcept {
  BLInternalFontImpl* impl = blInternalCast(impl_);

  impl->face.reset();
  impl->features.reset();
  impl->variations.reset();

  uint8_t* implBase = reinterpret_cast<uint8_t*>(impl);
  size_t implSize = sizeof(BLInternalFontImpl);
  uint32_t implTraits = impl->implTraits;
  uint32_t memPoolData = impl->memPoolData;

  if (implTraits & BL_IMPL_TRAIT_EXTERNAL) {
    implSize += sizeof(BLExternalImplPreface);
    implBase -= sizeof(BLExternalImplPreface);
    blImplDestroyExternal(impl);
  }

  if (implTraits & BL_IMPL_TRAIT_FOREIGN)
    return BL_SUCCESS;
  else
    return blRuntimeFreeImpl(implBase, implSize, memPoolData);
}

static BL_INLINE BLResult blFontImplRelease(BLInternalFontImpl* impl) noexcept {
  if (blAtomicFetchSub(&impl->refCount) != 1)
    return BL_SUCCESS;
  return blFontImplDelete(impl);
}

// ============================================================================
// [BLFont - Init / Reset]
// ============================================================================

BLResult blFontInit(BLFontCore* self) noexcept {
  self->impl = &blNullFontImpl;
  return BL_SUCCESS;
}

BLResult blFontReset(BLFontCore* self) noexcept {
  BLInternalFontImpl* selfI = blInternalCast(self->impl);
  self->impl = &blNullFontImpl;
  return blFontImplRelease(selfI);
}

// ============================================================================
// [BLFont - Assign]
// ============================================================================

BLResult blFontAssignMove(BLFontCore* self, BLFontCore* other) noexcept {
  BLInternalFontImpl* selfI = blInternalCast(self->impl);
  BLInternalFontImpl* otherI = blInternalCast(other->impl);

  self->impl = otherI;
  other->impl = &blNullFontImpl;

  return blFontImplRelease(selfI);
}

BLResult blFontAssignWeak(BLFontCore* self, const BLFontCore* other) noexcept {
  BLInternalFontImpl* selfI = blInternalCast(self->impl);
  BLInternalFontImpl* otherI = blInternalCast(other->impl);

  self->impl = blImplIncRef(otherI);
  return blFontImplRelease(selfI);
}

// ============================================================================
// [BLFont - Equals]
// ============================================================================

bool blFontEquals(const BLFontCore* a, const BLFontCore* b) noexcept {
  return a->impl == b->impl;
}

// ============================================================================
// [BLFont - Create]
// ============================================================================

BLResult blFontCreateFromFace(BLFontCore* self, const BLFontFaceCore* face, float size) noexcept {
  if (blDownCast(face)->isNone())
    return blTraceError(BL_ERROR_NOT_INITIALIZED);

  BLInternalFontImpl* selfI = blInternalCast(self->impl);
  if (selfI->refCount == 1) {
    BLFontFaceImpl* oldFaceI = selfI->face.impl;
    BLFontFaceImpl* newFaceI = face->impl;

    selfI->face.impl = blImplIncRef(newFaceI);
    selfI->features.clear();
    selfI->variations.clear();
    selfI->weight = 0;
    selfI->stretch = 0;
    selfI->style = 0;
    blFontImplCalcProperties(selfI, newFaceI, size);

    return blImplReleaseVirt(oldFaceI);
  }
  else {
    BLInternalFontImpl* newI = blFontImplNew(face->impl, size);
    if (BL_UNLIKELY(!newI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    self->impl = newI;
    return blFontImplRelease(selfI);
  }
}

// ============================================================================
// [BLFont - Properties]
// ============================================================================

BLResult blFontGetMatrix(const BLFontCore* self, BLFontMatrix* out) noexcept {
  *out = blDownCast(self)->matrix();
  return BL_SUCCESS;
}

BLResult blFontGetMetrics(const BLFontCore* self, BLFontMetrics* out) noexcept {
  *out = blDownCast(self)->metrics();
  return BL_SUCCESS;
}

BLResult blFontGetDesignMetrics(const BLFontCore* self, BLFontDesignMetrics* out) noexcept {
  *out = blDownCast(self)->designMetrics();
  return BL_SUCCESS;
}

// ============================================================================
// [BLFont - Shaping]
// ============================================================================

BLResult blFontShape(const BLFontCore* self, BLGlyphBufferCore* gb) noexcept {
  BL_PROPAGATE(blFontMapTextToGlyphs(self, gb, nullptr));
  BL_PROPAGATE(blFontPositionGlyphs(self, gb, 0xFFFFFFFFu));

  return BL_SUCCESS;
}

BLResult blFontMapTextToGlyphs(const BLFontCore* self, BLGlyphBufferCore* gb, BLGlyphMappingState* stateOut) noexcept {
  BLInternalGlyphBufferImpl* gbI = blInternalCast(gb->impl);
  if (!gbI->size)
    return BL_SUCCESS;

  if (BL_UNLIKELY(!(gbI->flags & BL_GLYPH_RUN_FLAG_UCS4_CONTENT)))
    return blTraceError(BL_ERROR_INVALID_STATE);

  BLGlyphMappingState state;
  if (!stateOut)
    stateOut = &state;

  BLInternalFontFaceImpl* faceI = blInternalCast(self->impl->face.impl);
  BL_PROPAGATE(faceI->funcs.mapTextToGlyphs(faceI, gbI->glyphItemData, gbI->size, stateOut));

  gbI->flags = gbI->flags & ~BL_GLYPH_RUN_FLAG_UCS4_CONTENT;
  if (stateOut->undefinedCount == 0)
    gbI->flags |= BL_GLYPH_RUN_FLAG_UNDEFINED_GLYPHS;
  return BL_SUCCESS;
}

BLResult blFontPositionGlyphs(const BLFontCore* self, BLGlyphBufferCore* gb, uint32_t positioningFlags) noexcept {
  BLInternalGlyphBufferImpl* gbI = blInternalCast(gb->impl);
  if (!gbI->size)
    return BL_SUCCESS;

  if (BL_UNLIKELY(gbI->flags & BL_GLYPH_RUN_FLAG_UCS4_CONTENT))
    return blTraceError(BL_ERROR_INVALID_STATE);

  BLInternalFontFaceImpl* faceI = blInternalCast(self->impl->face.impl);
  if (!(gbI->flags & BL_GLYPH_BUFFER_GLYPH_ADVANCES)) {
    BL_PROPAGATE(gbI->ensurePlacement());
    faceI->funcs.getGlyphAdvances(faceI, &gbI->glyphItemData->glyphId, sizeof(BLGlyphItem), gbI->placementData, gbI->size);
    gbI->glyphRun.placementType = uint8_t(BL_GLYPH_PLACEMENT_TYPE_ADVANCE_OFFSET);
    gbI->flags |= BL_GLYPH_BUFFER_GLYPH_ADVANCES;
  }

  if (positioningFlags) {
    faceI->funcs.applyKern(faceI, gbI->glyphItemData, gbI->placementData, gbI->size);
  }

  return BL_SUCCESS;
}

BLResult blFontApplyKerning(const BLFontCore* self, BLGlyphBufferCore* gb) noexcept {
  BLInternalGlyphBufferImpl* gbI = blInternalCast(gb->impl);
  if (!gbI->size)
    return BL_SUCCESS;

  if (BL_UNLIKELY(!(gbI->placementData)))
    return blTraceError(BL_ERROR_INVALID_STATE);

  BLInternalFontFaceImpl* faceI = blInternalCast(self->impl->face.impl);
  return faceI->funcs.applyKern(faceI, gbI->glyphItemData, gbI->placementData, gbI->size);
}

BLResult blFontApplyGSub(const BLFontCore* self, BLGlyphBufferCore* gb, size_t index, BLBitWord lookups) noexcept {
  BLInternalFontFaceImpl* faceI = blInternalCast(self->impl->face.impl);
  return faceI->funcs.applyGSub(faceI, static_cast<BLGlyphBuffer*>(gb), index, lookups);
}

BLResult blFontApplyGPos(const BLFontCore* self, BLGlyphBufferCore* gb, size_t index, BLBitWord lookups) noexcept {
  BLInternalGlyphBufferImpl* gbI = blInternalCast(gb->impl);
  if (!gbI->size)
    return BL_SUCCESS;

  if (BL_UNLIKELY(!(gbI->placementData)))
    return blTraceError(BL_ERROR_INVALID_STATE);

  BLInternalFontFaceImpl* faceI = blInternalCast(self->impl->face.impl);
  return faceI->funcs.applyGPos(faceI, static_cast<BLGlyphBuffer*>(gb), index, lookups);
}

BLResult blFontGetTextMetrics(const BLFontCore* self, BLGlyphBufferCore* gb, BLTextMetrics* out) noexcept {
  BLInternalFontImpl* selfI = blInternalCast(self->impl);
  BLInternalGlyphBufferImpl* gbI = blInternalCast(gb->impl);

  out->reset();
  if (!(gbI->flags & BL_GLYPH_BUFFER_GLYPH_ADVANCES)) {
    BL_PROPAGATE(blFontShape(self, gb));
    gbI = blInternalCast(gb->impl);
  }

  size_t size = gbI->size;
  if (!size)
    return BL_SUCCESS;

  BLPoint advance {};

  const BLGlyphItem* glyphItemData = gbI->glyphItemData;
  const BLGlyphPlacement* placementData = gbI->placementData;

  for (size_t i = 0; i < size; i++) {
    advance += BLPoint(placementData[i].advance.x, placementData[i].advance.y);
  }

  BLBoxI glyphBounds[2];
  BLGlyphId borderGlyphs[2] = { BLGlyphId(glyphItemData[0].glyphId), BLGlyphId(glyphItemData[size - 1].glyphId) };

  BL_PROPAGATE(blFontGetGlyphBounds(self, borderGlyphs, intptr_t(sizeof(BLGlyphId)), glyphBounds, 2));
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

// ============================================================================
// [BLFont - Low-Level API]
// ============================================================================

BLResult blFontGetGlyphBounds(const BLFontCore* self, const void* glyphIdData, intptr_t glyphIdAdvance, BLBoxI* out, size_t count) noexcept {
  BLInternalFontFaceImpl* faceI = blInternalCast(self->impl->face.impl);
  return faceI->funcs.getGlyphBounds(faceI, static_cast<const BLGlyphId*>(glyphIdData), glyphIdAdvance, out, count);
}

BLResult blFontGetGlyphAdvances(const BLFontCore* self, const void* glyphIdData, intptr_t glyphIdAdvance, BLGlyphPlacement* out, size_t count) noexcept {
  BLInternalFontFaceImpl* faceI = blInternalCast(self->impl->face.impl);
  return faceI->funcs.getGlyphAdvances(faceI, static_cast<const BLGlyphId*>(glyphIdData), glyphIdAdvance, out, count);
}

// ============================================================================
// [BLFont - Glyph Outlines]
// ============================================================================

static BLResult BL_CDECL blFontDummyPathSink(BLPathCore* path, const void* info, void* closure) noexcept {
  BL_UNUSED(path);
  BL_UNUSED(info);
  BL_UNUSED(closure);
  return BL_SUCCESS;
}

BLResult blFontGetGlyphOutlines(const BLFontCore* self, uint32_t glyphId, const BLMatrix2D* userMatrix, BLPathCore* out, BLPathSinkFunc sink, void* closure) noexcept {
  BLMatrix2D finalMatrix;
  const BLFontMatrix& fMat = self->impl->matrix;
  const BLInternalFontFaceImpl* faceI = blInternalCast(self->impl->face.impl);

  if (userMatrix)
    blFontMatrixMultiply(&finalMatrix, &fMat, userMatrix);
  else
    finalMatrix.reset(fMat.m00, fMat.m01, fMat.m10, fMat.m11, 0.0, 0.0);

  BLMemBufferTmp<BL_FONT_GET_GLYPH_OUTLINE_BUFFER_SIZE> tmpBuffer;
  BLGlyphOutlineSinkInfo sinkInfo;
  BL_PROPAGATE(faceI->funcs.getGlyphOutlines(faceI, glyphId, &finalMatrix, static_cast<BLPath*>(out), &sinkInfo.contourCount, &tmpBuffer));

  if (!sink)
    return BL_SUCCESS;

  sinkInfo.glyphIndex = 0;
  return sink(out, &sinkInfo, closure);
}

BLResult blFontGetGlyphRunOutlines(const BLFontCore* self, const BLGlyphRun* glyphRun, const BLMatrix2D* userMatrix, BLPathCore* out, BLPathSinkFunc sink, void* closure) noexcept {
  if (!glyphRun->size)
    return BL_SUCCESS;

  BLMatrix2D finalMatrix;
  const BLFontMatrix& fMat = self->impl->matrix;

  if (userMatrix) {
    blFontMatrixMultiply(&finalMatrix, &fMat, userMatrix);
  }
  else {
    userMatrix = &blMatrix2DIdentity;
    finalMatrix.reset(fMat.m00, fMat.m01, fMat.m10, fMat.m11, 0.0, 0.0);
  }

  const BLInternalFontFaceImpl* faceI = blInternalCast(self->impl->face.impl);
  uint32_t placementType = glyphRun->placementType;

  if (!sink)
    sink = blFontDummyPathSink;

  BLMemBufferTmp<BL_FONT_GET_GLYPH_OUTLINE_BUFFER_SIZE> tmpBuffer;
  BLGlyphOutlineSinkInfo sinkInfo;

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

// ============================================================================
// [Runtime Init]
// ============================================================================

void blFontRtInit(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);

  // Initialize BLFontData built-in null instance.
  blNullFontDataVirt.destroy = blNullFontDataImplDestroy;
  blNullFontDataVirt.listTags = blNullFontDataImplListTags;
  blNullFontDataVirt.queryTables = blNullFontDataImplQueryTables;

  BLFontDataImpl* fontDataI = &blNullFontDataImpl;
  fontDataI->virt = &blNullFontDataVirt;
  blInitBuiltInNull(fontDataI, BL_IMPL_TYPE_FONT_DATA, BL_IMPL_TRAIT_VIRT);
  blAssignBuiltInNull(fontDataI);

  // Initialize BLFontData virtual functions.
  blMemFontDataVirt.destroy = blMemFontDataImplDestroy;
  blMemFontDataVirt.listTags = blMemFontDataImplListTags;
  blMemFontDataVirt.queryTables = blMemFontDataImplQueryTables;

  // Initialize BLFontFace virtual functions.
  blNullFontFaceVirt.destroy = blNullFontFaceImplDestroy;

  blNullFontFaceFuncs.mapTextToGlyphs = blNullFontFaceMapTextToGlyphs;
  blNullFontFaceFuncs.getGlyphBounds = blNullFontFaceGetGlyphBounds;
  blNullFontFaceFuncs.getGlyphAdvances = blNullFontFaceGetGlyphAdvances;
  blNullFontFaceFuncs.getGlyphOutlines = blNullFontFaceGetGlyphOutlines;
  blNullFontFaceFuncs.applyKern = blNullFontFaceApplyKern;
  blNullFontFaceFuncs.applyGSub = blNullFontFaceApplyGSub;
  blNullFontFaceFuncs.applyGPos = blNullFontFaceApplyGPos;
  blNullFontFaceFuncs.positionGlyphs = blNullFontFacePositionGlyphs;

  // Initialize BLFontFace built-in null instance.
  BLInternalFontFaceImpl* fontFaceI = &blNullFontFaceImpl;
  blInitBuiltInNull(fontFaceI, BL_IMPL_TYPE_FONT_FACE, BL_IMPL_TRAIT_VIRT);
  fontFaceI->virt = &blNullFontFaceVirt;
  fontFaceI->data.impl = fontDataI;
  blCallCtor(fontFaceI->fullName);
  blCallCtor(fontFaceI->familyName);
  blCallCtor(fontFaceI->subfamilyName);
  blCallCtor(fontFaceI->postScriptName);
  fontFaceI->funcs = blNullFontFaceFuncs;
  blAssignBuiltInNull(fontFaceI);

  // Initialize BLFont built-in null instance.
  BLInternalFontImpl* fontI = &blNullFontImpl;
  blInitBuiltInNull(fontI, BL_IMPL_TYPE_FONT, 0);
  fontI->face.impl = fontFaceI;
  blCallCtor(fontI->features);
  blCallCtor(fontI->variations);
  blAssignBuiltInNull(fontI);

  // Initialize OpenType implementation.
  blOTFaceImplRtInit(rt);
}
