// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_RENDERJOB_P_H_INCLUDED
#define BLEND2D_RASTER_RENDERJOB_P_H_INCLUDED

#include "../geometry_p.h"
#include "../pipeline/pipedefs_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/renderbatch_p.h"
#include "../raster/rendercommand_p.h"
#include "../raster/rasterdefs_p.h"
#include "../raster/statedata_p.h"
#include "../raster/styledata_p.h"
#include "../support/arenaallocator_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace bl {
namespace RasterEngine {

enum class RenderJobType : uint8_t {
  kNone = 0,

  kFillGeometry = 1,
  kFillText = 2,

  kStrokeGeometry = 3,
  kStrokeText = 4,

  kMaxValue = 4
};

enum class RenderJobFlags : uint8_t {
  kNoFlags = 0u,

  kComputePendingFetchData = 0x01u
};
BL_DEFINE_ENUM_FLAGS(RenderJobFlags)

//! Render job.
struct RenderJob {
  //! Type of the text data stored in `RenderJob_TextOp`.
  enum TextDataType : uint8_t {
    kTextDataRawUTF8 = BL_TEXT_ENCODING_UTF8,
    kTextDataRawUTF16 = BL_TEXT_ENCODING_UTF16,
    kTextDataRawUTF32 = BL_TEXT_ENCODING_UTF32,
    kTextDataRawLatin1 = BL_TEXT_ENCODING_LATIN1,
    kTextDataGlyphRun = 0xFE,
    kTextDataGlyphBuffer = 0xFF
  };


  //! \name Job Data
  //! \{

  RenderJobType _jobType;
  RenderJobFlags _jobFlags;
  uint8_t _payloadType;
  uint8_t _metaTransformFixedType;
  uint8_t _finalTransformFixedType;
  uint8_t _reserved;
  uint16_t _commandIndex;
  BLPoint _originFixed;
  RenderCommandQueue* _commandQueue;

  //! \}

  //! \name Initialization
  //! \{

  BL_INLINE void _initInternal(RenderJobType jobType, RenderCommandQueue* commandQueue, size_t commandIndex) noexcept {
    _jobType = jobType;
    _jobFlags = RenderJobFlags::kNoFlags;
    _payloadType = 0;
    _metaTransformFixedType = 0;
    _finalTransformFixedType = 0;
    _commandIndex = uint16_t(commandIndex);
    _commandQueue = commandQueue;
  }

  BL_INLINE_NODEBUG void setOriginFixed(const BLPoint& pt) noexcept { _originFixed = pt; }

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE_NODEBUG RenderJobType jobType() const noexcept { return _jobType; }

  BL_INLINE_NODEBUG RenderJobFlags jobFlags() const noexcept { return _jobFlags; }
  BL_INLINE_NODEBUG bool hasJobFlag(RenderJobFlags flag) const noexcept { return blTestFlag(_jobFlags, flag); }
  BL_INLINE_NODEBUG void addJobFlags(RenderJobFlags flags) noexcept { _jobFlags |= flags; }

  BL_INLINE_NODEBUG RenderCommandQueue* commandQueue() const noexcept { return _commandQueue; }

  BL_INLINE_NODEBUG size_t commandIndex() const noexcept { return _commandIndex; }
  BL_INLINE_NODEBUG RenderCommand& command() const noexcept { return _commandQueue->at(_commandIndex); }
  BL_INLINE_NODEBUG const BLPoint& originFixed() const noexcept { return _originFixed;}

  //! \}
};

//! Base class for fill and stroke operations responsible for holding shared states.
struct RenderJob_BaseOp : public RenderJob {
  const SharedFillState* _sharedFillState;
  const SharedBaseStrokeState* _sharedStrokeState;

  BL_INLINE void initStates(const SharedFillState* sharedFillState, const SharedBaseStrokeState* sharedStrokeState = nullptr) noexcept {
    _sharedFillState = sharedFillState;
    _sharedStrokeState = sharedStrokeState;
  }

  BL_INLINE_NODEBUG const SharedFillState* fillState() const noexcept { return _sharedFillState; }
  BL_INLINE_NODEBUG const SharedBaseStrokeState* strokeState() const noexcept { return _sharedStrokeState; }

  BL_INLINE_NODEBUG BLTransformType metaTransformFixedType() const noexcept { return BLTransformType(_metaTransformFixedType); }
  BL_INLINE_NODEBUG BLTransformType finalTransformFixedType() const noexcept { return BLTransformType(_finalTransformFixedType); }

  BL_INLINE_NODEBUG void setMetaTransformFixedType(BLTransformType type) noexcept { _metaTransformFixedType = uint8_t(type); }
  BL_INLINE_NODEBUG void setFinalTransformFixedType(BLTransformType type) noexcept { _finalTransformFixedType = uint8_t(type); }
};

struct RenderJob_GeometryOp : public RenderJob_BaseOp {
  BL_INLINE void initFillJob(RenderCommandQueue* commandQueue, size_t commandIndex) noexcept {
    _initInternal(RenderJobType::kFillGeometry, commandQueue, commandIndex);
  }

  BL_INLINE void initStrokeJob(RenderCommandQueue* commandQueue, size_t commandIndex) noexcept {
    _initInternal(RenderJobType::kStrokeGeometry, commandQueue, commandIndex);
  }

  BL_INLINE_NODEBUG BLGeometryType geometryType() const noexcept { return (BLGeometryType)_payloadType; }

  BL_INLINE void setGeometryWithPath(const BLPathCore* path) noexcept {
    _payloadType = uint8_t(BL_GEOMETRY_TYPE_PATH);
    blObjectPrivateInitWeakTagged(geometryData<BLPathCore>(), path);
  }

  BL_INLINE void setGeometryWithShape(BLGeometryType geometryType, const void* srcDataPtr, size_t srcDataSize) noexcept {
    _payloadType = uint8_t(geometryType);
    memcpy(geometryData<void>(), srcDataPtr, srcDataSize);
  }

  BL_INLINE void setGeometry(BLGeometryType geometryType, const void* srcDataPtr, size_t srcDataSize) noexcept {
    switch (geometryType) {
      case BL_GEOMETRY_TYPE_PATH:
        setGeometryWithPath(static_cast<const BLPathCore*>(srcDataPtr));
        break;

      default:
        setGeometryWithShape(geometryType, srcDataPtr, srcDataSize);
        break;
    }
  }

  template<typename T>
  BL_INLINE_NODEBUG T* geometryData() noexcept { return reinterpret_cast<T*>(this + 1); }

  template<typename T>
  BL_INLINE_NODEBUG const T* geometryData() const noexcept { return reinterpret_cast<const T*>(this + 1); }
};

struct RenderJob_TextOp : public RenderJob_BaseOp {
  BLFontCore _font;

  union {
    BLArrayView<uint8_t> _textData;
    BLGlyphRun _glyphRun;
    BLGlyphBufferCore _glyphBuffer;
  };

  BL_INLINE void initFillJob(RenderCommandQueue* commandQueue, size_t commandIndex) noexcept {
    _initInternal(RenderJobType::kFillText, commandQueue, commandIndex);
  }

  BL_INLINE void initStrokeJob(RenderCommandQueue* commandQueue, size_t commandIndex) noexcept {
    _initInternal(RenderJobType::kStrokeText, commandQueue, commandIndex);
  }

  BL_INLINE void destroy() noexcept {
    _font.dcast().~BLFont();
    if (_payloadType == kTextDataGlyphBuffer)
      _glyphBuffer.dcast().~BLGlyphBuffer();
  }

  BL_INLINE void initFont(const BLFontCore& font) noexcept {
    blObjectPrivateInitWeakTagged(&_font, &font);
  }

  BL_INLINE void initTextData(const void* text, size_t size, BLTextEncoding encoding) noexcept {
    _payloadType = uint8_t(encoding);
    _textData.reset(static_cast<const uint8_t*>(text), size);
  }

  BL_INLINE void initGlyphRun(void* glyphData, void* placementData, size_t size, uint32_t placementType, uint32_t flags) noexcept {
    _payloadType = kTextDataGlyphRun;
    _glyphRun.glyphData = glyphData;
    _glyphRun.placementData = placementData;
    _glyphRun.size = size;
    _glyphRun.reserved = uint8_t(0);
    _glyphRun.placementType = uint8_t(placementType);
    _glyphRun.glyphAdvance = uint8_t(4);
    _glyphRun.placementAdvance = uint8_t(16);
    _glyphRun.flags = flags;
  }

  BL_INLINE void initGlyphBuffer(BLGlyphBufferImpl* gbI) noexcept {
    _payloadType = kTextDataGlyphBuffer;
    _glyphBuffer.impl = gbI;
  }

  BL_INLINE_NODEBUG uint32_t textDataType() const noexcept { return _payloadType; }
  BL_INLINE_NODEBUG const void* textData() const noexcept { return _textData.data; }
  BL_INLINE_NODEBUG size_t textSize() const noexcept { return _textData.size; }
  BL_INLINE_NODEBUG const BLGlyphBuffer& glyphBuffer() const noexcept { return _glyphBuffer.dcast(); }
};

} // {RasterEngine}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RENDERJOB_P_H_INCLUDED
