// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "./blapi-build_p.h"
#include "./blarray_p.h"
#include "./blglyphbuffer_p.h"
#include "./blfilesystem.h"
#include "./blfont_p.h"
#include "./blmatrix.h"
#include "./blpath.h"
#include "./blruntime_p.h"
#include "./blstring_p.h"
#include "./blsupport_p.h"
#include "./blthreading_p.h"
#include "./blunicode_p.h"
#include "./opentype/blotcore_p.h"
#include "./opentype/blotface_p.h"

// ============================================================================
// [Global Variables]
// ============================================================================

BLInternalFontFaceFuncs blNullFontFaceFuncs;

static BLWrap<BLInternalFontImpl> blNullFontImpl;
static BLWrap<BLInternalFontFaceImpl> blNullFontFaceImpl;
static BLWrap<BLFontDataImpl> blNullFontDataImpl;
static BLWrap<BLFontLoaderImpl> blNullFontLoaderImpl;

static BLFontFaceVirt blNullFontFaceVirt;
static BLAtomicUInt64Generator blFontFaceIdGenerator;

// ============================================================================
// [BLFontData / BLFontLoader - Null]
// ============================================================================

BL_DIAGNOSTIC_PUSH(BL_DIAGNOSTIC_NO_UNUSED_PARAMETERS)

static BLFontDataVirt blNullFontDataVirt;
static BLFontLoaderVirt blNullFontLoaderVirt;

static BLResult BL_CDECL blNullFontDataImplDestroy(BLFontDataImpl* impl) noexcept {
  return BL_SUCCESS;
}

static BLResult BL_CDECL blNullFontDataImplListTags(const BLFontDataImpl* impl, BLArrayCore* out) noexcept {
  return blArrayClear(out);
}

static size_t BL_CDECL blNullFontDataImplQueryTables(const BLFontDataImpl* impl, BLFontTable* dst, const BLTag* tags, size_t n) noexcept {
  for (size_t i = 0; i < n; i++)
    dst[i].reset();
  return 0;
}

static BLResult BL_CDECL blNullFontLoaderImplDestroy(BLFontLoaderImpl* impl) noexcept {
  return BL_SUCCESS;
}

static BLFontDataImpl* BL_CDECL blNullFontLoaderImplDataByFaceIndex(BLFontLoaderImpl* impl, uint32_t faceIndex) noexcept {
  return &blNullFontDataImpl;
}

BL_DIAGNOSTIC_POP

// ============================================================================
// [BLFontData / BLFontLoader - Utilities]
// ============================================================================

static BL_INLINE bool isOpenTypeVersionTag(uint32_t tag) noexcept {
  return tag == BL_MAKE_TAG('O', 'T', 'T', 'O') ||
         tag == BL_MAKE_TAG( 0,   1 ,  0 ,  0 ) ||
         tag == BL_MAKE_TAG('t', 'r', 'u', 'e') ;
}

// A callback that we use to destroy an array-impl we keep if `BLMemFontLoaderImpl`
// was created from `BLArray<uint8_t>()`.
static void BL_CDECL blDestroyArrayImpl(void* impl, void* arrayI) noexcept {
  blArrayImplRelease(static_cast<BLArrayImpl*>(arrayI));
}

// ============================================================================
// [BLFontData / BLFontLoader - Memory]
// ============================================================================

// Users can pass their own buffer with a destroy function that gets called when
// the `BLMemFontLoaderImpl` gets destroyed. However, the impl stores an array
// of `BLFontData` where each of them is implemented by `BLMemFontDataImpl` and
// stores a back-reference to the loader. So how to avoid a circular dependency
// that would prevent the destruction of the loader? We simply add an another
// reference count to the loader, which counts how many `BLMemFontDataImpl`
// instances back-reference it.
//
// The loader destructor is not a real destructor and it can be considered an
// interceptor instead. It intercepts the destroy call that is caused by the
// reference-count going to zero. When this happens we destroy all data, which
// would call a real-destructor when the `backRefCount` goes to zero. We take
// advantage of the fact that BLMemFontLoaderImpl's destroy function will be
// called always before its data is destroyed as `BLArray<BLFontData>` holds
// it.

static BLFontDataVirt blMemFontDataVirt;
static BLFontLoaderVirt blMemFontLoaderVirt;

struct BLMemFontLoaderImpl : public BLFontLoaderImpl {
  BLArray<BLFontData> dataArray;
  volatile size_t backRefCount;
};

struct BLMemFontDataImpl : public BLFontDataImpl {
  BLMemFontLoaderImpl* loaderI;
};

// Destroys `BLMemFontLoaderImpl` - this is a real destructor that would
// free the impl data.
static BLResult blMemFontLoaderImplRealDestroy(BLMemFontLoaderImpl* impl) noexcept {
  uint8_t* implBase = reinterpret_cast<uint8_t*>(impl);
  size_t implSize = sizeof(BLMemFontLoaderImpl);
  uint32_t implTraits = impl->implTraits;
  uint32_t memPoolData = impl->memPoolData;

  if (implTraits & BL_IMPL_TRAIT_EXTERNAL) {
    implSize += sizeof(BLExternalImplPreface);
    implBase -= sizeof(BLExternalImplPreface);
    blImplDestroyExternal(impl);
  }

  return blRuntimeFreeImpl(implBase, implSize, memPoolData);
}

// A fake `BLMemFontLoaderImpl` destructor that just intercepts when the loader
// reference-count gets to zero. This resets the data-array and would destroy
// all BLMemFontDataImpl's it holds. If user doesn't hold an of them then this
// would automatically call the real destructor.
static BLResult BL_CDECL blMemFontLoaderImplFakeDestroy(BLFontLoaderImpl* impl_) noexcept {
  BLMemFontLoaderImpl* impl = static_cast<BLMemFontLoaderImpl*>(impl_);
  return impl->dataArray.reset();
}

static BLFontDataImpl* BL_CDECL blMemFontLoaderImplDataByFaceIndex(BLFontLoaderImpl* impl_, uint32_t faceIndex) noexcept {
  BLMemFontLoaderImpl* impl = static_cast<BLMemFontLoaderImpl*>(impl_);
  if (faceIndex >= impl->dataArray.size())
    return &blNullFontDataImpl;
  return blImplIncRef(impl->dataArray[faceIndex].impl);
}

static BLResult BL_CDECL blMemFontDataImplDestroy(BLFontDataImpl* impl_) noexcept {
  BLMemFontDataImpl* impl = static_cast<BLMemFontDataImpl*>(impl_);
  uint32_t memPoolData = impl->memPoolData;

  BLMemFontLoaderImpl* loaderI = impl->loaderI;
  blRuntimeFreeImpl(impl, sizeof(BLMemFontDataImpl), memPoolData);

  if (blAtomicFetchDecRef(&loaderI->backRefCount) != 1)
    return BL_SUCCESS;

  return blMemFontLoaderImplRealDestroy(loaderI);
}

static BLResult BL_CDECL blMemFontDataImplListTags(const BLFontDataImpl* impl_, BLArrayCore* out) noexcept {
  using namespace BLOpenType;

  const BLMemFontDataImpl* impl = static_cast<const BLMemFontDataImpl*>(impl_);

  // We can safely multiply `tableCount` as SFNTHeader::numTables is `UInt16`.
  const SFNTHeader* sfnt = static_cast<const SFNTHeader*>(impl->data);
  size_t tableCount = sfnt->numTables();
  size_t minDataSize = sizeof(SFNTHeader) + tableCount * sizeof(SFNTHeader::TableRecord);

  if (BL_UNLIKELY(impl->size < minDataSize)) {
    blArrayClear(out);
    return blTraceError(BL_ERROR_INVALID_DATA);
  }

  uint32_t* dst;
  BL_PROPAGATE(blArrayModifyOp(out, BL_MODIFY_OP_ASSIGN_FIT, tableCount, (void**)&dst));

  const SFNTHeader::TableRecord* tables = sfnt->tableRecords();
  for (size_t tableIndex = 0; tableIndex < tableCount; tableIndex++)
    dst[tableIndex] = tables[tableIndex].tag();
  return BL_SUCCESS;
}

static size_t BL_CDECL blMemFontDataImplQueryTables(const BLFontDataImpl* impl_, BLFontTable* dst, const BLTag* tags, size_t n) noexcept {
  using namespace BLOpenType;

  const BLMemFontDataImpl* impl = static_cast<const BLMemFontDataImpl*>(impl_);

  const void* data = impl->data;
  size_t dataSize = impl->size;

  // We can safely multiply `tableCount` as SFNTHeader::numTables is `UInt16`.
  const SFNTHeader* sfnt = static_cast<const SFNTHeader*>(data);
  size_t tableCount = sfnt->numTables();
  size_t minDataSize = sizeof(SFNTHeader) + tableCount * sizeof(SFNTHeader::TableRecord);

  if (BL_UNLIKELY(dataSize < minDataSize)) {
    memset(dst, 0, n * sizeof(BLFontTable));
    return 0;
  }

  size_t matchCount = 0;
  const SFNTHeader::TableRecord* tables = sfnt->tableRecords();

  // Iterate over all tables and try to find all tables as specified by `tags`.
  for (size_t tagIndex = 0; tagIndex < n; tagIndex++) {
    uint32_t tag = blByteSwap32BE(tags[tagIndex]);
    dst[tagIndex].reset();

    for (size_t tableIndex = 0; tableIndex < tableCount; tableIndex++) {
      const SFNTHeader::TableRecord& table = tables[tableIndex];

      if (table.tag.rawValue() == tag) {
        uint32_t tableOffset = table.offset();
        uint32_t tableSize = table.length();

        if (tableOffset < dataSize && tableSize && tableSize <= dataSize - tableOffset) {
          matchCount++;
          dst[tagIndex].data = blOffsetPtr<uint8_t>(data, tableOffset);
          dst[tagIndex].size = tableSize;
        }

        break;
      }
    }
  }

  return matchCount;
}

// ============================================================================
// [BLFontData]
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

bool blFontDataEquals(const BLFontDataCore* a, const BLFontDataCore* b) noexcept {
  return a->impl == b->impl;
}

BLResult blFontDataListTags(const BLFontDataCore* self, BLArrayCore* dst) noexcept {
  BLFontDataImpl* selfI = self->impl;
  return selfI->virt->listTags(selfI, dst);
}

size_t blFontDataQueryTables(const BLFontDataCore* self, BLFontTable* dst, const BLTag* tags, size_t count) noexcept {
  BLFontDataImpl* selfI = self->impl;
  return selfI->virt->queryTables(selfI, dst, tags, count);
}

// ============================================================================
// [BLFontLoader - Init / Reset]
// ============================================================================

BLResult blFontLoaderInit(BLFontLoaderCore* self) noexcept {
  self->impl = &blNullFontLoaderImpl;
  return BL_SUCCESS;
}

BLResult blFontLoaderReset(BLFontLoaderCore* self) noexcept {
  BLFontLoaderImpl* selfI = self->impl;
  self->impl = &blNullFontLoaderImpl;
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLFontLoader - Assign]
// ============================================================================

BLResult blFontLoaderAssignMove(BLFontLoaderCore* self, BLFontLoaderCore* other) noexcept {
  BLFontLoaderImpl* selfI = self->impl;
  BLFontLoaderImpl* otherI = other->impl;

  self->impl = otherI;
  other->impl = &blNullFontLoaderImpl;

  return blImplReleaseVirt(selfI);
}

BLResult blFontLoaderAssignWeak(BLFontLoaderCore* self, const BLFontLoaderCore* other) noexcept {
  BLFontLoaderImpl* selfI = self->impl;
  BLFontLoaderImpl* otherI = other->impl;

  self->impl = blImplIncRef(otherI);
  return blImplReleaseVirt(selfI);
}

// ============================================================================
// [BLFontLoader - Equals]
// ============================================================================

bool blFontLoaderEquals(const BLFontLoaderCore* a, const BLFontLoaderCore* b) noexcept {
  return a->impl == b->impl;
}

// ============================================================================
// [BLFontLoader - Create]
// ============================================================================

BLResult blFontLoaderCreateFromFile(BLFontLoaderCore* self, const char* fileName) noexcept {
  BLArray<uint8_t> buffer;
  BL_PROPAGATE(BLFileSystem::readFile(fileName, buffer));
  return blFontLoaderCreateFromDataArray(self, &buffer);
}

BLResult blFontLoaderCreateFromDataArray(BLFontLoaderCore* self, const BLArrayCore* dataArray) noexcept {
  BLArrayImpl* arrI = dataArray->impl;
  BLResult result = blFontLoaderCreateFromData(self, arrI->data, arrI->size * arrI->itemSize, blDestroyArrayImpl, arrI);

  if (result == BL_SUCCESS)
    blImplIncRef(arrI);
  return result;
}

BLResult blFontLoaderCreateFromData(BLFontLoaderCore* self, const void* data, size_t size, BLDestroyImplFunc destroyFunc, void* destroyData) noexcept {
  using namespace BLOpenType;

  constexpr uint32_t kMinSize = blMin<uint32_t>(SFNTHeader::kMinSize, TTCFHeader::kMinSize);
  if (BL_UNLIKELY(size < kMinSize))
    return blTraceError(BL_ERROR_INVALID_DATA);

  uint32_t headerTag = blOffsetPtr<const UInt32>(data, 0)->value();
  uint32_t faceCount = 1;
  uint32_t loaderFlags = 0;

  const UInt32* offsetArray = nullptr;
  if (headerTag == BL_MAKE_TAG('t', 't', 'c', 'f')) {
    if (BL_UNLIKELY(size < TTCFHeader::kMinSize))
      return blTraceError(BL_ERROR_INVALID_DATA);

    const TTCFHeader* header = blOffsetPtr<const TTCFHeader>(data, 0);

    faceCount = header->fonts.count();
    if (BL_UNLIKELY(!faceCount || faceCount > BL_FONT_LOADER_MAX_FACE_COUNT))
      return blTraceError(BL_ERROR_INVALID_DATA);

    size_t ttcHeaderSize = header->calcSize(faceCount);
    if (BL_UNLIKELY(ttcHeaderSize < size))
      return blTraceError(BL_ERROR_INVALID_DATA);

    offsetArray = header->fonts.array();
    loaderFlags |= BL_FONT_LOADER_FLAG_COLLECTION;
  }
  else {
    if (!isOpenTypeVersionTag(headerTag))
      return blTraceError(BL_ERROR_INVALID_SIGNATURE);
  }

  uint16_t memPoolData;
  uint32_t faceIndex;

  BLArray<BLFontData> fontDataArray;
  BL_PROPAGATE(fontDataArray.reserve(faceCount));

  for (faceIndex = 0; faceIndex < faceCount; faceIndex++) {
    uint32_t faceOffset = 0;
    if (offsetArray)
      faceOffset = offsetArray[faceIndex].value();

    if (BL_UNLIKELY(faceOffset >= size))
      return blTraceError(BL_ERROR_INVALID_DATA);

    size_t faceDataSize = size - faceOffset;
    if (BL_UNLIKELY(faceDataSize < SFNTHeader::kMinSize))
      return blTraceError(BL_ERROR_INVALID_DATA);

    const SFNTHeader* sfnt = blOffsetPtr<const SFNTHeader>(data, faceOffset);
    uint32_t versionTag = sfnt->versionTag();
    uint32_t tableCount = sfnt->numTables();

    if (!isOpenTypeVersionTag(versionTag))
      return blTraceError(BL_ERROR_INVALID_DATA);

    if (faceDataSize < sizeof(SFNTHeader) + tableCount * sizeof(SFNTHeader::TableRecord))
      return blTraceError(BL_ERROR_INVALID_DATA);

    BLMemFontDataImpl* fontDataI = blRuntimeAllocImplT<BLMemFontDataImpl>(sizeof(BLMemFontDataImpl), &memPoolData);
    if (BL_UNLIKELY(!fontDataI))
      return blTraceError(BL_ERROR_OUT_OF_MEMORY);

    blImplInit(fontDataI, BL_IMPL_TYPE_FONT_DATA, BL_IMPL_TRAIT_VIRT, memPoolData);
    fontDataI->virt = &blMemFontDataVirt;
    fontDataI->data = const_cast<void*>(static_cast<const void*>(sfnt));
    fontDataI->size = size - faceOffset;
    fontDataI->flags = 0;
    fontDataI->loaderI = nullptr;

    // Cannot fail as we reserved enough space for data of all font-faces.
    fontDataArray.append(BLFontData(fontDataI));
  }

  // Finally - allocate the BLMemFontLoaderImpl and assign `fontDataArray` to it.
  size_t loaderSize = sizeof(BLMemFontLoaderImpl);
  uint32_t loaderTraits = BL_IMPL_TRAIT_VIRT;

  if (destroyFunc) {
    loaderSize += sizeof(BLExternalImplPreface);
    loaderTraits |= BL_IMPL_TRAIT_EXTERNAL;
  }

  BLMemFontLoaderImpl* loaderI = blRuntimeAllocImplT<BLMemFontLoaderImpl>(loaderSize, &memPoolData);
  if (BL_UNLIKELY(!loaderI))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  if (destroyFunc)
    loaderI = blImplInitExternal(loaderI, destroyFunc, destroyData);

  blImplInit(loaderI, BL_IMPL_TYPE_FONT_LOADER, loaderTraits, memPoolData);
  loaderI->virt = &blMemFontLoaderVirt;
  loaderI->data = const_cast<void*>(static_cast<const void*>(data));;
  loaderI->size = size;
  loaderI->faceType = uint8_t(BL_FONT_FACE_TYPE_OPENTYPE);
  loaderI->faceCount = faceCount;
  loaderI->loaderFlags = loaderFlags;
  loaderI->dataArray.impl = blImplIncRef(fontDataArray.impl);
  loaderI->backRefCount = faceCount;

  // Now fix all `BLMemFontDataImpl` instances to point to the newly created loader.
  for (faceIndex = 0; faceIndex < faceCount; faceIndex++)
    static_cast<BLMemFontDataImpl*>(fontDataArray[faceIndex].impl)->loaderI = loaderI;

  BLFontLoaderImpl* oldI = self->impl;
  self->impl = loaderI;
  return blImplReleaseVirt(oldI);
};

// ============================================================================
// [BLFontLoader - DataByFaceIndex]
// ============================================================================

BLFontDataImpl* blFontLoaderDataByFaceIndex(BLFontLoaderCore* self, uint32_t faceIndex) noexcept {
  BLFontLoaderImpl* selfI = self->impl;
  return selfI->virt->dataByFaceIndex(selfI, faceIndex);
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

static BLResult BL_CDECL blNullFontFaceApplyKern(
  const BLFontFaceImpl* faceI,
  BLGlyphItem* itemData,
  BLGlyphPlacement* placementData,
  size_t count) noexcept {

  return blTraceError(BL_ERROR_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceApplyGSub(
  const BLFontFaceImpl* impl,
  BLGlyphBuffer* buf,
  size_t index,
  BLBitWord lookups) noexcept {

  return blTraceError(BL_ERROR_NOT_INITIALIZED);
}

static BLResult BL_CDECL blNullFontFaceApplyGPos(
  const BLFontFaceImpl* impl,
  BLGlyphBuffer* buf,
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

static BLResult BL_CDECL blNullFontFaceDecodeGlyph(
  const BLFontFaceImpl* impl,
  uint32_t glyphId,
  const BLMatrix2D* userMatrix,
  BLPath* out,
  BLMemBuffer* tmpBuffer,
  BLPathSinkFunc sink, size_t sinkGlyphIndex, void* closure) noexcept {

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

BLResult blFontFaceCreateFromFile(BLFontFaceCore* self, const char* fileName) noexcept {
  BLFontLoader loader;
  BL_PROPAGATE(loader.createFromFile(fileName));
  return blFontFaceCreateFromLoader(self, &loader, 0);
}

BLResult blFontFaceCreateFromLoader(BLFontFaceCore* self, const BLFontLoaderCore* loader, uint32_t faceIndex) noexcept {
  if (BL_UNLIKELY(blDownCast(loader)->isNone()))
    return blTraceError(BL_ERROR_NOT_INITIALIZED);

  if (BL_UNLIKELY(faceIndex >= loader->impl->faceCount))
    return blTraceError(BL_ERROR_INVALID_VALUE);

  BLFontData fontData = blDownCast(loader)->dataByFaceIndex(faceIndex);
  if (BL_UNLIKELY(fontData.empty()))
    return blTraceError(BL_ERROR_OUT_OF_MEMORY);

  BLOTFaceImpl* newI;
  BL_PROPAGATE(blOTFaceImplNew(&newI, static_cast<const BLFontLoader*>(loader), &fontData, faceIndex));
  newI->faceUniqueId = blFontFaceIdGenerator.next();

  BLInternalFontFaceImpl* oldI = blInternalCast(self->impl);
  self->impl = newI;
  return blImplReleaseVirt(oldI);
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

  blImplInit(impl, BL_IMPL_TYPE_FONT, 0, memPoolData);
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
  if (blAtomicFetchDecRef(&impl->refCount) != 1)
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
// [BLFont - Shaping]
// ============================================================================

BLResult blFontShape(const BLFontCore* self, BLGlyphBufferCore* buf) noexcept {
  BL_PROPAGATE(blFontMapTextToGlyphs(self, buf, nullptr));
  BL_PROPAGATE(blFontPositionGlyphs(self, buf, 0xFFFFFFFFu));

  return BL_SUCCESS;
}

BLResult blFontMapTextToGlyphs(const BLFontCore* self, BLGlyphBufferCore* buf, BLGlyphMappingState* stateOut) noexcept {
  BLInternalGlyphBufferData* gbd = blInternalCast(buf->data);
  if (!gbd->size)
    return BL_SUCCESS;

  if (BL_UNLIKELY(!(gbd->flags & BL_GLYPH_RUN_FLAG_UCS4_CONTENT)))
    return blTraceError(BL_ERROR_INVALID_STATE);

  BLGlyphMappingState state;
  if (!stateOut)
    stateOut = &state;

  BLInternalFontFaceImpl* faceI = blInternalCast(self->impl->face.impl);
  BL_PROPAGATE(faceI->funcs.mapTextToGlyphs(faceI, gbd->glyphItemData, gbd->size, stateOut));

  gbd->flags = gbd->flags & ~BL_GLYPH_RUN_FLAG_UCS4_CONTENT;
  if (stateOut->undefinedCount == 0)
    gbd->flags |= BL_GLYPH_RUN_FLAG_UNDEFINED_GLYPHS;
  return BL_SUCCESS;
}

BLResult blFontPositionGlyphs(const BLFontCore* self, BLGlyphBufferCore* buf, uint32_t positioningFlags) noexcept {
  BLInternalGlyphBufferData* gbd = blInternalCast(buf->data);
  if (!gbd->size)
    return BL_SUCCESS;

  if (BL_UNLIKELY(gbd->flags & BL_GLYPH_RUN_FLAG_UCS4_CONTENT))
    return blTraceError(BL_ERROR_INVALID_STATE);

  BLInternalFontFaceImpl* faceI = blInternalCast(self->impl->face.impl);
  if (!(gbd->flags & BL_GLYPH_BUFFER_GLYPH_ADVANCES)) {
    BL_PROPAGATE(gbd->ensurePlacement());
    faceI->funcs.getGlyphAdvances(faceI, &gbd->glyphItemData->glyphId, sizeof(BLGlyphItem), gbd->placementData, gbd->size);
    gbd->glyphRun.placementType = uint8_t(BL_GLYPH_PLACEMENT_TYPE_ADVANCE_OFFSET);
    gbd->flags |= BL_GLYPH_BUFFER_GLYPH_ADVANCES;
  }

  if (positioningFlags) {
    faceI->funcs.applyKern(faceI, gbd->glyphItemData, gbd->placementData, gbd->size);
  }

  return BL_SUCCESS;
}

BLResult blFontApplyKerning(const BLFontCore* self, BLGlyphBufferCore* buf) noexcept {
  BLInternalGlyphBufferData* gbd = blInternalCast(buf->data);
  if (!gbd->size)
    return BL_SUCCESS;

  if (BL_UNLIKELY(!(gbd->placementData)))
    return blTraceError(BL_ERROR_INVALID_STATE);

  BLInternalFontFaceImpl* faceI = blInternalCast(self->impl->face.impl);
  return faceI->funcs.applyKern(faceI, gbd->glyphItemData, gbd->placementData, gbd->size);
}

BLResult blFontApplyGSub(const BLFontCore* self, BLGlyphBufferCore* buf, size_t index, BLBitWord lookups) noexcept {
  BLInternalFontFaceImpl* faceI = blInternalCast(self->impl->face.impl);
  return faceI->funcs.applyGSub(faceI, static_cast<BLGlyphBuffer*>(buf), index, lookups);
}

BLResult blFontApplyGPos(const BLFontCore* self, BLGlyphBufferCore* buf, size_t index, BLBitWord lookups) noexcept {
  BLInternalGlyphBufferData* gbd = blInternalCast(buf->data);
  if (!gbd->size)
    return BL_SUCCESS;

  if (BL_UNLIKELY(!(gbd->placementData)))
    return blTraceError(BL_ERROR_INVALID_STATE);

  BLInternalFontFaceImpl* faceI = blInternalCast(self->impl->face.impl);
  return faceI->funcs.applyGPos(faceI, static_cast<BLGlyphBuffer*>(buf), index, lookups);
}

BLResult blFontGetTextMetrics(const BLFontCore* self, BLGlyphBufferCore* buf, BLTextMetrics* out) noexcept {
  BLInternalGlyphBufferData* gbd = blInternalCast(buf->data);
  out->reset();

  if (!(gbd->flags & BL_GLYPH_BUFFER_GLYPH_ADVANCES)) {
    BL_PROPAGATE(blFontShape(self, buf));
    gbd = blInternalCast(buf->data);
  }

  size_t size = gbd->size;
  if (!size)
    return BL_SUCCESS;

  double advanceX = 0.0;
  double advanceY = 0.0;

  const BLGlyphItem* glyphItemData = gbd->glyphItemData;
  const BLGlyphPlacement* placementData = gbd->placementData;

  for (size_t i = 0; i < size; i++) {
    advanceX += double(placementData[i].advance.x);
    advanceY += double(placementData[i].advance.y);
  }

  BLBoxI glyphBounds[2];
  BLGlyphId borderGlyphs[2] = { BLGlyphId(glyphItemData[0].glyphId), BLGlyphId(glyphItemData[size - 1].glyphId) };

  BL_PROPAGATE(blFontGetGlyphBounds(self, borderGlyphs, 2, glyphBounds, 2));
  out->advance.reset(advanceX, advanceY);
  out->boundingBox.reset(glyphBounds[0].x0, 0.0, advanceX - placementData[size - 1].advance.x + glyphBounds[1].x1, 0.0);
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

BLResult blFontGetGlyphOutlines(const BLFontCore* self, uint32_t glyphId, const BLMatrix2D* userMatrix, BLPathCore* out, BLPathSinkFunc sink, void* closure) noexcept {
  BLMatrix2D finalMatrix;
  const BLFontMatrix& fMat = self->impl->matrix;

  if (userMatrix)
    blFontMatrixMultiply(&finalMatrix, &fMat, userMatrix);
  else
    finalMatrix.reset(fMat.m00, fMat.m01, fMat.m10, fMat.m11, 0.0, 0.0);

  const BLInternalFontFaceImpl* faceI = blInternalCast(self->impl->face.impl);

  BLMemBufferTmp<BL_FONT_GET_GLYPH_OUTLINE_BUFFER_SIZE> tmpBuffer;
  return faceI->funcs.decodeGlyph(faceI, glyphId, &finalMatrix, static_cast<BLPath*>(out), &tmpBuffer, sink, 0, closure);
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

  BLResult result = BL_SUCCESS;
  uint32_t placementType = glyphRun->placementType;

  BLMemBufferTmp<BL_FONT_GET_GLYPH_OUTLINE_BUFFER_SIZE> tmpBuffer;
  BLGlyphRunIterator it(*glyphRun);

  auto decodeFunc = faceI->funcs.decodeGlyph;
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

        result = decodeFunc(faceI, it.glyphId(), &finalMatrix, static_cast<BLPath*>(out), &tmpBuffer, sink, it.index, closure);
        if (BL_UNLIKELY(result != BL_SUCCESS))
          break;

        px = pos.advance.x;
        py = pos.advance.y;
        ox += px * offsetMatrix.m00 + py * offsetMatrix.m10;
        oy += px * offsetMatrix.m01 + py * offsetMatrix.m11;

        it.advance();
      }
    }
    else {
      while (!it.atEnd()) {
        const BLPoint& placement = it.placement<BLPoint>();
        finalMatrix.m20 = placement.x * offsetMatrix.m00 + placement.y * offsetMatrix.m10 + offsetMatrix.m20;
        finalMatrix.m21 = placement.x * offsetMatrix.m01 + placement.y * offsetMatrix.m11 + offsetMatrix.m21;

        result = decodeFunc(faceI, it.glyphId(), &finalMatrix, static_cast<BLPath*>(out), &tmpBuffer, sink, it.index, closure);
        if (BL_UNLIKELY(result != BL_SUCCESS))
          break;

        it.advance();
      }
    }
  }
  else {
    while (!it.atEnd()) {
      result = decodeFunc(faceI, it.glyphId(), &finalMatrix, static_cast<BLPath*>(out), &tmpBuffer, sink, it.index, closure);
      if (BL_UNLIKELY(result != BL_SUCCESS))
        break;
      it.advance();
    }
  }

  return result;
}

// ============================================================================
// [Runtime Init]
// ============================================================================

void blFontRtInit(BLRuntimeContext* rt) noexcept {
  BL_UNUSED(rt);

  // Initialize BLFontData virtual functions.
  blNullFontDataVirt.destroy = blNullFontDataImplDestroy;
  blNullFontDataVirt.listTags = blNullFontDataImplListTags;
  blNullFontDataVirt.queryTables = blNullFontDataImplQueryTables;

  blMemFontDataVirt.destroy = blMemFontDataImplDestroy;
  blMemFontDataVirt.listTags = blMemFontDataImplListTags;
  blMemFontDataVirt.queryTables = blMemFontDataImplQueryTables;

  // Initialize BLFontData built-in null instance.
  BLFontDataImpl* fontDataI = &blNullFontDataImpl;
  fontDataI->implType = uint8_t(BL_IMPL_TYPE_FONT_DATA);
  fontDataI->implTraits = uint8_t(BL_IMPL_TRAIT_NULL | BL_IMPL_TRAIT_VIRT);
  fontDataI->virt = &blNullFontDataVirt;
  blAssignBuiltInNull(fontDataI);

  // Initialize BLFontLoader virtual functions.
  blNullFontLoaderVirt.destroy = blNullFontLoaderImplDestroy;
  blNullFontLoaderVirt.dataByFaceIndex = blNullFontLoaderImplDataByFaceIndex;

  blMemFontLoaderVirt.destroy = blMemFontLoaderImplFakeDestroy;
  blMemFontLoaderVirt.dataByFaceIndex = blMemFontLoaderImplDataByFaceIndex;

  // Initialize BLFontLoader built-in null instance.
  BLFontLoaderImpl* fontLoaderI = &blNullFontLoaderImpl;
  fontLoaderI->implType = uint8_t(BL_IMPL_TYPE_FONT_LOADER);
  fontLoaderI->implTraits = uint8_t(BL_IMPL_TRAIT_NULL | BL_IMPL_TRAIT_VIRT);
  fontLoaderI->virt = &blNullFontLoaderVirt;
  blAssignBuiltInNull(fontLoaderI);

  // Initialize BLFontFace virtual functions.
  blNullFontFaceVirt.destroy = blNullFontFaceImplDestroy;

  blNullFontFaceFuncs.mapTextToGlyphs = blNullFontFaceMapTextToGlyphs;
  blNullFontFaceFuncs.getGlyphBounds = blNullFontFaceGetGlyphBounds;
  blNullFontFaceFuncs.getGlyphAdvances = blNullFontFaceGetGlyphAdvances;
  blNullFontFaceFuncs.applyKern = blNullFontFaceApplyKern;
  blNullFontFaceFuncs.applyGSub = blNullFontFaceApplyGSub;
  blNullFontFaceFuncs.applyGPos = blNullFontFaceApplyGPos;
  blNullFontFaceFuncs.positionGlyphs = blNullFontFacePositionGlyphs;
  blNullFontFaceFuncs.decodeGlyph = blNullFontFaceDecodeGlyph;

  // Initialize BLFontFace built-in null instance.
  BLInternalFontFaceImpl* fontFaceI = &blNullFontFaceImpl;
  fontFaceI->implType = uint8_t(BL_IMPL_TYPE_FONT_FACE);
  fontFaceI->implTraits = uint8_t(BL_IMPL_TRAIT_NULL | BL_IMPL_TRAIT_VIRT);
  fontFaceI->virt = &blNullFontFaceVirt;
  fontFaceI->data.impl = fontDataI;
  fontFaceI->loader.impl = fontLoaderI;
  blCallCtor(fontFaceI->fullName);
  blCallCtor(fontFaceI->familyName);
  blCallCtor(fontFaceI->subfamilyName);
  blCallCtor(fontFaceI->postScriptName);
  fontFaceI->funcs = blNullFontFaceFuncs;
  blAssignBuiltInNull(fontFaceI);

  // Initialize BLFont built-in null instance.
  BLInternalFontImpl* fontI = &blNullFontImpl;
  fontI->implType = uint8_t(BL_IMPL_TYPE_FONT);
  fontI->implTraits = uint8_t(BL_IMPL_TRAIT_NULL);
  fontI->face.impl = fontFaceI;
  blCallCtor(fontI->features);
  blCallCtor(fontI->variations);
  blAssignBuiltInNull(fontI);

  // Initialize implementations.
  blOTFaceImplRtInit(rt);
}
