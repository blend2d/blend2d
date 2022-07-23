// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_RASTER_RENDERJOB_P_H_INCLUDED
#define BLEND2D_RASTER_RENDERJOB_P_H_INCLUDED

#include "../geometry_p.h"
#include "../pipeline/pipedefs_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rendercommand_p.h"
#include "../raster/rasterdefs_p.h"
#include "../raster/statedata_p.h"
#include "../raster/styledata_p.h"
#include "../support/arenaallocator_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_raster_engine_impl
//! \{

namespace BLRasterEngine {

//! Render job.
struct RenderJob {
  //! Job type.
  enum Type : uint8_t {
    kTypeNone = 0,

    kTypeFillGeometry = 1,
    kTypeFillText = 2,

    kTypeStrokeGeometry = 3,
    kTypeStrokeText = 4,

    kTypeCount = 5
  };

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

  uint8_t _jobType;
  uint8_t _payloadType;
  uint8_t _metaMatrixFixedType;
  uint8_t _finalMatrixFixedType;
  uint8_t _reserved[4];
  RenderCommand* _command;

  //! \}

  //! \name Initialization
  //! \{

  BL_INLINE void _initInternal(uint32_t jobType, RenderCommand* command) noexcept {
    BL_ASSERT(jobType < kTypeCount);
    _jobType = uint8_t(jobType);
    _payloadType = 0;
    _metaMatrixFixedType = 0;
    _finalMatrixFixedType = 0;
    _command = command;
  }

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE uint32_t jobType() const noexcept { return _jobType; }
  BL_INLINE RenderCommand* command() const noexcept { return _command; }

  //! \}
};

//! Base class for fill and stroke operations responsible for holding shared
//! states.
struct RenderJob_BaseOp : public RenderJob {
  const SharedFillState* _sharedFillState;
  const SharedBaseStrokeState* _sharedStrokeState;

  BL_INLINE void initStates(const SharedFillState* sharedFillState,
                            const SharedBaseStrokeState* sharedStrokeState = nullptr) noexcept {
    _sharedFillState = sharedFillState;
    _sharedStrokeState = sharedStrokeState;
  }

  BL_INLINE const SharedFillState* fillState() const noexcept { return _sharedFillState; }
  BL_INLINE const SharedBaseStrokeState* strokeState() const noexcept { return _sharedStrokeState; }

  BL_INLINE uint32_t metaMatrixFixedType() const noexcept { return _metaMatrixFixedType; }
  BL_INLINE uint32_t finalMatrixFixedType() const noexcept { return _finalMatrixFixedType; }

  BL_INLINE void setMetaMatrixFixedType(uint32_t type) noexcept { _metaMatrixFixedType = uint8_t(type); }
  BL_INLINE void setFinalMatrixFixedType(uint32_t type) noexcept { _finalMatrixFixedType = uint8_t(type); }
};

struct RenderJob_GeometryOp : public RenderJob_BaseOp {
  BL_INLINE void initFillJob(RenderCommand* commandData) noexcept {
    _initInternal(kTypeFillGeometry, commandData);
  }

  BL_INLINE void initStrokeJob(RenderCommand* commandData) noexcept {
    _initInternal(kTypeStrokeGeometry, commandData);
  }

  BL_INLINE BLGeometryType geometryType() const noexcept { return (BLGeometryType)_payloadType; }

  BL_INLINE void setGeometry(BLGeometryType geometryType, const void* srcDataPtr, size_t srcDataSize) noexcept {
    _payloadType = uint8_t(geometryType);

    switch (geometryType) {
      case BL_GEOMETRY_TYPE_PATH:
        blObjectPrivateInitWeakTagged(geometryData<BLPathCore>(), static_cast<const BLPathCore*>(srcDataPtr));
        break;

      default:
        memcpy(geometryData<void>(), srcDataPtr, srcDataSize);
        break;
    }
  }

  template<typename T>
  BL_INLINE T* geometryData() noexcept { return reinterpret_cast<T*>(this + 1); }

  template<typename T>
  BL_INLINE const T* geometryData() const noexcept { return reinterpret_cast<const T*>(this + 1); }
};

struct RenderJob_TextOp : public RenderJob_BaseOp {
  BLPoint _pt;
  BLFontCore _font;

  union {
    BLArrayView<uint8_t> _textData;
    BLGlyphRun _glyphRun;
    BLGlyphBufferCore _glyphBuffer;
  };

  BL_INLINE void initFillJob(RenderCommand* commandData) noexcept {
    _initInternal(kTypeFillText, commandData);
  }

  BL_INLINE void initStrokeJob(RenderCommand* commandData) noexcept {
    _initInternal(kTypeStrokeText, commandData);
  }

  BL_INLINE void destroy() noexcept {
    _font.dcast().~BLFont();
    if (_payloadType == kTextDataGlyphBuffer)
      _glyphBuffer.dcast().~BLGlyphBuffer();
  }

  BL_INLINE void initFont(const BLFontCore& font) noexcept {
    blObjectPrivateInitWeakTagged(&_font, &font);
  }

  BL_INLINE void initCoordinates(const BLPoint& pt) noexcept {
    _pt = pt;
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
    _glyphRun.glyphSize = uint8_t(4);
    _glyphRun.placementType = uint8_t(placementType);
    _glyphRun.glyphAdvance = uint8_t(4);
    _glyphRun.placementAdvance = uint8_t(16);
    _glyphRun.flags = flags;
  }

  BL_INLINE void initGlyphBuffer(BLGlyphBufferImpl* gbI) noexcept {
    _payloadType = kTextDataGlyphBuffer;
    _glyphBuffer.impl = gbI;
  }

  BL_INLINE uint32_t textDataType() const noexcept { return _payloadType; }

  BL_INLINE const void* textData() const noexcept { return _textData.data; }
  BL_INLINE size_t textSize() const noexcept { return _textData.size; }

  BL_INLINE const BLGlyphBuffer& glyphBuffer() const noexcept { return _glyphBuffer.dcast(); }
};

} // {BLRasterEngine}

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RENDERJOB_P_H_INCLUDED
