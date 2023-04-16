// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../api-build_p.h"
#include "../font_p.h"
#include "../geometry_p.h"
#include "../matrix_p.h"
#include "../path_p.h"
#include "../tables_p.h"
#include "../opentype/otface_p.h"
#include "../opentype/otglyf_p.h"
#include "../support/intops_p.h"
#include "../support/memops_p.h"
#include "../support/ptrops_p.h"
#include "../support/scopedbuffer_p.h"

namespace BLOpenType {
namespace GlyfImpl {

// OpenType::GlyfImpl - FlagToSizeTable
// =====================================

// This table provides information about the number of bytes vertex data consumes per each flag. It's used to
// calculate the size of X and Y arrays of all contours a simple glyph is composed of to speed up decoding.
struct FlagToSizeGen {
  static constexpr uint32_t value(size_t i) noexcept {
    return ((uint32_t(i & (GlyfTable::Simple::kXIsByte >> 1)) ? 1 : (i & (GlyfTable::Simple::kXIsSameOrXByteIsPositive >> 1)) ? 0 : 2) <<  0) |
           ((uint32_t(i & (GlyfTable::Simple::kYIsByte >> 1)) ? 1 : (i & (GlyfTable::Simple::kYIsSameOrYByteIsPositive >> 1)) ? 0 : 2) << 16) ;
  }
};

static constexpr const auto vertexSizeTable_ = blMakeLookupTable<uint32_t, ((GlyfTable::Simple::kImportantFlagsMask + 1) >> 1), FlagToSizeGen>();

const BLLookupTable<uint32_t, ((GlyfTable::Simple::kImportantFlagsMask + 1) >> 1)> vertexSizeTable = vertexSizeTable_;

// OpenType::GlyfImpl - GetGlyphBounds
// ===================================

static const uint8_t blBlankGlyphData[sizeof(GlyfTable::GlyphData)] = { 0 };

static BLResult BL_CDECL getGlyphBounds(
  const BLFontFaceImpl* faceI_,
  const uint32_t* glyphData,
  intptr_t glyphAdvance,
  BLBoxI* boxes,
  size_t count) noexcept {

  BLResult result = BL_SUCCESS;

  const OTFaceImpl* faceI = static_cast<const OTFaceImpl*>(faceI_);
  BLFontTable glyfTable = faceI->glyf.glyfTable;
  BLFontTable locaTable = faceI->glyf.locaTable;
  uint32_t locaOffsetSize = faceI->locaOffsetSize();

  const uint8_t* blankGlyphData = blBlankGlyphData;

  for (size_t i = 0; i < count; i++) {
    uint32_t glyphId = glyphData[0] & 0xFFFFu;
    glyphData = BLPtrOps::offset(glyphData, glyphAdvance);

    size_t offset;
    size_t endOff;

    // NOTE: Maximum glyphId is 65535, so we are always safe here regarding multiplying the `glyphId` by 2 or 4
    // to calculate the correct index.
    if (locaOffsetSize == 2) {
      size_t index = size_t(glyphId) * 2u;
      if (BL_UNLIKELY(index + sizeof(UInt16) * 2 > locaTable.size))
        goto InvalidData;

      offset = uint32_t(reinterpret_cast<const UInt16*>(locaTable.data + index + 0)->value()) * 2u;
      endOff = uint32_t(reinterpret_cast<const UInt16*>(locaTable.data + index + 2)->value()) * 2u;
    }
    else {
      size_t index = size_t(glyphId) * 4u;
      if (BL_UNLIKELY(index + sizeof(UInt32) * 2 > locaTable.size))
        goto InvalidData;

      offset = reinterpret_cast<const UInt32*>(locaTable.data + index + 0)->value();
      endOff = reinterpret_cast<const UInt32*>(locaTable.data + index + 4)->value();
    }

    if (BL_LIKELY(endOff <= glyfTable.size)) {
      const uint8_t* gPtr = blankGlyphData;
      if (offset < endOff) {
        gPtr = glyfTable.data + offset;
        size_t remainingSize = endOff - offset;

        if (BL_UNLIKELY(remainingSize < sizeof(GlyfTable::GlyphData)))
          goto InvalidData;
      }

      int xMin = reinterpret_cast<const GlyfTable::GlyphData*>(gPtr)->xMin();
      int xMax = reinterpret_cast<const GlyfTable::GlyphData*>(gPtr)->xMax();

      // Y coordinates in fonts are bottom to top, we convert them to top-to-bottom.
      int yMin = -reinterpret_cast<const GlyfTable::GlyphData*>(gPtr)->yMax();
      int yMax = -reinterpret_cast<const GlyfTable::GlyphData*>(gPtr)->yMin();

      boxes[i].reset(xMin, yMin, xMax, yMax);
      continue;
    }

    // Invalid data or the glyph is not defined. In either case we just zero the box.
InvalidData:
    boxes[i].reset();
    result = BL_ERROR_INVALID_DATA;
  }

  return result;
}

// OpenType::GlyfImpl - GetGlyphOutlines
// =====================================

namespace {

class GlyfVertexDecoder {
public:
  const uint8_t* _xCoordPtr;
  const uint8_t* _yCoordPtr;
  const uint8_t* _endPtr;

  double _m00;
  double _m01;
  double _m10;
  double _m11;

  BL_INLINE GlyfVertexDecoder(const uint8_t* xCoordPtr, const uint8_t* yCoordPtr, const uint8_t* endPtr, const BLMatrix2D& m) noexcept
    : _xCoordPtr(xCoordPtr),
      _yCoordPtr(yCoordPtr),
      _endPtr(endPtr),
      _m00(m.m00),
      _m01(m.m01),
      _m10(m.m10),
      _m11(m.m11) {}

  BL_INLINE BLPoint decodeNext(uint32_t flags) noexcept {
    int x16 = 0;
    int y16 = 0;

    if (flags & GlyfTable::Simple::kXIsByte) {
      BL_ASSERT(_xCoordPtr <= _endPtr - 1);
      x16 = int(_xCoordPtr[0]);
      if (!(flags & GlyfTable::Simple::kXIsSameOrXByteIsPositive))
        x16 = -x16;
      _xCoordPtr += 1;
    }
    else if (!(flags & GlyfTable::Simple::kXIsSameOrXByteIsPositive)) {
      BL_ASSERT(_xCoordPtr <= _endPtr - 2);
      x16 = BLMemOps::readI16uBE(_xCoordPtr);
      _xCoordPtr += 2;
    }

    if (flags & GlyfTable::Simple::kYIsByte) {
      BL_ASSERT(_yCoordPtr <= _endPtr - 1);
      y16 = int(_yCoordPtr[0]);
      if (!(flags & GlyfTable::Simple::kYIsSameOrYByteIsPositive))
        y16 = -y16;
      _yCoordPtr += 1;
    }
    else if (!(flags & GlyfTable::Simple::kYIsSameOrYByteIsPositive)) {
      BL_ASSERT(_yCoordPtr <= _endPtr - 2);
      y16 = BLMemOps::readI16uBE(_yCoordPtr);
      _yCoordPtr += 2;
    }

    return BLPoint(double(x16) * _m00 + double(y16) * _m10, double(x16) * _m01 + double(y16) * _m11);
  }
};

} // {anonymous}

static BLResult BL_CDECL getGlyphOutlines(
  const BLFontFaceImpl* faceI_,
  uint32_t glyphId,
  const BLMatrix2D* matrix,
  BLPath* out,
  size_t* contourCountOut,
  BLScopedBuffer* tmpBuffer) noexcept {

  const OTFaceImpl* faceI = static_cast<const OTFaceImpl*>(faceI_);

  typedef GlyfTable::Simple Simple;
  typedef GlyfTable::Compound Compound;

  if (BL_UNLIKELY(glyphId >= faceI->faceInfo.glyphCount))
    return blTraceError(BL_ERROR_INVALID_GLYPH);

  BLFontTable glyfTable = faceI->glyf.glyfTable;
  BLFontTable locaTable = faceI->glyf.locaTable;
  uint32_t locaOffsetSize = faceI->locaOffsetSize();

  const uint8_t* gPtr = nullptr;
  size_t remainingSize = 0;
  size_t compoundLevel = 0;

  // Only matrix and compoundFlags are important in the root entry.
  CompoundEntry compoundData[CompoundEntry::kMaxLevel];
  compoundData[0].gPtr = nullptr;
  compoundData[0].remainingSize = 0;
  compoundData[0].compoundFlags = Compound::kArgsAreXYValues;
  compoundData[0].matrix = *matrix;

  BLPathAppender appender;
  size_t contourCountTotal = 0;

  for (;;) {
    size_t offset;
    size_t endOff;

    // NOTE: Maximum glyphId is 65535, so we are always safe here regarding multiplying the `glyphId` by 2 or 4
    // to calculate the correct index.
    if (locaOffsetSize == 2) {
      size_t index = size_t(glyphId) * 2u;
      if (BL_UNLIKELY(index + sizeof(UInt16) * 2u > locaTable.size))
        goto InvalidData;
      offset = uint32_t(reinterpret_cast<const UInt16*>(locaTable.data + index + 0)->value()) * 2u;
      endOff = uint32_t(reinterpret_cast<const UInt16*>(locaTable.data + index + 2)->value()) * 2u;
    }
    else {
      size_t index = size_t(glyphId) * 4u;
      if (BL_UNLIKELY(index + sizeof(UInt32) * 2u > locaTable.size))
        goto InvalidData;
      offset = reinterpret_cast<const UInt32*>(locaTable.data + index + 0)->value();
      endOff = reinterpret_cast<const UInt32*>(locaTable.data + index + 4)->value();
    }

    // Simple or Empty Glyph
    // ---------------------

    if (BL_UNLIKELY(offset >= endOff || endOff > glyfTable.size)) {
      // Only ALLOWED when `offset == endOff`.
      if (BL_UNLIKELY(offset != endOff || endOff > glyfTable.size))
        goto InvalidData;
    }
    else {
      gPtr = glyfTable.data + offset;
      remainingSize = endOff - offset;

      if (BL_UNLIKELY(remainingSize < sizeof(GlyfTable::GlyphData)))
        goto InvalidData;

      int contourCountSigned = reinterpret_cast<const GlyfTable::GlyphData*>(gPtr)->numberOfContours();
      if (contourCountSigned > 0) {
        size_t contourCount = size_t(unsigned(contourCountSigned));
        BLOverflowFlag of = 0;

        // Minimum data size is:
        //   10                     [GlyphData header]
        //   (numberOfContours * 2) [endPtsOfContours]
        //   2                      [instructionLength]
        gPtr += sizeof(GlyfTable::GlyphData);
        remainingSize = BLIntOps::subOverflow(remainingSize, sizeof(GlyfTable::GlyphData) + contourCount * 2u + 2u, &of);
        if (BL_UNLIKELY(of))
          goto InvalidData;

        const UInt16* contourArray = reinterpret_cast<const UInt16*>(gPtr);
        gPtr += contourCount * 2u;
        contourCountTotal += contourCount;

        // We don't use hinting instructions, so skip them.
        size_t instructionCount = BLMemOps::readU16uBE(gPtr);
        remainingSize = BLIntOps::subOverflow(remainingSize, instructionCount, &of);
        if (BL_UNLIKELY(of))
          goto InvalidData;

        gPtr += 2u + instructionCount;
        const uint8_t* gEnd = gPtr + remainingSize;

        // Number of vertices in TrueType sense (could be less than a number of points required by BLPath
        // representation, especially if TT outline contains consecutive off-curve points).
        size_t ttVertexCount = size_t(contourArray[contourCount - 1].value()) + 1u;

        // Only try to decode vertices if there is more than 1.
        if (ttVertexCount > 1u) {
          // Read TrueType Flags Data
          // ------------------------

          uint8_t* fDataPtr = static_cast<uint8_t*>(tmpBuffer->alloc(ttVertexCount));
          if (BL_UNLIKELY(!fDataPtr))
            return blTraceError(BL_ERROR_OUT_OF_MEMORY);

          // Sizes of xCoordinates[] and yCoordinates[] arrays in TrueType data.
          size_t xCoordinatesSize;
          size_t yCoordinatesSize;

          // Number of consecutive off curve vertices making a spline. We need this number to be able to calculate the
          // number of BLPath vertices we will need to convert this glyph into BLPath data.
          size_t offCurveSplineCount = 0;

          constexpr uint32_t kOffCurveSplineMask = Simple::kOnCurvePoint | (Simple::kOnCurvePoint << 7);

          {
            // Number of bytes required by both X and Y coordinates as a packed `(Y << 16) | X` value.
            //
            // NOTE: We know that the maximum number of contours of a single glyph is 32767, thus we can store both the
            // number of X and Y vertices in a single unsigned 32-bit integer, as each vertex has 2 bytes maximum, which
            // would be 65534.
            size_t xyCoordinatesSize = 0;

            // We parse flags one-by-one and calculate the size required by vertices by using our FLAG tables so we don't
            // have to do bounds checking during vertex decoding.
            size_t i = 0;
            uint32_t f = Simple::kOnCurvePoint;

            do {
              if (BL_UNLIKELY(gPtr == gEnd))
                goto InvalidData;

              uint32_t ttFlag = *gPtr++ & Simple::kImportantFlagsMask;
              uint32_t vertexSize = vertexSizeTable[ttFlag >> 1];

              f = ((f << 7) | ttFlag) & 0xFFu;
              fDataPtr[i++] = uint8_t(f);

              xyCoordinatesSize += vertexSize;
              offCurveSplineCount += uint32_t((f & kOffCurveSplineMask) == 0);

              // Most of flags are not repeated. Some contours have no repeated flags at all, so make this likely.
              if (BL_LIKELY(!(f & Simple::kRepeatFlag)))
                continue;

              if (BL_UNLIKELY(gPtr == gEnd))
                goto InvalidData;

              // When `kRepeatFlag` is set it means that the next byte contains how many times it should repeat
              // (the specification doesn't mention zero length, so we won't fail and just silently consume the byte).
              size_t n = *gPtr++;
              if (BL_UNLIKELY(n > ttVertexCount - i))
                goto InvalidData;

              xyCoordinatesSize += uint32_t(n) * vertexSize;
              offCurveSplineCount += n * size_t((f & Simple::kOnCurvePoint) == 0);

              BLMemOps::fillSmall(fDataPtr + i, uint8_t(f), n);
              i += n;
            } while (i < ttVertexCount);

            xCoordinatesSize = xyCoordinatesSize & 0xFFFFu;
            yCoordinatesSize = xyCoordinatesSize >> 16;
          }

          remainingSize = (size_t)(gEnd - gPtr);
          if (BL_UNLIKELY(xCoordinatesSize + yCoordinatesSize > remainingSize))
            goto InvalidData;

          // Read TrueType Vertex Data
          // -------------------------

          // Vertex data in `glyf` table doesn't map 1:1 to how BLPath stores its data. Multiple off-point curves in
          // TrueType data are decomposed into a quad spline, which is one vertex larger (BLPath doesn't offer multiple
          // off-point quads). This means that the number of vertices required by BLPath can be greater than the number
          // of vertices stored in TrueType 'glyf' data. However, we should know exactly how many vertices we have to
          // add to `ttVertexCount` as we calculated `offCurveSplineCount` during flags decoding.
          //
          // The number of resulting vertices is thus:
          //   - `ttVertexCount` - base number of vertices stored in TrueType data.
          //   - `offCurveSplineCount` - the number of additional vertices we will need to add for each off-curve spline
          //     used in TrueType data.
          //   - `contourCount` - Number of contours, we multiply this by 3 as we want to include one 'MoveTo', 'Close',
          //     and one additional off-curve spline point per each contour in case it starts - ends with an off-curve
          //     point.
          size_t maxVertexCount = ttVertexCount + offCurveSplineCount + contourCount * 3;

          // Increase maxVertexCount if the path was not allocated yet - this avoids a possible realloc of compound glyphs.
          if (out->capacity() == 0 && compoundLevel > 0)
            maxVertexCount += 128;

          BL_PROPAGATE(appender.beginAppend(out, maxVertexCount));

          // Since we know exactly how many bytes both vertex arrays consume we can decode both X and Y coordinates at
          // the same time. This gives us also the opportunity to start appending to BLPath immediately.
          GlyfVertexDecoder vertexDecoder(gPtr, gPtr + xCoordinatesSize, gEnd, compoundData[compoundLevel].matrix);

          // Vertices are stored relative to each other, this is the current point.
          BLPoint currentPt(compoundData[compoundLevel].matrix.m20, compoundData[compoundLevel].matrix.m21);

          // Current vertex index in TT sense, advanced until `ttVertexCount`, which must be end index of the last contour.
          size_t i = 0;

          for (size_t contourIndex = 0; contourIndex < contourCount; contourIndex++) {
            size_t iEnd = size_t(contourArray[contourIndex].value()) + 1;
            if (BL_UNLIKELY(iEnd <= i || iEnd > ttVertexCount))
              goto InvalidData;

            // We need to be able to handle a case in which the contour data starts off-curve.
            size_t offCurveStart = SIZE_MAX;

            // We do the first vertex here as we want to emit 'MoveTo' and we want to remember it for a possible
            // off-curve start.
            uint32_t f = fDataPtr[i];
            currentPt += vertexDecoder.decodeNext(f);

            if (f & Simple::kOnCurvePoint)
              appender.moveTo(currentPt);
            else
              offCurveStart = appender.currentIndex(*out);

            if (++i >= iEnd)
              continue;

            // Initial 'MoveTo' coordinates.
            BLPoint initialPt = currentPt;

            for (;;) {
              f = fDataPtr[i];

              BLPoint delta = vertexDecoder.decodeNext(f);
              currentPt += delta;

              if ((f & kOffCurveSplineMask) != 0) {
                BL_STATIC_ASSERT(BL_PATH_CMD_QUAD - 1 == BL_PATH_CMD_ON);
                uint8_t cmd = uint8_t(BL_PATH_CMD_QUAD - (f & Simple::kOnCurvePoint));
                appender.addVertex(cmd, currentPt);
              }
              else {
                BLPoint onPt = currentPt - delta * 0.5;
                appender.addVertex(BL_PATH_CMD_ON, onPt);
                appender.addVertex(BL_PATH_CMD_QUAD, currentPt);
              }

              if (++i >= iEnd)
                break;
            }

            if (offCurveStart != SIZE_MAX) {
              BLPathImpl* outI = BLPathPrivate::getImpl(out);
              BLPoint finalPt = outI->vertexData[offCurveStart];

              outI->commandData[offCurveStart] = BL_PATH_CMD_MOVE;

              if (!(f & Simple::kOnCurvePoint)) {
                BLPoint onPt = (currentPt + initialPt) * 0.5;
                appender.addVertex(BL_PATH_CMD_ON, onPt);
                finalPt = (initialPt + finalPt) * 0.5;
              }

              appender.addVertex(BL_PATH_CMD_QUAD, initialPt);
              appender.addVertex(BL_PATH_CMD_ON, finalPt);
            }
            else if (!(f & Simple::kOnCurvePoint)) {
              appender.addVertex(BL_PATH_CMD_ON, initialPt);
            }

            appender.close();
          }
          appender.done(out);
        }
      }
      else if (contourCountSigned == -1) {
        gPtr += sizeof(GlyfTable::GlyphData);
        remainingSize -= sizeof(GlyfTable::GlyphData);

        if (BL_UNLIKELY(++compoundLevel >= CompoundEntry::kMaxLevel))
          goto InvalidData;

        goto ContinueCompound;
      }
      else {
        // Cannot be less than -1, only -1 specifies compound glyph, lesser value is invalid according to the
        // specification.
        if (BL_UNLIKELY(contourCountSigned < -1))
          goto InvalidData;

        // Otherwise the glyph has no contours.
      }
    }

    // Compound Glyph
    // --------------

    if (compoundLevel) {
      while (!(compoundData[compoundLevel].compoundFlags & Compound::kMoreComponents))
        if (--compoundLevel == 0)
          break;

      if (compoundLevel) {
        gPtr = compoundData[compoundLevel].gPtr;
        remainingSize = compoundData[compoundLevel].remainingSize;

        // The structure that we are going to read is as follows:
        //
        //   [Header]
        //     uint16_t flags;
        //     uint16_t glyphId;
        //
        //   [Translation]
        //     a) int8_t arg1/arg2;
        //     b) int16_t arg1/arg2;
        //
        //   [Scale/Affine]
        //     a) <None>
        //     b) int16_t scale;
        //     c) int16_t scaleX, scaleY;
        //     d) int16_t m00, m01, m10, m11;

ContinueCompound:
        {
          uint32_t flags;
          int arg1, arg2;
          BLOverflowFlag of = 0;

          remainingSize = BLIntOps::subOverflow<size_t>(remainingSize, 6, &of);
          if (BL_UNLIKELY(of))
            goto InvalidData;

          flags = BLMemOps::readU16uBE(gPtr);
          glyphId = BLMemOps::readU16uBE(gPtr + 2);
          if (BL_UNLIKELY(glyphId >= faceI->faceInfo.glyphCount))
            goto InvalidData;

          arg1 = BLMemOps::readI8(gPtr + 4);
          arg2 = BLMemOps::readI8(gPtr + 5);
          gPtr += 6;

          if (flags & Compound::kArgsAreWords) {
            remainingSize = BLIntOps::subOverflow<size_t>(remainingSize, 2, &of);
            if (BL_UNLIKELY(of))
              goto InvalidData;

            arg1 = BLIntOps::shl(arg1, 8) | (arg2 & 0xFF);
            arg2 = BLMemOps::readI16uBE(gPtr);
            gPtr += 2;
          }

          if (!(flags & Compound::kArgsAreXYValues)) {
            // This makes them unsigned.
            arg1 &= 0xFFFFu;
            arg2 &= 0xFFFFu;

            // TODO: [OPENTYPE GLYF] ArgsAreXYValues not implemented. I don't know how atm.
          }

          constexpr double kScaleF2x14 = 1.0 / 16384.0;

          BLMatrix2D& cm = compoundData[compoundLevel].matrix;
          cm.reset(1.0, 0.0, 0.0, 1.0, double(arg1), double(arg2));

          if (flags & Compound::kAnyCompoundScale) {
            if (flags & Compound::kWeHaveScale) {
              // Simple scaling:
              //   [Sc, 0]
              //   [0, Sc]
              remainingSize = BLIntOps::subOverflow<size_t>(remainingSize, 2, &of);
              if (BL_UNLIKELY(of))
                goto InvalidData;

              double scale = double(BLMemOps::readI16uBE(gPtr)) * kScaleF2x14;
              cm.m00 = scale;
              cm.m11 = scale;
              gPtr += 2;
            }
            else if (flags & Compound::kWeHaveScaleXY) {
              // Simple scaling:
              //   [Sx, 0]
              //   [0, Sy]
              remainingSize = BLIntOps::subOverflow<size_t>(remainingSize, 4, &of);
              if (BL_UNLIKELY(of))
                goto InvalidData;

              cm.m00 = double(BLMemOps::readI16uBE(gPtr + 0)) * kScaleF2x14;
              cm.m11 = double(BLMemOps::readI16uBE(gPtr + 2)) * kScaleF2x14;
              gPtr += 4;
            }
            else {
              // Affine case:
              //   [A, B]
              //   [C, D]
              remainingSize = BLIntOps::subOverflow<size_t>(remainingSize, 8, &of);
              if (BL_UNLIKELY(of))
                goto InvalidData;

              cm.m00 = double(BLMemOps::readI16uBE(gPtr + 0)) * kScaleF2x14;
              cm.m01 = double(BLMemOps::readI16uBE(gPtr + 2)) * kScaleF2x14;
              cm.m10 = double(BLMemOps::readI16uBE(gPtr + 4)) * kScaleF2x14;
              cm.m11 = double(BLMemOps::readI16uBE(gPtr + 6)) * kScaleF2x14;
              gPtr += 8;
            }

            // Translation scale should only happen when `kArgsAreXYValues` is set. The default behavior according to
            // the specification is `kUnscaledComponentOffset`, which can be overridden by `kScaledComponentOffset`.
            // However, if both or neither are set then the behavior is the same as `kUnscaledComponentOffset`.
            if ((flags & (Compound::kArgsAreXYValues | Compound::kAnyCompoundOffset    )) ==
                         (Compound::kArgsAreXYValues | Compound::kScaledComponentOffset)) {
              // This is what FreeType does and what's not 100% according to the specificaion. However, according to
              // FreeType this would produce much better offsets so we will match FreeType instead of following the
              // specification.
              cm.m20 *= BLGeometry::length(BLPoint(cm.m00, cm.m01));
              cm.m21 *= BLGeometry::length(BLPoint(cm.m10, cm.m11));
            }
          }

          compoundData[compoundLevel].gPtr = gPtr;
          compoundData[compoundLevel].remainingSize = remainingSize;
          compoundData[compoundLevel].compoundFlags = flags;
          BLTransformPrivate::multiply(cm, cm, compoundData[compoundLevel - 1].matrix);
          continue;
        }
      }
    }

    break;
  }

  *contourCountOut = contourCountTotal;
  return BL_SUCCESS;

InvalidData:
  *contourCountOut = 0;
  return blTraceError(BL_ERROR_INVALID_DATA);
}

// OpenType::GlyfImpl - Init
// =========================

BLResult init(OTFaceImpl* faceI, BLFontTable glyfTable, BLFontTable locaTable) noexcept {
  faceI->glyf.glyfTable = glyfTable;
  faceI->glyf.locaTable = locaTable;
  faceI->funcs.getGlyphBounds = getGlyphBounds;

  // Don't reference any function that won't be used when certain optimizations are enabled across the whole binary.
#if defined(BL_TARGET_OPT_AVX2)
  faceI->funcs.getGlyphOutlines = getGlyphOutlines_AVX2;
#elif defined(BL_TARGET_OPT_SSE4_2)
  faceI->funcs.getGlyphOutlines = getGlyphOutlines_SSE4_2;
#else
#if defined(BL_BUILD_OPT_AVX2)
  if (blRuntimeHasAVX2(&blRuntimeContext)) {
    faceI->funcs.getGlyphOutlines = getGlyphOutlines_AVX2;
  }
  else
#endif
#if defined(BL_BUILD_OPT_SSE4_2)
  if (blRuntimeHasSSE4_2(&blRuntimeContext)) {
    faceI->funcs.getGlyphOutlines = getGlyphOutlines_SSE4_2;
  }
  else
#endif
  faceI->funcs.getGlyphOutlines = getGlyphOutlines;
#endif

  return BL_SUCCESS;
}

} // {GlyfImpl}
} // {BLOpenType}
