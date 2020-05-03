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

#ifndef BLEND2D_RASTER_RASTERJOB_P_H_INCLUDED
#define BLEND2D_RASTER_RASTERJOB_P_H_INCLUDED

#include "../geometry_p.h"
#include "../pipedefs_p.h"
#include "../zoneallocator_p.h"
#include "../raster/edgebuilder_p.h"
#include "../raster/rastercommand_p.h"
#include "../raster/rastercontextstate_p.h"
#include "../raster/rastercontextstyle_p.h"
#include "../raster/rasterdefs_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_raster
//! \{

// ============================================================================
// [Constants]
// ============================================================================

//! Raster job type.
enum BLRasterJobType : uint8_t {
  BL_RASTER_JOB_TYPE_NONE = 0,
  BL_RASTER_JOB_TYPE_COMPILE_PIPELINE = 1,

  BL_RASTER_JOB_TYPE_FILL_GEOMETRY = 2,
  BL_RASTER_JOB_TYPE_FILL_TEXT = 3,

  BL_RASTER_JOB_TYPE_STROKE_GEOMETRY = 4,
  BL_RASTER_JOB_TYPE_STROKE_TEXT = 5,

  BL_RASTER_JOB_TYPE_COUNT = 6
};

//! Type of the text data stored in `BLRasterJobData_TextOp`.
enum BLRasterJobTextDataType : uint8_t {
  BL_RASTER_JOB_TEXT_DATA_TYPE_RAW_UTF_8 = BL_TEXT_ENCODING_UTF8,
  BL_RASTER_JOB_TEXT_DATA_TYPE_RAW_UTF_16 = BL_TEXT_ENCODING_UTF16,
  BL_RASTER_JOB_TEXT_DATA_TYPE_RAW_UTF_32 = BL_TEXT_ENCODING_UTF32,
  BL_RASTER_JOB_TEXT_DATA_TYPE_RAW_LATIN1 = BL_TEXT_ENCODING_LATIN1,
  BL_RASTER_JOB_TEXT_DATA_TYPE_GLYPH_RUN = 0xFE,
  BL_RASTER_JOB_TEXT_DATA_TYPE_GLYPH_BUFFER = 0xFF
};

// ============================================================================
// [BLRasterJobData]
// ============================================================================

//! Raster job data.
struct BLRasterJobData {
  //! \name Job Data
  //! \{

  uint8_t _jobType;
  uint8_t _payloadType;
  uint8_t _metaMatrixFixedType;
  uint8_t _finalMatrixFixedType;
  uint8_t _reserved[4];

  BLRasterCommand* _commandData;

  //! \}

  //! \name Initialization
  //! \{

  BL_INLINE void _initInternal(uint32_t jobType, BLRasterCommand* commandData) noexcept {
    BL_ASSERT(jobType < BL_RASTER_JOB_TYPE_COUNT);
    _jobType = uint8_t(jobType);
    _payloadType = 0;
    _metaMatrixFixedType = 0;
    _finalMatrixFixedType = 0;
    _commandData = commandData;
  }

  //! \}

  //! \name Accessors
  //! \{

  BL_INLINE uint32_t jobType() const noexcept { return _jobType; }
  BL_INLINE BLRasterCommand* commandData() const noexcept { return _commandData; }

  //! \}
};

// ============================================================================
// [BLRasterJobData_CompilePipeline]
// ============================================================================

// TODO: [Rendering Context] This job is not used at the moment.
struct BLRasterJobData_CompilePipeline : public BLRasterJobData {
  BLPipeSignature _signature;

  BL_INLINE void initCompileJob(uint32_t signature, BLRasterCommand* commandData) noexcept {
    _initInternal(BL_RASTER_JOB_TYPE_COMPILE_PIPELINE, commandData);
    _signature.reset(signature);
  }

  BL_INLINE void reassignCommand(BLRasterCommand* command) noexcept {
    command->_fillPrev = _commandData;
    _commandData = command;
  }

  BL_INLINE const BLPipeSignature& signature() const noexcept { return _signature; }
};

// ============================================================================
// [BLRasterJobData_BaseOp]
// ============================================================================

//! Base class for fill and stroke operations responsible for holding shared
//! states.
struct BLRasterJobData_BaseOp : public BLRasterJobData {
  const BLRasterSharedFillState* _sharedFillState;
  const BLRasterSharedBaseStrokeState* _sharedStrokeState;

  BL_INLINE void initStates(const BLRasterSharedFillState* sharedFillState,
                            const BLRasterSharedBaseStrokeState* sharedStrokeState = nullptr) noexcept {
    _sharedFillState = sharedFillState;
    _sharedStrokeState = sharedStrokeState;
  }

  BL_INLINE const BLRasterSharedFillState* fillState() const noexcept { return _sharedFillState; }
  BL_INLINE const BLRasterSharedBaseStrokeState* strokeState() const noexcept { return _sharedStrokeState; }

  BL_INLINE uint32_t metaMatrixFixedType() const noexcept { return _metaMatrixFixedType; }
  BL_INLINE uint32_t finalMatrixFixedType() const noexcept { return _finalMatrixFixedType; }

  BL_INLINE void setMetaMatrixFixedType(uint32_t type) noexcept { _metaMatrixFixedType = uint8_t(type); }
  BL_INLINE void setFinalMatrixFixedType(uint32_t type) noexcept { _finalMatrixFixedType = uint8_t(type); }
};

// ============================================================================
// [BLRasterJobData_GeometryOp]
// ============================================================================

struct BLRasterJobData_GeometryOp : public BLRasterJobData_BaseOp {
  BL_INLINE void initFillJob(BLRasterCommand* commandData) noexcept {
    _initInternal(BL_RASTER_JOB_TYPE_FILL_GEOMETRY, commandData);
  }

  BL_INLINE void initStrokeJob(BLRasterCommand* commandData) noexcept {
    _initInternal(BL_RASTER_JOB_TYPE_STROKE_GEOMETRY, commandData);
  }

  BL_INLINE uint32_t geometryType() const noexcept { return _payloadType; }

  BL_INLINE void setGeometry(uint32_t geometryType, const void* srcDataPtr, size_t srcDataSize) noexcept {
    _payloadType = uint8_t(geometryType);

    switch (geometryType) {
      case BL_GEOMETRY_TYPE_PATH:
      case BL_GEOMETRY_TYPE_REGION:
        geometryData<BLVariantCore>()->impl = blImplIncRef(static_cast<const BLVariantCore*>(srcDataPtr)->impl);
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

// ============================================================================
// [BLRasterJobData_TextOp]
// ============================================================================

struct BLRasterJobData_TextOp : public BLRasterJobData_BaseOp {
  BLPoint _pt;
  BLFontCore _font;

  union {
    BLArrayView<void> _textData;
    BLGlyphRun _glyphRun;
    BLGlyphBufferCore _glyphBuffer;
  };

  BL_INLINE void initFillJob(BLRasterCommand* commandData) noexcept {
    _initInternal(BL_RASTER_JOB_TYPE_FILL_TEXT, commandData);
  }

  BL_INLINE void initStrokeJob(BLRasterCommand* commandData) noexcept {
    _initInternal(BL_RASTER_JOB_TYPE_STROKE_TEXT, commandData);
  }

  BL_INLINE void destroy() noexcept {
    blDownCast(_font).~BLFont();
    if (_payloadType == BL_RASTER_JOB_TEXT_DATA_TYPE_GLYPH_BUFFER)
      blDownCast(_glyphBuffer).~BLGlyphBuffer();
  }

  BL_INLINE void initFont(const BLFontCore& font) noexcept {
    _font.impl = blImplIncRef(font.impl);
  }

  BL_INLINE void initCoordinates(const BLPoint& pt) noexcept {
    _pt = pt;
  }

  BL_INLINE void initTextData(const void* text, size_t size, uint32_t encoding) noexcept {
    _payloadType = uint8_t(encoding);
    _textData.reset(text, size);
  }

  BL_INLINE void initGlyphRun(void* glyphData, void* placementData, size_t size, uint32_t placementType, uint32_t flags) noexcept {
    _payloadType = BL_RASTER_JOB_TEXT_DATA_TYPE_GLYPH_RUN;
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
    _payloadType = BL_RASTER_JOB_TEXT_DATA_TYPE_GLYPH_BUFFER;
    _glyphBuffer.impl = gbI;
  }

  BL_INLINE uint32_t textDataType() const noexcept { return _payloadType; }

  BL_INLINE const void* textData() const noexcept { return _textData.data; }
  BL_INLINE size_t textSize() const noexcept { return _textData.size; }

  BL_INLINE const BLGlyphBuffer& glyphBuffer() const noexcept { return blDownCast(_glyphBuffer); }
};

//! \}
//! \endcond

#endif // BLEND2D_RASTER_RASTERJOB_P_H_INCLUDED
