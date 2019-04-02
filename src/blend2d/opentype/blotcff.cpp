// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

#include "../blapi-build_p.h"
#include "../blfont_p.h"
#include "../blmatrix_p.h"
#include "../blpath_p.h"
#include "../blsupport_p.h"
#include "../bltables_p.h"
#include "../bltrace_p.h"
#include "../opentype/blotcff_p.h"
#include "../opentype/blotface_p.h"

namespace BLOpenType {
namespace CFFImpl {

// ============================================================================
// [BLOpenType::CFFImpl - Tracing]
// ============================================================================

#if defined(BL_TRACE_OT_ALL) && !defined(BL_TRACE_OT_CFF)
  #define BL_TRACE_OT_CFF
#endif

#if defined(BL_TRACE_OT_CFF)
  #define Trace BLDebugTrace
#else
  #define Trace BLDummyTrace
#endif

// ============================================================================
// [BLOpenType::CFFImpl - Utilities]
// ============================================================================

// Specified by "CFF - Local/Global Subrs INDEXes"
static BL_INLINE uint16_t calcSubRBias(uint32_t subrCount) noexcept {
  // NOTE: For CharStrings v1 this would return 0, but since OpenType fonts use
  // exclusively CharStrings v2 we always calculate the bias. The calculated
  // bias is added to each call to global or local subroutine before its index
  // is used to get its offset.
  if (subrCount < 1240u)
    return 107u;
  else if (subrCount < 33900u)
    return 1131u;
  else
    return 32768u;
}

template<typename T>
static BL_INLINE uint32_t readOffset(const T* p, size_t offsetSize) noexcept {
  const uint8_t* oPtr = reinterpret_cast<const uint8_t*>(p);
  const uint8_t* oEnd = oPtr + offsetSize;

  uint32_t offset = 0;
  for (;;) {
    offset |= oPtr[0];
    if (++oPtr == oEnd)
      break;
    offset <<= 8;
  }
  return offset;
}

template<typename T>
static BL_INLINE void readOffsetArray(const T* p, size_t offsetSize, uint32_t* offsetArrayOut, size_t n) noexcept {
  const uint8_t* oPtr = reinterpret_cast<const uint8_t*>(p);
  const uint8_t* oEnd = oPtr;

  for (size_t i = 0; i < n; i++) {
    uint32_t offset = 0;
    oEnd += offsetSize;

    for (;;) {
      offset |= oPtr[0];
      if (++oPtr == oEnd)
        break;
      offset <<= 8;
    }

    offsetArrayOut[i] = offset;
  }
}

// Read a CFF floating point value as specified by the CFF specification. The
// format is binary, but it's just a simplified text representation in the end.
//
// Each byte is divided into 2 nibbles (4 bits), which are accessed separately.
// Each nibble contains either a decimal value (0..9), decimal point, or other
// instructions which meaning is described by `NibbleAbove9` enum.
static BLResult readFloat(const uint8_t* p, const uint8_t* pEnd, double& valueOut, size_t& valueSizeInBytes) noexcept {
  // Maximum digits that we would attempt to read, excluding leading zeros.
  enum : uint32_t { kSafeDigits = 15 };

  // Meaning of nibbles above 9.
  enum NibbleAbove9 : uint32_t {
    kDecimalPoint     = 0xA,
    kPositiveExponent = 0xB,
    kNegativeExponent = 0xC,
    kReserved         = 0xD,
    kMinusSign        = 0xE,
    kEndOfNumber      = 0xF
  };

  const uint8_t* pStart = p;
  uint32_t acc = 0x100u;
  uint32_t nib = 0;
  uint32_t flags = 0;

  double value = 0.0;
  uint32_t digits = 0;
  int scale = 0;

  // Value.
  for (;;) {
    if (acc & 0x100u) {
      if (BL_UNLIKELY(p == pEnd))
        return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);
      acc = (uint32_t(*p++) << 24) | 0x1;
    }

    nib = acc >> 28;
    acc <<= 4;

    uint32_t msk = 1u << nib;
    if (nib < 10) {
      if (digits < kSafeDigits) {
        value = value * 10.0 + double(int(nib));
        digits += uint32_t(value != 0.0);
        if (blBitTest(flags, kDecimalPoint))
          scale--;
      }
      else {
        if (!blBitTest(flags, kDecimalPoint))
          scale++;
      }
      flags |= msk;
    }
    else {
      if (BL_UNLIKELY(flags & msk))
        return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

      flags |= msk;
      if (nib == kMinusSign) {
        // Minus must start the string, so check the whole mask...
        if (BL_UNLIKELY(flags & (0xFFFF ^ (1u << kMinusSign))))
          return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);
      }
      else if (nib != kDecimalPoint) {
        break;
      }
    }
  }

  // Exponent.
  if (nib == kPositiveExponent || nib == kNegativeExponent) {
    int expValue = 0;
    int expDigits = 0;
    bool positiveExponent = (nib == kPositiveExponent);

    for (;;) {
      if (acc & 0x100u) {
        if (BL_UNLIKELY(p == pEnd))
          return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);
        acc = (uint32_t(*p++) << 24) | 0x1;
      }

      nib = acc >> 28;
      acc <<= 4;

      if (nib >= 10)
        break;

      // If this happens the data is probably invalid anyway...
      if (BL_UNLIKELY(expDigits >= 6))
        return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

      expValue = expValue * 10u + nib;
      expDigits += int(expValue != 0);
    }

    if (positiveExponent)
      scale += expValue;
    else
      scale -= expValue;
  }

  if (nib != kEndOfNumber)
    return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

  if (scale) {
    double s = blPow(10.0, blAbs(double(scale)));
    value = scale > 0 ? value * s : value / s;
  }

  valueOut = blBitTest(flags, kMinusSign) ? -value : value;
  valueSizeInBytes = (size_t)(p - pStart);

  return BL_SUCCESS;
}

// ============================================================================
// [BLOpenType::CFFImpl - Index]
// ============================================================================

struct Index {
  uint32_t count;
  uint8_t headerSize;
  uint8_t offsetSize;
  uint16_t reserved;
  uint32_t payloadSize;
  uint32_t totalSize;
  const uint8_t* offsets;
  const uint8_t* payload;

  BL_INLINE uint32_t offsetAt(size_t index) const noexcept {
    BL_ASSERT(index <= count);
    return readOffset(offsets + index * offsetSize, offsetSize) - CFFTable::kOffsetAdjustment;
  }
};

// ============================================================================
// [BLOpenType::CFFImpl - DictEntry]
// ============================================================================

struct DictEntry {
  enum : uint32_t { kValueCapacity = 48 };

  uint32_t op;
  uint32_t count;
  uint64_t fpMask;
  double values[kValueCapacity];

  BL_INLINE bool isFpValue(uint32_t index) const noexcept {
    return (fpMask & (uint64_t(1) << index)) != 0;
  }
};

// ============================================================================
// [BLOpenType::CFFImpl - DictIterator]
// ============================================================================

class DictIterator {
public:
  const uint8_t* _dataPtr;
  const uint8_t* _dataEnd;

  BL_INLINE DictIterator() noexcept
    : _dataPtr(nullptr),
      _dataEnd(nullptr) {}

  BL_INLINE DictIterator(const uint8_t* data, size_t size) noexcept
    : _dataPtr(data),
      _dataEnd(data + size) {}

  BL_INLINE void reset(const uint8_t* data, size_t size) noexcept {
    _dataPtr = data;
    _dataEnd = data + size;
  }

  BL_INLINE bool hasNext() const noexcept {
    return _dataPtr != _dataEnd;
  }

  BLResult next(DictEntry& entry) noexcept;
};

// ============================================================================
// [BLOpenType::CFFImpl - ReadIndex]
// ============================================================================

static BLResult readIndex(const void* data, size_t dataSize, uint32_t cffVersion, Index* indexOut) noexcept {
  uint32_t count = 0;
  uint32_t headerSize = 0;

  if (cffVersion == CFFData::kVersion1) {
    if (BL_UNLIKELY(dataSize < 2))
      return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

    count = blMemReadU16uBE(data);
    headerSize = 2;
  }
  else {
    if (BL_UNLIKELY(dataSize < 4))
      return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

    count = blMemReadU32uBE(data);
    headerSize = 4;
  }

  // Index with no data is allowed by the specification.
  if (!count) {
    indexOut->totalSize = headerSize;
    return BL_SUCCESS;
  }

  // Include also `offsetSize` in header, if the `count` is non-zero.
  headerSize++;
  if (BL_UNLIKELY(dataSize < headerSize))
    return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

  uint32_t offsetSize = blMemReadU8(blOffsetPtr<const uint8_t>(data, headerSize - 1));
  uint32_t offsetArraySize = (count + 1) * offsetSize;
  uint32_t indexSizeIncludingOffsets = headerSize + offsetArraySize;

  if (BL_UNLIKELY(offsetSize < 1 || offsetSize > 4 || indexSizeIncludingOffsets > dataSize))
    return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

  const uint8_t* offsetArray = blOffsetPtr<const uint8_t>(data, headerSize);
  uint32_t offset = readOffset(offsetArray, offsetSize);

  // The first offset should be 1.
  if (BL_UNLIKELY(offset != 1))
    return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

  // Validate that the offsets are increasing and don't cross each other.
  // The specification says that size of each object stored in the table
  // can be determined by checking its offset and the next one, so valid
  // data should conform to these checks.
  //
  // Please notice the use of `kOffsetAdjustment`. Since all offsets are
  // relative to "RELATIVE TO THE BYTE THAT PRECEDES THE OBJECT DATA" we
  // must take that into consideration.
  uint32_t maxOffset = uint32_t(blMin<size_t>(blMaxValue<uint32_t>(), dataSize - indexSizeIncludingOffsets + CFFTable::kOffsetAdjustment));

  switch (offsetSize) {
    case 1: {
      for (uint32_t i = 1; i <= count; i++) {
        uint32_t next = blMemReadU8(offsetArray + i);
        if (BL_UNLIKELY(next < offset || next > maxOffset))
          return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);
        offset = next;
      }
      break;
    }

    case 2:
      for (uint32_t i = 1; i <= count; i++) {
        uint32_t next = blMemReadU16uBE(offsetArray + i * 2u);
        if (BL_UNLIKELY(next < offset || next > maxOffset))
          return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);
        offset = next;
      }
      break;

    case 3:
      for (uint32_t i = 1; i <= count; i++) {
        uint32_t next = blMemReadU24uBE(offsetArray + i * 3u);
        if (BL_UNLIKELY(next < offset || next > maxOffset))
          return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);
        offset = next;
      }
      break;

    case 4:
      for (uint32_t i = 1; i <= count; i++) {
        uint32_t next = blMemReadU32uBE(offsetArray + i * 4u);
        if (BL_UNLIKELY(next < offset || next > maxOffset))
          return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);
        offset = next;
      }
      break;
  }

  const uint8_t* payload = offsetArray + offsetArraySize;
  uint32_t payloadSize = offset - 1;

  indexOut->count = count;
  indexOut->headerSize = uint8_t(headerSize);
  indexOut->offsetSize = uint8_t(offsetSize);
  indexOut->reserved = 0;
  indexOut->payloadSize = payloadSize;
  indexOut->totalSize = size_t(headerSize) + size_t(offsetArraySize) + size_t(payloadSize);
  indexOut->offsets = offsetArray;
  indexOut->payload = payload;

  return BL_SUCCESS;
}

// ============================================================================
// [BLOpenType::CFFImpl - DictIterator]
// ============================================================================

BLResult DictIterator::next(DictEntry& entry) noexcept {
  BL_ASSERT(hasNext());

  uint32_t op = 0;
  uint32_t i = 0;

  for (;;) {
    uint32_t b0 = *_dataPtr++;

    // Operators are encoded in range [0..21].
    if (b0 < 22) {
      // 12 is a special escape code to encode additional operators.
      if (b0 == CFFTable::kEscapeDictOp) {
        if (BL_UNLIKELY(_dataPtr == _dataEnd))
          return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);
        b0 = (b0 << 8) | (*_dataPtr++);
      }
      op = b0;
      break;
    }
    else {
      double v;

      if (b0 == 30) {
        size_t size;
        BL_PROPAGATE(readFloat(_dataPtr, _dataEnd, v, size));

        entry.fpMask |= uint64_t(1) << i;
        _dataPtr += size;
      }
      else {
        int32_t vInt = 0;
        if (b0 >= 32 && b0 <= 246) {
          vInt = int32_t(b0) - 139;
        }
        else if (b0 >= 247 && b0 <= 254) {
          if (BL_UNLIKELY(_dataPtr == _dataEnd))
            return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

          uint32_t b1 = *_dataPtr++;
          vInt = b0 <= 250 ? (108 - 247 * 256) + int(b0 * 256 + b1)
                           : (251 * 256 - 108) - int(b0 * 256 + b1);
        }
        else if (b0 == 28) {
          _dataPtr += 2;
          if (BL_UNLIKELY(_dataPtr > _dataEnd))
            return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);
          vInt = blMemReadI16uBE(_dataPtr - 2);
        }
        else if (b0 == 29) {
          _dataPtr += 4;
          if (BL_UNLIKELY(_dataPtr > _dataEnd))
            return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);
          vInt = blMemReadI32uBE(_dataPtr - 4);
        }
        else {
          // Byte values 22..27, 31, and 255 are reserved.
          return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);
        }

        v = double(vInt);
      }

      if (BL_UNLIKELY(i == DictEntry::kValueCapacity - 1))
        return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

      entry.values[i++] = v;
    }
  }

  // Specification doesn't talk about entries that have no values.
  if (BL_UNLIKELY(!i))
    return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

  entry.op = op;
  entry.count = i;

  return BL_SUCCESS;
}

// ============================================================================
// [BLOpenType::CFFImpl - Constants]
// ============================================================================

// ADOBE uses a limit of 20 million instructions in their AVALON rasterizer, but
// it's not clear that it's because of font complexity or their PostScript support.
//
// It seems that this limit is too optimistic to be reached by any OpenType font.
// We use a different metric, a program size, which is referenced by `bytesProcessed`
// counter in the decoder. This counter doesn't have to be advanced every time we
// process an opcode, instead, we advance it every time we enter a subroutine (or
// CharString program itself). if we reach `kCFFProgramLimit` the interpreter is
// terminated immediately.
static constexpr uint32_t kCFFProgramLimit = 1000000;
static constexpr uint32_t kCFFCallStackSize = 10;

static constexpr uint32_t kCFFValueStackSizeV1 = 48;
static constexpr uint32_t kCFFValueStackSizeV2 = 513;

// We use `double` precision in our implementation, so this constant is used
// to convert a fixed-point (as specified by CFF and CFF2)
static constexpr double kCFFDoubleFromF16x16 = (1.0 / 65536.0);

enum CSFlags : uint32_t {
  kCSFlagHasWidth  = 0x01, // Width has been already parsed (implicit in CFF2 mode).
  kCSFlagPathOpen  = 0x02  // Path is open (set after the first 'MoveTo').
};

enum CSOpCode : uint32_t {
  // We use the same notation as used by ADOBE specifications:
  //
  //   |- at the beginning means the beginning (bottom) of the stack.
  //   |- at the end means stack-cleaning operator.
  //    - at the end means to pop stack by one.
  //
  // CFF Version 1
  // -------------
  //
  // The first stack-clearing operator, which must be one of 'MoveTo', 'Stem',
  // 'Hint', or 'EndChar', takes an additional argument - the width, which may
  // be expressed as zero or one numeric argument.
  //
  // CFF Version 2
  // -------------
  //
  // The concept of "width" specified in the program was removed. Arithmetic and
  // Conditional operators were also removed and control flow operators like
  // 'Return' and 'EndChar' were made implicit and were removed as well.

  // Core Operators / Escapes:
  kCSOpEscape     = 0x000C,
  kCSOpPushI16    = 0x001C,
  kCSOpPushF16x16 = 0x00FF,

  // Path Construction Operators:
  kCSOpRMoveTo    = 0x0015, // CFFv*: |- dx1 dy1         rmoveto (21) |-
  kCSOpHMoveTo    = 0x0016, // CFFv*: |- dx1             hmoveto (22) |-
  kCSOpVMoveTo    = 0x0004, // CFFv*: |- dy1             vmoveto (4)  |-
  kCSOpRLineTo    = 0x0005, // CFFv*: |- {dxa dya}+      rlineto (5)  |-
  kCSOpHLineTo    = 0x0006, // CFFv*: |- dx1 {dya dxb}*  hlineto (6)  |-   or   |- {dxa dyb}+    hlineto    (6)  |-
  kCSOpVLineTo    = 0x0007, // CFFv*: |- dy1 {dxa dyb}*  vlineto (7)  |-   or   |- {dya dxb}+    vlineto    (7)  |-

  kCSOpRRCurveTo  = 0x0008, // CFFv*: |-                 {dxa dya dxb dyb dxc dyc}+              rrcurveto  (8)  |-
  kCSOpVVCurveTo  = 0x001A, // CFFv*: |- dx1?            {dya dxb dyb dyc}+                      vvcurveto  (26) |-
  kCSOpHHCurveTo  = 0x001B, // CFFv*: |- dy1?            {dxa dxb dyb dxc}+                      hhcurveto  (27) |-
  kCSOpVHCurveTo  = 0x001E, // CFFv*: |- dy1 dx2 dy2 dx3 {dxa dxb dyb dyc dyd dxe dye dxf}* dyf? vhcurveto  (30) |-
                            // CFFv*: |-                 {dya dxb dyb dxc dxd dxe dye dyf}+ dxf? vhcurveto  (30) |-
  kCSOpHVCurveTo  = 0x001F, // CFFv*: |- dx1 dx2 dy2 dy3 {dya dxb dyb dxc dxd dxe dye dyf}* dxf? hvcurveto  (31) |-
                            // CFFv*: |-                 {dxa dxb dyb dyc dyd dxe dye dxf}+ dyf? hvcurveto  (31) |-
  kCSOpRCurveLine = 0x0018, // CFFv*: |-                 {dxa dya dxb dyb dxc dyc}+ dxd dyd      rcurveline (24) |-
  kCSOpRLineCurve = 0x0019, // CFFv*: |-                 {dxa dya}+ dxb dyb dxc dyc dxd dyd      rlinecurve (25) |-

  kCSOpFlex       = 0x0C23, // CFFv*: |- dx1 dy1 dx2 dy2 dx3 dy3 dx4 dy4 dx5 dy5 dx6 dy6 fd      flex    (12 35) |-
  kCSOpFlex1      = 0x0C25, // CFFv*: |- dx1 dy1 dx2 dy2 dx3 dy3 dx4 dy4 dx5 dy5 d6              flex1   (12 37) |-
  kCSOpHFlex      = 0x0C22, // CFFv*: |- dx1 dx2 dy2 dx3 dx4 dx5 dx6                             hflex   (12 34) |-
  kCSOpHFlex1     = 0x0C24, // CFFv*: |- dx1 dy1 dx2 dy2 dx3 dx4 dx5 dy5 dx6                     hflex1  (12 36) |-

  // Hint Operators:
  kCSOpHStem      = 0x0001, // CFFv*: |- y dy {dya dyb}* hstem     (1)       |-
  kCSOpVStem      = 0x0003, // CFFv*: |- x dx {dxa dxb}* vstem     (3)       |-
  kCSOpHStemHM    = 0x0012, // CFFv*: |- y dy {dya dyb}* hstemhm   (18)      |-
  kCSOpVStemHM    = 0x0017, // CFFv*: |- x dx {dxa dxb}* vstemhm   (23)      |-
  kCSOpHintMask   = 0x0013, // CFFv*: |-                 hintmask  (19) mask |-
  kCSOpCntrMask   = 0x0014, // CFFv*: |-                 cntrmask  (20) mask |-

  // Variation Data Operators:
  kCSOpVSIndex    = 0x000F, // CFFv2: |- ivs vsindex (15) |-
  kCSOpBlend      = 0x0010, // CFFv2: in(0)...in(N-1), d(0,0)...d(K-1,0), d(0,1)...d(K-1,1) ... d(0,N-1)...d(K-1,N-1) N blend (16) out(0)...(N-1)

  // Control Flow Operators:
  kCSOpCallLSubR  = 0x000A, // CFFv*:          lsubr# calllsubr (10) -
  kCSOpCallGSubR  = 0x001D, // CFFv*:          gsubr# callgsubr (29) -
  kCSOpReturn     = 0x000B, // CFFv1:                 return    (11)
  kCSOpEndChar    = 0x000E, // CFFv1:                 endchar   (14)

  // Conditional & Arithmetic Operators (CFFv1 only!):
  kCSOpAnd        = 0x0C03, // CFFv1: in1 in2         and    (12 3)  out {in1 && in2}
  kCSOpOr         = 0x0C04, // CFFv1: in1 in2         or     (12 4)  out {in1 || in2}
  kCSOpEq         = 0x0C0F, // CFFv1: in1 in2         eq     (12 15) out {in1 == in2}
  kCSOpIfElse     = 0x0C16, // CFFv1: s1 s2 v1 v2     ifelse (12 22) out {v1 <= v2 ? s1 : s2}
  kCSOpNot        = 0x0C05, // CFFv1: in              not    (12 5)  out {!in}
  kCSOpNeg        = 0x0C0E, // CFFv1: in              neg    (12 14) out {-in}
  kCSOpAbs        = 0x0C09, // CFFv1: in              abs    (12 9)  out {abs(in)}
  kCSOpSqrt       = 0x0C1A, // CFFv1: in              sqrt   (12 26) out {sqrt(in)}
  kCSOpAdd        = 0x0C0A, // CFFv1: in1 in2         add    (12 10) out {in1 + in2}
  kCSOpSub        = 0x0C0B, // CFFv1: in1 in2         sub    (12 11) out {in1 - in2}
  kCSOpMul        = 0x0C18, // CFFv1: in1 in2         mul    (12 24) out {in1 * in2}
  kCSOpDiv        = 0x0C0C, // CFFv1: in1 in2         div    (12 12) out {in1 / in2}
  kCSOpRandom     = 0x0C17, // CFFv1:                 random (12 23) out
  kCSOpDup        = 0x0C1B, // CFFv1: in              dup    (12 27) out out
  kCSOpDrop       = 0x0C12, // CFFv1: in              drop   (12 18)
  kCSOpExch       = 0x0C1C, // CFFv1: in1 in2         exch   (12 28) out1 out2
  kCSOpIndex      = 0x0C1D, // CFFv1: nX...n0 I       index  (12 29) nX...n0 n[I]
  kCSOpRoll       = 0x0C1E, // CFFv1: n(N–1)...n0 N J roll   (12 30) n((J–1) % N)...n0 n(N–1)...n(J % N)

  // Storage Operators (CFFv1 only!):
  kCSOpPut        = 0x0C14, // CFFv1: in I put (12 20)
  kCSOpGet        = 0x0C15  // CFFv1:    I get (12 21) out
};

// ============================================================================
// [BLOpenType::CFFImpl - ExecutionFeaturesInfo]
// ============================================================================

//! Describes features that can be used during execution and their requirements.
//!
//! There are two versions of `ExecutionFeaturesInfo` selected at runtime based on
//! the font - either CFF or CFF2. CFF provides some operators that are hardly
//! used in fonts. CFF2 removed such operators and introduced new ones that are
//! used to support "OpenType BLFont Variations" feature.
//!
//! Both CFF and CFF2 specifications state that unsupported operators should be
//! skipped and value stack cleared. This is implemented by assigning `kUnknown`
//! to all operators that are unsupported. The value is much higher than a
//! possible value stack size so when it's used it would always force the engine
//! to decide between an unsupported operator or operator that was called with
//! less operands than it needs (in that case the execution is terminated
//! immediately).
struct ExecutionFeaturesInfo {
  static constexpr const uint32_t kBaseOpCount = 32;
  static constexpr const uint32_t kEscapedOpCount = 48;
  static constexpr const uint16_t kUnknown = 0xFFFFu;

  //! Stack size required to process a base operator.
  BLLookupTable<uint16_t, kBaseOpCount> baseOpStackSize;
  //! Stack size required to process an escaped operator.
  BLLookupTable<uint16_t, kEscapedOpCount> escapedOpStackSize;
};

template<uint32_t Escape, uint32_t V>
struct ExecutionFeaturesInfoOpStackSizeGen {
  static constexpr uint16_t value(size_t op) noexcept {
    return ((op | Escape) == kCSOpEscape           ) ? uint16_t(0)
         : ((op | Escape) == kCSOpPushI16          ) ? uint16_t(0)

         : ((op | Escape) == kCSOpRMoveTo          ) ? uint16_t(2)
         : ((op | Escape) == kCSOpHMoveTo          ) ? uint16_t(1)
         : ((op | Escape) == kCSOpVMoveTo          ) ? uint16_t(1)
         : ((op | Escape) == kCSOpRLineTo          ) ? uint16_t(2)
         : ((op | Escape) == kCSOpHLineTo          ) ? uint16_t(1)
         : ((op | Escape) == kCSOpVLineTo          ) ? uint16_t(1)
         : ((op | Escape) == kCSOpRRCurveTo        ) ? uint16_t(6)
         : ((op | Escape) == kCSOpHHCurveTo        ) ? uint16_t(4)
         : ((op | Escape) == kCSOpVVCurveTo        ) ? uint16_t(4)
         : ((op | Escape) == kCSOpVHCurveTo        ) ? uint16_t(4)
         : ((op | Escape) == kCSOpHVCurveTo        ) ? uint16_t(4)
         : ((op | Escape) == kCSOpRCurveLine       ) ? uint16_t(8)
         : ((op | Escape) == kCSOpRLineCurve       ) ? uint16_t(8)

         : ((op | Escape) == kCSOpFlex             ) ? uint16_t(13)
         : ((op | Escape) == kCSOpFlex1            ) ? uint16_t(11)
         : ((op | Escape) == kCSOpHFlex            ) ? uint16_t(7)
         : ((op | Escape) == kCSOpHFlex1           ) ? uint16_t(9)

         : ((op | Escape) == kCSOpHStem            ) ? uint16_t(2)
         : ((op | Escape) == kCSOpVStem            ) ? uint16_t(2)
         : ((op | Escape) == kCSOpHStemHM          ) ? uint16_t(2)
         : ((op | Escape) == kCSOpVStemHM          ) ? uint16_t(2)
         : ((op | Escape) == kCSOpHintMask         ) ? uint16_t(0)
         : ((op | Escape) == kCSOpCntrMask         ) ? uint16_t(0)

         : ((op | Escape) == kCSOpCallLSubR        ) ? uint16_t(1)
         : ((op | Escape) == kCSOpCallGSubR        ) ? uint16_t(1)
         : ((op | Escape) == kCSOpReturn  && V == 1) ? uint16_t(0)
         : ((op | Escape) == kCSOpEndChar && V == 1) ? uint16_t(0)

         : ((op | Escape) == kCSOpVSIndex && V == 2) ? uint16_t(1)
         : ((op | Escape) == kCSOpBlend   && V == 2) ? uint16_t(1)

         : ((op | Escape) == kCSOpAnd     && V == 1) ? uint16_t(2)
         : ((op | Escape) == kCSOpOr      && V == 1) ? uint16_t(2)
         : ((op | Escape) == kCSOpEq      && V == 1) ? uint16_t(2)
         : ((op | Escape) == kCSOpIfElse  && V == 1) ? uint16_t(4)
         : ((op | Escape) == kCSOpNot     && V == 1) ? uint16_t(1)
         : ((op | Escape) == kCSOpNeg     && V == 1) ? uint16_t(1)
         : ((op | Escape) == kCSOpAbs     && V == 1) ? uint16_t(1)
         : ((op | Escape) == kCSOpSqrt    && V == 1) ? uint16_t(1)
         : ((op | Escape) == kCSOpAdd     && V == 1) ? uint16_t(2)
         : ((op | Escape) == kCSOpSub     && V == 1) ? uint16_t(2)
         : ((op | Escape) == kCSOpMul     && V == 1) ? uint16_t(2)
         : ((op | Escape) == kCSOpDiv     && V == 1) ? uint16_t(2)
         : ((op | Escape) == kCSOpRandom  && V == 1) ? uint16_t(0)
         : ((op | Escape) == kCSOpDup     && V == 1) ? uint16_t(1)
         : ((op | Escape) == kCSOpDrop    && V == 1) ? uint16_t(1)
         : ((op | Escape) == kCSOpExch    && V == 1) ? uint16_t(2)
         : ((op | Escape) == kCSOpIndex   && V == 1) ? uint16_t(2)
         : ((op | Escape) == kCSOpRoll    && V == 1) ? uint16_t(2)
         : ((op | Escape) == kCSOpPut     && V == 1) ? uint16_t(2)
         : ((op | Escape) == kCSOpGet     && V == 1) ? uint16_t(1)

         : ExecutionFeaturesInfo::kUnknown;
  }
};

static constexpr const ExecutionFeaturesInfo executionFeaturesInfo[2] = {
  // CFFv1 [Index #0]
  {
    blLookupTable<uint16_t, ExecutionFeaturesInfo::kBaseOpCount   , ExecutionFeaturesInfoOpStackSizeGen<0x0000, 1>>(),
    blLookupTable<uint16_t, ExecutionFeaturesInfo::kEscapedOpCount, ExecutionFeaturesInfoOpStackSizeGen<0x0C00, 1>>()
  },

  // CFFv2 [Index #1]
  {
    blLookupTable<uint16_t, ExecutionFeaturesInfo::kBaseOpCount   , ExecutionFeaturesInfoOpStackSizeGen<0x0000, 2>>(),
    blLookupTable<uint16_t, ExecutionFeaturesInfo::kEscapedOpCount, ExecutionFeaturesInfoOpStackSizeGen<0x0C00, 2>>()
  }
};

// ============================================================================
// [BLOpenType::CFFImpl - ExecutionState]
// ============================================================================

//! Execution state is used in a call-stack array to remember from where a
//! subroutine was called. When a subroutine reaches the end of a "Return"
//! opcode it would pop the state from call-stack and return the execution
//! after the "CallLSubR" or "CallGSubR" instruction.
struct ExecutionState {
  BL_INLINE void reset(const uint8_t* ptr, const uint8_t* end) noexcept {
    _ptr = ptr;
    _end = end;
  }

  const uint8_t* _ptr;
  const uint8_t* _end;
};

// ============================================================================
// [BLOpenType::CFFImpl - Matrix2x2]
// ============================================================================

struct Matrix2x2 {
  BL_INLINE double xByA(double x, double y) const noexcept { return x * m00 + y * m10; }
  BL_INLINE double yByA(double x, double y) const noexcept { return x * m01 + y * m11; }

  BL_INLINE double xByX(double x) const noexcept { return x * m00; }
  BL_INLINE double xByY(double y) const noexcept { return y * m10; }

  BL_INLINE double yByX(double x) const noexcept { return x * m01; }
  BL_INLINE double yByY(double y) const noexcept { return y * m11; }

  double m00, m01;
  double m10, m11;
};

// ============================================================================
// [BLOpenType::CFFImpl - Trace]
// ============================================================================

#ifdef BL_TRACE_OT_CFF
static void traceCharStringOp(BLOTFaceImpl* faceI, Trace& trace, uint32_t op, const double* values, size_t count) noexcept {
  char buf[64];
  const char* opName = "";

  switch (op) {
    #define CASE(op) case kCSOp##op: opName = #op; break

    CASE(Escape);
    CASE(PushI16);
    CASE(PushF16x16);

    CASE(RMoveTo);
    CASE(HMoveTo);
    CASE(VMoveTo);
    CASE(RLineTo);
    CASE(HLineTo);
    CASE(VLineTo);
    CASE(RRCurveTo);
    CASE(HHCurveTo);
    CASE(HVCurveTo);
    CASE(VHCurveTo);
    CASE(VVCurveTo);
    CASE(RCurveLine);
    CASE(RLineCurve);
    CASE(Flex);
    CASE(Flex1);
    CASE(HFlex);
    CASE(HFlex1);

    CASE(HStem);
    CASE(VStem);
    CASE(HStemHM);
    CASE(VStemHM);
    CASE(HintMask);
    CASE(CntrMask);

    CASE(CallLSubR);
    CASE(CallGSubR);
    CASE(Return);
    CASE(EndChar);

    CASE(VSIndex);
    CASE(Blend);

    CASE(And);
    CASE(Or);
    CASE(Eq);
    CASE(IfElse);
    CASE(Not);
    CASE(Neg);
    CASE(Abs);
    CASE(Sqrt);
    CASE(Add);
    CASE(Sub);
    CASE(Mul);
    CASE(Div);
    CASE(Random);
    CASE(Drop);
    CASE(Exch);
    CASE(Index);
    CASE(Roll);
    CASE(Dup);

    CASE(Put);
    CASE(Get);

    #undef CASE

    default:
      std::snprintf(buf, BL_ARRAY_SIZE(buf), "Op #%04X", op);
      opName = buf;
      break;
  }

  trace.info("%s", opName);
  if (count) {
    trace.out(" [");
    for (size_t i = 0; i < count; i++)
      trace.out(i == 0 ? "%g" : " %g", values[i]);
    trace.out("]");
  }

  if (count > 0 && (op == kCSOpCallGSubR || op == kCSOpCallLSubR)) {
    int32_t idx = int32_t(values[count - 1]);
    idx += faceI->cff.index[op == kCSOpCallLSubR ? CFFData::kIndexLSubR : CFFData::kIndexGSubR].bias;
    trace.out(" {SubR #%d}", idx);
  }

  trace.out("\n");
}
#endif

// ============================================================================
// [BLOpenType::CFFImpl - DecodeGlyph - Implementation]
// ============================================================================

static BLResult BL_CDECL decodeGlyph(
  const BLFontFaceImpl* faceI_,
  uint32_t glyphId,
  const BLMatrix2D* matrix,
  BLPath* out,
  BLMemBuffer* tmpBuffer,
  BLPathSinkFunc sink, size_t sinkGlyphIndex, void* closure) noexcept {

  BL_UNUSED(tmpBuffer);

  const BLOTFaceImpl* faceI = static_cast<const BLOTFaceImpl*>(faceI_);

  // Will only do something if tracing is enabled.
  Trace trace;
  trace.info("BLOpenType::CFFImpl::DecodeGlyph #%u\n", glyphId);
  trace.indent();

  // --------------------------------------------------------------------------
  // [Prepare for Execution]
  // --------------------------------------------------------------------------

  const uint8_t* ip    = nullptr;                // Pointer in the instruction array.
  const uint8_t* ipEnd = nullptr;                // End of the instruction array.

  ExecutionState cBuf[kCFFCallStackSize + 1]; // Call stack.
  double vBuf[kCFFValueStackSizeV1 + 1];         // Value stack.

  uint32_t cIdx = 0;                             // Call stack index.
  uint32_t vIdx = 0;                             // Value stack index.
  size_t bytesProcessed = 0;                     // Bytes processed, increasing counter.

  uint32_t hintBitCount = 0;                     // Number of bits required by 'HintMask' and 'CntrMask' operators.
  uint32_t executionFlags = 0;                   // Execution status flags.
  uint32_t vMinOperands = 0;                     // Minimum operands the current opcode requires (updated per opcode).

  double px = matrix->m20;                       // Current X coordinate.
  double py = matrix->m21;                       // Current Y coordinate.

  // Sink information.
  BLGlyphOutlineSinkInfo sinkInfo;
  sinkInfo.glyphIndex = sinkGlyphIndex;
  sinkInfo.contourCount = 0;

  const CFFData& cffInfo = faceI->cff;
  const uint8_t* cffData = faceI->cff.table.data;

  // Execution features describe either CFFv1 or CFFv2 environment. It contains
  // minimum operand count for each opcode (or operator) and some other data.
  const ExecutionFeaturesInfo* executionFeatures = &executionFeaturesInfo[0];

  BLPathAppender appender;
  BL_PROPAGATE(appender.beginAppend(out, 64));

  // This is used to perform a function (subroutine) call. Initially we set it
  // to the charstring referenced by the `glyphId`. Later, when we process a
  // a function call opcode it would be changed to either GSubR or LSubR index.
  const CFFData::IndexData* subrIndex = &cffInfo.index[CFFData::kIndexCharString];
  uint32_t subrId = glyphId;

  // We really want to report a correct error when we face an invalid glyphId,
  // this is the only difference between handling a function call and handling
  // the initial CharString program.
  if (BL_UNLIKELY(glyphId >= subrIndex->entryCount)) {
    trace.fail("Invalid Glyph ID\n");
    return blTraceError(BL_ERROR_INVALID_GLYPH);
  }

  // Compiler can better optimize the transform if it knows that it won't be
  // changed outside of this function by either calling `sink` or realloc.
  Matrix2x2 m { matrix->m00, matrix->m01, matrix->m10, matrix->m11 };

  // --------------------------------------------------------------------------
  // [Program | SubR - Init]
  // --------------------------------------------------------------------------

OnSubRCall:
  {
    uint32_t offsetSize = subrIndex->offsetSize;
    uint32_t payloadSize = subrIndex->payloadSize();

    ip = cffData + subrIndex->dataRange.offset;

    uint32_t oArray[2];
    readOffsetArray(ip + subrIndex->offsetsOffset() + subrId * offsetSize, offsetSize, oArray, 2);

    ip += subrIndex->payloadOffset();
    ipEnd = ip;

    oArray[0] -= CFFTable::kOffsetAdjustment;
    oArray[1] -= CFFTable::kOffsetAdjustment;

    if (BL_UNLIKELY(oArray[0] >= oArray[1] || oArray[1] > payloadSize)) {
      trace.fail("Invalid SubR range [Start=%u End=%u Max=%u]\n", oArray[0], oArray[1], payloadSize);
      return blTraceError(BL_ERROR_INVALID_DATA);
    }

    ip    += oArray[0];
    ipEnd += oArray[1];

    size_t programSize = oArray[1] - oArray[0];
    if (BL_UNLIKELY(kCFFProgramLimit - bytesProcessed < programSize)) {
      trace.fail("Program limit exceeded [%zu bytes processed]\n", bytesProcessed);
      return blTraceError(BL_ERROR_FONT_PROGRAM_TERMINATED);
    }
    bytesProcessed += programSize;
  }

  // --------------------------------------------------------------------------
  // [Program | SubR - Execute]
  // --------------------------------------------------------------------------

  for (;;) {
    // Current opcode read from `ip`.
    uint32_t b0;

    if (BL_UNLIKELY(ip >= ipEnd)) {
      // CFF vs CFF2 diverged a bit. CFF2 doesn't require 'Return' and 'EndChar'
      // operators and made them implicit. When we reach the end of the current
      // subroutine then a 'Return' is implied, similarly when we reach the end
      // of the current CharString 'EndChar' is implied as well.
      if (cIdx > 0)
        goto OnReturn;
      break;
    }

    // Read the opcode byte.
    b0 = *ip++;

    if (b0 >= 32) {
      if (BL_UNLIKELY(++vIdx > kCFFValueStackSizeV1)) {
        goto InvalidData;
      }
      else {
        // --------------------------------------------------------------------
        // [Push Number (Small)]
        // --------------------------------------------------------------------

        if (ip < ipEnd) {
          if (b0 <= 246) {
            // Number in range [-107..107].
            int v = int(b0) - 139;
            vBuf[vIdx - 1] = double(v);

            // There is a big chance that there would be another number. If it's
            // true then this acts as 2x unrolled push. If not then we perform
            // a direct jump to handle the operator as we would have done anyway.
            b0 = *ip++;
            if (b0 < 32)
              goto OnOperator;

            if (BL_UNLIKELY(++vIdx >= kCFFValueStackSizeV1))
              goto InvalidData;

            if (b0 <= 246) {
              v = int(b0) - 139;
              vBuf[vIdx - 1] = double(v);
              continue;
            }

            if (ip == ipEnd)
              goto InvalidData;
          }

          if (b0 <= 254) {
            // Number in range [-1131..-108] or [108..1131].
            uint32_t b1 = *ip++;
            int v = b0 <= 250 ? (108 - 247 * 256) + int(b0 * 256 + b1)
                              : (251 * 256 - 108) - int(b0 * 256 + b1);

            vBuf[vIdx - 1] = double(v);
          }
          else {
            // Number encoded as 16x16 fixed-point.
            BL_ASSERT(b0 == kCSOpPushF16x16);

            ip += 4;
            if (BL_UNLIKELY(ip > ipEnd))
              goto InvalidData;

            int v = blMemReadI32uBE(ip - 4);
            vBuf[vIdx - 1] = double(v) * kCFFDoubleFromF16x16;
          }
          continue;
        }
        else {
          // If this is the end of the program the number must be in range [-107..107].
          if (b0 > 246)
            goto InvalidData;

          // Number in range [-107..107].
          int v = int(b0) - 139;
          vBuf[vIdx - 1] = double(v);
          continue;
        }
      }
    }
    else {
OnOperator:
      #ifdef BL_TRACE_OT_CFF
      traceCharStringOp(self, trace, b0, vBuf, vIdx);
      #endif

      vMinOperands = executionFeatures->baseOpStackSize[b0];
      if (BL_UNLIKELY(vIdx < vMinOperands)) {
        // If this is not an unknown operand it would mean that we have less
        // values on stack than the operator requires. That's an error in CS.
        if (vMinOperands != ExecutionFeaturesInfo::kUnknown)
          goto InvalidData;

        // Unknown operators should clear the stack and act as NOPs.
        vIdx = 0;
        continue;
      }

      switch (b0) {
        // --------------------------------------------------------------------
        // [Push Number (2's Complement Int16)]
        // --------------------------------------------------------------------

        case kCSOpPushI16: {
          ip += 2;
          if (BL_UNLIKELY(ip > ipEnd || ++vIdx > kCFFValueStackSizeV1))
            goto InvalidData;

          int v = blMemReadI16uBE(ip - 2);
          vBuf[vIdx - 1] = double(v);
          continue;
        }

        // --------------------------------------------------------------------
        // [MoveTo]
        // --------------------------------------------------------------------

        // |- dx1 dy1 rmoveto (21) |-
        case kCSOpRMoveTo: {
          BL_ASSERT(vMinOperands >= 2);
          BL_PROPAGATE(appender.ensure(out, 2));

          if (executionFlags & kCSFlagPathOpen)
            appender.close();

          px += m.xByA(vBuf[vIdx - 2], vBuf[vIdx - 1]);
          py += m.yByA(vBuf[vIdx - 2], vBuf[vIdx - 1]);

          appender.moveTo(px, py);
          sinkInfo.contourCount++;

          vIdx = 0;
          executionFlags |= kCSFlagHasWidth | kCSFlagPathOpen;
          continue;
        }

        // |- dx1 hmoveto (22) |-
        case kCSOpHMoveTo: {
          BL_ASSERT(vMinOperands >= 1);
          BL_PROPAGATE(appender.ensure(out, 2));

          if (executionFlags & kCSFlagPathOpen)
            appender.close();

          px += m.xByX(vBuf[vIdx - 1]);
          py += m.yByX(vBuf[vIdx - 1]);

          appender.moveTo(px, py);
          sinkInfo.contourCount++;

          vIdx = 0;
          executionFlags |= kCSFlagHasWidth | kCSFlagPathOpen;
          continue;
        }

        // |- dy1 vmoveto (4) |-
        case kCSOpVMoveTo: {
          BL_ASSERT(vMinOperands >= 1);
          BL_PROPAGATE(appender.ensure(out, 2));

          if (executionFlags & kCSFlagPathOpen)
            appender.close();

          px += m.xByY(vBuf[vIdx - 1]);
          py += m.yByY(vBuf[vIdx - 1]);

          appender.moveTo(px, py);
          sinkInfo.contourCount++;

          vIdx = 0;
          executionFlags |= kCSFlagHasWidth | kCSFlagPathOpen;
          continue;
        }

        // --------------------------------------------------------------------
        // [LineTo]
        // --------------------------------------------------------------------

        // |- {dxa dya}+ rlineto (5) |-
        case kCSOpRLineTo: {
          BL_ASSERT(vMinOperands >= 2);
          BL_PROPAGATE(appender.ensure(out, (vIdx + 1) / 2u));

          // NOTE: The specification talks about a pair of numbers, however,
          // other implementations like FreeType allow odd number of arguments
          // implicitly adding zero as the last one argument missing... It's a
          // specification violation that we follow for compatibility reasons.
          size_t i = 0;
          while ((i += 2) <= vIdx) {
            px += m.xByA(vBuf[i - 2], vBuf[i - 1]);
            py += m.yByA(vBuf[i - 2], vBuf[i - 1]);
            appender.lineTo(px, py);
          }

          if (vIdx & 1) {
            px += m.xByX(vBuf[vIdx - 1]);
            py += m.yByX(vBuf[vIdx - 1]);
            appender.lineTo(px, py);
          }

          vIdx = 0;
          continue;
        }

        // |- dx1 {dya dxb}* hlineto (6) |- or |- {dxa dyb}+ hlineto (6) |-
        // |- dy1 {dxa dyb}* vlineto (7) |- or |- {dya dxb}+ vlineto (7) |-
        case kCSOpHLineTo:
        case kCSOpVLineTo: {
          BL_ASSERT(vMinOperands >= 1);
          BL_PROPAGATE(appender.ensure(out, vIdx));

          size_t i = 0;
          if (b0 == kCSOpVLineTo)
            goto OnVLineTo;

          for (;;) {
            px += m.xByX(vBuf[i]);
            py += m.yByX(vBuf[i]);
            appender.lineTo(px, py);

            if (++i >= vIdx)
              break;
OnVLineTo:
            px += m.xByY(vBuf[i]);
            py += m.yByY(vBuf[i]);
            appender.lineTo(px, py);

            if (++i >= vIdx)
              break;
          }

          vIdx = 0;
          continue;
        }

        // --------------------------------------------------------------------
        // [CurveTo]
        // --------------------------------------------------------------------

        // |- {dxa dya dxb dyb dxc dyc}+ rrcurveto (8) |-
        case kCSOpRRCurveTo: {
          BL_ASSERT(vMinOperands >= 6);
          BL_PROPAGATE(appender.ensure(out, vIdx / 2u));

          size_t i = 0;
          double x1, y1, x2, y2;

          while ((i += 6) <= vIdx) {
            x1 = px + m.xByA(vBuf[i - 6], vBuf[i - 5]);
            y1 = py + m.yByA(vBuf[i - 6], vBuf[i - 5]);
            x2 = x1 + m.xByA(vBuf[i - 4], vBuf[i - 3]);
            y2 = y1 + m.yByA(vBuf[i - 4], vBuf[i - 3]);
            px = x2 + m.xByA(vBuf[i - 2], vBuf[i - 1]);
            py = y2 + m.yByA(vBuf[i - 2], vBuf[i - 1]);
            appender.cubicTo(x1, y1, x2, y2, px, py);
          }

          vIdx = 0;
          continue;
        }

        // |- dy1 dx2 dy2 dx3 {dxa dxb dyb dyc dyd dxe dye dxf}* dyf? vhcurveto (30) |- or |- {dya dxb dyb dxc dxd dxe dye dyf}+ dxf? vhcurveto (30) |-
        // |- dx1 dx2 dy2 dy3 {dya dxb dyb dxc dxd dxe dye dyf}* dxf? hvcurveto (31) |- or |- {dxa dxb dyb dyc dyd dxe dye dxf}+ dyf? hvcurveto (31) |-
        case kCSOpVHCurveTo:
        case kCSOpHVCurveTo: {
          BL_ASSERT(vMinOperands >= 4);
          BL_PROPAGATE(appender.ensure(out, vIdx));

          size_t i = 0;
          double x1, y1, x2, y2;

          if (b0 == kCSOpVHCurveTo)
            goto OnVHCurveTo;

          while ((i += 4) <= vIdx) {
            x1 = px + m.xByX(vBuf[i - 4]);
            y1 = py + m.yByX(vBuf[i - 4]);
            x2 = x1 + m.xByA(vBuf[i - 3], vBuf[i - 2]);
            y2 = y1 + m.yByA(vBuf[i - 3], vBuf[i - 2]);
            px = x2 + m.xByY(vBuf[i - 1]);
            py = y2 + m.yByY(vBuf[i - 1]);

            if (vIdx - i == 1) {
              px += m.xByX(vBuf[i]);
              py += m.yByX(vBuf[i]);
            }
            appender.cubicTo(x1, y1, x2, y2, px, py);
OnVHCurveTo:
            if ((i += 4) > vIdx)
              break;

            x1 = px + m.xByY(vBuf[i - 4]);
            y1 = py + m.yByY(vBuf[i - 4]);
            x2 = x1 + m.xByA(vBuf[i - 3], vBuf[i - 2]);
            y2 = y1 + m.yByA(vBuf[i - 3], vBuf[i - 2]);
            px = x2 + m.xByX(vBuf[i - 1]);
            py = y2 + m.yByX(vBuf[i - 1]);

            if (vIdx - i == 1) {
              px += m.xByY(vBuf[i]);
              py += m.yByY(vBuf[i]);
            }
            appender.cubicTo(x1, y1, x2, y2, px, py);
          }

          vIdx = 0;
          continue;
        }

        // |- dy1? {dxa dxb dyb dxc}+ hhcurveto (27) |-
        case kCSOpHHCurveTo: {
          BL_ASSERT(vMinOperands >= 4);
          BL_PROPAGATE(appender.ensure(out, vIdx));

          size_t i = 0;
          double x1, y1, x2, y2;

          // Odd argument case.
          if (vIdx & 0x1) {
            px += m.xByY(vBuf[0]);
            py += m.yByY(vBuf[0]);
            i++;
          }

          while ((i += 4) <= vIdx) {
            x1 = px + m.xByX(vBuf[i - 4]);
            y1 = py + m.yByX(vBuf[i - 4]);
            x2 = x1 + m.xByA(vBuf[i - 3], vBuf[i - 2]);
            y2 = y1 + m.yByA(vBuf[i - 3], vBuf[i - 2]);
            px = x2 + m.xByX(vBuf[i - 1]);
            py = y2 + m.yByX(vBuf[i - 1]);
            appender.cubicTo(x1, y1, x2, y2, px, py);
          }

          vIdx = 0;
          continue;
        }

        // |- dx1? {dya dxb dyb dyc}+ vvcurveto (26) |-
        case kCSOpVVCurveTo: {
          BL_ASSERT(vMinOperands >= 4);
          BL_PROPAGATE(appender.ensure(out, vIdx));

          size_t i = 0;
          double x1, y1, x2, y2;

          // Odd argument case.
          if (vIdx & 0x1) {
            px += m.xByX(vBuf[0]);
            py += m.yByX(vBuf[0]);
            i++;
          }

          while ((i += 4) <= vIdx) {
            x1 = px + m.xByY(vBuf[i - 4]);
            y1 = py + m.yByY(vBuf[i - 4]);
            x2 = x1 + m.xByA(vBuf[i - 3], vBuf[i - 2]);
            y2 = y1 + m.yByA(vBuf[i - 3], vBuf[i - 2]);
            px = x2 + m.xByY(vBuf[i - 1]);
            py = y2 + m.yByY(vBuf[i - 1]);
            appender.cubicTo(x1, y1, px, y2, px, py);
          }

          vIdx = 0;
          continue;
        }

        // |- {dxa dya dxb dyb dxc dyc}+ dxd dyd rcurveline (24) |-
        case kCSOpRCurveLine: {
          BL_ASSERT(vMinOperands >= 8);
          BL_PROPAGATE(appender.ensure(out, vIdx / 2u));

          size_t i = 0;
          double x1, y1, x2, y2;

          vIdx -= 2;
          while ((i += 6) <= vIdx) {
            x1 = px + m.xByA(vBuf[i - 6], vBuf[i - 5]);
            y1 = py + m.yByA(vBuf[i - 6], vBuf[i - 5]);
            x2 = x1 + m.xByA(vBuf[i - 4], vBuf[i - 3]);
            y2 = y1 + m.yByA(vBuf[i - 4], vBuf[i - 3]);
            px = x2 + m.xByA(vBuf[i - 2], vBuf[i - 1]);
            py = y2 + m.yByA(vBuf[i - 2], vBuf[i - 1]);
            appender.cubicTo(x1, y1, x2, y2, px, py);
          }

          px += m.xByA(vBuf[vIdx + 0], vBuf[vIdx + 1]);
          py += m.yByA(vBuf[vIdx + 0], vBuf[vIdx + 1]);
          appender.lineTo(px, py);

          vIdx = 0;
          continue;
        }

        // |- {dxa dya}+ dxb dyb dxc dyc dxd dyd rlinecurve (25) |-
        case kCSOpRLineCurve: {
          BL_ASSERT(vMinOperands >= 8);
          BL_PROPAGATE(appender.ensure(out, vIdx / 2u));

          size_t i = 0;
          double x1, y1, x2, y2;

          vIdx -= 6;
          while ((i += 2) <= vIdx) {
            px += m.xByA(vBuf[i - 2], vBuf[i - 1]);
            py += m.yByA(vBuf[i - 2], vBuf[i - 1]);
            appender.lineTo(px, py);
          }

          x1 = px + m.xByA(vBuf[vIdx + 0], vBuf[vIdx + 1]);
          y1 = py + m.yByA(vBuf[vIdx + 0], vBuf[vIdx + 1]);
          x2 = x1 + m.xByA(vBuf[vIdx + 2], vBuf[vIdx + 3]);
          y2 = y1 + m.yByA(vBuf[vIdx + 2], vBuf[vIdx + 3]);
          px = x2 + m.xByA(vBuf[vIdx + 4], vBuf[vIdx + 5]);
          py = y2 + m.yByA(vBuf[vIdx + 4], vBuf[vIdx + 5]);
          appender.cubicTo(x1, y1, x2, y2, px, py);

          vIdx = 0;
          continue;
        }

        // --------------------------------------------------------------------
        // [Hints]
        // --------------------------------------------------------------------

        // |- y dy {dya dyb}* hstem   (1)  |-
        // |- x dx {dxa dxb}* vstem   (3)  |-
        // |- y dy {dya dyb}* hstemhm (18) |-
        // |- x dx {dxa dxb}* vstemhm (23) |-
        case kCSOpHStem:
        case kCSOpVStem:
        case kCSOpHStemHM:
        case kCSOpVStemHM: {
          hintBitCount += (vIdx / 2);

          vIdx = 0;
          continue;
        }

        // |- hintmask (19) mask |-
        // |- cntrmask (20) mask |-
        case kCSOpHintMask:
        case kCSOpCntrMask: {
          // Acts as an implicit VSTEM.
          hintBitCount += (vIdx / 2);

          size_t hintByteSize = (hintBitCount + 7u) / 8u;
          if (BL_UNLIKELY((size_t)(ipEnd - ip) < hintByteSize))
            goto InvalidData;

          // TODO: [CFF HINTING]: These bits are ignored atm.
          ip += hintByteSize;

          vIdx = 0;
          executionFlags |= kCSFlagHasWidth;
          continue;
        }

        // --------------------------------------------------------------------
        // [Variation Data Operators]
        // --------------------------------------------------------------------

        // |- ivs vsindex (15) |-
        case kCSOpVSIndex: {
          // TODO: [CFF OPENTYPE VARIATIONS]
          vIdx = 0;
          continue;
        }

        // in(0)...in(N-1), d(0,0)...d(K-1,0), d(0,1)...d(K-1,1) ... d(0,N-1)...d(K-1,N-1) N blend (16) out(0)...(N-1)
        case kCSOpBlend: {
          // TODO: [CFF OPENTYPE VARIATIONS]
          vIdx = 0;
          continue;
        }

        // --------------------------------------------------------------------
        // [Control Flow]
        // --------------------------------------------------------------------

        // lsubr# calllsubr (10) -
        case kCSOpCallLSubR: {
          BL_ASSERT(vMinOperands >= 1);

          cBuf[cIdx].reset(ip, ipEnd);
          if (BL_UNLIKELY(++cIdx >= kCFFCallStackSize))
            goto InvalidData;

          subrId = uint32_t(int32_t(vBuf[--vIdx]) + int32_t(cffInfo.index[CFFData::kIndexLSubR].bias));
          subrIndex = &cffInfo.index[CFFData::kIndexLSubR];

          if (subrId < subrIndex->entryCount)
            goto OnSubRCall;

          goto InvalidData;
        }

        // gsubr# callgsubr (29) -
        case kCSOpCallGSubR: {
          BL_ASSERT(vMinOperands >= 1);

          cBuf[cIdx].reset(ip, ipEnd);
          if (BL_UNLIKELY(++cIdx >= kCFFCallStackSize))
            goto InvalidData;

          subrId = uint32_t(int32_t(vBuf[--vIdx]) + int32_t(cffInfo.index[CFFData::kIndexGSubR].bias));
          subrIndex = &cffInfo.index[CFFData::kIndexGSubR];

          if (subrId < subrIndex->entryCount)
            goto OnSubRCall;

          goto InvalidData;
        }

        // return (11)
        case kCSOpReturn: {
          if (BL_UNLIKELY(cIdx == 0))
            goto InvalidData;
OnReturn:
          cIdx--;
          ip    = cBuf[cIdx]._ptr;
          ipEnd = cBuf[cIdx]._end;
          continue;
        }

        // endchar (14)
        case kCSOpEndChar: {
          goto EndCharString;
        }

        // --------------------------------------------------------------------
        // [Escaped Operators]
        // --------------------------------------------------------------------

        case kCSOpEscape: {
          if (BL_UNLIKELY(ip >= ipEnd))
            goto InvalidData;
          b0 = *ip++;

          #ifdef BL_TRACE_OT_CFF
          traceCharStringOp(self, trace, 0x0C00 | b0, vBuf, vIdx);
          #endif

          if (BL_UNLIKELY(b0 >= ExecutionFeaturesInfo::kEscapedOpCount)) {
            // Unknown operators should clear the stack and act as NOPs.
            vIdx = 0;
            continue;
          }

          vMinOperands = executionFeatures->escapedOpStackSize[b0];
          if (BL_UNLIKELY(vIdx < vMinOperands)) {
            // If this is not an unknown operand it would mean that we have less
            // values on stack than the operator requires. That's an error in CS.
            if (vMinOperands != ExecutionFeaturesInfo::kUnknown)
              goto InvalidData;

            // Unknown operators should clear the stack and act as NOPs.
            vIdx = 0;
            continue;
          }

          // NOTE: CSOpCode enumeration uses escaped values, what we have in
          // `b0` is an unescaped value. It's much easier in terms of resulting
          // machine code to clear the escape sequence in the constant (kCSOp...)
          // rather than adding it to `b0`.
          switch (b0) {
            // |- dx1 dy1 dx2 dy2 dx3 dy3 dx4 dy4 dx5 dy5 dx6 dy6 fd flex (12 35) |-
            case kCSOpFlex & 0xFFu: {
              double x1, y1, x2, y2;
              BL_PROPAGATE(appender.ensure(out, 6));

              x1 = px + m.xByA(vBuf[0], vBuf[1]);
              y1 = py + m.yByA(vBuf[0], vBuf[1]);
              x2 = x1 + m.xByA(vBuf[2], vBuf[3]);
              y2 = y1 + m.yByA(vBuf[2], vBuf[3]);
              px = x2 + m.xByA(vBuf[4], vBuf[5]);
              py = y2 + m.yByA(vBuf[4], vBuf[5]);
              appender.cubicTo(x1, y1, x2, y2, px, py);

              x1 = px + m.xByA(vBuf[6], vBuf[7]);
              y1 = py + m.yByA(vBuf[6], vBuf[7]);
              x2 = x1 + m.xByA(vBuf[8], vBuf[9]);
              y2 = y1 + m.yByA(vBuf[8], vBuf[9]);
              px = x2 + m.xByA(vBuf[10], vBuf[11]);
              py = y2 + m.yByA(vBuf[10], vBuf[11]);
              appender.cubicTo(x1, y1, x2, y2, px, py);

              vIdx = 0;
              continue;
            }

            // |- dx1 dy1 dx2 dy2 dx3 dy3 dx4 dy4 dx5 dy5 d6 flex1 (12 37) |-
            case kCSOpFlex1 & 0xFFu: {
              double x1, y1, x2, y2, x3, y3, x4, y4, x5, y5;
              BL_PROPAGATE(appender.ensure(out, 6));

              x1 = px + m.xByA(vBuf[0], vBuf[1]);
              y1 = py + m.yByA(vBuf[0], vBuf[1]);
              x2 = x1 + m.xByA(vBuf[2], vBuf[3]);
              y2 = y1 + m.yByA(vBuf[2], vBuf[3]);
              x3 = x2 + m.xByA(vBuf[4], vBuf[5]);
              y3 = y2 + m.yByA(vBuf[4], vBuf[5]);
              appender.cubicTo(x1, y1, x2, y2, x3, y3);

              x4 = x3 + m.xByA(vBuf[6], vBuf[7]);
              y4 = y3 + m.yByA(vBuf[6], vBuf[7]);
              x5 = x4 + m.xByA(vBuf[8], vBuf[9]);
              y5 = y4 + m.yByA(vBuf[8], vBuf[9]);

              double dx = blAbs(vBuf[0] + vBuf[2] + vBuf[4] + vBuf[6] + vBuf[8]);
              double dy = blAbs(vBuf[1] + vBuf[3] + vBuf[5] + vBuf[7] + vBuf[9]);
              if (dx > dy) {
                px = x5 + m.xByX(vBuf[10]);
                py = y5 + m.yByX(vBuf[10]);
              }
              else {
                px = x5 + m.xByY(vBuf[10]);
                py = y5 + m.yByY(vBuf[10]);
              }
              appender.cubicTo(x4, y4, x5, y5, px, py);

              vIdx = 0;
              continue;
            }

            // |- dx1 dx2 dy2 dx3 dx4 dx5 dx6 hflex (12 34) |-
            case kCSOpHFlex & 0xFFu: {
              double x1, y1, x2, y2, x3, y3, x4, y4, x5, y5;
              BL_PROPAGATE(appender.ensure(out, 6));

              x1 = px + m.xByX(vBuf[0]);
              y1 = py + m.yByX(vBuf[0]);
              x2 = x1 + m.xByA(vBuf[1], vBuf[2]);
              y2 = y1 + m.yByA(vBuf[1], vBuf[2]);
              x3 = x2 + m.xByX(vBuf[3]);
              y3 = y2 + m.yByX(vBuf[3]);
              appender.cubicTo(x1, y1, x2, y2, x3, y3);

              x4 = x3 + m.xByX(vBuf[4]);
              y4 = y3 + m.yByX(vBuf[4]);
              x5 = x4 + m.xByA(vBuf[5], -vBuf[2]);
              y5 = y4 + m.yByA(vBuf[5], -vBuf[2]);
              px = x5 + m.xByX(vBuf[6]);
              py = y5 + m.yByX(vBuf[6]);
              appender.cubicTo(x4, y4, x5, y5, px, py);

              vIdx = 0;
              continue;
            }

            // |- dx1 dy1 dx2 dy2 dx3 dx4 dx5 dy5 dx6 hflex1 (12 36) |-
            case kCSOpHFlex1 & 0xFFu: {
              double x1, y1, x2, y2, x3, y3, x4, y4, x5, y5;
              BL_PROPAGATE(appender.ensure(out, 6));

              x1 = px + m.xByA(vBuf[0], vBuf[1]);
              y1 = py + m.yByA(vBuf[0], vBuf[1]);
              x2 = x1 + m.xByA(vBuf[2], vBuf[3]);
              y2 = y1 + m.yByA(vBuf[2], vBuf[3]);
              x3 = x2 + m.xByX(vBuf[4]);
              y3 = y2 + m.yByX(vBuf[4]);
              appender.cubicTo(x1, y1, x2, y2, x3, y3);

              x4 = x3 + m.xByX(vBuf[5]);
              y4 = y3 + m.yByX(vBuf[5]);
              x5 = x4 + m.xByA(vBuf[6], vBuf[7]);
              y5 = y4 + m.yByA(vBuf[6], vBuf[7]);
              px = x5 + m.xByX(vBuf[8]);
              py = y5 + m.yByX(vBuf[8]);
              appender.cubicTo(x4, y4, x5, y5, px, py);

              vIdx = 0;
              continue;
            }

            // in1 in2 and (12 3) out {in1 && in2}
            case kCSOpAnd & 0xFFu: {
              BL_ASSERT(vMinOperands >= 2);
              vBuf[vIdx - 2] = double((vBuf[vIdx - 1] != 0.0) & (vBuf[vIdx - 1] != 0.0));
              vIdx--;
              continue;
            }

            // in1 in2 or (12 4) out {in1 || in2}
            case kCSOpOr & 0xFFu: {
              BL_ASSERT(vMinOperands >= 2);
              vBuf[vIdx - 2] = double((vBuf[vIdx - 1] != 0.0) | (vBuf[vIdx - 1] != 0.0));
              vIdx--;
              continue;
            }

            // in1 in2 eq (12 15) out {in1 == in2}
            case kCSOpEq & 0xFFu: {
              BL_ASSERT(vMinOperands >= 2);
              vBuf[vIdx - 2] = double(vBuf[vIdx - 2] == vBuf[vIdx - 1]);
              vIdx--;
              continue;
            }

            // s1 s2 v1 v2 ifelse (12 22) out {v1 <= v2 ? s1 : s2}
            case kCSOpIfElse & 0xFFu: {
              BL_ASSERT(vMinOperands >= 4);
              vBuf[vIdx - 4] = vBuf[vIdx - 4 + size_t(vBuf[vIdx - 2] > vBuf[vIdx - 1])];
              vIdx -= 3;
              continue;
            }

            // in not (12 5) out {!in}
            case kCSOpNot & 0xFFu: {
              BL_ASSERT(vMinOperands >= 1);
              vBuf[vIdx - 1] = double(vBuf[vIdx - 1] == 0.0);
              continue;
            }

            // in neg (12 14) out {-in}
            case kCSOpNeg & 0xFFu: {
              BL_ASSERT(vMinOperands >= 1);
              vBuf[vIdx - 1] = -vBuf[vIdx - 1];
              continue;
            }

            // in abs (12 9) out {abs(in)}
            case kCSOpAbs & 0xFFu: {
              BL_ASSERT(vMinOperands >= 1);
              vBuf[vIdx - 1] = blAbs(vBuf[vIdx - 1]);
              continue;
            }

            // in sqrt (12 26) out {sqrt(in)}
            case kCSOpSqrt & 0xFFu: {
              BL_ASSERT(vMinOperands >= 1);
              vBuf[vIdx - 1] = blSqrt(blMax(vBuf[vIdx - 1], 0.0));
              continue;
            }

            // in1 in2 add (12 10) out {in1 + in2}
            case kCSOpAdd & 0xFFu: {
              BL_ASSERT(vMinOperands >= 2);
              double result = vBuf[vIdx - 2] + vBuf[vIdx - 1];
              vBuf[vIdx - 2] = blIsFinite(result) ? result : 0.0;
              vIdx--;
              continue;
            }

            // // in1 in2 sub (12 11) out {in1 - in2}
            case kCSOpSub & 0xFFu: {
              BL_ASSERT(vMinOperands >= 2);
              double result = vBuf[vIdx - 2] - vBuf[vIdx - 1];
              vBuf[vIdx - 2] = blIsFinite(result) ? result : 0.0;
              vIdx--;
              continue;
            }

            // CFFv1: in1 in2 mul (12 24) out {in1 * in2}
            case kCSOpMul & 0xFFu: {
              BL_ASSERT(vMinOperands >= 2);
              double result = vBuf[vIdx - 2] * vBuf[vIdx - 1];
              vBuf[vIdx - 2] = blIsFinite(result) ? result : 0.0;
              vIdx--;
              continue;
            }

            // CFFv1: in1 in2 div (12 12) out {in1 / in2}
            case kCSOpDiv & 0xFFu: {
              BL_ASSERT(vMinOperands >= 2);
              double result = vBuf[vIdx - 2] / vBuf[vIdx - 1];
              vBuf[vIdx - 2] = blIsFinite(result) ? result : 0.0;
              vIdx--;
              continue;
            }

            // random (12 23) out
            case kCSOpRandom & 0xFFu: {
              if (BL_UNLIKELY(++vIdx > kCFFValueStackSizeV1))
                goto InvalidData;

              // NOTE: Don't allow anything random.
              vBuf[vIdx - 1] = 0.5;
              continue;
            }

            // in dup (12 27) out out
            case kCSOpDup & 0xFFu: {
              BL_ASSERT(vMinOperands >= 1);
              if (BL_UNLIKELY(++vIdx > kCFFValueStackSizeV1))
                goto InvalidData;
              vBuf[vIdx - 1] = vBuf[vIdx - 2];
              continue;
            }

            // in drop (12 18)
            case kCSOpDrop & 0xFFu: {
              if (BL_UNLIKELY(vIdx == 0))
                goto InvalidData;
              vIdx--;
              continue;
            }

            // in1 in2 exch (12 28) out1 out2
            case kCSOpExch & 0xFFu: {
              BL_ASSERT(vMinOperands >= 2);
              std::swap(vBuf[vIdx - 2], vBuf[vIdx - 1]);
              continue;
            }

            // nX...n0 I index (12 29) nX...n0 n[I]
            case kCSOpIndex & 0xFFu: {
              BL_ASSERT(vMinOperands >= 2);

              double idxValue = vBuf[vIdx - 1];
              double valToPush = 0.0;

              if (idxValue < 0.0) {
                // If I is negative, top element is copied.
                valToPush = vBuf[vIdx - 2];
              }
              else {
                // It will overflow if idxValue is greater than `vIdx - 1`, thus,
                // `indexToRead` would become a very large number that would not
                // pass the condition afterwards.
                size_t indexToRead = vIdx - 1 - size_t(unsigned(idxValue));
                if (indexToRead < vIdx - 1)
                  valToPush = vBuf[indexToRead];
              }

              vBuf[vIdx - 1] = valToPush;
              continue;
            }

            // n(N–1)...n0 N J roll (12 30) n((J–1) % N)...n0 n(N–1)...n(J % N)
            case kCSOpRoll & 0xFFu: {
              // TODO: [OPENTYPE CFF] ROLL
              goto InvalidData;
            }

            // in I put (12 20)
            case kCSOpPut & 0xFFu: {
              // TODO: [OPENTYPE CFF] PUT
              goto InvalidData;
            }

            // I get (12 21) out
            case kCSOpGet & 0xFFu: {
              // TODO: [OPENTYPE CFF] GET
              goto InvalidData;
            }

            // Unknown operator - drop the stack and continue.
            default: {
              vIdx = 0;
              continue;
            }
          }
        }

        // Unknown operator - drop the stack and continue.
        default: {
          vIdx = 0;
          continue;
        }
      }
    }
  }

EndCharString:
  if (executionFlags & kCSFlagPathOpen) {
    BL_PROPAGATE(appender.ensure(out, 1));
    appender.close();
  }

  appender.done(out);
  trace.info("[%zu bytes processed]\n", bytesProcessed);

  return sink ? sink(out, &sinkInfo, closure) : BL_SUCCESS;

InvalidData:
  appender.done(out);
  trace.fail("Invalid data [%zu bytes processed]\n", bytesProcessed);

  return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);
}

// ============================================================================
// [BLOpenType::CFFImpl - Init]
// ============================================================================

BLResult init(BLOTFaceImpl* faceI, BLFontTable fontTable, uint32_t cffVersion) noexcept {
  DictIterator dictIter;

  uint32_t nameOffset = 0;
  uint32_t topDictOffset = 0;
  uint32_t stringOffset = 0;
  uint32_t gsubrOffset = 0;
  uint32_t charStringOffset = 0;

  uint32_t beginDataOffset = 0;
  uint32_t privateOffset = 0;
  uint32_t privateLength = 0;
  uint32_t lsubrOffset = 0;

  Index nameIndex {};
  Index topDictIndex {};
  Index stringIndex {};
  Index gsubrIndex {};
  Index lsubrIndex {};
  Index charStringIndex {};

  // --------------------------------------------------------------------------
  // [CFF Header]
  // --------------------------------------------------------------------------

  if (BL_UNLIKELY(!blFontTableFitsT<CFFTable>(fontTable)))
    return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

  BLFontTableT<CFFTable> cff { fontTable };

  // The specification says that the implementation should refuse MAJOR version,
  // which it doesn't understand. We understand version 1 & 2 (there seems to be
  // no other version) so refuse anything else. It also says that change in MINOR
  // version should never cause an incompatibility, so we ignore it completely.
  if (BL_UNLIKELY(cffVersion + 1 != cff->header.majorVersion()))
    return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

  uint32_t topDictSize = 0;
  uint32_t headerSize = cff->header.headerSize();

  if (cffVersion == CFFData::kVersion1) {
    if (BL_UNLIKELY(headerSize < 4 || headerSize > cff.size - 4))
      return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

    uint32_t offsetSize = cff->headerV1()->offsetSize();
    if (BL_UNLIKELY(offsetSize < 1 || offsetSize > 4))
      return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);
  }
  else {
    if (BL_UNLIKELY(headerSize < 5 || headerSize > cff.size - 5))
      return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

    topDictSize = cff->headerV2()->topDictLength();
  }

  // --------------------------------------------------------------------------
  // [CFF NameIndex]
  // --------------------------------------------------------------------------

  // NameIndex is only used by CFF, CFF2 doesn't use it.
  if (cffVersion == CFFData::kVersion1) {
    nameOffset = headerSize;
    BL_PROPAGATE(readIndex(cff.data + nameOffset, cff.size - nameOffset, cffVersion, &nameIndex));

    // There should be exactly one font in the table according to OpenType specification.
    if (BL_UNLIKELY(nameIndex.count != 1))
      return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

    topDictOffset = nameOffset + nameIndex.totalSize;
  }
  else {
    topDictOffset = headerSize;
  }

  // --------------------------------------------------------------------------
  // [CFF TopDictIndex]
  // --------------------------------------------------------------------------

  if (cffVersion == CFFData::kVersion1) {
    // CFF doesn't have the size specified in the header, so we have to compute it.
    topDictSize = uint32_t(cff.size - topDictOffset);
  }
  else {
    // CFF2 specifies the size in the header, so make sure it doesn't overflow our limits.
    if (BL_UNLIKELY(topDictSize > cff.size - topDictOffset))
      return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);
  }

  BL_PROPAGATE(readIndex(cff.data + topDictOffset, topDictSize, cffVersion, &topDictIndex));
  if (cffVersion == CFFData::kVersion1) {
    // TopDict index must match NameIndex (v1).
    if (BL_UNLIKELY(nameIndex.count != topDictIndex.count))
      return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);
  }

  dictIter.reset(topDictIndex.payload + topDictIndex.offsetAt(0), topDictIndex.payloadSize);
  while (dictIter.hasNext()) {
    DictEntry entry;
    BL_PROPAGATE(dictIter.next(entry));

    switch (entry.op) {
      case CFFTable::kDictOpTopCharStrings: {
        if (BL_UNLIKELY(entry.count != 1))
          return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

        charStringOffset = uint32_t(entry.values[0]);
        break;
      }

      case CFFTable::kDictOpTopPrivate: {
        if (BL_UNLIKELY(entry.count != 2))
          return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

        privateOffset = uint32_t(entry.values[1]);
        privateLength = uint32_t(entry.values[0]);
        break;
      }
    }
  }

  // --------------------------------------------------------------------------
  // [CFF StringIndex]
  // --------------------------------------------------------------------------

  // StringIndex is only used by CFF, CFF2 doesn't use it.
  if (cffVersion == CFFData::kVersion1) {
    stringOffset = topDictOffset + topDictIndex.totalSize;
    BL_PROPAGATE(readIndex(cff.data + stringOffset, cff.size - stringOffset, cffVersion, &stringIndex));
    gsubrOffset = stringOffset + stringIndex.totalSize;
  }
  else {
    gsubrOffset = topDictOffset + topDictIndex.totalSize;
  }

  // --------------------------------------------------------------------------
  // [CFF GSubRIndex]
  // --------------------------------------------------------------------------

  BL_PROPAGATE(readIndex(cff.data + gsubrOffset, cff.size - gsubrOffset, cffVersion, &gsubrIndex));
  beginDataOffset = gsubrOffset + gsubrIndex.totalSize;

  // --------------------------------------------------------------------------
  // [CFF PrivateDict]
  // --------------------------------------------------------------------------

  if (privateOffset) {
    if (BL_UNLIKELY(privateOffset < beginDataOffset ||
                    privateOffset >= cff.size ||
                    privateLength > cff.size - privateOffset))
      return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

    dictIter.reset(cff.data + privateOffset, privateLength);
    while (dictIter.hasNext()) {
      DictEntry entry;
      BL_PROPAGATE(dictIter.next(entry));

      switch (entry.op) {
        case CFFTable::kDictOpPrivSubrs: {
          if (BL_UNLIKELY(entry.count != 1))
            return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

          lsubrOffset = uint32_t(entry.values[0]);
          break;
        }
      }
    }
  }

  // --------------------------------------------------------------------------
  // [CFF LSubRIndex]
  // --------------------------------------------------------------------------

  if (lsubrOffset) {
    // `lsubrOffset` is relative to `privateOffset`.
    if (BL_UNLIKELY(lsubrOffset < privateLength || lsubrOffset > cff.size - privateOffset))
      return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

    lsubrOffset += privateOffset;
    BL_PROPAGATE(readIndex(cff.data + lsubrOffset, cff.size - lsubrOffset, cffVersion, &lsubrIndex));
  }

  // --------------------------------------------------------------------------
  // [CFF CharStrings]
  // --------------------------------------------------------------------------

  if (BL_UNLIKELY(charStringOffset < beginDataOffset || charStringOffset >= cff.size))
    return blTraceError(BL_ERROR_FONT_CFF_INVALID_DATA);

  BL_PROPAGATE(readIndex(cff.data + charStringOffset, cff.size - charStringOffset, cffVersion, &charStringIndex));

  // --------------------------------------------------------------------------
  // [Done - Fill CFFData]
  // --------------------------------------------------------------------------

  faceI->cff.table = fontTable;

  faceI->cff.index[CFFData::kIndexGSubR].reset(
    DataRange { gsubrOffset, gsubrIndex.totalSize },
    gsubrIndex.headerSize,
    gsubrIndex.offsetSize,
    gsubrIndex.count,
    calcSubRBias(gsubrIndex.count));

  faceI->cff.index[CFFData::kIndexLSubR].reset(
    DataRange { lsubrOffset, lsubrIndex.totalSize },
    lsubrIndex.headerSize,
    lsubrIndex.offsetSize,
    lsubrIndex.count,
    calcSubRBias(lsubrIndex.count));

  faceI->cff.index[CFFData::kIndexCharString].reset(
    DataRange { charStringOffset, charStringIndex.totalSize },
    charStringIndex.headerSize,
    charStringIndex.offsetSize,
    charStringIndex.count,
    0);

  faceI->funcs.decodeGlyph = decodeGlyph;
  return BL_SUCCESS;
};

// ============================================================================
// [BLOpenType::CFFImpl - Unit Tests]
// ============================================================================

#ifdef BL_BUILD_TEST
static void testReadFloat() noexcept {
  struct TestEntry {
    char data[16];
    uint32_t size;
    uint32_t pass;
    double value;
  };

  const double kTolerance = 1e-9;

  static const TestEntry entries[] = {
    #define PASS_ENTRY(DATA, VAL) { DATA, sizeof(DATA) - 1, 1, VAL }
    #define FAIL_ENTRY(DATA)      { DATA, sizeof(DATA) - 1, 0, 0.0 }

    PASS_ENTRY("\xE2\xA2\x5F"            ,-2.25       ),
    PASS_ENTRY("\x0A\x14\x05\x41\xC3\xFF", 0.140541e-3),
    PASS_ENTRY("\x0F"                    , 0          ),
    PASS_ENTRY("\x00\x0F"                , 0          ),
    PASS_ENTRY("\x00\x0A\x1F"            , 0.1        ),
    PASS_ENTRY("\x1F"                    , 1          ),
    PASS_ENTRY("\x10\x00\x0F"            , 10000      ),
    PASS_ENTRY("\x12\x34\x5F"            , 12345      ),
    PASS_ENTRY("\x12\x34\x5A\xFF"        , 12345      ),
    PASS_ENTRY("\x12\x34\x5A\x00\xFF"    , 12345      ),
    PASS_ENTRY("\x12\x34\x5A\x67\x89\xFF", 12345.6789 ),
    PASS_ENTRY("\xA1\x23\x45\x67\x89\xFF", .123456789 ),

    FAIL_ENTRY(""),
    FAIL_ENTRY("\xA2"),
    FAIL_ENTRY("\x0A\x14"),
    FAIL_ENTRY("\x0A\x14\x05"),
    FAIL_ENTRY("\x0A\x14\x05\x51"),
    FAIL_ENTRY("\x00\x0A\x1A\xFF"),
    FAIL_ENTRY("\x0A\x14\x05\x51\xC3")

    #undef PASS_ENTRY
    #undef FAIL_ENTRY
  };

  for (size_t i = 0; i < BL_ARRAY_SIZE(entries); i++) {
    const TestEntry& entry = entries[i];
    double valueOut = 0.0;
    size_t valueSizeInBytes = 0;

    BLResult result = readFloat(
      reinterpret_cast<const uint8_t*>(entry.data),
      reinterpret_cast<const uint8_t*>(entry.data) + entry.size,
      valueOut,
      valueSizeInBytes);

    if (entry.pass) {
      EXPECT(result == BL_SUCCESS,
             "Entry %zu should have passed {Error=%08X}", i, unsigned(result));

      double a = valueOut;
      double b = entry.value;

      EXPECT(blAbs(a - b) <= kTolerance,
             "Entry %zu returned value '%g' which doesn't match the expected value '%g'", i, a, b);
    }
    else {
      EXPECT(result != BL_SUCCESS,
             "Entry %zu should have failed", i);
    }
  }
}

static void testDictIterator() noexcept {
  // This example dump was taken from "The Compact BLFont Format Specification" Appendix D.
  static const uint8_t dump[] = {
    0xF8, 0x1B, 0x00, 0xF8, 0x1C, 0x02, 0xF8, 0x1D, 0x03, 0xF8,
    0x19, 0x04, 0x1C, 0x6F, 0x00, 0x0D, 0xFB, 0x3C, 0xFB, 0x6E,
    0xFA, 0x7C, 0xFA, 0x16, 0x05, 0xE9, 0x11, 0xB8, 0xF1, 0x12
  };

  struct TestEntry {
    uint32_t op;
    uint32_t count;
    double values[4];
  };

  static const TestEntry testEntries[] = {
    { CFFTable::kDictOpTopVersion    , 1, { 391                   } },
    { CFFTable::kDictOpTopFullName   , 1, { 392                   } },
    { CFFTable::kDictOpTopFamilyName , 1, { 393                   } },
    { CFFTable::kDictOpTopWeight     , 1, { 389                   } },
    { CFFTable::kDictOpTopUniqueId   , 1, { 28416                 } },
    { CFFTable::kDictOpTopFontBBox   , 4, { -168, -218, 1000, 898 } },
    { CFFTable::kDictOpTopCharStrings, 1, { 94                    } },
    { CFFTable::kDictOpTopPrivate    , 2, { 45, 102               } }
  };

  uint32_t index = 0;
  DictIterator iter(dump, BL_ARRAY_SIZE(dump));

  while (iter.hasNext()) {
    EXPECT(index < BL_ARRAY_SIZE(testEntries),
           "DictIterator found more entries than the data contains");

    DictEntry entry;
    EXPECT(iter.next(entry) == BL_SUCCESS,
           "DictIterator failed to read entry #%u", index);

    EXPECT(entry.count == testEntries[index].count,
           "DictIterator failed to read entry #%u properly {entry.count == 0}", index);
    for (uint32_t j = 0; j < entry.count; j++) {
      EXPECT(entry.values[j] == testEntries[index].values[j],
             "DictIterator failed to read entry #%u properly {entry.values[%u] (%f) != %f)", index, j, entry.values[j], testEntries[index].values[j]);
    }
    index++;
  }

  EXPECT(index == BL_ARRAY_SIZE(testEntries),
         "DictIterator must iterate over all entries, only %u of %u iterated", index, unsigned(BL_ARRAY_SIZE(testEntries)));
}

UNIT(blend2d_opentype_cff) {
  INFO("BLOpenType::CFFImpl::readFloat()");
  testReadFloat();

  INFO("BLOpenType::CFFImpl::DictIterator");
  testDictIterator();
}
#endif

} // {CFFImpl}
} // {BLOpenType}
