// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_p.h"
#if !defined(BL_BUILD_NO_JIT)

#include "../../pipeline/jit/fetchutilspixelgather_p.h"

namespace bl {
namespace Pipeline {
namespace JIT {
namespace FetchUtils {

// bl::Pipeline::JIT::FetchUtils - IndexExtractor
// ==============================================

IndexExtractor::IndexExtractor(PipeCompiler* pc) noexcept
  : _pc(pc),
    _vec(),
    _mem(),
    _type(kTypeNone),
    _indexSize(0),
    _memSize(0) {}

void IndexExtractor::begin(uint32_t type, const Vec& vec) noexcept {
  BL_ASSERT(type != kTypeNone);
  BL_ASSERT(type < kTypeCount);

#if defined(BL_JIT_ARCH_X86)
  Mem mem = _pc->tmpStack(PipeCompiler::StackId::kIndex, vec.size());

  _pc->v_storeavec(mem, vec, Alignment{16});
  begin(type, mem, vec.size());
#else
  _type = type;
  _vec = vec;

  switch (_type) {
    case kTypeInt16:
    case kTypeUInt16:
      _indexSize = 2;
      break;

    case kTypeInt32:
    case kTypeUInt32:
      _indexSize = 4;
      break;

    default:
      BL_NOT_REACHED();
  }
#endif
}

void IndexExtractor::begin(uint32_t type, const Mem& mem, uint32_t memSize) noexcept {
  BL_ASSERT(type != kTypeNone);
  BL_ASSERT(type < kTypeCount);

  _type = type;
  _vec.reset();
  _mem = mem;
  _memSize = uint16_t(memSize);

  switch (_type) {
    case kTypeInt16:
    case kTypeUInt16:
      _indexSize = 2;
      break;

    case kTypeInt32:
    case kTypeUInt32:
      _indexSize = 4;
      break;

    default:
      BL_NOT_REACHED();
  }
}

void IndexExtractor::extract(const Gp& dst, uint32_t index) noexcept {
  BL_ASSERT(dst.size() >= 4);
  BL_ASSERT(_type != kTypeNone);

  if (!_vec.isValid()) {
    BL_ASSERT((index + 1u) * _indexSize <= _memSize);
    Mem m = _mem;
    m.addOffset(int(index * _indexSize));

    switch (_type) {
      case kTypeInt16 : _pc->load_i16(dst, m); return;
      case kTypeUInt16: _pc->load_u16(dst, m); return;
      case kTypeInt32 : _pc->load_i32(dst, m); return;
      case kTypeUInt32: _pc->load_u32(dst, m); return;

      default:
        BL_NOT_REACHED();
    }
  }
  else {
#if defined(BL_JIT_ARCH_X86)
    BL_NOT_REACHED();
#elif defined(BL_JIT_ARCH_A64)
    AsmCompiler* cc = _pc->cc;
    switch (_type) {
      case kTypeInt16 : cc->smov(dst      , _vec.h(index)); return;
      case kTypeUInt16: cc->umov(dst.r32(), _vec.h(index)); return;
      case kTypeInt32 : cc->smov(dst      , _vec.s(index)); return;
      case kTypeUInt32: cc->umov(dst.r32(), _vec.s(index)); return;

      default:
        BL_NOT_REACHED();
    }
#endif
  }
}

// bl::Pipeline::JIT::FetchUtils - FetchContext
// ============================================

void FetchContext::_init(PixelCount n) noexcept {
  BL_ASSERT(n >= 4);

  _pixel->setCount(n);
  _fetchMode = FetchMode::kNone;
  _fetchDone = false;

  _initFetchMode();
  _initFetchRegs();
  _initTargetPixel();
}

void FetchContext::_initFetchMode() noexcept {
  switch (_pixel->type()) {
    case PixelType::kA8: {
      if (!blTestFlag(_fetchFlags, PixelFlags::kPA_PI_UA_UI))
        _fetchFlags |= PixelFlags::kPA;

      switch (fetchFormat()) {
        case FormatExt::kA8: {
          if (blTestFlag(_fetchFlags, PixelFlags::kPA))
            _fetchMode = FetchMode::kA8FromA8_PA;
          else
            _fetchMode = FetchMode::kA8FromA8_UA;
          break;
        }

        case FormatExt::kPRGB32:
        case FormatExt::kFRGB32:
        case FormatExt::kZERO32: {
          if (blTestFlag(_fetchFlags, PixelFlags::kPA))
            _fetchMode = FetchMode::kA8FromRGBA32_PA;
          else
            _fetchMode = FetchMode::kA8FromRGBA32_UA;
          break;
        }

        default:
          BL_NOT_REACHED();
      }
      break;
    }

    case PixelType::kRGBA32: {
      if (!blTestFlag(_fetchFlags, PixelFlags::kPA_PI_UA_UI | PixelFlags::kPC_UC))
        _fetchFlags |= PixelFlags::kPC;

      switch (fetchFormat()) {
        case FormatExt::kA8: {
          if (blTestFlag(_fetchFlags, PixelFlags::kPC))
            _fetchMode = FetchMode::kRGBA32FromA8_PC;
          else
            _fetchMode = FetchMode::kRGBA32FromA8_UC;
          break;
        }

        case FormatExt::kPRGB32:
        case FormatExt::kFRGB32:
        case FormatExt::kXRGB32:
        case FormatExt::kZERO32: {
          if (blTestFlag(_fetchFlags, PixelFlags::kPC))
            _fetchMode = FetchMode::kRGBA32FromRGBA32_PC;
          else
            _fetchMode = FetchMode::kRGBA32FromRGBA32_UC;
          break;
        }

        default:
          BL_NOT_REACHED();
      }

      break;
    }

    case PixelType::kRGBA64: {
      _fetchMode = FetchMode::kRGBA64FromRGBA64_PC;
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

static BL_INLINE_NODEBUG uint32_t vec128RegCountFromBytes(uint32_t n) noexcept {
  return (n + 15) / 16u;
}

void FetchContext::_initFetchRegs() noexcept {
  uint32_t pixelCount = _pixel->count().value();
  BL_ASSERT(pixelCount >= 2u);

#if defined(BL_JIT_ARCH_X86)
  uint32_t alphaAccSize = 0;
  WideningOp defaultWideningOp = WideningOp::kInterleave;
#endif // BL_JIT_ARCH_X86

  _pixelIndex = 0u;
  _vecIndex = 0u;
  _vecStep = 1u;
  _laneIndex = 0u;

  uint32_t fullByteCount = 0;
  uint32_t p128VecCount = 0;

  bool packed = (_fetchMode == FetchMode::kA8FromA8_PA ||
                 _fetchMode == FetchMode::kA8FromRGBA32_PA ||
                 _fetchMode == FetchMode::kRGBA32FromA8_PC ||
                 _fetchMode == FetchMode::kRGBA32FromA8_UC ||
                 _fetchMode == FetchMode::kRGBA32FromRGBA32_PC ||
                 _fetchMode == FetchMode::kRGBA64FromRGBA64_PC);

  switch (_fetchMode) {
    case FetchMode::kA8FromA8_PA:
    case FetchMode::kA8FromA8_UA:
    case FetchMode::kA8FromRGBA32_PA:
    case FetchMode::kA8FromRGBA32_UA: {
#if defined(BL_JIT_ARCH_X86)
      alphaAccSize = packed && pixelCount <= 4 ? uint32_t(4u) : uint32_t(_pc->registerSize());
#endif // BL_JIT_ARCH_X86

      _laneCount = blMin<uint32_t>(uint32_t(packed ? 16u : 8u), pixelCount);

      fullByteCount = packed ? pixelCount : pixelCount * 2u;
      p128VecCount = vec128RegCountFromBytes(fullByteCount);
      break;
    }

    case FetchMode::kRGBA32FromA8_PC: {
#if defined(BL_JIT_ARCH_X86)
      alphaAccSize = packed && pixelCount <= 4 ? uint32_t(4u) : uint32_t(_pc->registerSize());
#endif // BL_JIT_ARCH_X86

      _laneCount = blMin<uint32_t>(8u << uint32_t(_pc->use512BitSimd()), pixelCount);

      fullByteCount = pixelCount * 4u;
      p128VecCount = blMax<uint32_t>(vec128RegCountFromBytes(fullByteCount) >> uint32_t(_pc->vecWidth()), 1u);

#if defined(BL_JIT_ARCH_X86)
      defaultWideningOp = WideningOp::kRepeat;
#endif // BL_JIT_ARCH_X86
      break;
    }

    case FetchMode::kRGBA32FromA8_UC: {
#if defined(BL_JIT_ARCH_X86)
      alphaAccSize = packed && pixelCount <= 4 ? uint32_t(4u) : uint32_t(_pc->registerSize());
#endif // BL_JIT_ARCH_X86

      _laneCount = blMin<uint32_t>(8u, pixelCount);

      fullByteCount = pixelCount * 8u;

#if defined(BL_JIT_ARCH_X86)
      if (_pc->use512BitSimd() && pixelCount >= 8u) {
        defaultWideningOp = WideningOp::kRepeat8xA8ToRGBA32_UC_AVX512;
        p128VecCount = (pixelCount + 7u) / 8u;
      }
      else if (_pc->use256BitSimd() && pixelCount >= 4u) {
        defaultWideningOp = WideningOp::kUnpack2x;
        p128VecCount = (pixelCount + 1u) / 2u;
        _vecStep = 2u;
      }
      else
#endif // BL_JIT_ARCH_X86
      {
        p128VecCount = (pixelCount + 1u) / 2u;
        _vecStep = blMin<uint32_t>(p128VecCount, 4u);
      }
      break;
    }

    case FetchMode::kRGBA32FromRGBA32_PC: {
      _laneCount = blMin<uint32_t>(4u, pixelCount);

      fullByteCount = pixelCount * 4u;
      p128VecCount = vec128RegCountFromBytes(fullByteCount);
      break;
    }

    case FetchMode::kRGBA32FromRGBA32_UC: {
      _laneCount = blMin<uint32_t>(2u << uint32_t(_pc->use256BitSimd()), pixelCount);

      fullByteCount = pixelCount * 8u;
      p128VecCount = vec128RegCountFromBytes(fullByteCount >> uint32_t(_pc->use256BitSimd()));
      break;
    }

    case FetchMode::kRGBA64FromRGBA64_PC: {
      _laneCount = 2u;

      fullByteCount = pixelCount * 8u;
      p128VecCount = vec128RegCountFromBytes(fullByteCount);
      break;
    }

    default:
      BL_NOT_REACHED();
  }

  BL_ASSERT(p128VecCount != 0u);
  _p128Count = uint8_t(p128VecCount);

  _pTmp[0] = _pc->newV128("@pTmp[0]");
  _pTmp[1] = _pc->newV128("@pTmp[1]");

  for (uint32_t i = 0; i < p128VecCount; i++) {
    _p128[i] = _pc->newV128("@p128[%u]", i);
  }

#if defined(BL_JIT_ARCH_X86)
  // Let's only use GP accumulator on X86 platform as that's pretty easy to implement and
  // it's fast. Other platforms seem to be just okay with SIMD lane to lane insertion.
  if (alphaAccSize > 4)
    _aAcc = _pc->newGp64("@aAcc");
  else if (alphaAccSize > 0)
    _aAcc = _pc->newGp32("@aAcc");

  _widening256Op = WideningOp::kNone;
  _widening512Op = WideningOp::kNone;

  if (_pc->use256BitSimd() && fullByteCount > 16u) {
    uint32_t p256VecCount = (fullByteCount + 31u) / 32u;
    _pc->newV256Array(_p256, p256VecCount, "@p256");

    if (_pc->use512BitSimd() && fullByteCount > 32u) {
      uint32_t p512VecCount = (fullByteCount + 63u) / 64u;
      _pc->newV512Array(_p512, p512VecCount, "@p512");

      _widening256Op = defaultWideningOp;
      _widening512Op = packed ? defaultWideningOp : WideningOp::kUnpack;
    }
    else {
      _widening256Op = packed ? defaultWideningOp : WideningOp::kUnpack;
      _widening512Op = WideningOp::kNone;
    }
  }
#endif // BL_JIT_ARCH_X86
}

void FetchContext::_initTargetPixel() noexcept {
  const Vec* vArray = _p128;
  uint32_t vCount = _p128Count;

#if defined(BL_JIT_ARCH_X86)
  if (_p512.size()) {
    vArray = static_cast<const Vec*>(static_cast<const Operand_*>(_p512.v));
    vCount = _p512.size();
  }
  else if (_p256.size()) {
    vArray = static_cast<const Vec*>(static_cast<const Operand_*>(_p256.v));
    vCount = _p256.size();
  }
#endif // BL_JIT_ARCH_X86

  switch (_pixel->type()) {
    case PixelType::kA8: {
      if (blTestFlag(_fetchFlags, PixelFlags::kPA)) {
        _pixel->pa.init(vArray, vCount);
        _pc->rename(_pixel->pa, _pixel->name(), "pa");
      }
      else {
        _pixel->ua.init(vArray, vCount);
        _pc->rename(_pixel->ua, _pixel->name(), "ua");
      }
      break;
    }

    case PixelType::kRGBA32: {
      if (blTestFlag(_fetchFlags, PixelFlags::kPC)) {
        _pixel->pc.init(vArray, vCount);
        _pc->rename(_pixel->pc, _pixel->name(), "pc");
      }
      else {
        _pixel->uc.init(vArray, vCount);
        _pc->rename(_pixel->uc, _pixel->name(), "uc");
      }
      break;
    }

    case PixelType::kRGBA64: {
      _pixel->uc.init(vArray, vCount);
      _pc->rename(_pixel->uc, _pixel->name(), "uc");
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

void FetchContext::fetchPixel(const Mem& src) noexcept {
  uint32_t pixelCount = _pixel->count().value();
  BL_ASSERT(_pixelIndex < pixelCount);

  Vec v = _p128[_vecIndex];
  BL_ASSERT(v.isValid());

  Mem m(src);

  uint32_t quantity = (_pixelIndex + 2u == pixelCount && _gatherMode == GatherMode::kNeverFull) ? 2u : 1u;

  switch (_fetchMode) {
    case FetchMode::kA8FromA8_PA:
    case FetchMode::kA8FromA8_UA:
    case FetchMode::kA8FromRGBA32_PA:
    case FetchMode::kA8FromRGBA32_UA:
    case FetchMode::kRGBA32FromA8_PC:
    case FetchMode::kRGBA32FromA8_UC: {
      bool fetchPacked = (_fetchMode == FetchMode::kA8FromA8_PA ||
                          _fetchMode == FetchMode::kA8FromRGBA32_PA ||
                          _fetchMode == FetchMode::kRGBA32FromA8_PC ||
                          _fetchMode == FetchMode::kRGBA32FromA8_UC);

      bool a8FromRgba32 = (_fetchMode == FetchMode::kA8FromRGBA32_PA ||
                           _fetchMode == FetchMode::kA8FromRGBA32_UA);

#if defined(BL_JIT_ARCH_A64)
      if (_laneIndex == 0) {
        if (a8FromRgba32 && _fetchInfo.fetchAlphaOffset()) {
          _pc->v_loadu32(v, m);
          _pc->v_srli_u32(v, v, 24);
        }
        else {
          _pc->v_load8(v, m);
        }
      }
      else {
        uint32_t srcLane = 0;
        if (a8FromRgba32 && _fetchInfo.fetchAlphaOffset()) {
          _pc->v_loadu32(_pTmp[0], m);
          srcLane = 3;
        }
        else {
          _pc->v_load8(_pTmp[0], m);
        }

        _pc->cc->ins(v.b(fetchPacked ? _laneIndex : _laneIndex * 2u), _pTmp[0].b(srcLane));
      }
#else
      uint32_t accByteSize = _aAcc.size();

      if (a8FromRgba32) {
        m.addOffset(_fetchInfo.fetchAlphaOffset());
      }

      if (_aAccIndex == 0)
        _pc->load_u8(_aAcc, m);
      else
        _pc->load_merge_u8(_aAcc, m);

      _pc->ror(_aAcc, _aAcc, (fetchPacked ? 8u : 16u) * quantity);

      uint32_t accBytesScale = fetchPacked ? 1 : 2;
      uint32_t accBytes = (_aAccIndex + quantity) * accBytesScale;

      _aAccIndex++;

      if (accBytes >= accByteSize || _pixelIndex + quantity >= pixelCount) {
        if (accByteSize == 4) {
          uint32_t dstLaneIndex = (fetchPacked ? _laneIndex : _laneIndex * 2u) / 4u;

          if (dstLaneIndex == 0) {
            _pc->s_mov(v, _aAcc);
          }
          else if (!_pc->hasSSE4_1()) {
            if (dstLaneIndex == 1) {
              _pc->s_mov(_pTmp[0], _aAcc);
              _pc->v_interleave_lo_u32(v, v, _pTmp[0]);
            }
            else if (dstLaneIndex == 2) {
              _pc->s_mov(_pTmp[0], _aAcc);
            }
            else if (dstLaneIndex == 3) {
              _pc->s_mov(_pTmp[1], _aAcc);
              _pc->v_interleave_lo_u32(_pTmp[0], _pTmp[0], _pTmp[1]);
              _pc->v_interleave_lo_u64(v, v, _pTmp[0]);
            }
          }
          else {
            _pc->s_insert_u32(v, _aAcc, dstLaneIndex);
          }
        }
        else {
          uint32_t dstLaneIndex = (fetchPacked ? _laneIndex : _laneIndex * 2u) / 8u;
          if (dstLaneIndex == 0) {
            _pc->s_mov(v, _aAcc);
          }
          else if (!_pc->hasSSE4_1()) {
            _pc->s_mov(_pTmp[0], _aAcc);
            _pc->v_interleave_lo_u64(v, v, _pTmp[0]);
          }
          else {
            _pc->s_insert_u64(v, _aAcc, dstLaneIndex);
          }
        }

        _aAccIndex = 0;
      }
#endif
      break;
    }

    case FetchMode::kRGBA32FromRGBA32_PC:
    case FetchMode::kRGBA32FromRGBA32_UC: {
      if (_laneIndex == 0) {
        _pc->v_loadu32(v, m);
      }
      else {
#if defined(BL_JIT_ARCH_X86)
        if (!_pc->hasSSE4_1()) {
          if (_laneIndex == 1) {
            _pc->v_loadu32(_pTmp[0], m);
            _pc->v_interleave_lo_u32(v, v, _pTmp[0]);
          }
          else if (_laneIndex == 2) {
            _pc->v_loadu32(_pTmp[0], m);

            // If quantity == 2 it means we are avoiding the last pixel and the following branch would never get called.
            if (quantity == 2u)
              _pc->v_interleave_lo_u64(v, v, _pTmp[0]);
          }
          else {
            _pc->v_loadu32(_pTmp[1], m);
            _pc->v_interleave_lo_u32(_pTmp[0], _pTmp[0], _pTmp[1]);
            _pc->v_interleave_lo_u64(v, v, _pTmp[0]);
          }
        }
        else
#endif
        {
          _pc->v_insert_u32(v, m, _laneIndex);
        }
      }

      break;
    }

    case FetchMode::kRGBA64FromRGBA64_PC: {
      if (_laneIndex == 0)
        _pc->v_loadu64(v, m);
      else
        _pc->v_insert_u64(v, m, _laneIndex);
      break;
    }

    default:
      BL_NOT_REACHED();
  }

  // NOTE: This is better to be done with a loop as it perfectly emulates the "fetch" of a possible last pixel,
  // which was avoided. Theoretically we could just do `_laneIndex += quantity` here, but it could be source of
  // future bugs if more features are added.
  do {
    if (++_laneIndex >= _laneCount) {
      _laneIndex = 0u;
      _doneVec(_vecIndex);
      _vecIndex += _vecStep;
    }

    _pixelIndex++;
  } while (--quantity != 0);
}

void FetchContext::_fetchAll(const Mem& src, uint32_t srcShift, IndexExtractor& extractor, const uint8_t* indexes, InterleaveCallback cb, void* cbData) noexcept {
  // Fetching all pixels assumes no pixels were fetched previously.
  BL_ASSERT(_pixelIndex == 0);

  uint32_t pixelCount = _pixel->count().value();

  Gp idx0 = _pc->newGpPtr("@idx0");
  Gp idx1 = _pc->newGpPtr("@idx1");

  Mem src0 = src;
  Mem src1 = src;

  src0.setIndex(idx0, srcShift);
  src1.setIndex(idx1, srcShift);

  switch (pixelCount) {
    case 2: {
      extractor.extract(idx0, indexes[0]);
      extractor.extract(idx1, indexes[1]);

      cb(0, cbData);
      fetchPixel(src0);

      cb(1, cbData);
      fetchPixel(src1);

      cb(0xFF, cbData);
      break;
    }

    case 4: {
      extractor.extract(idx0, indexes[0]);
      extractor.extract(idx1, indexes[1]);

      cb(0, cbData);
      fetchPixel(src0);
      extractor.extract(idx0, indexes[2]);

      cb(1, cbData);
      fetchPixel(src1);

      if (_gatherMode == GatherMode::kFetchAll)
        extractor.extract(idx1, indexes[3]);

      cb(2, cbData);
      fetchPixel(src0);

      cb(3, cbData);
      if (_gatherMode == GatherMode::kFetchAll)
        fetchPixel(src1);

      cb(0xFF, cbData);
      break;
    }

    case 8:
    case 16: {
#if defined(BL_JIT_ARCH_X86)
      bool hasFastInsert32 = _pc->hasSSE4_1();
#else
      constexpr bool hasFastInsert32 = true;
#endif

      if (_fetchMode == FetchMode::kRGBA32FromRGBA32_PC && hasFastInsert32) {
        for (uint32_t i = 0; i < pixelCount; i += 8) {
          Vec& v0 = _p128[_vecIndex];
          Vec& v1 = _p128[_vecIndex + _vecStep];

          extractor.extract(idx0, indexes[i + 0u]);
          extractor.extract(idx1, indexes[i + 4u]);

          cb(i + 0u, cbData);
          _pc->v_loada32(v0, src0);
          extractor.extract(idx0, indexes[i + 1u]);

          cb(i + 1u, cbData);
          _pc->v_loada32(v1, src1);
          extractor.extract(idx1, indexes[i + 5u]);

          cb(i + 2u, cbData);
          _pc->v_insert_u32(v0, src0, 1);
          extractor.extract(idx0, indexes[i + 2u]);

          cb(i + 3u, cbData);
          _pc->v_insert_u32(v1, src1, 1);
          extractor.extract(idx1, indexes[i + 6u]);

          cb(i + 4u, cbData);
          _pc->v_insert_u32(v0, src0, 2);
          extractor.extract(idx0, indexes[i + 3u]);

          cb(i + 5u, cbData);
          _pc->v_insert_u32(v1, src1, 2);

          if (_gatherMode == GatherMode::kFetchAll)
            extractor.extract(idx1, indexes[i + 7u]);

          cb(i + 6u, cbData);
          _pc->v_insert_u32(v0, src0, 3);

          _pixelIndex += 4;
          _doneVec(_vecIndex);
          _vecIndex += _vecStep;

          cb(i + 7u, cbData);
          if (_gatherMode == GatherMode::kFetchAll)
            _pc->v_insert_u32(v1, src1, 3);

          _pixelIndex += 4;
          _doneVec(_vecIndex);
          _vecIndex += _vecStep;
        }
      }
      else {
        for (uint32_t i = 0; i < pixelCount; i += 8) {
          extractor.extract(idx0, indexes[i + 0u]);
          extractor.extract(idx1, indexes[i + 1u]);

          cb(i + 0u, cbData);
          fetchPixel(src0);
          extractor.extract(idx0, indexes[i + 2u]);

          cb(i + 1u, cbData);
          fetchPixel(src1);
          extractor.extract(idx1, indexes[i + 3u]);

          cb(i + 2u, cbData);
          fetchPixel(src0);
          extractor.extract(idx0, indexes[i + 4u]);

          cb(i + 3u, cbData);
          fetchPixel(src1);
          extractor.extract(idx1, indexes[i + 5u]);

          cb(i + 4u, cbData);
          fetchPixel(src0);
          extractor.extract(idx0, indexes[i + 6u]);

          cb(i + 5u, cbData);
          fetchPixel(src1);
          if (_gatherMode == GatherMode::kFetchAll)
            extractor.extract(idx1, indexes[i + 7u]);

          cb(i + 6u, cbData);
          fetchPixel(src0);

          cb(i + 7u, cbData);
          if (_gatherMode == GatherMode::kFetchAll)
            fetchPixel(src1);
        }
      }

      cb(0xFF, cbData);
      break;
    }

    default:
      BL_NOT_REACHED();
  }
}

void FetchContext::_doneVec(uint32_t index) noexcept {
  if (_fetchMode == FetchMode::kRGBA32FromA8_PC) {
    if (_laneCount <= 4u) {
      _pc->v_interleave_lo_u8(_p128[index + 0], _p128[index + 0], _p128[index + 0]);
    }

    if (_laneCount <= 8u) {
      _pc->v_interleave_lo_u8(_p128[index + 0], _p128[index + 0], _p128[index + 0]);
    }

    if (_vecStep == 2u) {
      _pc->v_interleave_hi_u8(_p128[index + 1], _p128[index + 0], _p128[index + 0]);
      _pc->v_interleave_lo_u8(_p128[index + 0], _p128[index + 0], _p128[index + 0]);
    }
  }

  if (_fetchMode == FetchMode::kRGBA32FromA8_UC) {
#if defined(BL_JIT_ARCH_X86)
    if (_widening512Op != WideningOp::kNone) {
      // Keep it AS IS as we are widening 8 packed bytes to 64 unpacked bytes - a single byte to [0A 0A 0A 0A].
      BL_ASSERT(_pixel->count() >= 8u);
    }
    else if (_widening256Op != WideningOp::kNone) {
      BL_ASSERT(_pixel->count() >= 4u);
      BL_ASSERT(_widening256Op == WideningOp::kUnpack2x);

      _pc->v_interleave_lo_u8(_p128[index + 0], _p128[index + 0], _p128[index + 0]);     // [a7 a7 a6 a6 a5 a5 a4 a4 a3 a3 a2 a2 a1 a1 a0 a0]
      _pc->v_interleave_hi_u8(_p128[index + 1], _p128[index + 0], _p128[index + 0]);     // [a7 a7 a7 a7 a6 a6 a6 a6 a5 a5 a5 a5 a4 a4 a4 a4]
      _pc->v_interleave_lo_u8(_p128[index + 0], _p128[index + 0], _p128[index + 0]);     // [a3 a3 a3 a3 a2 a2 a2 a2 a1 a1 a1 a1 a0 a0 a0 a0]
    }
    else
#endif
    {
      switch (_vecStep) {
        case 1: {
          _pc->v_interleave_lo_u8(_p128[index + 0], _p128[index + 0], _p128[index + 0]); // [?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? ?? a1 a1 a0 a0]
          _pc->v_interleave_lo_u8(_p128[index + 0], _p128[index + 0], _p128[index + 0]); // [?? ?? ?? ?? ?? ?? ?? ?? a1 a1 a1 a1 a0 a0 a0 a0]
          _pc->v_cvt_u8_lo_to_u16(_p128[index + 0], _p128[index + 0]);                   // [00 a1 00 a1 00 a1 00 a1 00 a0 00 a0 00 a0 00 a0]
          break;
        }

        case 2: {
          _pc->v_interleave_lo_u8(_p128[index + 0], _p128[index + 0], _p128[index + 0]); // [?? ?? ?? ?? ?? ?? ?? ?? a3 a3 a2 a2 a1 a1 a0 a0]
          _pc->v_interleave_lo_u8(_p128[index + 0], _p128[index + 0], _p128[index + 0]); // [a3 a3 a3 a3 a2 a2 a2 a2 a1 a1 a1 a1 a0 a0 a0 a0]
          _pc->v_cvt_u8_hi_to_u16(_p128[index + 1], _p128[index + 0]);                   // [00 a3 00 a3 00 a3 00 a3 00 a2 00 a2 00 a2 00 a2]
          _pc->v_cvt_u8_lo_to_u16(_p128[index + 0], _p128[index + 0]);                   // [00 a1 00 a1 00 a1 00 a1 00 a0 00 a0 00 a0 00 a0]
          break;
        }

        case 4: {
          _pc->v_interleave_lo_u8(_p128[index + 0], _p128[index + 0], _p128[index + 0]); // [a7 a7 a6 a6 a5 a5 a4 a4 a3 a3 a2 a2 a1 a1 a0 a0]
          _pc->v_interleave_hi_u8(_p128[index + 2], _p128[index + 0], _p128[index + 0]); // [a7 a7 a7 a7 a6 a6 a6 a6 a5 a5 a5 a5 a4 a4 a4 a4]
          _pc->v_interleave_lo_u8(_p128[index + 0], _p128[index + 0], _p128[index + 0]); // [a3 a3 a3 a3 a2 a2 a2 a2 a1 a1 a1 a1 a0 a0 a0 a0]
          _pc->v_cvt_u8_hi_to_u16(_p128[index + 3], _p128[index + 2]);                   // [00 a7 00 a7 00 a7 00 a7 00 a6 00 a6 00 a6 00 a6]
          _pc->v_cvt_u8_lo_to_u16(_p128[index + 2], _p128[index + 2]);                   // [00 a5 00 a5 00 a5 00 a5 00 a4 00 a4 00 a4 00 a4]
          _pc->v_cvt_u8_hi_to_u16(_p128[index + 1], _p128[index + 0]);                   // [00 a3 00 a3 00 a3 00 a3 00 a2 00 a2 00 a2 00 a2]
          _pc->v_cvt_u8_lo_to_u16(_p128[index + 0], _p128[index + 0]);                   // [00 a1 00 a1 00 a1 00 a1 00 a0 00 a0 00 a0 00 a0]
          break;
        }

        default:
          BL_NOT_REACHED();
      }
    }
  }

  if (_fetchMode == FetchMode::kRGBA32FromRGBA32_UC) {
    if (_laneCount == 2u) {
      _pc->v_cvt_u8_lo_to_u16(_p128[index], _p128[index]);
    }
  }

#if defined(BL_JIT_ARCH_X86)
  // Firstly, widen to 256-bit wide registers and then decide whether to widen to 512-bit registers. In general both
  // can execute if we want to for example interleave and then unpack. However, if both widening operations are to
  // interleave, then they would not execute both here (as interleave to 512-bit requires 4 128-bit registers).
  bool widen512 = false;

  switch (_widening256Op) {
    case WideningOp::kNone: {
      break;
    }

    case WideningOp::kInterleave: {
      // We can interleave two vectors once we processed them, so check whether the currently processed vector was odd.
      if ((index & 0x1u) == 1u) {
        uint32_t index256 = index / 2u;
        uint32_t index128a = index - 1u;
        uint32_t index128b = index;

        _pc->v_insert_v128(_p256[index256], _p128[index128a].ymm(), _p128[index128b], 1u);
        widen512 = true;
      }
      break;
    }

    case WideningOp::kUnpack: {
      _pc->v_cvt_u8_lo_to_u16(_p256[index], _p128[index]);
      // Only used to unpack to 128-bit vector to 256-bit vector, so keep `widen512` false.
      break;
    }

    case WideningOp::kUnpack2x: {
      // Only used to unpack to 128-bit vectors to 256-bit vectors, so keep `widen512` false.
      _pc->v_cvt_u8_lo_to_u16(_p256[index + 0], _p128[index + 0]);
      _pc->v_cvt_u8_lo_to_u16(_p256[index + 1], _p128[index + 1]);
      break;
    }

    case WideningOp::kRepeat: {
      if (_widening512Op == WideningOp::kUnpack) {
        _pc->v_cvt_u8_to_u32(_p512[index], _p128[index]);
        _pc->v_swizzlev_u8(_p512[index], _p512[index], _pc->simdConst(&_pc->ct.swizu8_xxx3xxx2xxx1xxx0_to_z3z3z2z2z1z1z0z0, Bcst::kNA, _p512[index]));
      }
      else if (_widening512Op == WideningOp::kRepeat) {
        _pc->v_cvt_u8_to_u32(_p512[index], _p128[index]);
        _pc->v_swizzlev_u8(_p512[index], _p512[index], _pc->simdConst(&_pc->ct.swizu8_xxx3xxx2xxx1xxx0_to_3333222211110000, Bcst::kNA, _p512[index]));
      }
      else {
        _pc->v_cvt_u8_lo_to_u16(_p256[index], _p128[index]);
        _pc->v_mul_u16(_p256[index], _p256[index], _pc->simdConst(&_pc->ct.i_0101010101010101, Bcst::k32, _p256[index]));
        widen512 = true;
      }
      break;
    }

    case WideningOp::kRepeat8xA8ToRGBA32_UC_AVX512: {
      // This case widens 128-bit vector directly to a 512-bit vector, so keep `widen512` as false.
      if (_pc->hasAVX512_VBMI()) {
        Vec pred = _pc->simdVecConst(&_pc->ct.permu8_a8_to_rgba32_uc, Bcst::kNA_Unique, _p512[index]);
        _pc->v_permute_u8(_p512[index], pred, _p128[index].zmm());
      }
      else {
        _pc->cc->vpmovzxbq(_p512[index], _p128[index]);
        _pc->v_swizzle_lo_u16x4(_p512[index], _p512[index], swizzle(0, 0, 0, 0));
        _pc->v_swizzle_hi_u16x4(_p512[index], _p512[index], swizzle(0, 0, 0, 0));
      }
      break;
    }
  }

  // Secondly, widen to 512-bit wide registers.
  //
  // NOTE: Widening to 512-bit registers is there more for testing than practical use, because in general
  // we expect that gathers would be faster than scalar loads in AVX-512 mode. However, since DOWNFALL made
  // gathers pretty slow after a microcode update it's still possible we would hit this path in production.
  if (widen512) {
    switch (_widening512Op) {
      case WideningOp::kNone: {
        break;
      }

      case WideningOp::kInterleave: {
        if ((index & 0x3u) == 3u) {
          uint32_t index512 = index / 4u;
          uint32_t index256a = (index / 2u) - 1u;
          uint32_t index256b = (index / 2u);

          _pc->v_insert_v256(_p512[index512], _p256[index256a].zmm(), _p256[index256b], 1u);
        }
        break;
      }

      case WideningOp::kUnpack: {
        if ((index & 0x1u) == 1u) {
          uint32_t index512 = index / 2u;
          uint32_t index256 = index / 2u;
          _pc->v_cvt_u8_lo_to_u16(_p512[index512], _p256[index256]);
        }
        break;
      }

      case WideningOp::kRepeat: {
        if ((index & 0x1u) == 1u) {
          uint32_t index512 = index / 2u;
          uint32_t index256 = index;

          _pc->v_cvt_u8_lo_to_u16(_p512[index512], _p256[index256]);
          _pc->v_mul_u16(_p512[index512], _p512[index512], _pc->simdConst(&_pc->ct.i_0101010101010101, Bcst::k32, _p512[index]));
        }
        break;
      }

      default:
        BL_NOT_REACHED();
    }
  }
#endif // BL_JIT_ARCH_X86
}

void FetchContext::end() noexcept {}

// bl::Pipeline::JIT::FetchUtils - Convert Gathered Pixels
// =======================================================

static void convertGatheredPixels(PipeCompiler* pc, Pixel& p, PixelCount n, PixelFlags flags, const VecArray& gPix) noexcept {
  if (p.isA8()) {
    pc->v_srli_u32(gPix, gPix, 24);

    if (blTestFlag(flags, PixelFlags::kPA)) {
      VecWidth paVecWidth = pc->vecWidthOf(DataWidth::k8, n);
      uint32_t paRegCount = pc->vecCountOf(DataWidth::k8, n);

      pc->newVecArray(p.pa, paRegCount, paVecWidth, p.name(), "pa");
      BL_ASSERT(p.pa.size() == 1);

#if defined(BL_JIT_ARCH_X86)
      if (pc->hasAVX512()) {
        pc->cc->vpmovdb(p.pa[0], gPix[0]);
      }
      else
#endif // BL_JIT_ARCH_X86
      {
        pc->x_packs_i16_u8(p.pa[0].cloneAs(gPix[0]), gPix[0], gPix[0]);
        pc->x_packs_i16_u8(p.pa[0], p.pa[0], p.pa[0]);
      }
    }
    else {
      VecWidth uaVecWidth = pc->vecWidthOf(DataWidth::k16, n);
      uint32_t uaRegCount = pc->vecCountOf(DataWidth::k16, n);

      pc->newVecArray(p.ua, uaRegCount, uaVecWidth, p.name(), "ua");
      BL_ASSERT(p.ua.size() == 1);

#if defined(BL_JIT_ARCH_X86)
      if (pc->hasAVX512()) {
        pc->cc->vpmovdw(p.ua[0], gPix[0]);
      }
      else
#endif // BL_JIT_ARCH_X86
      {
        pc->x_packs_i16_u8(p.ua[0].cloneAs(gPix[0]), gPix[0], gPix[0]);
      }
    }
  }
  else if (p.isRGBA32()) {
    p.pc = gPix;
    pc->rename(p.pc, p.name(), "pc");
  }
  else {
#if defined(BL_JIT_ARCH_X86)
    if (!pc->use256BitSimd() && gPix[0].isVec256()) {
      Vec uc1 = pc->newV128(p.name(), "uc1");
      p.uc.init(gPix[0].xmm(), uc1);
      pc->cc->vextracti128(uc1, gPix[0], 1);
    }
    else
#endif // BL_JIT_ARCH_X86
    {
      p.uc = gPix;
      pc->rename(p.uc, p.name(), "uc");
    }
  }
}

// bl::Pipeline::JIT::FetchUtils - Gather Pixels
// =============================================

void gatherPixels(PipeCompiler* pc, Pixel& p, PixelCount n, PixelFlags flags, PixelFetchInfo fInfo, const Mem& src, const Vec& idx, uint32_t shift, IndexLayout indexLayout, GatherMode mode, InterleaveCallback cb, void* cbData) noexcept {
  Mem mem(src);

#if defined(BL_JIT_ARCH_X86)
  uint32_t bpp = fInfo.bpp();

  // Disabled gather means that we would gather to a wider register than enabled by the pipeline.
  bool disabledGather = pc->vecWidth() == VecWidth::k128 && n * bpp > 16u;

  // Forced gather means that we have to use gather because of the width of gathered data.
  bool forcedGather = pc->hasAVX512() && n > 8u;

  if (!disabledGather && (pc->hasOptFlag(PipeOptFlags::kFastGather) || forcedGather)) {
    // NOTE: Gathers are provided by AVX2 and later, thus if we are here it means at least AVX2 is available.
    BL_ASSERT(pc->hasAVX2());

    AsmCompiler* cc = pc->cc;
    uint32_t count = p.count().value();

    if (bpp == 4) {
      VecArray pixels;

      if (n <= 4u)
        pc->newV128Array(pixels, 1, p.name(), "pc");
      else if (n <= 8u)
        pc->newV256Array(pixels, 1, p.name(), "pc");
      else
        pc->newV512Array(pixels, 1, p.name(), "pc");

      Vec gatherIndex = idx.cloneAs(pixels[0]);

      switch (indexLayout) {
        case IndexLayout::kUInt16:
          gatherIndex = pc->newSimilarReg(pixels[0], "gatherIndex");
          cc->vpmovzxwd(gatherIndex, idx.xmm());
          break;

        case IndexLayout::kUInt32:
        case IndexLayout::kUInt32Lo16:
          // UInt32Lo16 expects that the high part is zero, so we can treat it as 32-bit index.
          break;

        case IndexLayout::kUInt32Hi16:
          gatherIndex = pc->newSimilarReg(pixels[0], "gatherIndex");
          pc->v_srli_u32(gatherIndex, idx.cloneAs(gatherIndex), 16);
          break;

        default:
          BL_NOT_REACHED();
      }

      mem.setIndex(gatherIndex);
      mem.setShift(shift);

      pc->v_zero_i(pixels[0]);
      if (pc->hasAVX512()) {
        KReg pred = cc->newKw("pred");
        cc->kxnorw(pred, pred, pred);
        cc->k(pred).vpgatherdd(pixels[0], mem);
      }
      else {
        Vec pred = pc->newSimilarReg(pixels[0], "pred");
        pc->v_ones_i(pred);
        cc->vpgatherdd(pixels[0], mem, pred);
      }

      for (uint32_t i = 0; i < count; i++)
        cb(i, cbData);

      convertGatheredPixels(pc, p, n, flags, pixels);
      cb(0xFF, cbData);
      return;
    }

    if (bpp == 8) {
      VecArray pixels;

      if (n <= 4u) {
        pc->newV256Array(pixels, 1, p.name(), "pc");
      }
      else if (pc->use512BitSimd()) {
        pc->newV512Array(pixels, n.value() / 8, p.name(), "pc");
      }
      else {
        pc->newV256Array(pixels, 2, p.name(), "pc");
      }

      Vec gatherIndex = idx.cloneAs(pixels[0]);

      switch (indexLayout) {
        case IndexLayout::kUInt16:
          gatherIndex = pc->newSimilarReg(pixels[0], "gatherIndex");
          cc->vpmovzxwd(gatherIndex, idx.xmm());
          break;

        case IndexLayout::kUInt32:
        case IndexLayout::kUInt32Lo16:
          // UInt32Lo16 expects that the high part is zero, so we can treat it as 32-bit index.
          break;

        case IndexLayout::kUInt32Hi16:
          gatherIndex = pc->newSimilarReg(pixels[0], "gatherIndex");
          pc->v_srli_u32(gatherIndex, idx.cloneAs(gatherIndex), 16);
          break;

        default:
          BL_NOT_REACHED();
      }

      if (pc->use512BitSimd() && n.value() >= 8)
        mem.setIndex(gatherIndex.ymm());
      else
        mem.setIndex(gatherIndex.xmm());
      mem.setShift(shift);

      for (uint32_t i = 0; i < pixels.size(); i++) {
        if (i == 1) {
          if (pc->use512BitSimd() && n.value() == 16) {
            Vec gi2 = pc->newSimilarReg(gatherIndex, "gatherIndex2");
            cc->vextracti32x8(gi2.ymm(), gatherIndex.zmm(), 1);
            mem.setIndex(gi2.ymm());
          }
          else {
            Vec gi2 = pc->newSimilarReg(gatherIndex, "gatherIndex2");
            cc->vextracti128(gi2.xmm(), gatherIndex.ymm(), 1);
            mem.setIndex(gi2.xmm());
          }
        }

        pc->v_zero_i(pixels[i]);
        if (pc->hasAVX512()) {
          KReg pred = cc->newKw("pred");
          cc->kxnorw(pred, pred, pred);
          cc->k(pred).vpgatherdq(pixels[i], mem);
        }
        else {
          Vec pred = pc->newSimilarReg(pixels[i], "pred");
          pc->v_ones_i(pred);
          cc->vpgatherdq(pixels[i], mem, pred);
        }

        uint32_t granularity = pixels[i].size() / 8u;
        for (uint32_t step = 0; step < granularity; step++)
          cb(i * granularity + step, cbData);
      }

      convertGatheredPixels(pc, p, n, flags, pixels);
      cb(0xFF, cbData);
      return;
    }
  }
#endif // BL_JIT_ARCH_X86

  uint32_t indexType = 0;
  const uint8_t* indexSequence = nullptr;

  static const uint8_t oddIndexes[] = { 1, 3, 5, 7, 9, 11, 13, 15 };
  static const uint8_t evenIndexes[] = { 0, 2, 4, 6, 8, 10, 12, 14 };
  static const uint8_t consecutiveIndexes[] = { 0, 1, 2, 3, 4, 5, 6, 7 };

  switch (indexLayout) {
    case IndexLayout::kUInt16:
      indexType = IndexExtractor::kTypeUInt16;
      indexSequence = consecutiveIndexes;
      break;

    case IndexLayout::kUInt32:
      indexType = IndexExtractor::kTypeUInt32;
      indexSequence = consecutiveIndexes;
      break;

    case IndexLayout::kUInt32Lo16:
      indexType = IndexExtractor::kTypeUInt16;
      indexSequence = evenIndexes;
      break;

    case IndexLayout::kUInt32Hi16:
      indexType = IndexExtractor::kTypeUInt16;
      indexSequence = oddIndexes;
      break;

    default:
      BL_NOT_REACHED();
  }

  IndexExtractor indexExtractor(pc);
  indexExtractor.begin(indexType, idx);

  FetchContext fCtx(pc, &p, n, flags, fInfo, mode);
  fCtx._fetchAll(src, shift, indexExtractor, indexSequence, cb, cbData);
  fCtx.end();
}

} // {FetchUtils}
} // {JIT}
} // {Pipeline}
} // {bl}

#endif // !BL_BUILD_NO_JIT
