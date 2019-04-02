// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#include "../blfont_p.h"
#include "../blgeometry_p.h"
#include "../blmatrix_p.h"
#include "../blpath_p.h"
#include "../blsupport_p.h"
#include "../bltables_p.h"
#include "../opentype/blotface_p.h"
#include "../opentype/blotglyf_p.h"

namespace BLOpenType {
namespace GlyfImpl {

// ============================================================================
// [BLOpenType::GlyphImpl - Globals]
// ============================================================================

// These tables contains information about the number of bytes vertex data consumes
// per each flag. It's used to calculate the size of X and Y arrays of all contours
// a simple glyph defines. It's used to speedup vertex processing during glyph decoding.

struct FlagToXSizeGen {
  static constexpr uint8_t value(size_t i) noexcept {
    return uint8_t(
      (i & GlyfTable::Simple::kXIsByte                 ) ? 1 :
      (i & GlyfTable::Simple::kXIsSameOrXByteIsPositive) ? 0 : 2);
  }
};

struct FlagToYSizeGen {
  static constexpr uint8_t value(size_t i) noexcept {
    return uint8_t(
      (i & GlyfTable::Simple::kYIsByte                 ) ? 1 :
      (i & GlyfTable::Simple::kYIsSameOrYByteIsPositive) ? 0 : 2);
  }
};

static constexpr const auto flagToXSizeTable = blLookupTable<uint8_t, GlyfTable::Simple::kImportantFlagsMask + 1, FlagToXSizeGen>();
static constexpr const auto flagToYSizeTable = blLookupTable<uint8_t, GlyfTable::Simple::kImportantFlagsMask + 1, FlagToYSizeGen>();

// ============================================================================
// [BLOpenType::GlyfImpl - CompoundEntry]
// ============================================================================

struct CompoundEntry {
  enum : uint32_t { kMaxLevel = 16 };

  const uint8_t* gPtr;
  size_t remainingSize;
  uint32_t compoundFlags;
  BLMatrix2D matrix;
};

// ============================================================================
// [BLOpenType::GlyfImpl - GetGlyphBounds]
// ============================================================================

static BLResult BL_CDECL getGlyphBounds(
  const BLFontFaceImpl* faceI_,
  const BLGlyphId* glyphIdData,
  intptr_t glyphIdAdvance,
  BLBoxI* boxes,
  size_t count) noexcept {

  const BLOTFaceImpl* faceI = static_cast<const BLOTFaceImpl*>(faceI_);
  BLFontTable glyfTable = faceI->glyf.glyfTable;
  BLFontTable locaTable = faceI->glyf.locaTable;
  uint32_t locaOffsetSize = faceI->locaOffsetSize();

  for (size_t i = 0; i < count; i++) {
    uint32_t glyphId = glyphIdData[0];
    glyphIdData = blOffsetPtr(glyphIdData, glyphIdAdvance);

    size_t offset;
    size_t endOff;

    // NOTE: Maximum glyphId is 65535, so we are always safe here regarding
    // multiplying the `glyphId` by 2 or 4 to calculate the correct index.
    if (locaOffsetSize == 2) {
      size_t index = size_t(glyphId) * 2u;
      if (BL_UNLIKELY(index + 4 > locaTable.size))
        goto InvalidData;
      offset = uint32_t(reinterpret_cast<const UInt16*>(locaTable.data + index + 0)->value()) * 2u;
      endOff = uint32_t(reinterpret_cast<const UInt16*>(locaTable.data + index + 2)->value()) * 2u;
    }
    else {
      size_t index = size_t(glyphId) * 4u;
      if (BL_UNLIKELY(index + 8 > locaTable.size))
        goto InvalidData;
      offset = reinterpret_cast<const UInt32*>(locaTable.data + index + 0)->value();
      endOff = reinterpret_cast<const UInt32*>(locaTable.data + index + 4)->value();
    }

    if (offset < endOff && endOff <= glyfTable.size) {
      const uint8_t* gPtr = glyfTable.data + offset;
      size_t remainingSize = endOff - offset;

      if (BL_UNLIKELY(remainingSize < sizeof(GlyfTable::GlyphData)))
        goto InvalidData;

      boxes[i].reset(reinterpret_cast<const GlyfTable::GlyphData*>(gPtr)->xMin(),
                     reinterpret_cast<const GlyfTable::GlyphData*>(gPtr)->yMin(),
                     reinterpret_cast<const GlyfTable::GlyphData*>(gPtr)->xMax(),
                     reinterpret_cast<const GlyfTable::GlyphData*>(gPtr)->yMax());
      continue;
    }

    // Invalid data or the glyph is not defined. In either case we just zero the box.
InvalidData:
    boxes[i].reset();
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLOpenType::GlyfImpl - DecodeGlyph]
// ============================================================================

static BLResult BL_CDECL decodeGlyph(
  const BLFontFaceImpl* faceI_,
  uint32_t glyphId,
  const BLMatrix2D* matrix,
  BLPath* out,
  BLMemBuffer* tmpBuffer,
  BLPathSinkFunc sink, size_t sinkGlyphIndex, void* closure) noexcept {

  const BLOTFaceImpl* faceI = static_cast<const BLOTFaceImpl*>(faceI_);

  typedef GlyfTable::Simple Simple;
  typedef GlyfTable::Compound Compound;

  if (BL_UNLIKELY(glyphId >= faceI->glyphCount))
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

  BLGlyphOutlineSinkInfo sinkInfo;
  sinkInfo.glyphIndex = sinkGlyphIndex;
  sinkInfo.contourCount = 0;

  BLPathAppender appender;
  for (;;) {
    size_t offset;
    size_t endOff;

    // NOTE: Maximum glyphId is 65535, so we are always safe here regarding
    // multiplying the `glyphId` by 2 or 4 to calculate the correct index.
    if (locaOffsetSize == 2) {
      size_t index = size_t(glyphId) * 2u;
      if (BL_UNLIKELY(index + 4 > locaTable.size))
        goto InvalidData;
      offset = uint32_t(reinterpret_cast<const UInt16*>(locaTable.data + index + 0)->value()) * 2u;
      endOff = uint32_t(reinterpret_cast<const UInt16*>(locaTable.data + index + 2)->value()) * 2u;
    }
    else {
      size_t index = size_t(glyphId) * 4u;
      if (BL_UNLIKELY(index + 8 > locaTable.size))
        goto InvalidData;
      offset = reinterpret_cast<const UInt32*>(locaTable.data + index + 0)->value();
      endOff = reinterpret_cast<const UInt32*>(locaTable.data + index + 4)->value();
    }

    // ------------------------------------------------------------------------
    // [Simple / Empty Glyph]
    // ------------------------------------------------------------------------

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
        uint32_t i;
        uint32_t contourCount = unsigned(contourCountSigned);
        BLOverflowFlag of = 0;

        // --------------------------------------------------------------------
        // The structure that we are going to read is as follows:
        //
        //   [Header]
        //     uint16_t endPtsOfContours[numberOfContours];
        //
        //   [Hinting Bytecode]
        //     uint16_t instructionLength;
        //     uint8_t instructions[instructionLength];
        //
        //   [Contours]
        //     uint8_t flags[?];
        //     uint8_t/uint16_t xCoordinates[?];
        //     uint8_t/uint16_t yCoordinates[?];
        //
        // The problem with contours data is that it's three arrays next to each
        // other and there is no way to create an iterator as these arrays must be
        // read sequentially. The reader must first read flags, then X coordinates,
        // and then Y coordinates.
        //
        // Minimum data size would be:
        //   10                     [GlyphData header]
        //   (numberOfContours * 2) [endPtsOfContours]
        //   2                      [instructionLength]
        // --------------------------------------------------------------------

        gPtr += sizeof(GlyfTable::GlyphData);
        remainingSize = blSubOverflow(remainingSize, sizeof(GlyfTable::GlyphData) + 2u * contourCount + 2u, &of);
        if (BL_UNLIKELY(of))
          goto InvalidData;

        const UInt16* contourArray = reinterpret_cast<const UInt16*>(gPtr);
        gPtr += contourCount * 2u;

        // We don't use hinting instructions, so skip them.
        size_t instructionCount = blMemReadU16uBE(gPtr);
        remainingSize = blSubOverflow(remainingSize, instructionCount, &of);
        if (BL_UNLIKELY(of))
          goto InvalidData;
        gPtr += 2u + instructionCount;

        // --------------------------------------------------------------------
        // We are finally at the beginning of contours data:
        //   flags[]
        //   xCoordinates[]
        //   yCoordinates[]
        // --------------------------------------------------------------------

        // Number of vertices in TrueType sense (could be less than number of
        // points required by BLPath representation, especially if TT outline
        // contains consecutive off-curve points).
        uint32_t vertexCount = size_t(contourArray[contourCount - 1].value()) + 1u;
        uint8_t* flags = static_cast<uint8_t*>(tmpBuffer->alloc(vertexCount));

        if (BL_UNLIKELY(!flags))
          return blTraceError(BL_ERROR_OUT_OF_MEMORY);

        // --------------------------------------------------------------------
        // [Read Flags]
        // --------------------------------------------------------------------

        // Number of bytes required by both X and Y coordinates.
        uint32_t xDataSize = 0;
        uint32_t yDataSize = 0;

        // Number of consecutive off curve vertices making a spline. We need
        // this number to be able to calculate the number of BLPath vertices
        // we will need to convert this glyph into BLPath data.
        uint32_t offCurveSplineCount = 0;

        // We start as off-curve, this would cause adding one more vertex to
        // `offCurveSplineCount` if the start really is off-curve.
        uint32_t prevFlag = 0;

        // Carefully picked bits. It means that this is a second off-curve vertex
        // and it has to be connected by on-curve vertex before the off-curve
        // vertex can be emitted.
        constexpr uint32_t kOffCurveSplineShift = 3;
        constexpr uint32_t kOffCurveSplineBit   = 1 << kOffCurveSplineShift;

        // We parse flags one-by-one and calculate the size required by vertices
        // by using our FLAG tables so we don't have to do bounds checking during
        // vertex decoding.
        const uint8_t* gEnd = gPtr + remainingSize;
        i = 0;

        do {
          if (BL_UNLIKELY(gPtr == gEnd))
            goto InvalidData;

          uint32_t flag = (*gPtr++ & Simple::kImportantFlagsMask);
          uint32_t offCurveSpline = ((prevFlag | flag) & 1) ^ 1;

          xDataSize += flagToXSizeTable[flag];
          yDataSize += flagToYSizeTable[flag];
          offCurveSplineCount += offCurveSpline;

          if (!(flag & Simple::kRepeatFlag)) {
            flag |= offCurveSpline << kOffCurveSplineShift;
            flags[i++] = uint8_t(flag);
          }
          else {
            // When `kRepeatFlag` is set it means that the next byte contains how
            // many times it should repeat (specification doesn't mention zero
            // length, so we won't fail and just silently consume the byte).
            flag ^= Simple::kRepeatFlag;
            flag |= offCurveSpline << kOffCurveSplineShift;

            if (BL_UNLIKELY(gPtr == gEnd))
              goto InvalidData;

            uint32_t n = *gPtr++;
            flags[i++] = uint8_t(flag);
            offCurveSpline = (flag & 1) ^ 1;

            if (BL_UNLIKELY(n > vertexCount - i))
              goto InvalidData;

            xDataSize += n * flagToXSizeTable[flag];
            yDataSize += n * flagToYSizeTable[flag];
            offCurveSplineCount += n * offCurveSpline;

            flag |= offCurveSpline << kOffCurveSplineShift;

            while (n) {
              flags[i++] = uint8_t(flag);
              n--;
            }
          }

          prevFlag = flag;
        } while (i != vertexCount);

        remainingSize = (size_t)(gEnd - gPtr);
        if (BL_UNLIKELY(xDataSize + yDataSize > remainingSize))
          goto InvalidData;

        // --------------------------------------------------------------------
        // [Read Vertices]
        // --------------------------------------------------------------------

        // Vertex data in `glyf` table doesn't map 1:1 to how BLPath stores
        // contours. Multiple off-point curves in TT data are decomposed into
        // a quad spline, which is one vertex larger (BLPath doesn't offer
        // multiple off-point quads). This means that the number of vertices
        // required by BLPath can be greater than the number of vertices stored
        // in TT 'glyf' data. However, we should know exactly how many vertices
        // we have to add to `vertexCount` as we calculated `offCurveSplineCount`
        // during flags decoding.
        //
        // The number of resulting vertices is thus:
        //   - `vertexCount` - base number of vertices stored in TT data.
        //   - `offCurveSplineCount` - the number of additional vertices we
        //     will need to add for each off-curve spline used in TT data.
        //   - `contourCount` - Number of contours, we multiply this by 3
        //     as we want to include one 'MoveTo', 'Close', and one additional
        //     off-curve spline point per each contour in case it starts
        //     ends with off-curve point.
        size_t pathVertexCount = vertexCount + offCurveSplineCount + contourCount * 3;
        BL_PROPAGATE(appender.beginAppend(out, pathVertexCount));

        // Since we know exactly how many bytes both vertex arrays consume we
        // can decode both X and Y coordinates at the same time. This gives us
        // also the opportunity to start appending to BLPath immediately.
        const uint8_t* yPtr = gPtr + xDataSize;

        // Affine transform applied to each vertex.
        double m00 = compoundData[compoundLevel].matrix.m00;
        double m01 = compoundData[compoundLevel].matrix.m01;
        double m10 = compoundData[compoundLevel].matrix.m10;
        double m11 = compoundData[compoundLevel].matrix.m11;

        // Vertices are stored relative to each other, this is the current point.
        double px = compoundData[compoundLevel].matrix.m20;
        double py = compoundData[compoundLevel].matrix.m21;

        // Current vertex index in TT sense, advanced until `vertexCount`,
        // which must be end index of the last contour.
        i = 0;

        for (uint32_t contourIndex = 0; contourIndex < contourCount; contourIndex++) {
          uint32_t iEnd = uint32_t(contourArray[contourIndex].value()) + 1;
          if (BL_UNLIKELY(iEnd <= i || iEnd > vertexCount))
            goto InvalidData;

          // We do the first vertex here as we want to emit 'MoveTo' and we
          // want to remember it for a possible off-curve start. Currently
          // this means there is some code duplicated, unfortunately...
          uint32_t flag = flags[i];

          {
            double xOff = 0.0;
            double yOff = 0.0;

            if (flag & Simple::kXIsByte) {
              BL_ASSERT(gPtr <= gEnd - 1);
              xOff = double(gPtr[0]);
              if (!(flag & Simple::kXIsSameOrXByteIsPositive))
                xOff = -xOff;
              gPtr += 1;
            }
            else if (!(flag & Simple::kXIsSameOrXByteIsPositive)) {
              BL_ASSERT(gPtr <= gEnd - 2);
              xOff = double(blMemReadI16uBE(gPtr));
              gPtr += 2;
            }

            if (flag & Simple::kYIsByte) {
              BL_ASSERT(yPtr <= gEnd - 1);
              yOff = double(yPtr[0]);
              if (!(flag & Simple::kYIsSameOrYByteIsPositive))
                yOff = -yOff;
              yPtr += 1;
            }
            else if (!(flag & Simple::kYIsSameOrYByteIsPositive)) {
              BL_ASSERT(yPtr <= gEnd - 2);
              yOff = double(blMemReadI16uBE(yPtr));
              yPtr += 2;
            }

            px += xOff * m00 + yOff * m10;
            py += xOff * m01 + yOff * m11;
          }

          if (++i >= iEnd)
            continue;

          // Initial 'MoveTo' coordinates.
          double mx = px;
          double my = py;

          // We need to be able to handle a stupic case when the countour starts off curve.
          uint32_t kCurveCmd = 0x80000000u;
          uint32_t cmd = BL_PATH_CMD_ON;
          BLPoint* offCurveStart = flag & Simple::kOnCurvePoint ? nullptr : appender.vtx;

          if (offCurveStart)
            cmd = BL_PATH_CMD_MOVE;
          else
            appender.moveTo(mx, my);

          for (;;) {
            flag = flags[i];
            double dx = 0.0;
            double dy = 0.0;

            if (flag & Simple::kXIsByte) {
              BL_ASSERT(gPtr <= gEnd - 1);
              double xOff = double(gPtr[0]);
              if (!(flag & Simple::kXIsSameOrXByteIsPositive))
                xOff = -xOff;
              gPtr += 1;
              dx = xOff * m00;
              dy = xOff * m01;
            }
            else if (!(flag & Simple::kXIsSameOrXByteIsPositive)) {
              BL_ASSERT(gPtr <= gEnd - 2);
              double xOff = double(blMemReadI16uBE(gPtr));
              gPtr += 2;
              dx = xOff * m00;
              dy = xOff * m01;
            }

            if (flag & Simple::kYIsByte) {
              BL_ASSERT(yPtr <= gEnd - 1);
              double yOff = double(yPtr[0]);
              if (!(flag & Simple::kYIsSameOrYByteIsPositive))
                yOff = -yOff;
              yPtr += 1;
              dx += yOff * m10;
              dy += yOff * m11;
            }
            else if (!(flag & Simple::kYIsSameOrYByteIsPositive)) {
              BL_ASSERT(yPtr <= gEnd - 2);
              double yOff = double(blMemReadI16uBE(yPtr));
              yPtr += 2;
              dx += yOff * m10;
              dy += yOff * m11;
            }

            px += dx;
            py += dy;

            if (flag & Simple::kOnCurvePoint) {
              appender.addVertex(uint8_t(cmd & 0xFFu), px, py);
              cmd = BL_PATH_CMD_ON;
            }
            else if (flag & kOffCurveSplineBit) {
              double qx = px - dx * 0.5;
              double qy = py - dy * 0.5;

              appender.addVertex(uint8_t(cmd & 0xFFu), qx, qy);
              appender.addVertex(BL_PATH_CMD_QUAD, px, py);
              cmd = BL_PATH_CMD_ON | kCurveCmd;
            }
            else {
              appender.addVertex(BL_PATH_CMD_QUAD, px, py);
              cmd = BL_PATH_CMD_ON | kCurveCmd;
            }

            if (++i >= iEnd)
              break;
          }

          if (cmd & kCurveCmd) {
            if (offCurveStart) {
              appender.addVertex(BL_PATH_CMD_ON, (px + mx) * 0.5, (py + my) * 0.5);
              appender.addVertex(BL_PATH_CMD_QUAD, mx, my);
              appender.addVertex(BL_PATH_CMD_ON, (mx + offCurveStart->x) * 0.5, (my + offCurveStart->y) * 0.5);
            }
            else {
              appender.addVertex(BL_PATH_CMD_ON, mx, my);
            }
          }
          else {
            if (offCurveStart) {
              appender.addVertex(BL_PATH_CMD_QUAD, mx, my);
              appender.addVertex(BL_PATH_CMD_ON, offCurveStart->x, offCurveStart->y);
            }
          }

          appender.close();
        }

        appender.done(out);
        if (sink) {
          sinkInfo.contourCount = contourCount;
          BL_PROPAGATE(sink(out, &sinkInfo, closure));
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
        // Cannot be less than -1, only -1 specifies compound glyph, lesser value
        // is invalid according to the specification.
        if (BL_UNLIKELY(contourCountSigned < -1))
          goto InvalidData;

        // Otherwise the glyph has no contours.
      }
    }

    // ----------------------------------------------------------------------
    // [Compound Glyph]
    // ----------------------------------------------------------------------

    if (compoundLevel) {
      while (!(compoundData[compoundLevel].compoundFlags & Compound::kMoreComponents))
        if (--compoundLevel == 0)
          break;

      if (compoundLevel) {
        gPtr = compoundData[compoundLevel].gPtr;
        remainingSize = compoundData[compoundLevel].remainingSize;

        // --------------------------------------------------------------------
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
        // --------------------------------------------------------------------

ContinueCompound:
        {
          uint32_t flags;
          int arg1, arg2;
          BLOverflowFlag of = 0;

          remainingSize = blSubOverflow<size_t>(remainingSize, 6, &of);
          if (BL_UNLIKELY(of))
            goto InvalidData;

          flags = blMemReadU16uBE(gPtr);
          glyphId = blMemReadU16uBE(gPtr + 2);
          if (BL_UNLIKELY(glyphId >= faceI->glyphCount))
            goto InvalidData;

          arg1 = blMemReadI8(gPtr + 4);
          arg2 = blMemReadI8(gPtr + 5);
          gPtr += 6;

          if (flags & Compound::kArgsAreWords) {
            remainingSize = blSubOverflow<size_t>(remainingSize, 2, &of);
            if (BL_UNLIKELY(of))
              goto InvalidData;

            arg1 = blBitShl(arg1, 8) | (arg2 & 0xFF);
            arg2 = blMemReadI16uBE(gPtr);
            gPtr += 2;
          }

          if (!(flags & Compound::kArgsAreXYValues)) {
            // This makes them unsigned.
            arg1 &= 0xFFFFu;
            arg2 &= 0xFFFFu;

            // TODO: [OPENTYPE GLYF] ArgsAreXYValues not implemented. I don't know how atm.
          }

          constexpr double kScaleF2x14 = 1.0 / 16384.0;

          BLMatrix2D& matrix = compoundData[compoundLevel].matrix;
          matrix.reset(1.0, 0.0, 0.0, 1.0, double(arg1), double(arg2));

          if (flags & Compound::kAnyCompoundScale) {
            if (flags & Compound::kWeHaveScale) {
              // Simple scaling:
              //   [Sc, 0]
              //   [0, Sc]
              remainingSize = blSubOverflow<size_t>(remainingSize, 2, &of);
              if (BL_UNLIKELY(of))
                goto InvalidData;

              double scale = double(blMemReadI16uBE(gPtr)) * kScaleF2x14;
              matrix.m00 = scale;
              matrix.m11 = scale;
              gPtr += 2;
            }
            else if (flags & Compound::kWeHaveScaleXY) {
              // Simple scaling:
              //   [Sx, 0]
              //   [0, Sy]
              remainingSize = blSubOverflow<size_t>(remainingSize, 4, &of);
              if (BL_UNLIKELY(of))
                goto InvalidData;

              matrix.m00 = double(blMemReadI16uBE(gPtr + 0)) * kScaleF2x14;
              matrix.m11 = double(blMemReadI16uBE(gPtr + 2)) * kScaleF2x14;
              gPtr += 4;
            }
            else {
              // Affine case:
              //   [A, B]
              //   [C, D]
              remainingSize = blSubOverflow<size_t>(remainingSize, 8, &of);
              if (BL_UNLIKELY(of))
                goto InvalidData;

              matrix.m00 = double(blMemReadI16uBE(gPtr + 0)) * kScaleF2x14;
              matrix.m01 = double(blMemReadI16uBE(gPtr + 2)) * kScaleF2x14;
              matrix.m10 = double(blMemReadI16uBE(gPtr + 4)) * kScaleF2x14;
              matrix.m11 = double(blMemReadI16uBE(gPtr + 6)) * kScaleF2x14;
              gPtr += 8;
            }

            // Translation scale should only happen when `kArgsAreXYValues` is set. The
            // default behavior according to the specification is `kUnscaledComponentOffset`,
            // which can be overridden by setting `kScaledComponentOffset`. However, if both
            // or neither are set then the behavior is the same as `kUnscaledComponentOffset`.
            if ((flags & (Compound::kArgsAreXYValues | Compound::kAnyCompoundOffset    )) ==
                         (Compound::kArgsAreXYValues | Compound::kScaledComponentOffset)) {
              // This is what FreeType does and what's not 100% according to the specificaion.
              // However, according to FreeType this would produce much better offsets so we
              // will match FreeType instead of following the specification.
              matrix.m20 *= blLength(BLPoint(matrix.m00, matrix.m01));
              matrix.m21 *= blLength(BLPoint(matrix.m10, matrix.m11));
            }
          }

          compoundData[compoundLevel].gPtr = gPtr;
          compoundData[compoundLevel].remainingSize = remainingSize;
          compoundData[compoundLevel].compoundFlags = flags;
          blMatrix2DMultiply(matrix, matrix, compoundData[compoundLevel - 1].matrix);
          continue;
        }
      }
    }

    break;
  }

  return BL_SUCCESS;

InvalidData:
  return blTraceError(BL_ERROR_INVALID_DATA);
}

// ============================================================================
// [BLOpenType::GlyfImpl - Init]
// ============================================================================

BLResult init(BLOTFaceImpl* faceI, BLFontTable glyfTable, BLFontTable locaTable) noexcept {
  faceI->glyf.glyfTable = glyfTable;
  faceI->glyf.locaTable = locaTable;

  faceI->funcs.getGlyphBounds = getGlyphBounds;
  faceI->funcs.decodeGlyph = decodeGlyph;

  return BL_SUCCESS;
}

} // {GlyfImpl}
} // {BLOpenType}
