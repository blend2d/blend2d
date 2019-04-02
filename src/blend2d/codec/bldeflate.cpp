// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// ZLIB - See LICENSE.md file in the package.

// The DEFLATE algorithm is based on stb_image <https://github.com/nothings/stb>
// released into PUBLIC DOMAIN. It was initially part of Blend2D's PNG decoder,
// but later split as it could be useful outside of PNG implementation as well.

#include "../blapi-build_p.h"
#include "../blsupport_p.h"
#include "../codec/bldeflate_p.h"

// ============================================================================
// [BLDeflateConstants]
// ============================================================================

static const uint16_t blDeflateSizeBase[31] = {
  3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67,
  83, 99, 115, 131, 163, 195, 227, 258, 0, 0
};

static const uint8_t blDeflateSizeExtra[31] = {
  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5,
  5, 5, 0, 0, 0
};

static const uint16_t blDeflateDistBase[32] = {
  1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
  1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577, 0, 0
};

static const uint8_t blDeflateDistExtra[32] = {
  0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11,
  11, 12, 12, 13, 13
};

static const uint8_t blDeflateDeZigZag[19] = {
  16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

static const uint8_t blDeflateFixedZSize[288] = {
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8,
  8, 8
};

static const uint8_t blDeflateFixedZDist[32] = {
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5
};

// ============================================================================
// [BLDeflateHelpers]
// ============================================================================

template<typename T>
static BL_INLINE T blDeflateBitRev16Internal(T v) noexcept {
  v = ((v & 0xAAAA) >> 1) | ((v & 0x5555) << 1);
  v = ((v & 0xCCCC) >> 2) | ((v & 0x3333) << 2);
  v = ((v & 0xF0F0) >> 4) | ((v & 0x0F0F) << 4);
  v = ((v & 0xFF00) >> 8) | ((v & 0x00FF) << 8);

  return v;
}

template<typename T>
static BL_INLINE T blDeflateBitRev(T v, uint32_t n) noexcept {
  BL_ASSERT(n <= 16);
  return blDeflateBitRev16Internal(v) >> (16 - n);
}

// ============================================================================
// [BLDeflateTable]
// ============================================================================

// Huffman table used by `DeflateDecoder`.
struct DeflateTable {
  enum : uint32_t {
    kFastBits = 9,
    kFastSize = 1 << kFastBits,
    kFastMask = kFastSize - 1
  };

  uint16_t fast[kFastSize];
  int32_t delta[16];
  uint32_t maxCode[17];
  uint8_t size[288];
  uint16_t value[288];
};

static BLResult blDeflateBuildTable(DeflateTable* table, const uint8_t* sizeList, uint32_t sizeCount) noexcept {
  uint32_t i;

  uint32_t sizes[17];
  uint32_t nextCode[16];

  memset(table->fast, 0, sizeof(table->fast));
  table->delta[0] = 0;
  table->maxCode[0] = 0;        // Not used.
  table->maxCode[16] = 0x10000; // Sentinel.

  // DEFLATE spec for generating codes.
  memset(sizes, 0, sizeof(sizes));
  for (i = 0; i < sizeCount; i++)
    sizes[sizeList[i]]++;

  sizes[0] = 0;
  for (i = 1; i < 16; i++) {
    BL_ASSERT(sizes[i] <= (1u << i));
  }

  {
    uint32_t code = 0;
    uint32_t k = 0;

    for (i = 1; i < 16; i++) {
      nextCode[i] = code;
      table->delta[i] = int32_t(k) - int32_t(code);

      code += sizes[i];
      if (sizes[i] && code - 1 >= (1u << i))
        return blTraceError(BL_ERROR_INVALID_DATA);

      // Preshift for inner loop.
      table->maxCode[i] = code << (16 - i);
      code <<= 1;
      k += sizes[i];
    }
  }

  for (i = 0; i < sizeCount; i++) {
    uint32_t s = sizeList[i];

    if (s) {
      uint32_t code = uint32_t(int32_t(nextCode[s]) + table->delta[s]);
      uint32_t fast = ((s << 9) | i);

      table->size[code] = uint8_t(s);
      table->value[code] = uint16_t(i);

      if (s <= DeflateTable::kFastBits) {
        uint32_t k = blDeflateBitRev(nextCode[s], s);
        while (k < DeflateTable::kFastSize) {
          table->fast[k] = uint16_t(fast);
          k += (1u << s);
        }
      }

      nextCode[s]++;
    }
  }

  return BL_SUCCESS;
}

// ============================================================================
// [BLDeflateDecoderStatus]
// ============================================================================

enum DeflateState {
  kDeflateStateZLibHeader        = 0,    // The decoder will process ZLIB's header.
  kDeflateStateBlockHeader       = 1,    // The decoder will process BLOCK's header.
  kDeflateStateBlockUncompressed = 2,    // The decoder will process an uncompressed block.
  kDeflateStateBlockCompressed   = 3     // The decoder will process a compressed block.
};

// ============================================================================
// [BLDeflateDecoder]
// ============================================================================

// In-memory DEFLATE decoder.
struct DeflateDecoder {
  DeflateDecoder(BLArray<uint8_t>& output, void* readCtx, Deflate::ReadFunc readFunc) noexcept;
  ~DeflateDecoder() noexcept;

  BL_INLINE BLResult _ensureDstSize(size_t maxLen) noexcept {
    size_t remain = (size_t)(_dstEnd - _dstPtr);
    if (BL_UNLIKELY(remain < maxLen)) {
      size_t pos = (size_t)(_dstPtr - _dstStart);
      _dstBuffer.impl->size = pos;
      BL_PROPAGATE(_dstBuffer.modifyOp(BL_MODIFY_OP_APPEND_GROW, maxLen, &_dstPtr));

      _dstStart = _dstPtr - pos;
      _dstEnd = _dstStart + _dstBuffer.capacity();
    }

    return BL_SUCCESS;
  }

  BLResult _decode() noexcept;

  // --------------------------------------------------------------------------
  // [Members]
  // --------------------------------------------------------------------------

  //! Read context - passed to `_readFunc`.
  void* _readCtx;
  //! Read function - Callback that provides next input chunk.
  Deflate::ReadFunc _readFunc;

  //! Destination buffer reference.
  BLArray<uint8_t>& _dstBuffer;
  //! The start of `_dstBuffer`.
  uint8_t* _dstStart;
  //! The current position in `_dstBuffer`.
  uint8_t* _dstPtr;
  //! The end of `_dstBuffer` (the first invalid byte).
  uint8_t* _dstEnd;

  //! The current position in the last chunk retrieved by calling `_readFunc`.
  const uint8_t* _srcPtr;
  //! The end of the last chunk retrieved by calling `_readFunc`.
  const uint8_t* _srcEnd;

  //! The current code data (bits).
  BLBitWord _codeData;
  //! The current code size in bits.
  uint32_t _codeSize;
  //! The current decoder state.
  uint32_t _state;

  DeflateTable _zSize;
  DeflateTable _zDist;
};

// ============================================================================
// [BLDeflateContext - Construction / Destruction]
// ============================================================================

DeflateDecoder::DeflateDecoder(BLArray<uint8_t>& output, void* readCtx, Deflate::ReadFunc readFunc) noexcept
  : _readCtx(readCtx),
    _readFunc(readFunc),
    _dstBuffer(output),
    _dstStart(nullptr),
    _dstPtr(nullptr),
    _dstEnd(nullptr),
    _srcPtr(nullptr),
    _srcEnd(nullptr),
    _codeData(0),
    _codeSize(0),
    _state(kDeflateStateZLibHeader) {}
DeflateDecoder::~DeflateDecoder() noexcept {}

// ============================================================================
// [BLDeflateContext - Decode]
// ============================================================================

#define BL_DEFLATE_INIT(SELF)                                   \
  BLResult err = BL_SUCCESS;                                    \
                                                                \
  BLBitWord dflData = SELF->_codeData;                          \
  uint32_t dflSize = SELF->_codeSize;                           \
                                                                \
  const uint8_t* dflPtr = SELF->_srcPtr;                        \
  const uint8_t* dflEnd = SELF->_srcEnd;                        \
                                                                \
  {                                                             \

#define BL_DEFLATE_FINI(SELF)                                   \
  }                                                             \
                                                                \
DeflateOnFailure:                                               \
  err = blTraceError(BL_ERROR_INVALID_DATA);                    \
                                                                \
DeflateOnReturn:                                                \
  SELF->_codeData = dflData;                                    \
  SELF->_codeSize = dflSize;                                    \
                                                                \
  SELF->_srcPtr = dflPtr;                                       \
  SELF->_srcEnd = dflEnd;                                       \
                                                                \
  SELF->_dstBuffer.impl->size = ((size_t)(_dstPtr - _dstStart));\
  return err

#define BL_DEFLATE_CONSUME(_N_)                                 \
  do {                                                          \
    dflData >>= (_N_);                                          \
    dflSize -= (_N_);                                           \
  } while (0)

#define BL_DEFLATE_PEEK(_N_) (dflData & ((1u << (_N_)) - 1))
#define BL_DEFLATE_SUCCESS() goto DeflateOnReturn
#define BL_DEFLATE_INVALID() goto DeflateOnFailure

#define BL_DEFLATE_PROPAGATE(ERR)                               \
  do {                                                          \
    if ((err = (ERR)) != BL_SUCCESS)                            \
      goto DeflateOnReturn;                                     \
  } while (0)

#define BL_DEFLATE_NEED_BITS(_N_)                               \
  do {                                                          \
    if (dflSize < uint32_t(_N_))                                \
      BL_DEFLATE_INVALID();                                     \
  } while (0)

#define BL_DEFLATE_FILL_BITS()                                  \
  do {                                                          \
    while (dflSize <= blBitSizeOf<BLBitWord>() - 8) {           \
      if (BL_UNLIKELY(dflPtr == dflEnd) &&                      \
          !_readFunc(_readCtx, &dflPtr, &dflEnd))               \
        break;                                                  \
                                                                \
      BLBitWord tmpByte = *dflPtr++;                            \
      dflData += tmpByte << dflSize;                            \
      dflSize += 8;                                             \
    }                                                           \
  } while (0)

// Read `_N_` bits and zero extend.
#define BL_DEFLATE_READ_BITS(_Dst_, _N_)                        \
  do {                                                          \
    BL_ASSERT(dflSize >= uint32_t(_N_));                        \
                                                                \
    _Dst_ = uint32_t(BL_DEFLATE_PEEK(_N_));                     \
    BL_DEFLATE_CONSUME(_N_);                                    \
  } while (0)

#define BL_DEFLATE_READ_CODE(_Dst_, _Table_)                    \
  do {                                                          \
    uint32_t tmpCode = (_Table_)->fast[                         \
      BL_DEFLATE_PEEK(DeflateTable::kFastBits)];                \
    uint32_t tmpSize;                                           \
                                                                \
    if (tmpCode) {                                              \
      tmpSize = tmpCode >> 9;                                   \
      tmpCode &= DeflateTable::kFastMask;                       \
    }                                                           \
    else {                                                      \
      /* Not resolved by a fast table... */                     \
      tmpCode = uint32_t(blDeflateBitRev(dflData, 16));         \
      tmpSize = DeflateTable::kFastBits + 1;                    \
                                                                \
      while (tmpCode >= (_Table_)->maxCode[tmpSize])            \
        tmpSize++;                                              \
                                                                \
      /* Invalid code. */                                       \
      if (tmpSize == 16) BL_DEFLATE_INVALID();                  \
                                                                \
      /* Code size is `tmpSize`. */                             \
      tmpCode = uint32_t(int32_t(tmpCode >> (16 - tmpSize)) +   \
                (_Table_)->delta[tmpSize]);                     \
                                                                \
      BL_ASSERT((_Table_)->size[tmpCode] == tmpSize);           \
      tmpCode = (_Table_)->value[tmpCode];                      \
    }                                                           \
                                                                \
    BL_DEFLATE_CONSUME(tmpSize);                                \
    _Dst_ = tmpCode;                                            \
  } while (0)

BLResult DeflateDecoder::_decode() noexcept {
  BL_PROPAGATE(_ensureDstSize(32768));
  BL_DEFLATE_INIT(this);

  uint32_t state = _state;
  uint32_t final = false;

  for (;;) {
    BL_DEFLATE_FILL_BITS();

    // ------------------------------------------------------------------------
    // [Decode ZLIB's Header]
    // ------------------------------------------------------------------------

    if (state == kDeflateStateZLibHeader) {
      BL_DEFLATE_NEED_BITS(16);

      uint32_t cmf, flg;
      BL_DEFLATE_READ_BITS(cmf, 8); // CMF - Compression method & info.
      BL_DEFLATE_READ_BITS(flg, 8); // FLG - Flags.

      // ZLIB - `(CMF << 8) | FLG` has to be divisible by `31`.
      if ((cmf * 256 + flg) % 31 != 0)
        BL_DEFLATE_PROPAGATE(blTraceError(BL_ERROR_INVALID_DATA));

      // ZLIB - The only allowed compression method is DEFLATE (8).
      if ((cmf & 0xF) != 8)
        BL_DEFLATE_PROPAGATE(blTraceError(BL_ERROR_INVALID_DATA));

      // TODO: Fix this, we can be ZLIB compliant here and make this configurable.
      // Preset dictionary not allowed in PNG.
      if ((flg & 0x20) != 0)
        BL_DEFLATE_PROPAGATE(blTraceError(BL_ERROR_INVALID_DATA));

      state = kDeflateStateBlockHeader;
      continue;
    }

    // ------------------------------------------------------------------------
    // [Decode Block - Header]
    // ------------------------------------------------------------------------

    if (state == kDeflateStateBlockHeader) {
      BL_DEFLATE_NEED_BITS(3);

      uint32_t type;
      BL_DEFLATE_READ_BITS(final, 1); // This is the last block.
      BL_DEFLATE_READ_BITS(type , 2); // Type of this block.

      // TYPE 0 - No compression.
      if (type == 0) {
        // Discard all bits that don't form BYTE anymore. These are ignored
        // by uncompressed blocks. We exploit the fact that we refill at the
        // beginning of each state, and if we discard these bytes now, the
        // refill will make sure that we have all 32-bits that define how
        // many uncompressed bytes follow.
        uint32_t n = dflSize & 0x7;
        BL_DEFLATE_CONSUME(n);

        state = kDeflateStateBlockUncompressed;
        continue;
      }

      // TYPE 1 - Compressed with fixed Huffman codes.
      if (type == 1) {
        BL_DEFLATE_PROPAGATE(blDeflateBuildTable(&_zSize, blDeflateFixedZSize, 288));
        BL_DEFLATE_PROPAGATE(blDeflateBuildTable(&_zDist, blDeflateFixedZDist, 32));

        state = kDeflateStateBlockCompressed;
        continue;
      }

      // TYPE 2 - Compressed with dynamic Huffman codes.
      if (type == 2) {
        DeflateTable zCodeSize;
        uint8_t bufCodes[286 + 32 + 137];
        uint8_t bufSizes[19];

        BL_DEFLATE_NEED_BITS(14);
        uint32_t hlit, hdist, hclen;

        BL_DEFLATE_READ_BITS(hlit , 5);
        BL_DEFLATE_READ_BITS(hdist, 5);
        BL_DEFLATE_READ_BITS(hclen, 4);

        hlit  += 257;
        hdist += 1;
        hclen += 4;
        memset(bufSizes, 0, sizeof(bufSizes));

        uint32_t i = 0;
        do {
          BL_DEFLATE_FILL_BITS();

          uint32_t iEnd = blMin<uint32_t>(i + 8, hclen);
          BL_DEFLATE_NEED_BITS(iEnd * 3);

          do {
            uint32_t s;
            BL_DEFLATE_READ_BITS(s, 3);
            bufSizes[blDeflateDeZigZag[i]] = uint8_t(s);
          } while (++i < iEnd);
        } while (i < hclen);

        BL_DEFLATE_PROPAGATE(blDeflateBuildTable(&zCodeSize, bufSizes, 19));

        uint32_t n = 0;
        while (n < hlit + hdist) {
          uint32_t code;

          BL_DEFLATE_FILL_BITS();
          BL_DEFLATE_READ_CODE(code, &zCodeSize);

          if (code <= 15) {
            bufCodes[n++] = uint8_t(code);
          }
          else if (code == 16) {
            BL_DEFLATE_NEED_BITS(2);
            BL_DEFLATE_READ_BITS(code, 2);

            code += 3;
            memset(bufCodes + n, bufCodes[n - 1], code);
            n += code;
          }
          else if (code == 17) {
            BL_DEFLATE_NEED_BITS(3);
            BL_DEFLATE_READ_BITS(code, 3);

            code += 3;
            memset(bufCodes + n, 0, code);
            n += code;
          }
          else if (code == 18) {
            BL_DEFLATE_NEED_BITS(7);
            BL_DEFLATE_READ_BITS(code, 7);

            code += 11;
            memset(bufCodes + n, 0, code);
            n += code;
          }
          else {
            BL_DEFLATE_INVALID();
          }
        }

        if (n != hlit + hdist)
          BL_DEFLATE_INVALID();

        BL_DEFLATE_PROPAGATE(blDeflateBuildTable(&_zSize, bufCodes       , hlit));
        BL_DEFLATE_PROPAGATE(blDeflateBuildTable(&_zDist, bufCodes + hlit, hdist));

        state = kDeflateStateBlockCompressed;
        continue;
      }

      // TYPE 3 - Undefined/Error
      BL_DEFLATE_INVALID();
    }

    // ----------------------------------------------------------------------
    // [Decode Block - Uncompressed]
    // ----------------------------------------------------------------------

    if (state == kDeflateStateBlockUncompressed) {
      BL_ASSERT((dflSize & 0x7) == 0);
      BL_DEFLATE_NEED_BITS(32);

      uint32_t uLen, nLen;
      BL_DEFLATE_READ_BITS(uLen, 16);
      BL_DEFLATE_READ_BITS(nLen, 16);

      if ((uLen ^ 0xFFFF) != nLen)
        BL_DEFLATE_PROPAGATE(blTraceError(BL_ERROR_INVALID_DATA));
      BL_DEFLATE_PROPAGATE(_ensureDstSize(uLen));

      // First read bytes from `dflData` if running on 64-bit (otherwise we
      // have already consumed all 32-bits from the entropy buffer).
      if (blBitSizeOf<BLBitWord>() > 32) {
        while (dflSize != 0 && uLen) {
          *_dstPtr++ = uint8_t(BL_DEFLATE_PEEK(8));
          BL_DEFLATE_CONSUME(8);
          uLen--;
        }
      }

      while (uLen > 0) {
        if (dflPtr == dflEnd && !_readFunc(_readCtx, &dflPtr, &dflEnd))
          BL_DEFLATE_PROPAGATE(blTraceError(BL_ERROR_INVALID_DATA));

        nLen = blMin(uLen, uint32_t((size_t)(dflEnd - dflPtr)));
        memcpy(_dstPtr, dflPtr, nLen);

        _dstPtr += nLen;
        dflPtr += nLen;

        uLen -= nLen;
      }

      _dstPtr += uLen;
      if (final)
        BL_DEFLATE_SUCCESS();

      state = kDeflateStateBlockHeader;
      continue;
    }

    // ----------------------------------------------------------------------
    // [Decode Block - Compressed]
    // ----------------------------------------------------------------------

    if (state == kDeflateStateBlockCompressed) {
      for (;;) {
        uint32_t code;

        BL_DEFLATE_FILL_BITS();
        BL_DEFLATE_READ_CODE(code, &_zSize);

        if (code < 256) {
          if (BL_UNLIKELY(_dstPtr == _dstEnd)) {
            BL_DEFLATE_PROPAGATE(_ensureDstSize(32768));
          }

          *_dstPtr++ = uint8_t(code);
        }
        else {
          if (code == 256)
            break;
          code -= 257;

          if (blBitSizeOf<BLBitWord>() <= 32)
            BL_DEFLATE_FILL_BITS();

          uint32_t s = blDeflateSizeExtra[code];
          uint32_t size = 0;
          if (s) {
            BL_DEFLATE_NEED_BITS(s);
            BL_DEFLATE_READ_BITS(size, s);
          }
          size += blDeflateSizeBase[code];

          BL_DEFLATE_READ_CODE(code, &_zDist);
          s = blDeflateDistExtra[code];

          uint32_t dist = 0;
          if (s) {
            BL_DEFLATE_NEED_BITS(s);
            BL_DEFLATE_READ_BITS(dist, s);
          }
          dist += blDeflateDistBase[code];

          if ((size_t)(_dstPtr - _dstStart) < dist)
            BL_DEFLATE_PROPAGATE(blTraceError(BL_ERROR_INVALID_DATA));

          BL_DEFLATE_PROPAGATE(_ensureDstSize(size));
          uint8_t* p = _dstPtr - dist;

          // Run of one byte; common in images.
          if (dist == 1) {
            uint8_t v = p[0];
            do { *_dstPtr++ = v; } while (--size);
          }
          else {
            do { *_dstPtr++ = *p++; } while (--size);
          }
        }
      }

      if (final)
        BL_DEFLATE_SUCCESS();

      state = kDeflateStateBlockHeader;
      continue;
    }
  }

  BL_DEFLATE_FINI(this);
}

// ============================================================================
// [BLDeflate]
// ============================================================================

BLResult Deflate::deflate(BLArray<uint8_t>& dst, void* readCtx, ReadFunc readFunc, bool hasHeader) noexcept {
  DeflateDecoder decoder(dst, readCtx, readFunc);
  if (!hasHeader)
    decoder._state = kDeflateStateBlockHeader;
  return decoder._decode();
}
