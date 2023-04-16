// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_PIPECOMPILER_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_PIPECOMPILER_P_H_INCLUDED

#include "../../runtime_p.h"
#include "../../pipeline/jit/pipegencore_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace BLPipeline {
namespace JIT {

//! Pipeline compiler.
class PipeCompiler {
public:
  BL_NONCOPYABLE(PipeCompiler)

  enum : uint32_t { kMaxKRegConstCount = 4 };

  //! \name Members
  //! \{

  //! AsmJit compiler.
  x86::Compiler* cc = nullptr;

  //! Target CPU features.
  CpuFeatures _features {};
  //! Optimization flags.
  PipeOptFlags _optFlags = PipeOptFlags::kNone;

  //! Empty predicate, used in cases where a predicate is required, but it's empty.
  PixelPredicate _emptyPredicate {};

  //! SIMD width.
  SimdWidth _simdWidth = SimdWidth::k128;
  //! SIMD multiplier, derived from `_simdWidth` (1, 2, 4).
  uint8_t _simdMultiplier = 0;
  //! SIMD register type (AsmJit).
  asmjit::RegType _simdRegType = asmjit::RegType::kNone;
  //! SIMD type id (AsmJit).
  asmjit::TypeId _simdTypeId = asmjit::TypeId::kVoid;

  //! Function node.
  asmjit::FuncNode* _funcNode = nullptr;
  //! Function initialization hook.
  asmjit::BaseNode* _funcInit = nullptr;
  //! Function end hook (to add 'unlikely' branches).
  asmjit::BaseNode* _funcEnd = nullptr;

  //! Invalid GP register.
  x86::Gp _gpNone;
  //! Holds `ctxData` argument.
  x86::Gp _ctxData;
  //! Holds `fillData` argument.
  x86::Gp _fillData;
  //! Holds `fetchData` argument.
  x86::Gp _fetchData;
  //! Temporary stack used to transfer SIMD regs to GP/MM.
  x86::Mem _tmpStack;

  //! Offset to the first constant to the `blCommonTable` global.
  int32_t _commonTableOff = 0;
  //! Pointer to the `blCommonTable` constant pool (only used in 64-bit mode).
  x86::Gp _commonTablePtr;

  x86::KReg _kReg[kMaxKRegConstCount];
  uint64_t _kImm[kMaxKRegConstCount] {};

  struct VecConst {
    const void* ptr;
    uint32_t vRegId;
  };

  asmjit::ZoneVector<VecConst> _vecConsts;

  //! \}

  //! \name Packed Instruction
  //! \{

  //! Packing generic instructions and SSE+AVX instructions into a single 32-bit integer.
  //!
  //! AsmJit has more than 1500 instructions for X86|X64, which means that we need
  //! at least 11 bits to represent each. Typically we need just one instruction
  //! ID at a time, however, since SSE and AVX instructions use different IDs
  //! we need a way to pack both SSE and AVX instruction ID into one integer as
  //! it's much easier to use a unified instruction set rather than using specific
  //! paths for SSE and AVX code.
  //!
  //! PackedInst allows to specify the following:
  //!
  //!   - SSE instruction ID for SSE+ code generation.
  //!   - AVX instruction ID for AVX+ code generation.
  //!   - Maximum operation width (0=XMM, 1=YMM, 2=ZMM).
  //!   - Special intrinsic used only by PipeCompiler.
  struct PackedInst {
    //! Limit width of operands of vector instructions to Xmm|Ymm|Zmm.
    enum WidthLimit : uint32_t {
      kWidthX = 0,
      kWidthY = 1,
      kWidthZ = 2
    };

    enum Bits : uint32_t {
      kSseIdShift  = 0,
      kSseIdBits   = 0xFFF,

      kAvxIdShift  = 12,
      kAvxIdBits   = 0xFFF,

      kWidthShift  = 24,
      kWidthBits   = 0x3,

      kIntrinShift = 31,
      kIntrinBits  = 0x1
    };

    static constexpr inline uint32_t packIntrin(uint32_t intrinId, uint32_t width = kWidthZ) noexcept {
      return (intrinId << kSseIdShift ) |
             (width    << kWidthShift ) |
             (1u       << kIntrinShift) ;
    }

    static constexpr inline uint32_t packAvxSse(uint32_t avxId, uint32_t sseId, uint32_t width = kWidthZ) noexcept {
      return (avxId << kAvxIdShift) |
             (sseId << kSseIdShift) |
             (width << kWidthShift) ;
    }

    static constexpr inline uint32_t avxId(uint32_t packedId) noexcept { return (packedId >> kAvxIdShift) & kAvxIdBits; }
    static constexpr inline uint32_t sseId(uint32_t packedId) noexcept { return (packedId >> kSseIdShift) & kSseIdBits; }
    static constexpr inline uint32_t width(uint32_t packedId) noexcept { return (packedId >> kWidthShift) & kWidthBits; }

    static constexpr inline uint32_t isIntrin(uint32_t packedId) noexcept { return (packedId & (kIntrinBits << kIntrinShift)) != 0; }
    static constexpr inline uint32_t intrinId(uint32_t packedId) noexcept { return (packedId >> kSseIdShift) & kSseIdBits; }
  };

  //! Intrinsic ID.
  //!
  //! Some operations are not available as a single instruction or are part
  //! of CPU extensions outside of the baseline instruction set. These are
  //! handled as intrinsics.
  enum IntrinId : uint32_t {
    kIntrin2Vloadi128uRO,

    kIntrin2Vmovu8u16,
    kIntrin2Vmovu8u32,
    kIntrin2Vmovu16u32,
    kIntrin2Vabsi8,
    kIntrin2Vabsi16,
    kIntrin2Vabsi32,
    kIntrin2Vabsi64,
    kIntrin2Vinv255u16,
    kIntrin2Vinv256u16,
    kIntrin2Vinv255u32,
    kIntrin2Vinv256u32,
    kIntrin2Vduplpd,
    kIntrin2Vduphpd,

    kIntrin2VBroadcastU8,
    kIntrin2VBroadcastU16,
    kIntrin2VBroadcastU32,
    kIntrin2VBroadcastU64,
    kIntrin2VBroadcastI32x4,
    kIntrin2VBroadcastI64x2,
    kIntrin2VBroadcastF32x4,
    kIntrin2VBroadcastF64x2,

    kIntrin2iVswizps,
    kIntrin2iVswizpd,

    kIntrin3Vandi32,
    kIntrin3Vandi64,
    kIntrin3Vnandi32,
    kIntrin3Vnandi64,
    kIntrin3Vori32,
    kIntrin3Vori64,
    kIntrin3Vxori32,
    kIntrin3Vxori64,
    kIntrin3Vcombhli64,
    kIntrin3Vcombhld64,
    kIntrin3Vminu16,
    kIntrin3Vmaxu16,
    kIntrin3Vmulu64x32,
    kIntrin3Vhaddpd,

    kIntrin4Vpblendvb,
    kIntrin4VpblendvbDestructive
  };

  //! \}

  //! \name Construction & Destruction
  //! \{

  PipeCompiler(x86::Compiler* cc, const asmjit::CpuFeatures& features, PipeOptFlags optFlags) noexcept;
  ~PipeCompiler() noexcept;

  //! \}

  //! \name Allocators
  //! \{

  inline asmjit::ZoneAllocator* zoneAllocator() noexcept { return &cc->_allocator; }

  //! \}

  //! \name CPU SIMD Width and SIMD Width Utilities
  //! \{

  //! Returns the current SIMD width (in bytes) that this compiler and all its parts must use.
  //!
  //! \note The returned width is in bytes and it's calculated from the maximum supported widths of all pipeline parts.
  //! This means that SIMD width returned could be actually lower than a SIMD width supported by the target CPU.
  inline SimdWidth simdWidth() const noexcept { return _simdWidth; }

  //! Returns whether the compiler and all parts use 256-bit SIMD.
  inline bool use256BitSimd() const noexcept { return _simdWidth >= SimdWidth::k256; }
  //! Returns whether the compiler and all parts use 512-bit SIMD.
  inline bool use512BitSimd() const noexcept { return _simdWidth >= SimdWidth::k512; }

  //! Returns a constant that can be used to multiply a baseline SIMD width to get the value returned by `simdWidth()`.
  //!
  //! \note A baseline SIMD width would be 16 bytes on most platforms.
  inline uint32_t simdMultiplier() const noexcept { return _simdMultiplier; }

  inline SimdWidth simdWidthOf(DataWidth dataWidth, uint32_t n) const noexcept { return SimdWidthUtils::simdWidthOf(simdWidth(), dataWidth, n); }
  inline uint32_t regCountOf(DataWidth dataWidth, uint32_t n) const noexcept { return SimdWidthUtils::regCountOf(simdWidth(), dataWidth, n); }

  inline SimdWidth simdWidthOf(DataWidth dataWidth, PixelCount pixelCount) const noexcept { return SimdWidthUtils::simdWidthOf(simdWidth(), dataWidth, pixelCount.value()); }
  inline uint32_t regCountOf(DataWidth dataWidth, PixelCount pixelCount) const noexcept { return SimdWidthUtils::regCountOf(simdWidth(), dataWidth, pixelCount.value()); }

  //! \}

  //! \name CPU Features and Optimization Options
  //! \{

  //! Tests whether SSE2 extensions are available (this should always return true as Blend2D requires SSE2).
  inline bool hasSSE2() const noexcept { return _features.x86().hasSSE2(); }
  //! Tests whether SSE3 extensions are available.
  inline bool hasSSE3() const noexcept { return _features.x86().hasSSE3(); }
  //! Tests whether SSSE3 extensions are available.
  inline bool hasSSSE3() const noexcept { return _features.x86().hasSSSE3(); }
  //! Tests whether SSE4.1 extensions are available.
  inline bool hasSSE4_1() const noexcept { return _features.x86().hasSSE4_1(); }
  //! Tests whether SSE4.2 extensions are available.
  inline bool hasSSE4_2() const noexcept { return _features.x86().hasSSE4_2(); }
  //! Tests whether AVX extensions are available.
  inline bool hasAVX() const noexcept { return _features.x86().hasAVX(); }
  //! Tests whether AVX2 extensions are available.
  inline bool hasAVX2() const noexcept { return _features.x86().hasAVX2(); }
  //! Tests whether a baseline AVX-512 extensions are available.
  //!
  //! \note Baseline for us is slightly more than AVX512-F, however, there are no CPUs that would implement
  //! AVX512-F without other extensions that we consider baseline, so we only check AVX512_BW as it's enough
  //! to verify that the CPU has all the required features.
  inline bool hasAVX512() const noexcept { return _features.x86().hasAVX512_BW(); }

  inline bool hasADX() const noexcept { return _features.x86().hasADX(); }
  inline bool hasBMI() const noexcept { return _features.x86().hasBMI(); }
  inline bool hasBMI2() const noexcept { return _features.x86().hasBMI2(); }
  inline bool hasLZCNT() const noexcept { return _features.x86().hasLZCNT(); }
  inline bool hasPOPCNT() const noexcept { return _features.x86().hasPOPCNT(); }

  inline PipeOptFlags optFlags() const noexcept { return _optFlags; }
  inline bool hasOptFlag(PipeOptFlags flag) const noexcept { return blTestFlag(_optFlags, flag); }

  bool hasMaskedAccessOf(uint32_t dataSize) const noexcept;

  //! \}

  //! \name Function Definition
  //! \{

  void beginFunction() noexcept;
  void endFunction() noexcept;

  //! \}

  //! \name Parts Management
  //! \{

  // TODO: [PIPEGEN] There should be a getter on asmjit side that will return
  // the `ZoneAllocator` object that can be used for these kind of purposes.
  // It doesn't make sense to create another ZoneAllocator.
  template<typename T>
  inline T* newPartT() noexcept {
    void* p = cc->_codeZone.alloc(sizeof(T), 8);
    if (BL_UNLIKELY(!p))
      return nullptr;
    return new(BLInternal::PlacementNew{p}) T(this);
  }

  template<typename T, typename... Args>
  inline T* newPartT(Args&&... args) noexcept {
    void* p = cc->_codeZone.alloc(sizeof(T), 8);
    if (BL_UNLIKELY(!p))
      return nullptr;
    return new(BLInternal::PlacementNew{p}) T(this, std::forward<Args>(args)...);
  }

  FillPart* newFillPart(FillType fillType, FetchPart* dstPart, CompOpPart* compOpPart) noexcept;
  FetchPart* newFetchPart(FetchType fetchType, BLInternalFormat format) noexcept;
  CompOpPart* newCompOpPart(uint32_t compOp, FetchPart* dstPart, FetchPart* srcPart) noexcept;

  //! \}

  //! \name Initialization
  //! \{

  void _initSimdWidth(PipePart* root) noexcept;
  void initPipeline(PipePart* root) noexcept;

  //! Generate a function of the given `signature`.
  void compileFunc(uint32_t signature) noexcept;

  //! \}

  //! \name Miscellaneous Helpers
  //! \{

  BL_INLINE void rename(const OpArray& opArray, const char* name) noexcept {
    for (uint32_t i = 0; i < opArray.size(); i++)
      cc->rename(opArray[i].as<asmjit::BaseReg>(), "%s%u", name, unsigned(i));
  }

  BL_INLINE void rename(const OpArray& opArray, const char* prefix, const char* name) noexcept {
    for (uint32_t i = 0; i < opArray.size(); i++)
      cc->rename(opArray[i].as<asmjit::BaseReg>(), "%s%s%u", prefix, name, unsigned(i));
  }

  //! \}

  //! \name Constants
  //! \{

  void _initCommonTablePtr() noexcept;

  x86::KReg kConst(uint64_t value) noexcept;

  Operand simdConst(const void* c, Bcst bcstWidth, SimdWidth constWidth) noexcept;
  Operand simdConst(const void* c, Bcst bcstWidth, const x86::Vec& similarTo) noexcept;
  Operand simdConst(const void* c, Bcst bcstWidth, const VecArray& similarTo) noexcept;

  x86::Vec simdVecConst(const void* c, SimdWidth constWidth) noexcept;
  x86::Vec simdVecConst(const void* c, const x86::Vec& similarTo) noexcept;
  x86::Vec simdVecConst(const void* c, const VecArray& similarTo) noexcept;

  x86::Mem simdMemConst(const void* c, Bcst bcstWidth, SimdWidth constWidth) noexcept;
  x86::Mem simdMemConst(const void* c, Bcst bcstWidth, const x86::Vec& similarTo) noexcept;
  x86::Mem simdMemConst(const void* c, Bcst bcstWidth, const VecArray& similarTo) noexcept;

  x86::Mem _getMemConst(const void* c) noexcept;
  x86::Vec _newVecConst(const void* c) noexcept;

  //! \}

  //! \name Predication
  //! \{

  BL_INLINE PixelPredicate& emptyPredicate() noexcept { return _emptyPredicate; }

  //! \}

  //! \name Registers & Memory
  //! \{

  template<typename... Args>
  BL_INLINE x86::Vec newVec(const char* name, Args&&... args) noexcept {
    x86::Vec reg;
    cc->_newRegFmt(&reg, _simdTypeId, name, std::forward<Args>(args)...);
    return reg;
  }

  template<typename... Args>
  BL_INLINE x86::Vec newVec(SimdWidth simdWidth, const char* name, Args&&... args) noexcept {
    x86::Vec reg;
    cc->_newRegFmt(&reg, SimdWidthUtils::typeIdOf(simdWidth), name, std::forward<Args>(args)...);
    return reg;
  }

  template<typename... Args>
  BL_INLINE x86::Xmm newXmm(const char* name, Args&&... args) noexcept {
    x86::Xmm reg;
    cc->_newRegFmt(&reg, asmjit::TypeId::kInt32x4, name, std::forward<Args>(args)...);
    return reg;
  }

  template<typename... Args>
  BL_INLINE x86::Ymm newYmm(const char* name, Args&&... args) noexcept {
    x86::Ymm reg;
    cc->_newRegFmt(&reg, asmjit::TypeId::kInt32x8, name, std::forward<Args>(args)...);
    return reg;
  }

  template<typename... Args>
  BL_INLINE x86::Zmm newZmm(const char* name, Args&&... args) noexcept {
    x86::Zmm reg;
    cc->_newRegFmt(&reg, asmjit::TypeId::kInt32x16, name, std::forward<Args>(args)...);
    return reg;
  }

  BL_NOINLINE void newRegArray(OpArray& dst, uint32_t n, asmjit::TypeId typeId, const char* name) noexcept {
    BL_ASSERT(n <= OpArray::kMaxSize);
    dst._size = n;
    for (uint32_t i = 0; i < n; i++) {
      cc->_newRegFmt(&dst[i].as<asmjit::BaseReg>(), typeId, "%s%u", name, i);
    }
  }

  BL_NOINLINE void newRegArray(OpArray& dst, uint32_t n, asmjit::TypeId typeId, const char* prefix, const char* name) noexcept {
    BL_ASSERT(n <= OpArray::kMaxSize);
    dst._size = n;
    for (uint32_t i = 0; i < n; i++) {
      cc->_newRegFmt(&dst[i].as<asmjit::BaseReg>(), typeId, "%s%s%u", prefix, name, i);
    }
  }

  BL_NOINLINE void newRegArray(OpArray& dst, uint32_t n, const asmjit::BaseReg& ref, const char* name) noexcept {
    BL_ASSERT(n <= OpArray::kMaxSize);
    dst._size = n;
    for (uint32_t i = 0; i < n; i++) {
      cc->_newRegFmt(&dst[i].as<asmjit::BaseReg>(), ref, "%s%u", name, i);
    }
  }

  BL_NOINLINE void newRegArray(OpArray& dst, uint32_t n, const asmjit::BaseReg& ref, const char* prefix, const char* name) noexcept {
    BL_ASSERT(n <= OpArray::kMaxSize);
    dst._size = n;
    for (uint32_t i = 0; i < n; i++) {
      cc->_newRegFmt(&dst[i].as<asmjit::BaseReg>(), ref, "%s%s%u", prefix, name, i);
    }
  }

  BL_INLINE void newXmmArray(OpArray& dst, uint32_t n, const char* name) noexcept {
    newRegArray(dst, n, asmjit::TypeId::kInt32x4, name);
  }

  BL_INLINE void newXmmArray(OpArray& dst, uint32_t n, const char* prefix, const char* name) noexcept {
    newRegArray(dst, n, asmjit::TypeId::kInt32x4, prefix, name);
  }

  BL_INLINE void newYmmArray(OpArray& dst, uint32_t n, const char* name) noexcept {
    newRegArray(dst, n, asmjit::TypeId::kInt32x8, name);
  }

  BL_INLINE void newYmmArray(OpArray& dst, uint32_t n, const char* prefix, const char* name) noexcept {
    newRegArray(dst, n, asmjit::TypeId::kInt32x8, prefix, name);
  }

  // TODO: This should be removed - this can lead to bugs.
  BL_INLINE void newVecArray(OpArray& dst, uint32_t n, const char* name) noexcept {
    newRegArray(dst, n, _simdTypeId, name);
  }

  BL_INLINE void newVecArray(OpArray& dst, uint32_t n, const char* prefix, const char* name) noexcept {
    newRegArray(dst, n, _simdTypeId, prefix, name);
  }

  BL_INLINE void newVecArray(OpArray& dst, uint32_t n, SimdWidth simdWidth, const char* name) noexcept {
    newRegArray(dst, n, SimdWidthUtils::typeIdOf(simdWidth), name);
  }

  BL_INLINE void newVecArray(OpArray& dst, uint32_t n, SimdWidth simdWidth, const char* prefix, const char* name) noexcept {
    newRegArray(dst, n, SimdWidthUtils::typeIdOf(simdWidth), prefix, name);
  }

  BL_INLINE void newVecArray(OpArray& dst, uint32_t n, const x86::Vec& ref, const char* name) noexcept {
    newRegArray(dst, n, ref, name);
  }

  BL_INLINE void newVecArray(OpArray& dst, uint32_t n, const x86::Vec& ref, const char* prefix, const char* name) noexcept {
    newRegArray(dst, n, ref, prefix, name);
  }

  x86::Mem tmpStack(uint32_t size) noexcept;

  //! \}

  //! \name Utilities
  //! \{

  inline void bind(const Label& label) noexcept { cc->bind(label); }

  //! \}

  //! \name Utilities
  //! \{

  void embedJumpTable(const Label* jumpTable, size_t jumpTableSize, const Label& jumpTableBase, uint32_t entrySize) noexcept;

  //! \}

  //! \name Emit - Commons
  //! \{

  // Emit helpers used by GP.
  void i_emit_2(InstId instId, const Operand_& op1, int imm) noexcept;
  void i_emit_2(InstId instId, const Operand_& op1, const Operand_& op2) noexcept;
  void i_emit_3(InstId instId, const Operand_& op1, const Operand_& op2, int imm) noexcept;

  // Emit helpers to emit MOVE from SrcT to DstT, used by pre-AVX instructions. The `width` parameter is important
  // as it describes how many bytes to read in case that `src` is a memory location. It's important as some
  // instructions like PMOVZXBW read only 8 bytes, but to make the same operation in pre-SSE4.1 code we need to
  // read 8 bytes from memory and use PUNPCKLBW to interleave that bytes with zero. PUNPCKLBW would read 16 bytes
  // from memory and would require them to be aligned to 16 bytes, if used with memory operand.
  void v_emit_xmov(const Operand_& dst, const Operand_& src, uint32_t width) noexcept;
  void v_emit_xmov(const OpArray& dst, const Operand_& src, uint32_t width) noexcept;
  void v_emit_xmov(const OpArray& dst, const OpArray& src, uint32_t width) noexcept;

  // Emit helpers used by SSE|AVX intrinsics.
  void v_emit_vv_vv(uint32_t packedId, const Operand_& dst_, const Operand_& src_) noexcept;
  void v_emit_vv_vv(uint32_t packedId, const OpArray& dst_, const Operand_& src_) noexcept;
  void v_emit_vv_vv(uint32_t packedId, const OpArray& dst_, const OpArray& src_) noexcept;

  void v_emit_vvi_vi(uint32_t packedId, const Operand_& dst_, const Operand_& src_, uint32_t imm) noexcept;
  void v_emit_vvi_vi(uint32_t packedId, const OpArray& dst_, const Operand_& src_, uint32_t imm) noexcept;
  void v_emit_vvi_vi(uint32_t packedId, const OpArray& dst_, const OpArray& src_, uint32_t imm) noexcept;

  void v_emit_vvi_vvi(uint32_t packedId, const Operand_& dst_, const Operand_& src_, uint32_t imm) noexcept;
  void v_emit_vvi_vvi(uint32_t packedId, const OpArray& dst_, const Operand_& src_, uint32_t imm) noexcept;
  void v_emit_vvi_vvi(uint32_t packedId, const OpArray& dst_, const OpArray& src_, uint32_t imm) noexcept;

  void v_emit_vvv_vv(uint32_t packedId, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_) noexcept;
  void v_emit_vvv_vv(uint32_t packedId, const OpArray& dst_, const Operand_& src1_, const OpArray& src2_) noexcept;
  void v_emit_vvv_vv(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const Operand_& src2_) noexcept;
  void v_emit_vvv_vv(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_) noexcept;

  void v_emit_vvvi_vvi(uint32_t packedId, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_, uint32_t imm) noexcept;
  void v_emit_vvvi_vvi(uint32_t packedId, const OpArray& dst_, const Operand_& src1_, const OpArray& src2_, uint32_t imm) noexcept;
  void v_emit_vvvi_vvi(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const Operand_& src2_, uint32_t imm) noexcept;
  void v_emit_vvvi_vvi(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_, uint32_t imm) noexcept;

  void v_emit_vvvv_vvv(uint32_t packedId, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_, const Operand_& src3_) noexcept;
  void v_emit_vvvv_vvv(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_, const Operand_& src3_) noexcept;
  void v_emit_vvvv_vvv(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_, const OpArray& src3_) noexcept;

  void v_emit_k_vv(InstId instId, const x86::KReg& mask, const Operand_& dst, const Operand_& src) noexcept;
  void v_emit_k_vv(InstId instId, const x86::KReg& mask, OpArray& dst, const Operand_& src) noexcept;
  void v_emit_k_vv(InstId instId, const x86::KReg& mask, OpArray& dst, const OpArray& src) noexcept;

  void v_emit_k_vvi(InstId instId, const x86::KReg& mask, const Operand_& dst, const Operand_& src, uint32_t imm8) noexcept;
  void v_emit_k_vvi(InstId instId, const x86::KReg& mask, OpArray& dst, const Operand_& src, uint32_t imm8) noexcept;
  void v_emit_k_vvi(InstId instId, const x86::KReg& mask, OpArray& dst, const OpArray& src, uint32_t imm8) noexcept;

  void v_emit_k_vvv(InstId instId, const x86::KReg& mask, const Operand_& dst, const Operand_& src1, const Operand_& src2) noexcept;
  void v_emit_k_vvv(InstId instId, const x86::KReg& mask, const OpArray& dst, const Operand_& src1, const OpArray& src2) noexcept;
  void v_emit_k_vvv(InstId instId, const x86::KReg& mask, const OpArray& dst, const OpArray& src1, const Operand_& src2) noexcept;
  void v_emit_k_vvv(InstId instId, const x86::KReg& mask, const OpArray& dst, const OpArray& src1, const OpArray& src2) noexcept;

  void v_emit_k_vvvi(InstId instId, const x86::KReg& mask, const Operand_& dst, const Operand_& src1, const Operand_& src2, uint32_t imm8) noexcept;
  void v_emit_k_vvvi(InstId instId, const x86::KReg& mask, const OpArray& dst, const Operand_& src1, const OpArray& src2, uint32_t imm8) noexcept;
  void v_emit_k_vvvi(InstId instId, const x86::KReg& mask, const OpArray& dst, const OpArray& src1, const Operand_& src2, uint32_t imm8) noexcept;
  void v_emit_k_vvvi(InstId instId, const x86::KReg& mask, const OpArray& dst, const OpArray& src1, const OpArray& src2, uint32_t imm8) noexcept;

  #define I_EMIT_2(NAME, INST_ID)                                             \
  template<typename Op1, typename Op2>                                        \
  inline void NAME(const Op1& o1,                                             \
                   const Op2& o2) noexcept {                                  \
    i_emit_2(x86::Inst::kId##INST_ID, o1, o2);                                \
  }

  #define I_EMIT_3(NAME, INST_ID)                                             \
  template<typename Op1, typename Op2, typename Op3>                          \
  inline void NAME(const Op1& o1,                                             \
                   const Op2& o2,                                             \
                   const Op3& o3) noexcept {                                  \
    i_emit_3(x86::Inst::kId##INST_ID, o1, o2, o3);                            \
  }

  #define V_EMIT_VV_VV(NAME, PACKED_ID)                                       \
  template<typename DstT, typename SrcT>                                      \
  inline void NAME(const DstT& dst,                                           \
                   const SrcT& src) noexcept {                                \
    v_emit_vv_vv(PACKED_ID, dst, src);                                        \
  }

  #define V_EMIT_VVI_VI(NAME, PACKED_ID)                                      \
  template<typename DstT, typename SrcT>                                      \
  inline void NAME(const DstT& dst,                                           \
                   const SrcT& src,                                           \
                   uint32_t imm) noexcept {                                   \
    v_emit_vvi_vi(PACKED_ID, dst, src, imm);                                  \
  }

  #define V_EMIT_VVI_VVI(NAME, PACKED_ID)                                     \
  template<typename DstT, typename SrcT>                                      \
  inline void NAME(const DstT& dst,                                           \
                   const SrcT& src,                                           \
                   uint32_t imm) noexcept {                                   \
    v_emit_vvi_vvi(PACKED_ID, dst, src, imm);                                 \
  }

  #define V_EMIT_VVI_VVI_ENUM(NAME, PACKED_ID, ENUM_TYPE)                     \
  template<typename DstT, typename SrcT>                                      \
  inline void NAME(const DstT& dst,                                           \
                   const SrcT& src,                                           \
                   ENUM_TYPE imm) noexcept {                                  \
    v_emit_vvi_vvi(PACKED_ID, dst, src, uint32_t(imm));                       \
  }

  #define V_EMIT_VVi_VVi(NAME, PACKED_ID, IMM_VALUE)                          \
  template<typename DstT, typename SrcT>                                      \
  inline void NAME(const DstT& dst,                                           \
                   const SrcT& src) noexcept {                                \
    v_emit_vvi_vvi(PACKED_ID, dst, src, IMM_VALUE);                           \
  }

  #define V_EMIT_VVV_VV(NAME, PACKED_ID)                                      \
  template<typename DstT, typename Src1T, typename Src2T>                     \
  inline void NAME(const DstT& dst,                                           \
                   const Src1T& src1,                                         \
                   const Src2T& src2) noexcept {                              \
    v_emit_vvv_vv(PACKED_ID, dst, src1, src2);                                \
  }

  #define V_EMIT_VVVI_VVI(NAME, PACKED_ID)                                    \
  template<typename DstT, typename Src1T, typename Src2T>                     \
  inline void NAME(const DstT& dst,                                           \
                   const Src1T& src1,                                         \
                   const Src2T& src2,                                         \
                   uint32_t imm) noexcept {                                   \
    v_emit_vvvi_vvi(PACKED_ID, dst, src1, src2, imm);                         \
  }

  #define V_EMIT_VVVI_VVI_ENUM(NAME, PACKED_ID, ENUM_TYPE)                    \
  template<typename DstT, typename Src1T, typename Src2T>                     \
  inline void NAME(const DstT& dst,                                           \
                   const Src1T& src1,                                         \
                   const Src2T& src2,                                         \
                   ENUM_TYPE imm) noexcept {                                  \
    v_emit_vvvi_vvi(PACKED_ID, dst, src1, src2, uint32_t(imm));               \
  }

  #define V_EMIT_VVVI(NAME, PACKED_ID)                                        \
  template<typename DstT, typename Src1T, typename Src2T>                     \
  inline void NAME(const DstT& dst,                                           \
                   const Src1T& src1,                                         \
                   const Src2T& src2,                                         \
                   uint32_t imm) noexcept {                                   \
    v_emit_vvvi_vvi(PACKED_ID, dst, src1, src2, imm);                         \
  }

  #define V_EMIT_VVVi_VVi(NAME, PACKED_ID, IMM_VALUE)                         \
  template<typename DstT, typename Src1T, typename Src2T>                     \
  inline void NAME(const DstT& dst,                                           \
                   const Src1T& src1,                                         \
                   const Src2T& src2) noexcept {                              \
    v_emit_vvvi_vvi(PACKED_ID, dst, src1, src2, IMM_VALUE);                   \
  }

  #define V_EMIT_VVVV_VVV(NAME, PACKED_ID)                                    \
  template<typename DstT, typename Src1T, typename Src2T, typename Src3T>     \
  inline void NAME(const DstT& dst,                                           \
                   const Src1T& src1,                                         \
                   const Src2T& src2,                                         \
                   const Src3T& src3) noexcept {                              \
    v_emit_vvvv_vvv(PACKED_ID, dst, src1, src2, src3);                        \
  }

  #define PACK_AVX_SSE(AVX_ID, SSE_ID, W) \
    PackedInst::packAvxSse(x86::Inst::kId##AVX_ID, x86::Inst::kId##SSE_ID, PackedInst::kWidth##W)

  //! \}

  //! \name Emit - 'I' General Purpose Instructions
  //! \{

  BL_NOINLINE void i_load_u8(const x86::Gp& dst, const x86::Mem& src) noexcept {
    x86::Mem src8(src);
    src8.setSize(1);
    cc->movzx(dst.r32(), src8);
  }

  BL_NOINLINE void i_load_u16(const x86::Gp& dst, const x86::Mem& src) noexcept {
    x86::Mem src16(src);
    src16.setSize(2);
    cc->movzx(dst.r32(), src16);
  }

  BL_NOINLINE void i_load_u32(const x86::Gp& dst, const x86::Mem& src) noexcept {
    x86::Mem src32(src);
    src32.setSize(4);
    cc->mov(dst.r32(), src32);
  }

  BL_NOINLINE void i_load_u64(const x86::Gp& dst, const x86::Mem& src) noexcept {
    x86::Mem src64(src);
    src64.setSize(8);
    cc->mov(dst.r64(), src64);
  }

  BL_NOINLINE void i_store_u8(const x86::Mem& dst, const x86::Gp& src) noexcept {
    x86::Mem dst8 = dst;
    dst8.setSize(1);
    cc->mov(dst8, src.r8());
  }

  BL_NOINLINE void i_store_u16(const x86::Mem& dst, const x86::Gp& src) noexcept {
    x86::Mem dst16 = dst;
    dst16.setSize(2);
    cc->mov(dst16, src.r16());
  }

  BL_NOINLINE void i_store_u32(const x86::Mem& dst, const x86::Gp& src) noexcept {
    x86::Mem dst32 = dst;
    dst32.setSize(4);
    cc->mov(dst32, src.r32());
  }

  BL_NOINLINE void i_store_u64(const x86::Mem& dst, const x86::Gp& src) noexcept {
    x86::Mem dst64 = dst;
    dst64.setSize(8);
    cc->mov(dst64, src.r64());
  }

  BL_INLINE void i_mov(const x86::Gp& dst, const x86::Gp& src) noexcept {
    cc->mov(dst, src);
  }

  BL_INLINE void i_mov(const x86::Gp& dst, const x86::Mem& src) noexcept {
    cc->mov(dst, src);
  }

  //! dst = src1 + src2.
  BL_NOINLINE void i_add(const x86::Gp& dst, const x86::Gp& src1, const x86::Gp& src2) noexcept {
    BL_ASSERT(dst.size() == src1.size());
    BL_ASSERT(dst.size() == src2.size());

    if (dst.id() == src1.id()) {
      cc->add(dst, src2);
    }
    else if (dst.id() == src2.id()) {
      cc->add(dst, src1);
    }
    else if (dst.size() >= 4) {
      cc->lea(dst, x86::ptr(src1, src2));
    }
    else {
      cc->mov(dst, src1);
      cc->add(dst, src2);
    }
  }

  //! dst = src1 + [src2].
  BL_NOINLINE void i_add(const x86::Gp& dst, const x86::Gp& src1, const x86::Mem& src2) noexcept {
    if (dst.id() != src1.id())
      cc->mov(dst, src1);
    cc->add(dst, src2);
  }

  //! dst = src1 + src2.
  BL_NOINLINE void i_add(const x86::Gp& dst, const x86::Gp& src1, const Imm& src2) noexcept {
    if (dst.id() != src1.id() && src2.isInt32()) {
      cc->lea(dst, x86::ptr(src1, src2.valueAs<int32_t>()));
    }
    else {
      if (dst.id() != src1.id())
        cc->mov(dst, src1);
      cc->add(dst, src2);
    }
  }

  BL_NOINLINE void i_adds_u8(const x86::Gp& dst, const x86::Gp& src1, const x86::Gp& src2) noexcept {
    BL_ASSERT(dst.size() == src1.size());
    BL_ASSERT(dst.size() == src2.size());

    if (dst.id() == src1.id()) {
      cc->add(dst.r8(), src2.r8());
    }
    else if (dst.id() == src2.id()) {
      cc->add(dst.r8(), src1.r8());
    }
    else {
      cc->mov(dst, src1);
      cc->add(dst, src2);
    }

    x86::Gp u8_msk = cc->newUInt32("@u8_msk");
    cc->sbb(u8_msk, u8_msk);
    cc->or_(dst.r8(), u8_msk.r8());
  }

  //! dst = src1 - src2.
  BL_NOINLINE void i_sub(const x86::Gp& dst, const x86::Gp& src1, const x86::Gp& src2) noexcept {
    BL_ASSERT(dst.size() == src1.size());
    BL_ASSERT(dst.size() == src2.size());

    if (src1.id() == src2.id()) {
      cc->xor_(dst, dst);
    }
    else if (dst.id() == src1.id()) {
      cc->sub(dst, src2);
    }
    else if (dst.id() == src2.id()) {
      cc->neg(dst);
      cc->add(dst, src1);
    }
    else {
      cc->mov(dst, src1);
      cc->sub(dst, src2);
    }
  }

  //! dst = src1 - [src2].
  BL_NOINLINE void i_sub(const x86::Gp& dst, const x86::Gp& src1, const x86::Mem& src2) noexcept {
    if (dst.id() != src1.id())
      cc->mov(dst, src1);
    cc->sub(dst, src2);
  }

  //! dst = src1 + src2.
  BL_NOINLINE void i_sub(const x86::Gp& dst, const x86::Gp& src1, const Imm& src2) noexcept {
    if (dst.id() != src1.id())
      cc->mov(dst, src1);
    cc->sub(dst, src2);
  }

  //! dst = src1 * src2.
  BL_NOINLINE void i_mul(const x86::Gp& dst, const x86::Gp& src1, const x86::Gp& src2) noexcept {
    BL_ASSERT(dst.size() == src1.size());
    BL_ASSERT(dst.size() == src2.size());

    if (dst.id() == src1.id()) {
      cc->imul(dst, src2);
    }
    else if (dst.id() == src2.id()) {
      cc->imul(dst, src1);
    }
    else {
      cc->mov(dst, src1);
      cc->imul(dst, src2);
    }
  }

  //! dst = src1 * [src2].
  BL_NOINLINE void i_mul(const x86::Gp& dst, const x86::Gp& src1, const x86::Mem& src2) noexcept {
    BL_ASSERT(dst.size() == src1.size());
    BL_ASSERT(dst.size() == src2.size());

    if (dst.id() != src1.id())
      cc->mov(dst, src1);
    cc->imul(dst, src2);
  }

  //! dst = src1 * src2.
  BL_NOINLINE void i_mul(const x86::Gp& dst, const x86::Gp& src1, int src2) noexcept {
    BL_ASSERT(dst.size() == src1.size());

    if (src2 > 0) {
      switch (src2) {
        case 1:
          if (dst.id() != src1.id())
            cc->mov(dst, src1);
          return;

        case 2:
          if (dst.id() == src1.id())
            cc->shl(dst, 1);
          else
            cc->lea(dst, x86::ptr(src1, src1, 0));
          return;

        case 3:
          cc->lea(dst, x86::ptr(src1, src1, 1));
          return;

        case 4:
        case 8: {
          int shift = 2 + (src2 == 8);
          if (dst.id() == src1.id())
            cc->shl(dst, shift);
          else
            break;
          return;
        }
      }
    }

    if (dst.id() == src1.id())
      cc->imul(dst, src2);
    else
      cc->imul(dst, src1, src2);
  }

  BL_NOINLINE void i_umin(const x86::Gp& dst, const x86::Gp& src1, const x86::Gp& src2) noexcept {
    cc->cmp(src1, src2);
    if (dst.id() == src1.id()) {
      cc->cmova(dst, src2);
    }
    else if (dst.id() == src2.id()) {
      cc->cmovb(dst, src1);
    }
    else {
      cc->mov(dst, src1);
      cc->cmova(dst, src2);
    }
  }

  BL_NOINLINE void i_umax(const x86::Gp& dst, const x86::Gp& src1, const x86::Gp& src2) noexcept {
    cc->cmp(src1, src2);
    if (dst.id() == src1.id()) {
      cc->cmovb(dst, src2);
    }
    else if (dst.id() == src2.id()) {
      cc->cmova(dst, src1);
    }
    else {
      cc->mov(dst, src1);
      cc->cmovb(dst, src2);
    }
  }

  BL_NOINLINE void i_inv_u8(const x86::Gp& dst, const x86::Gp& src) noexcept {
    if (dst.id() != src.id())
      cc->mov(dst, src);
    cc->xor_(dst.r8(), 0xFF);
  }

  //! Integer division by 255 with correct rounding semantics.
  BL_NOINLINE void i_div_255_u32(const x86::Gp& dst, const x86::Gp& src) noexcept {
    BL_ASSERT(dst.size() == src.size());

    if (dst.id() == src.id()) {
      // tmp = src + 128;
      // dst = (tmp + (tmp >> 8)) >> 8
      x86::Gp tmp = cc->newSimilarReg(dst, "@tmp");
      cc->sub(dst, -128);
      cc->mov(tmp, dst);
      cc->shr(tmp, 8);
      cc->add(dst, tmp);
      cc->shr(dst, 8);
    }
    else {
      // dst = (src + 128 + ((src + 128) >> 8)) >> 8
      cc->lea(dst, x86::ptr(src, 128));
      cc->shr(dst, 8);
      cc->lea(dst, x86::ptr(dst, src, 0, 128));
      cc->shr(dst, 8);
    }
  }

  BL_NOINLINE void i_mul_257_hu16(const x86::Gp& dst, const x86::Gp& src) {
    BL_ASSERT(dst.size() == src.size());
    cc->imul(dst, src, 257);
    cc->shr(dst, 16);
  }

  template<typename A, typename B>
  BL_NOINLINE void i_zero_if_equal(const A& a, const B& b) noexcept {
    Label L = cc->newLabel();

    cc->cmp(a, b);
    cc->jne(L);
    cc->mov(a, 0);
    cc->bind(L);
  }

  BL_NOINLINE void i_jump_if_not_opaque_mask(const x86::Gp& msk, const Label& target) noexcept {
    cc->cmp(msk.r8(), 255);
    cc->jnz(target);
  }

  // dst = abs(src)
  template<typename DstT, typename SrcT>
  BL_NOINLINE void i_abs(const DstT& dst, const SrcT& src) noexcept {
    if (dst.id() == src.id()) {
      x86::Gp tmp = cc->newSimilarReg(dst, "@tmp");

      cc->mov(tmp, dst);
      cc->neg(dst);
      cc->cmovs(dst, tmp);
    }
    else {
      cc->mov(dst, src);
      cc->neg(dst);
      cc->cmovs(dst, src);
    }
  }

  template<typename DstT, typename ValueT, typename LimitT>
  BL_NOINLINE void i_ubound(const DstT& dst, const ValueT& value, const LimitT& limit) noexcept {
    if (dst.id() == value.id()) {
      x86::Gp zero = cc->newSimilarReg(dst, "@zero");

      cc->xor_(zero, zero);
      cc->cmp(dst, limit);
      cc->cmova(dst, zero);
      cc->cmovg(dst, limit);
    }
    else {
      cc->xor_(dst, dst);
      cc->cmp(value, limit);
      cc->cmovbe(dst, value);
      cc->cmovg(dst, limit);
    }
  }

  template<typename DstT, typename SrcT>
  BL_NOINLINE void i_ureflect(const DstT& dst, const SrcT& src) noexcept {
    BL_ASSERT(dst.size() == src.size());
    int nBits = int(dst.size()) * 8 - 1;

    if (dst.id() == src.id()) {
      DstT copy = cc->newSimilarReg(dst, "@copy");
      cc->mov(copy, dst);
      cc->sar(copy, nBits);
      cc->xor_(dst, copy);
    }
    else {
      cc->mov(dst, src);
      cc->sar(dst, nBits);
      cc->xor_(dst, src);
    }
  }

  template<typename DstT, typename SrcT>
  BL_NOINLINE void i_umod(const DstT& dst, const SrcT& src) noexcept {
    x86::Gp mod = cc->newSimilarReg(dst, "@mod");

    cc->xor_(mod, mod);
    cc->div(mod, dst, src);
    cc->mov(dst, mod);
  }

  BL_NOINLINE void i_advance_and_decrement(const x86::Gp& p, int pAdd, const x86::Gp& i, int iDec) noexcept {
    cc->add(p, pAdd);
    cc->sub(i, iDec);
  }

  //! dst += a * b.
  BL_NOINLINE void i_add_scaled(const x86::Gp& dst, const x86::Gp& a, int b) noexcept {
    switch (b) {
      case 1:
        cc->add(dst, a);
        return;

      case 2:
      case 4:
      case 8: {
        uint32_t shift = b == 2 ? 1 :
                         b == 4 ? 2 : 3;
        cc->lea(dst, x86::ptr(dst, a, shift));
        return;
      }

      default: {
        x86::Gp tmp = cc->newSimilarReg(dst, "tmp");
        cc->imul(tmp, a, b);
        cc->add(dst, tmp);
        return;
      }
    }
  }

  BL_NOINLINE void i_lea_bpp(const x86::Gp& dst, const x86::Gp& src_, const x86::Gp& idx_, uint32_t scale, int32_t disp = 0) noexcept {
    x86::Gp src = src_.cloneAs(dst);
    x86::Gp idx = idx_.cloneAs(dst);

    switch (scale) {
      case 1:
        if (dst.id() == src.id() && disp == 0)
          cc->add(dst, idx);
        else
          cc->lea(dst, x86::ptr(src, idx, 0, disp));
        break;

      case 2:
        cc->lea(dst, x86::ptr(src, idx, 1, disp));
        break;

      case 3:
        cc->lea(dst, x86::ptr(src, idx, 1, disp));
        cc->add(dst, idx);
        break;

      case 4:
        cc->lea(dst, x86::ptr(src, idx, 2, disp));
        break;

      default:
        BL_NOT_REACHED();
    }
  }

  BL_NOINLINE void i_shl(const x86::Gp& dst, const x86::Gp& src1, const x86::Gp& src2) noexcept {
    if (hasBMI2()) {
      cc->shlx(dst, src1, src2.cloneAs(dst));
    }
    else {
      if (dst.id() != src1.id())
        cc->mov(dst, src1);
      cc->shl(dst, src2.r8());
    }
  }

  BL_NOINLINE void i_shl(const x86::Gp& dst, const x86::Gp& src1, const Imm& src2) noexcept {
    if (dst.id() != src1.id())
      cc->mov(dst, src1);
    cc->shl(dst, src2);
  }

  BL_NOINLINE void i_shr(const x86::Gp& dst, const x86::Gp& src1, const x86::Gp& src2) noexcept {
    if (hasBMI2()) {
      cc->shrx(dst, src1, src2.cloneAs(dst));
    }
    else {
      if (dst.id() != src1.id())
        cc->mov(dst, src1);
      cc->shr(dst, src2.r8());
    }
  }

  BL_NOINLINE void i_shr(const x86::Gp& dst, const x86::Gp& src1, const Imm& src2) noexcept {
    if (dst.id() != src1.id())
      cc->mov(dst, src1);
    cc->shr(dst, src2);
  }

  inline void i_ctz(const Operand& dst, const Operand& src) noexcept {
    // INTEL - No difference, `bsf` and `tzcnt` both have latency ~2.5 cycles.
    // AMD   - Big difference, `tzcnt` has only ~1.5 cycle latency while `bsf` has ~2.5 cycles.
    cc->emit(hasBMI() ? x86::Inst::kIdTzcnt : x86::Inst::kIdBsf, dst, src);
  }

  inline void i_test(const x86::Gp& ptr, uint32_t mask) noexcept {
    if (mask <= 0xFF && cc->is64Bit()) {
      // Shorter, but limits as to use AL|BL|CL|DL, so we don't wanna use this construct in 32-bit mode.
      cc->test(ptr.r8(), mask);
    }
    else {
      cc->test(ptr, mask);
    }
  }

  // NOTE: AMD has no performance difference between 'test' and 'bt' instructions, however, Intel can execute less
  // 'bt' instructions per cycle than 'test's, so we prefer 'test' if bitIndex is low. Additionally, we only use
  // test on 64-bit hardware as it's guaranteed that any register index is encodable. On 32-bit hardware only the
  // first 4 registers can be used, which could mean that the register would have to be moved just to be tested,
  // which is something we would like to avoid.
  BL_NOINLINE void i_jmp_if_bit_set(const x86::Gp& val, uint32_t bitIndex, const Label& dst) noexcept {
    if (bitIndex < 8 && cc->is64Bit()) {
      cc->test(val.r8(), 1u << bitIndex);
      cc->jnz(dst);
    }
    else {
      cc->bt(val, bitIndex);
      cc->jc(dst);
    }
  }

  BL_NOINLINE void i_jmp_if_bit_zero(const x86::Gp& val, uint32_t bitIndex, const Label& dst) noexcept {
    if (bitIndex < 8 && cc->is64Bit()) {
      cc->test(val.r8(), 1u << bitIndex);
      cc->jz(dst);
    }
    else {
      cc->bt(val, bitIndex);
      cc->jnc(dst);
    }
  }

  inline void i_prefetch(const x86::Mem& mem) noexcept {
    cc->prefetcht0(mem);
  }

  //! \}

  //! \name Emit - 'V' Vector Instructions (128..512-bit SSE|AVX|AVX512)
  //! \{

  // To make the code generation easier and more parametrizable we support both SSE|AVX through the same interface
  // (always non-destructive source form) and each intrinsic can accept either `Operand_` or `OpArray`, which can
  // hold up to 4 registers to form scalars, pairs and quads. Each 'V' instruction maps directly to the ISA so check
  // the optimization level before using them or use instructions starting with 'x' that are generic and designed to
  // map to the best instruction(s) possible.
  //
  // Also, multiple overloads are provided for convenience, similarly to AsmJit design, we don't want to inline
  // expansion of `OpArray(op)` here so these overloads are implemented in pipecompiler.cpp.

  // SSE instructions that require SSE3+ are suffixed with `_` to make it clear that they are not part of the
  // baseline instruction set. Some instructions like that are always provided don't have such suffix, and will be
  // emulated.

  // Integer SIMD - Core.

  V_EMIT_VV_VV(v_mov               , PACK_AVX_SSE(Vmovaps    , Movaps    , Z))       // AVX  | SSE2
  V_EMIT_VV_VV(v_mov_i64           , PACK_AVX_SSE(Vmovq      , Movq      , X))       // AVX  | SSE2

  V_EMIT_VV_VV(v_mov_i8_i16_       , PACK_AVX_SSE(Vpmovsxbw  , Pmovsxbw  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(v_mov_u8_u16_       , PACK_AVX_SSE(Vpmovzxbw  , Pmovzxbw  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(v_mov_i8_i32_       , PACK_AVX_SSE(Vpmovsxbd  , Pmovsxbd  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(v_mov_u8_u32_       , PACK_AVX_SSE(Vpmovzxbd  , Pmovzxbd  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(v_mov_i8_i64_       , PACK_AVX_SSE(Vpmovsxbq  , Pmovsxbq  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(v_mov_u8_u64_       , PACK_AVX_SSE(Vpmovzxbq  , Pmovzxbq  , Z))       // AVX2 | SSE4.1

  V_EMIT_VV_VV(v_mov_i16_i32_      , PACK_AVX_SSE(Vpmovsxwd  , Pmovsxwd  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(v_mov_u16_u32_      , PACK_AVX_SSE(Vpmovzxwd  , Pmovzxwd  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(v_mov_i16_i64_      , PACK_AVX_SSE(Vpmovsxwq  , Pmovsxwq  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(v_mov_u16_u64_      , PACK_AVX_SSE(Vpmovzxwq  , Pmovzxwq  , Z))       // AVX2 | SSE4.1

  V_EMIT_VV_VV(v_mov_i32_i64_      , PACK_AVX_SSE(Vpmovsxdq  , Pmovsxdq  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(v_mov_u32_u64_      , PACK_AVX_SSE(Vpmovzxdq  , Pmovzxdq  , Z))       // AVX2 | SSE4.1

  V_EMIT_VV_VV(v_mov_mask_u8       , PACK_AVX_SSE(Vpmovmskb  , Pmovmskb  , Z))       // AVX2 | SSE2

  V_EMIT_VVVI_VVI(v_insert_u8_     , PACK_AVX_SSE(Vpinsrb    , Pinsrb    , X))       // AVX2 | SSE4_1
  V_EMIT_VVVI_VVI(v_insert_u16     , PACK_AVX_SSE(Vpinsrw    , Pinsrw    , X))       // AVX2 | SSE2
  V_EMIT_VVVI_VVI(v_insert_u32_    , PACK_AVX_SSE(Vpinsrd    , Pinsrd    , X))       // AVX2 | SSE4_1
  V_EMIT_VVVI_VVI(v_insert_u64_    , PACK_AVX_SSE(Vpinsrq    , Pinsrq    , X))       // AVX2 | SSE4_1

  V_EMIT_VVI_VVI(v_extract_u8_     , PACK_AVX_SSE(Vpextrb    , Pextrb    , X))       // AVX2 | SSE4_1
  V_EMIT_VVI_VVI(v_extract_u16     , PACK_AVX_SSE(Vpextrw    , Pextrw    , X))       // AVX2 | SSE2
  V_EMIT_VVI_VVI(v_extract_u32_    , PACK_AVX_SSE(Vpextrd    , Pextrd    , X))       // AVX2 | SSE4_1
  V_EMIT_VVI_VVI(v_extract_u64_    , PACK_AVX_SSE(Vpextrq    , Pextrq    , X))       // AVX2 | SSE4_1

  V_EMIT_VVV_VV(v_and_i32          , PackedInst::packIntrin(kIntrin3Vandi32))        // AVX2 | SSE2
  V_EMIT_VVV_VV(v_and_i64          , PackedInst::packIntrin(kIntrin3Vandi64))        // AVX2 | SSE2
  V_EMIT_VVV_VV(v_and_f32          , PACK_AVX_SSE(Vandps     , Andps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_and_f64          , PACK_AVX_SSE(Vandpd     , Andpd     , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(v_nand_i32         , PackedInst::packIntrin(kIntrin3Vnandi32))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_nand_i64         , PackedInst::packIntrin(kIntrin3Vnandi64))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_nand_f32         , PACK_AVX_SSE(Vandnps    , Andnps    , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_nand_f64         , PACK_AVX_SSE(Vandnpd    , Andnpd    , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(v_or_i32           , PackedInst::packIntrin(kIntrin3Vori32))         // AVX2 | SSE2
  V_EMIT_VVV_VV(v_or_i64           , PackedInst::packIntrin(kIntrin3Vori64))         // AVX2 | SSE2
  V_EMIT_VVV_VV(v_or_f32           , PACK_AVX_SSE(Vorps      , Orps      , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_or_f64           , PACK_AVX_SSE(Vorpd      , Orpd      , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(v_xor_i32          , PackedInst::packIntrin(kIntrin3Vxori32))        // AVX2 | SSE2
  V_EMIT_VVV_VV(v_xor_i64          , PackedInst::packIntrin(kIntrin3Vxori64))        // AVX2 | SSE2
  V_EMIT_VVV_VV(v_xor_f32          , PACK_AVX_SSE(Vxorps     , Xorps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_xor_f64          , PACK_AVX_SSE(Vxorpd     , Xorpd     , Z))       // AVX  | SSE2

  V_EMIT_VVI_VI(v_sll_i16          , PACK_AVX_SSE(Vpsllw     , Psllw     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(v_sll_i32          , PACK_AVX_SSE(Vpslld     , Pslld     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(v_sll_i64          , PACK_AVX_SSE(Vpsllq     , Psllq     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(v_srl_i16          , PACK_AVX_SSE(Vpsrlw     , Psrlw     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(v_srl_i32          , PACK_AVX_SSE(Vpsrld     , Psrld     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(v_srl_i64          , PACK_AVX_SSE(Vpsrlq     , Psrlq     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(v_sra_i16          , PACK_AVX_SSE(Vpsraw     , Psraw     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(v_sra_i32          , PACK_AVX_SSE(Vpsrad     , Psrad     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(v_sllb_i128        , PACK_AVX_SSE(Vpslldq    , Pslldq    , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(v_srlb_i128        , PACK_AVX_SSE(Vpsrldq    , Psrldq    , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(v_shuffle_i8       , PACK_AVX_SSE(Vpshufb    , Pshufb    , Z))       // AVX2 | SSSE3
  V_EMIT_VVVI_VVI(v_shuffle_f32    , PACK_AVX_SSE(Vshufps    , Shufps    , Z))       // AVX  | SSE
  V_EMIT_VVVI_VVI(v_shuffle_f64    , PACK_AVX_SSE(Vshufpd    , Shufpd    , Z))       // AVX  | SSE2

  V_EMIT_VVI_VVI(v_swizzle_lo_i16  , PACK_AVX_SSE(Vpshuflw   , Pshuflw   , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VVI(v_swizzle_hi_i16  , PACK_AVX_SSE(Vpshufhw   , Pshufhw   , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VVI(v_swizzle_i32     , PACK_AVX_SSE(Vpshufd    , Pshufd    , Z))       // AVX2 | SSE2

  V_EMIT_VVVI_VVI(v_shuffle_i32    , PACK_AVX_SSE(Vshufps    , Shufps    , Z))       // AVX  | SSE
  V_EMIT_VVVI_VVI(v_shuffle_i64    , PACK_AVX_SSE(Vshufpd    , Shufpd    , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(v_interleave_lo_i8 , PACK_AVX_SSE(Vpunpcklbw , Punpcklbw , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_interleave_hi_i8 , PACK_AVX_SSE(Vpunpckhbw , Punpckhbw , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_interleave_lo_i16, PACK_AVX_SSE(Vpunpcklwd , Punpcklwd , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_interleave_hi_i16, PACK_AVX_SSE(Vpunpckhwd , Punpckhwd , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_interleave_lo_i32, PACK_AVX_SSE(Vpunpckldq , Punpckldq , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_interleave_hi_i32, PACK_AVX_SSE(Vpunpckhdq , Punpckhdq , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_interleave_lo_i64, PACK_AVX_SSE(Vpunpcklqdq, Punpcklqdq, Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_interleave_hi_i64, PACK_AVX_SSE(Vpunpckhqdq, Punpckhqdq, Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_interleave_lo_f32, PACK_AVX_SSE(Vunpcklps  , Unpcklps  , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_interleave_lo_f64, PACK_AVX_SSE(Vunpcklpd  , Unpcklpd  , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(v_interleave_hi_f32, PACK_AVX_SSE(Vunpckhps  , Unpckhps  , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_interleave_hi_f64, PACK_AVX_SSE(Vunpckhpd  , Unpckhpd  , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(v_packs_i32_i16    , PACK_AVX_SSE(Vpackssdw  , Packssdw  , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_packs_i32_u16_   , PACK_AVX_SSE(Vpackusdw  , Packusdw  , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(v_packs_i16_i8     , PACK_AVX_SSE(Vpacksswb  , Packsswb  , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_packs_i16_u8     , PACK_AVX_SSE(Vpackuswb  , Packuswb  , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(v_avg_u8           , PACK_AVX_SSE(Vpavgb     , Pavgb     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_avg_u16          , PACK_AVX_SSE(Vpavgw     , Pavgw     , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(v_sign_i8_         , PACK_AVX_SSE(Vpsignb    , Psignb    , Z))       // AVX2 | SSSE3
  V_EMIT_VVV_VV(v_sign_i16_        , PACK_AVX_SSE(Vpsignw    , Psignw    , Z))       // AVX2 | SSSE3
  V_EMIT_VVV_VV(v_sign_i32_        , PACK_AVX_SSE(Vpsignd    , Psignd    , Z))       // AVX2 | SSSE3

  V_EMIT_VVV_VV(v_add_i8           , PACK_AVX_SSE(Vpaddb     , Paddb     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_add_i16          , PACK_AVX_SSE(Vpaddw     , Paddw     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_add_i32          , PACK_AVX_SSE(Vpaddd     , Paddd     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_add_i64          , PACK_AVX_SSE(Vpaddq     , Paddq     , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(v_sub_i8           , PACK_AVX_SSE(Vpsubb     , Psubb     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_sub_i16          , PACK_AVX_SSE(Vpsubw     , Psubw     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_sub_i32          , PACK_AVX_SSE(Vpsubd     , Psubd     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_sub_i64          , PACK_AVX_SSE(Vpsubq     , Psubq     , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(v_adds_i8          , PACK_AVX_SSE(Vpaddsb    , Paddsb    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_adds_u8          , PACK_AVX_SSE(Vpaddusb   , Paddusb   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_adds_i16         , PACK_AVX_SSE(Vpaddsw    , Paddsw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_adds_u16         , PACK_AVX_SSE(Vpaddusw   , Paddusw   , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(v_subs_i8          , PACK_AVX_SSE(Vpsubsb    , Psubsb    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_subs_i16         , PACK_AVX_SSE(Vpsubsw    , Psubsw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_subs_u8          , PACK_AVX_SSE(Vpsubusb   , Psubusb   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_subs_u16         , PACK_AVX_SSE(Vpsubusw   , Psubusw   , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(v_mul_i16          , PACK_AVX_SSE(Vpmullw    , Pmullw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_mul_u16          , PACK_AVX_SSE(Vpmullw    , Pmullw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_mulh_i16         , PACK_AVX_SSE(Vpmulhw    , Pmulhw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_mulh_u16         , PACK_AVX_SSE(Vpmulhuw   , Pmulhuw   , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(v_mul_i32_         , PACK_AVX_SSE(Vpmulld    , Pmulld    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(v_mul_u32_         , PACK_AVX_SSE(Vpmulld    , Pmulld    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(v_mulx_ll_i32_     , PACK_AVX_SSE(Vpmuldq    , Pmuldq    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(v_mulx_ll_u32_     , PACK_AVX_SSE(Vpmuludq   , Pmuludq   , Z))       // AVX2 | SSE2

  V_EMIT_VVVi_VVi(v_mulx_ll_u64_   , PACK_AVX_SSE(Vpclmulqdq , Pclmulqdq , Z), 0x00) // AVX2 | PCLMULQDQ
  V_EMIT_VVVi_VVi(v_mulx_hl_u64_   , PACK_AVX_SSE(Vpclmulqdq , Pclmulqdq , Z), 0x01) // AVX2 | PCLMULQDQ
  V_EMIT_VVVi_VVi(v_mulx_lh_u64_   , PACK_AVX_SSE(Vpclmulqdq , Pclmulqdq , Z), 0x10) // AVX2 | PCLMULQDQ
  V_EMIT_VVVi_VVi(v_mulx_hh_u64_   , PACK_AVX_SSE(Vpclmulqdq , Pclmulqdq , Z), 0x11) // AVX2 | PCLMULQDQ

  V_EMIT_VVV_VV(v_min_i8_          , PACK_AVX_SSE(Vpminsb    , Pminsb    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(v_min_u8           , PACK_AVX_SSE(Vpminub    , Pminub    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_min_i16          , PACK_AVX_SSE(Vpminsw    , Pminsw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_min_i32_         , PACK_AVX_SSE(Vpminsd    , Pminsd    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(v_min_u32_         , PACK_AVX_SSE(Vpminud    , Pminud    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(v_max_i8_          , PACK_AVX_SSE(Vpmaxsb    , Pmaxsb    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(v_max_u8           , PACK_AVX_SSE(Vpmaxub    , Pmaxub    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_max_i16          , PACK_AVX_SSE(Vpmaxsw    , Pmaxsw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_max_i32_         , PACK_AVX_SSE(Vpmaxsd    , Pmaxsd    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(v_max_u32_         , PACK_AVX_SSE(Vpmaxud    , Pmaxud    , Z))       // AVX2 | SSE4.1

  V_EMIT_VVV_VV(v_cmp_eq_i8        , PACK_AVX_SSE(Vpcmpeqb   , Pcmpeqb   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_cmp_eq_i16       , PACK_AVX_SSE(Vpcmpeqw   , Pcmpeqw   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_cmp_eq_i32       , PACK_AVX_SSE(Vpcmpeqd   , Pcmpeqd   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_cmp_eq_i64_      , PACK_AVX_SSE(Vpcmpeqq   , Pcmpeqq   , Z))       // AVX2 | SSE4.1

  V_EMIT_VVV_VV(v_cmp_gt_i8        , PACK_AVX_SSE(Vpcmpgtb   , Pcmpgtb   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_cmp_gt_i16       , PACK_AVX_SSE(Vpcmpgtw   , Pcmpgtw   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_cmp_gt_i32       , PACK_AVX_SSE(Vpcmpgtd   , Pcmpgtd   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(v_cmp_gt_i64_      , PACK_AVX_SSE(Vpcmpgtq   , Pcmpgtq   , Z))       // AVX2 | SSE4.2

  V_EMIT_VVVV_VVV(v_blendv_u8_     , PACK_AVX_SSE(Vpblendvb  , Pblendvb  , Z))       // AVX2 | SSE4.1
  V_EMIT_VVVI_VVI(v_blend_u16_     , PACK_AVX_SSE(Vpblendw   , Pblendw   , Z))       // AVX2 | SSE4.1

  V_EMIT_VVV_VV(v_hadd_i16_        , PACK_AVX_SSE(Vphaddw    , Phaddw    , Z))       // AVX2 | SSSE3
  V_EMIT_VVV_VV(v_hadd_i32_        , PACK_AVX_SSE(Vphaddd    , Phaddd    , Z))       // AVX2 | SSSE3

  V_EMIT_VVV_VV(v_hsub_i16_        , PACK_AVX_SSE(Vphsubw    , Phsubw    , Z))       // AVX2 | SSSE3
  V_EMIT_VVV_VV(v_hsub_i32_        , PACK_AVX_SSE(Vphsubd    , Phsubd    , Z))       // AVX2 | SSSE3

  V_EMIT_VVV_VV(v_hadds_i16_       , PACK_AVX_SSE(Vphaddsw   , Phaddsw   , Z))       // AVX2 | SSSE3
  V_EMIT_VVV_VV(v_hsubs_i16_       , PACK_AVX_SSE(Vphsubsw   , Phsubsw   , Z))       // AVX2 | SSSE3

  // Integer SIMD - Miscellaneous.

  V_EMIT_VV_VV(v_test_             , PACK_AVX_SSE(Vptest     , Ptest     , Z))       // AVX2 | SSE4_1

  // Integer SIMD - Consult X86 manual before using these...

  V_EMIT_VVV_VV(v_sad_u8           , PACK_AVX_SSE(Vpsadbw    , Psadbw    , Z))       // AVX2 | SSE2      [dst.u64[0..X] = SUM{0.7}(ABS(src1.u8[N] - src2.u8[N]))))]
  V_EMIT_VVV_VV(v_mulrh_i16_       , PACK_AVX_SSE(Vpmulhrsw  , Pmulhrsw  , Z))       // AVX2 | SSSE3     [dst.i16[0..X] = ((((src1.i16[0] * src2.i16[0])) >> 14)) + 1)) >> 1))]
  V_EMIT_VVV_VV(v_madds_u8_i8_     , PACK_AVX_SSE(Vpmaddubsw , Pmaddubsw , Z))       // AVX2 | SSSE3     [dst.i16[0..X] = SAT(src1.u8[0] * src2.i8[0] + src1.u8[1] * src2.i8[1]))
  V_EMIT_VVV_VV(v_madd_i16_i32     , PACK_AVX_SSE(Vpmaddwd   , Pmaddwd   , Z))       // AVX2 | SSE2      [dst.i32[0..X] = (src1.i16[0] * src2.i16[0] + src1.i16[1] * src2.i16[1]))
  V_EMIT_VVVI_VVI(v_mpsad_u8_      , PACK_AVX_SSE(Vmpsadbw   , Mpsadbw   , Z))       // AVX2 | SSE4.1
  V_EMIT_VVVI_VVI(v_alignr_u8_     , PACK_AVX_SSE(Vpalignr   , Palignr   , Z))       // AVX2 | SSSE3
  V_EMIT_VV_VV(v_hmin_pos_u16_     , PACK_AVX_SSE(Vphminposuw, Phminposuw, Z))       // AVX2 | SSE4_1

  // Floating Point - Core.

  V_EMIT_VV_VV(vmovaps             , PACK_AVX_SSE(Vmovaps    , Movaps    , Z))       // AVX  | SSE
  V_EMIT_VV_VV(vmovapd             , PACK_AVX_SSE(Vmovapd    , Movapd    , Z))       // AVX  | SSE2
  V_EMIT_VV_VV(vmovups             , PACK_AVX_SSE(Vmovups    , Movups    , Z))       // AVX  | SSE
  V_EMIT_VV_VV(vmovupd             , PACK_AVX_SSE(Vmovupd    , Movupd    , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(vmovlps2x          , PACK_AVX_SSE(Vmovlps    , Movlps    , X))       // AVX  | SSE
  V_EMIT_VVV_VV(vmovhps2x          , PACK_AVX_SSE(Vmovhps    , Movhps    , X))       // AVX  | SSE

  V_EMIT_VVV_VV(vmovlhps2x         , PACK_AVX_SSE(Vmovlhps   , Movlhps   , X))       // AVX  | SSE
  V_EMIT_VVV_VV(vmovhlps2x         , PACK_AVX_SSE(Vmovhlps   , Movhlps   , X))       // AVX  | SSE

  V_EMIT_VVV_VV(vmovlpd            , PACK_AVX_SSE(Vmovlpd    , Movlpd    , X))       // AVX  | SSE
  V_EMIT_VVV_VV(vmovhpd            , PACK_AVX_SSE(Vmovhpd    , Movhpd    , X))       // AVX  | SSE

  V_EMIT_VV_VV(vmovduplps_         , PACK_AVX_SSE(Vmovsldup  , Movsldup  , Z))       // AVX  | SSE3
  V_EMIT_VV_VV(vmovduphps_         , PACK_AVX_SSE(Vmovshdup  , Movshdup  , Z))       // AVX  | SSE3

  V_EMIT_VV_VV(vmov_dupl_2xf32_    , PACK_AVX_SSE(Vmovddup   , Movddup   , Z))       // AVX  | SSE3

  V_EMIT_VV_VV(v_move_mask_f32     , PACK_AVX_SSE(Vmovmskps  , Movmskps  , Z))       // AVX  | SSE
  V_EMIT_VV_VV(v_move_mask_f64     , PACK_AVX_SSE(Vmovmskpd  , Movmskpd  , Z))       // AVX  | SSE2

  V_EMIT_VVI_VVI(v_insert_f32_     , PACK_AVX_SSE(Vinsertps  , Insertps  , X))       // AVX  | SSE4_1
  V_EMIT_VVI_VVI(v_extract_f32_    , PACK_AVX_SSE(Vextractps , Extractps , X))       // AVX  | SSE4_1

  V_EMIT_VVV_VV(s_add_f32          , PACK_AVX_SSE(Vaddss     , Addss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_add_f64          , PACK_AVX_SSE(Vaddsd     , Addsd     , X))       // AVX  | SSE2
  V_EMIT_VVV_VV(s_sub_f32          , PACK_AVX_SSE(Vsubss     , Subss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_sub_f64          , PACK_AVX_SSE(Vsubsd     , Subsd     , X))       // AVX  | SSE2
  V_EMIT_VVV_VV(s_mul_f32          , PACK_AVX_SSE(Vmulss     , Mulss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_mul_f64          , PACK_AVX_SSE(Vmulsd     , Mulsd     , X))       // AVX  | SSE2
  V_EMIT_VVV_VV(s_div_f32          , PACK_AVX_SSE(Vdivss     , Divss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_div_f64          , PACK_AVX_SSE(Vdivsd     , Divsd     , X))       // AVX  | SSE2
  V_EMIT_VVV_VV(s_min_f32          , PACK_AVX_SSE(Vminss     , Minss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_min_f64          , PACK_AVX_SSE(Vminsd     , Minsd     , X))       // AVX  | SSE2
  V_EMIT_VVV_VV(s_max_f32          , PACK_AVX_SSE(Vmaxss     , Maxss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_max_f64          , PACK_AVX_SSE(Vmaxsd     , Maxsd     , X))       // AVX  | SSE2

  V_EMIT_VVV_VV(s_rcp_f32          , PACK_AVX_SSE(Vrcpss     , Rcpss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_rsqrt_f32        , PACK_AVX_SSE(Vrsqrtss   , Rsqrtss   , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_sqrt_f32         , PACK_AVX_SSE(Vsqrtss    , Sqrtss    , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_sqrt_f64         , PACK_AVX_SSE(Vsqrtsd    , Sqrtsd    , X))       // AVX  | SSE2

  V_EMIT_VVVI_VVI_ENUM(s_round_f32_, PACK_AVX_SSE(Vroundss   , Roundss   , X), x86::RoundImm) // AVX  | SSE4.1
  V_EMIT_VVVI_VVI_ENUM(s_round_f64_, PACK_AVX_SSE(Vroundsd   , Roundsd   , X), x86::RoundImm) // AVX  | SSE4.1
  V_EMIT_VVVI_VVI_ENUM(s_cmp_f32   , PACK_AVX_SSE(Vcmpss     , Cmpss     , X), x86::VCmpImm)  // AVX  | SSE
  V_EMIT_VVVI_VVI_ENUM(s_cmp_f64   , PACK_AVX_SSE(Vcmpsd     , Cmpsd     , X), x86::VCmpImm)  // AVX  | SSE2

  V_EMIT_VVV_VV(v_add_f32          , PACK_AVX_SSE(Vaddps     , Addps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_add_f64          , PACK_AVX_SSE(Vaddpd     , Addpd     , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(v_sub_f32          , PACK_AVX_SSE(Vsubps     , Subps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_sub_f64          , PACK_AVX_SSE(Vsubpd     , Subpd     , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(v_mul_f32          , PACK_AVX_SSE(Vmulps     , Mulps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_mul_f64          , PACK_AVX_SSE(Vmulpd     , Mulpd     , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(v_div_f32          , PACK_AVX_SSE(Vdivps     , Divps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_div_f64          , PACK_AVX_SSE(Vdivpd     , Divpd     , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(v_min_f32          , PACK_AVX_SSE(Vminps     , Minps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_min_f64          , PACK_AVX_SSE(Vminpd     , Minpd     , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(v_max_f32          , PACK_AVX_SSE(Vmaxps     , Maxps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(v_max_f64          , PACK_AVX_SSE(Vmaxpd     , Maxpd     , Z))       // AVX  | SSE2

  V_EMIT_VV_VV(v_rcp_f32           , PACK_AVX_SSE(Vrcpps     , Rcpps     , Z))       // AVX  | SSE
  V_EMIT_VV_VV(v_rsqrt_f32         , PACK_AVX_SSE(Vrsqrtps   , Rsqrtps   , Z))       // AVX  | SSE
  V_EMIT_VV_VV(v_sqrt_f32          , PACK_AVX_SSE(Vsqrtps    , Sqrtps    , Z))       // AVX  | SSE
  V_EMIT_VV_VV(v_sqrt_f64          , PACK_AVX_SSE(Vsqrtpd    , Sqrtpd    , Z))       // AVX  | SSE2

  V_EMIT_VVI_VVI_ENUM(v_round_f32_ , PACK_AVX_SSE(Vroundps   , Roundps   , Z), x86::RoundImm) // AVX  | SSE4.1
  V_EMIT_VVI_VVI_ENUM(v_round_f64_ , PACK_AVX_SSE(Vroundpd   , Roundpd   , Z), x86::RoundImm) // AVX  | SSE4.1
  V_EMIT_VVVI_VVI_ENUM(v_cmp_f32   , PACK_AVX_SSE(Vcmpps     , Cmpps     , Z), x86::VCmpImm)  // AVX  | SSE
  V_EMIT_VVVI_VVI_ENUM(v_cmp_f64   , PACK_AVX_SSE(Vcmppd     , Cmppd     , Z), x86::VCmpImm)  // AVX  | SSE2

  V_EMIT_VVV_VV(v_addsub_f32_      , PACK_AVX_SSE(Vaddsubps  , Addsubps  , Z))       // AVX  | SSE3
  V_EMIT_VVV_VV(v_addsub_f64_      , PACK_AVX_SSE(Vaddsubpd  , Addsubpd  , Z))       // AVX  | SSE3
  V_EMIT_VVVI_VVI(v_dot_f32_       , PACK_AVX_SSE(Vdpps      , Dpps      , Z))       // AVX  | SSE4.1
  V_EMIT_VVVI_VVI(v_dot_f64_       , PACK_AVX_SSE(Vdppd      , Dppd      , Z))       // AVX  | SSE4.1

  V_EMIT_VVVV_VVV(v_blendv_f32_    , PACK_AVX_SSE(Vblendvps  , Blendvps  , Z))       // AVX  | SSE4.1
  V_EMIT_VVVV_VVV(v_blendv_f64_    , PACK_AVX_SSE(Vblendvpd  , Blendvpd  , Z))       // AVX  | SSE4.1
  V_EMIT_VVVI_VVI(v_blend_f32_     , PACK_AVX_SSE(Vblendps   , Blendps   , Z))       // AVX  | SSE4.1
  V_EMIT_VVVI_VVI(v_blend_f64_     , PACK_AVX_SSE(Vblendpd   , Blendpd   , Z))       // AVX  | SSE4.1

  V_EMIT_VVV_VV(s_cvt_f32_f64      , PACK_AVX_SSE(Vcvtss2sd  , Cvtss2sd  , X))       // AVX  | SSE2
  V_EMIT_VVV_VV(s_cvt_f64_f32      , PACK_AVX_SSE(Vcvtsd2ss  , Cvtsd2ss  , X))       // AVX  | SSE2

  V_EMIT_VVV_VV(s_cvt_int_f32      , PACK_AVX_SSE(Vcvtsi2ss  , Cvtsi2ss  , X))       // AVX  | SSE
  V_EMIT_VVV_VV(s_cvt_int_f64      , PACK_AVX_SSE(Vcvtsi2sd  , Cvtsi2sd  , X))       // AVX  | SSE2

  V_EMIT_VV_VV(s_cvt_f32_int       , PACK_AVX_SSE(Vcvtss2si  , Cvtss2si  , X))       // AVX  | SSE
  V_EMIT_VV_VV(s_cvt_f64_int       , PACK_AVX_SSE(Vcvtsd2si  , Cvtsd2si  , X))       // AVX  | SSE2

  V_EMIT_VV_VV(s_cvtt_f32_int      , PACK_AVX_SSE(Vcvttss2si , Cvttss2si , X))       // AVX  | SSE
  V_EMIT_VV_VV(s_cvtt_f64_int      , PACK_AVX_SSE(Vcvttsd2si , Cvttsd2si , X))       // AVX  | SSE2

  V_EMIT_VV_VV(v_cvt_f32_f64       , PACK_AVX_SSE(Vcvtps2pd  , Cvtps2pd  , Z))       // AVX  | SSE2
  V_EMIT_VV_VV(v_cvt_f64_f32       , PACK_AVX_SSE(Vcvtpd2ps  , Cvtpd2ps  , Z))       // AVX  | SSE2

  V_EMIT_VV_VV(v_cvt_i32_f32       , PACK_AVX_SSE(Vcvtdq2ps  , Cvtdq2ps  , Z))       // AVX  | SSE2
  V_EMIT_VV_VV(v_cvt_i32_f64       , PACK_AVX_SSE(Vcvtdq2pd  , Cvtdq2pd  , Z))       // AVX  | SSE2

  V_EMIT_VV_VV(v_cvt_f32_i32       , PACK_AVX_SSE(Vcvtps2dq  , Cvtps2dq  , Z))       // AVX  | SSE2
  V_EMIT_VV_VV(v_cvt_f64_i32       , PACK_AVX_SSE(Vcvtpd2dq  , Cvtpd2dq  , Z))       // AVX  | SSE2

  V_EMIT_VV_VV(v_cvtt_f32_i32      , PACK_AVX_SSE(Vcvttps2dq , Cvttps2dq , Z))       // AVX  | SSE2
  V_EMIT_VV_VV(v_cvtt_f64_i32      , PACK_AVX_SSE(Vcvttpd2dq , Cvttpd2dq , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(v_hadd_f32_        , PACK_AVX_SSE(Vhaddps    , Haddps    , Z))       // AVX  | SSE3
  V_EMIT_VVV_VV(v_hadd_f64_        , PACK_AVX_SSE(Vhaddpd    , Haddpd    , Z))       // AVX  | SSE3
  V_EMIT_VVV_VV(v_hsub_f32_        , PACK_AVX_SSE(Vhsubps    , Hsubps    , Z))       // AVX  | SSE3
  V_EMIT_VVV_VV(v_hsub_f64_        , PACK_AVX_SSE(Vhsubpd    , Hsubpd    , Z))       // AVX  | SSE3

  // Floating Point - Miscellaneous.

  V_EMIT_VV_VV(s_comi_f32          , PACK_AVX_SSE(Vcomiss    , Comiss    , X))       // AVX  | SSE
  V_EMIT_VV_VV(s_comi_f64          , PACK_AVX_SSE(Vcomisd    , Comisd    , X))       // AVX  | SSE2
  V_EMIT_VV_VV(s_ucomi_f32         , PACK_AVX_SSE(Vucomiss   , Ucomiss   , X))       // AVX  | SSE
  V_EMIT_VV_VV(s_ucomi_f64         , PACK_AVX_SSE(Vucomisd   , Ucomisd   , X))       // AVX  | SSE2

  // AVX2 and AVX-512.

  V_EMIT_VVI_VVI(v_extract_i128    , PACK_AVX_SSE(Vextracti128 , None    , Y))       // AVX2
  V_EMIT_VVI_VVI(v_extract_i256    , PACK_AVX_SSE(Vextracti32x8, None    , Z))       // AVX2
  V_EMIT_VVI_VVI(v_perm_i64        , PACK_AVX_SSE(Vpermq       , None    , Z))       // AVX2 | AVX512

  V_EMIT_VVVI(v_insert_i256        , PACK_AVX_SSE(Vinserti32x8 , None    , Z))       // AVX2
  V_EMIT_VVVI(v_insert_i128        , PACK_AVX_SSE(Vinserti128  , None    , Y))       // AVX512

  // Initialization.

  inline void v_zero_i(const Operand_& dst) noexcept { v_emit_vvv_vv(PACK_AVX_SSE(Vpxor , Pxor , Z), dst, dst, dst); }
  inline void v_zero_f(const Operand_& dst) noexcept { v_emit_vvv_vv(PACK_AVX_SSE(Vxorps, Xorps, Z), dst, dst, dst); }
  inline void v_zero_d(const Operand_& dst) noexcept { v_emit_vvv_vv(PACK_AVX_SSE(Vxorpd, Xorpd, Z), dst, dst, dst); }

  inline void v_zero_i(const OpArray& dst) noexcept { for (uint32_t i = 0; i < dst.size(); i++) v_zero_i(dst[i]); }
  inline void v_zero_f(const OpArray& dst) noexcept { for (uint32_t i = 0; i < dst.size(); i++) v_zero_f(dst[i]); }
  inline void v_zero_d(const OpArray& dst) noexcept { for (uint32_t i = 0; i < dst.size(); i++) v_zero_d(dst[i]); }

  BL_NOINLINE void v_ones_i(const Operand_& dst) noexcept {
    /*
    if (hasAVX512())
      cc->vpternlogd(dst, dst, dst, 0xFFu);
    */
    v_emit_vvv_vv(PACK_AVX_SSE(Vpcmpeqb, Pcmpeqb, Z), dst, dst, dst);
  }

  // Conversion.

  inline void s_mov_i32(const x86::Vec& dst, const x86::Gp& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovd, Movd, X), dst, src); }
  inline void s_mov_i64(const x86::Vec& dst, const x86::Gp& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovq, Movq, X), dst, src); }

  inline void s_mov_i32(const x86::Gp& dst, const x86::Vec& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovd, Movd, X), dst, src); }
  inline void s_mov_i64(const x86::Gp& dst, const x86::Vec& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovq, Movq, X), dst, src); }

  //! \name Memory Loads & Stores
  //! \{

  BL_NOINLINE void v_load_i8(const Operand_& dst, const x86::Mem& src) noexcept {
    x86::Xmm dst_xmm = dst.as<x86::Vec>().xmm();
    if (hasSSE4_1()) {
      v_zero_i(dst_xmm);
      v_insert_u8_(dst_xmm, dst_xmm, src, 0);
    }
    else {
      x86::Gp tmp = cc->newUInt32("@tmp");
      i_load_u8(tmp, src);
      s_mov_i32(dst_xmm, tmp);
    }
  }

  BL_NOINLINE void v_load_i16(const Operand_& dst, const x86::Mem& src) noexcept {
    x86::Xmm dst_xmm = dst.as<x86::Vec>().xmm();
    if (hasSSE4_1()) {
      v_zero_i(dst_xmm);
      v_insert_u16(dst_xmm, dst_xmm, src, 0);
    }
    else {
      x86::Gp tmp = cc->newUInt32("@tmp");
      i_load_u16(tmp, src);
      s_mov_i32(dst_xmm, tmp);
    }
  }

  BL_NOINLINE void v_load_u8_u16_2x(const Operand_& dst, const x86::Mem& lo, const x86::Mem& hi) noexcept {
    x86::Gp reg = cc->newUInt32("@tmp");
    x86::Mem mLo(lo);
    x86::Mem mHi(hi);

    mLo.setSize(1);
    mHi.setSize(1);

    cc->movzx(reg, mHi);
    cc->shl(reg, 16);
    cc->mov(reg.r8(), mLo);
    s_mov_i32(dst.as<x86::Vec>().xmm(), reg);
  }

  inline void v_load_i32(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovd, Movd, X), dst, src); }
  inline void v_load_i64(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovq, Movq, X), dst, src); }
  inline void v_load_f32(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovss, Movss, X), dst, src); }
  inline void v_load_f64(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovsd, Movsd, X), dst, src); }

  inline void v_loadl_2xf32(const Operand_& dst, const Operand_& src1, const x86::Mem& src2) noexcept { v_emit_vvv_vv(PACK_AVX_SSE(Vmovlps, Movlps, X), dst, src1, src2); }
  inline void v_loadh_2xf32(const Operand_& dst, const Operand_& src1, const x86::Mem& src2) noexcept { v_emit_vvv_vv(PACK_AVX_SSE(Vmovhps, Movhps, X), dst, src1, src2); }

  inline void v_loadl_f64(const Operand_& dst, const Operand_& src1, const x86::Mem& src2) noexcept { v_emit_vvv_vv(PACK_AVX_SSE(Vmovlpd, Movlpd, X), dst, src1, src2); }
  inline void v_loadh_f64(const Operand_& dst, const Operand_& src1, const x86::Mem& src2) noexcept { v_emit_vvv_vv(PACK_AVX_SSE(Vmovhpd, Movhpd, X), dst, src1, src2); }

  inline void v_loada_i128(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqa, Movaps, X), dst, src); }
  inline void v_loadu_i128(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqu, Movups, X), dst, src); }
  inline void v_loadu_i128_ro(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vloadi128uRO), dst, src); }

  inline void v_loada_f128(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovaps, Movaps, X), dst, src); }
  inline void v_loadu_f128(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovups, Movups, X), dst, src); }
  inline void v_loada_d128(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovapd, Movaps, X), dst, src); }
  inline void v_loadu_d128(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovupd, Movups, X), dst, src); }

  inline void v_load_i128(const Operand_& dst, const x86::Mem& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovdqu, Movups, X), PACK_AVX_SSE(Vmovdqa, Movaps, X) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 16)], dst, src);
  }

  inline void v_load_f128(const Operand_& dst, const x86::Mem& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovups, Movups, X), PACK_AVX_SSE(Vmovaps, Movaps, X) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 16)], dst, src);
  }

  inline void v_load_d128(const Operand_& dst, const x86::Mem& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovupd, Movupd, X), PACK_AVX_SSE(Vmovapd, Movapd, X) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 16)], dst, src);
  }

  inline void v_loada_i256(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqa, Movaps, Y), dst, src); }
  inline void v_loadu_i256(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqu, Movups, Y), dst, src); }
  inline void v_loadu_i256_ro(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vloadi128uRO), dst, src); }

  inline void v_loada_f256(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovaps, Movaps, Y), dst, src); }
  inline void v_loadu_f256(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovups, Movups, Y), dst, src); }
  inline void v_loada_d256(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovapd, Movaps, Y), dst, src); }
  inline void v_loadu_d256(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovupd, Movups, Y), dst, src); }

  inline void v_load_i256(const Operand_& dst, const x86::Mem& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovdqu, None, Y), PACK_AVX_SSE(Vmovdqa, None, Y) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 32)], dst, src);
  }

  inline void v_load_f256(const Operand_& dst, const x86::Mem& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovups, None, Y), PACK_AVX_SSE(Vmovaps, None, Y) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 32)], dst, src);
  }

  inline void v_load_d256(const Operand_& dst, const x86::Mem& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovupd, None, Y), PACK_AVX_SSE(Vmovapd, None, Y) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 32)], dst, src);
  }

  BL_NOINLINE void v_store_i8(const x86::Mem& dst, const x86::Vec& src) noexcept {
    if (hasSSE4_1()) {
      v_emit_vvi_vvi(PACK_AVX_SSE(Vpextrb, Pextrb, X), dst, src, 0);
    }
    else {
      x86::Gp tVal = cc->newUInt32("tVal");
      cc->movd(tVal, src.as<x86::Xmm>());
      cc->mov(dst, tVal.r8());
    }
  }

  BL_NOINLINE void v_store_i16(const x86::Mem& dst, const x86::Vec& src) noexcept {
    if (hasSSE4_1()) {
      v_emit_vvi_vvi(PACK_AVX_SSE(Vpextrw, Pextrw, X), dst, src, 0);
    }
    else {
      x86::Gp tVal = cc->newUInt32("tVal");
      cc->movd(tVal, src.as<x86::Xmm>());
      cc->mov(dst, tVal.r16());
    }
  }

  inline void v_store_i32(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovd, Movd, X), dst, src); }
  inline void v_store_i64(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovq, Movq, X), dst, src); }

  inline void v_store_f32(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovss, Movss, X), dst, src); }
  inline void v_store_f64(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovsd, Movsd, X), dst, src); }

  inline void v_storel_2xf32(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovlps, Movlps, X), dst, src); }
  inline void v_storeh_2xf32(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovhps, Movhps, X), dst, src); }

  inline void v_storel_f64(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovsd , Movsd , X), dst, src); }
  inline void v_storeh_f64(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovhpd, Movhpd, X), dst, src); }

  inline void v_storea_i128(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqa, Movaps, X), dst, src); }
  inline void v_storeu_i128(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqu, Movups, X), dst, src); }
  inline void v_storea_f128(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovaps, Movaps, X), dst, src); }
  inline void v_storeu_f128(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovups, Movups, X), dst, src); }
  inline void v_storea_d128(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovapd, Movaps, X), dst, src); }
  inline void v_storeu_d128(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovupd, Movups, X), dst, src); }

  inline void v_store_i128(const x86::Mem& dst, const x86::Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovdqu, Movups, X), PACK_AVX_SSE(Vmovdqa, Movaps, X) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 16)], dst, src);
  }

  inline void v_store_f128(const x86::Mem& dst, const x86::Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovups, Movups, X), PACK_AVX_SSE(Vmovaps, Movaps, X) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 16)], dst, src);
  }

  inline void v_store_d128(const x86::Mem& dst, const x86::Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovupd, Movupd, X), PACK_AVX_SSE(Vmovapd, Movapd, X) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 16)], dst, src);
  }

  inline void v_storea_i256(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqa, None, Y), dst, src); }
  inline void v_storeu_i256(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqu, None, Y), dst, src); }
  inline void v_storea_f256(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovaps, None, Y), dst, src); }
  inline void v_storeu_f256(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovups, None, Y), dst, src); }
  inline void v_storea_d256(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovapd, None, Y), dst, src); }
  inline void v_storeu_d256(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovupd, None, Y), dst, src); }

  inline void v_store_i256(const x86::Mem& dst, const x86::Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovdqu, None, Y), PACK_AVX_SSE(Vmovdqa, None, Y) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 32)], dst, src);
  }

  inline void v_store_f256(const x86::Mem& dst, const x86::Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovups, None, Y), PACK_AVX_SSE(Vmovaps, None, Y) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 32)], dst, src);
  }

  inline void v_store_d256(const x86::Mem& dst, const x86::Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovupd, None, Y), PACK_AVX_SSE(Vmovapd, None, Y) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 32)], dst, src);
  }

  inline void v_storea_i512(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqa32, None, Z), dst, src); }
  inline void v_storeu_i512(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovdqu32, None, Z), dst, src); }
  inline void v_storea_f512(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovaps, None, Z), dst, src); }
  inline void v_storeu_f512(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovups, None, Z), dst, src); }
  inline void v_storea_d512(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovapd, None, Z), dst, src); }
  inline void v_storeu_d512(const x86::Mem& dst, const Operand_& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovupd, None, Z), dst, src); }

  inline void v_store_i512(const x86::Mem& dst, const x86::Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovdqu32, None, Z), PACK_AVX_SSE(Vmovdqa32, None, Z) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 64)], dst, src);
  }

  inline void v_store_f512(const x86::Mem& dst, const x86::Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovups, None, Z), PACK_AVX_SSE(Vmovaps, None, Z) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 64)], dst, src);
  }

  inline void v_store_d512(const x86::Mem& dst, const x86::Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovupd, None, Z), PACK_AVX_SSE(Vmovapd, None, Z) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= 64)], dst, src);
  }

  //! \}

  //! \name Memory Loads & Stores with Packing and Unpacking
  //! \{

  inline void v_load_i64_u8u16_(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovzxbw, Pmovzxbw, X), dst, src); }
  inline void v_load_i32_u8u32_(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovzxbd, Pmovzxbd, X), dst, src); }
  inline void v_load_i16_u8u64_(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovzxbq, Pmovzxbq, X), dst, src); }
  inline void v_load_i64_u16u32_(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovzxwd, Pmovzxwd, X), dst, src); }
  inline void v_load_i32_u16u64_(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovzxwq, Pmovzxwq, X), dst, src); }
  inline void v_load_i64_u32u64_(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovzxdq, Pmovzxdq, X), dst, src); }

  inline void v_load_i64_i8i16_(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovsxbw, Pmovsxbw, X), dst, src); }
  inline void v_load_i32_i8i32_(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovsxbd, Pmovsxbd, X), dst, src); }
  inline void v_load_i16_i8i64_(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovsxbq, Pmovsxbq, X), dst, src); }
  inline void v_load_i64_i16i32_(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovsxwd, Pmovsxwd, X), dst, src); }
  inline void v_load_i32_i16i64_(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovsxwq, Pmovsxwq, X), dst, src); }
  inline void v_load_i64_i32i64_(const Operand_& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vpmovsxdq, Pmovsxdq, X), dst, src); }

  //! \}

  //! \name Memory Loads & Stores of Vector Sizes
  //! \{

  BL_NOINLINE void v_loada_ivec(const x86::Vec& dst, const x86::Mem& src) noexcept {
    if (dst.isZmm())
      cc->vmovdqa32(dst, src);
    else
      v_emit_vv_vv(PACK_AVX_SSE(Vmovdqa, Movaps, Z), dst, src);
  }

  inline void v_loada_fvec(const x86::Vec& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovaps, Movaps, Z), dst, src); }
  inline void v_loada_dvec(const x86::Vec& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovapd, Movapd, Z), dst, src); }

  BL_NOINLINE void v_loadu_ivec(const x86::Vec& dst, const x86::Mem& src) noexcept {
    if (dst.isZmm())
      cc->vmovdqu32(dst, src);
    else
      v_emit_vv_vv(PACK_AVX_SSE(Vmovdqu, Movups, Z), dst, src);
  }

  inline void v_loadu_fvec(const x86::Vec& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovups, Movups, Z), dst, src); }
  inline void v_loadu_dvec(const x86::Vec& dst, const x86::Mem& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovupd, Movupd, Z), dst, src); }

  BL_NOINLINE void v_load_ivec(const x86::Vec& dst, const x86::Mem& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable512[2] = { PACK_AVX_SSE(Vmovdqu32, None, Z), PACK_AVX_SSE(Vmovdqa32, None, Z) };
    static const uint32_t kMovTable256[2] = { PACK_AVX_SSE(Vmovdqu, Movups, Z), PACK_AVX_SSE(Vmovdqa, Movaps, Z) };

    if (dst.isZmm())
      v_emit_vv_vv(kMovTable512[size_t(alignment >= src.size())], dst, src);
    else
      v_emit_vv_vv(kMovTable256[size_t(alignment >= src.size())], dst, src);
  }

  inline void v_load_fvec(const x86::Vec& dst, const x86::Mem& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovups, Movups, Z), PACK_AVX_SSE(Vmovaps, Movaps, Z) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= src.size())], dst, src);
  }

  inline void v_load_dvec(const x86::Vec& dst, const x86::Mem& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovupd, Movupd, Z), PACK_AVX_SSE(Vmovapd, Movapd, Z) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= src.size())], dst, src);
  }

  BL_NOINLINE void v_storea_ivec(const x86::Mem& dst, const x86::Vec& src) noexcept {
    if (src.isZmm())
      v_emit_vv_vv(PACK_AVX_SSE(Vmovdqa32, None, Z), dst, src);
    else
      v_emit_vv_vv(PACK_AVX_SSE(Vmovdqa, Movaps, Z), dst, src);
  }

  inline void v_storea_fvec(const x86::Mem& dst, const x86::Vec& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovaps, Movaps, Z), dst, src); }
  inline void v_storea_dvec(const x86::Mem& dst, const x86::Vec& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovapd, Movapd, Z), dst, src); }

  BL_NOINLINE void v_storeu_ivec(const x86::Mem& dst, const x86::Vec& src) noexcept {
    if (src.isZmm())
      v_emit_vv_vv(PACK_AVX_SSE(Vmovdqu32, None, Z), dst, src);
    else
      v_emit_vv_vv(PACK_AVX_SSE(Vmovdqu, Movups, Z), dst, src);
  }

  inline void v_storeu_fvec(const x86::Mem& dst, const x86::Vec& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovups, Movups, Z), dst, src); }
  inline void v_storeu_dvec(const x86::Mem& dst, const x86::Vec& src) noexcept { v_emit_vv_vv(PACK_AVX_SSE(Vmovupd, Movupd, Z), dst, src); }

  BL_NOINLINE void v_store_ivec(const x86::Mem& dst, const x86::Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable512[2] = { PACK_AVX_SSE(Vmovdqu32, None, Z), PACK_AVX_SSE(Vmovdqa32, None, Z) };
    static const uint32_t kMovTable256[2] = { PACK_AVX_SSE(Vmovdqu, Movups, Z), PACK_AVX_SSE(Vmovdqa, Movaps, Z) };

    if (src.isZmm())
      v_emit_vv_vv(kMovTable512[size_t(alignment >= src.size())], dst, src);
    else
      v_emit_vv_vv(kMovTable256[size_t(alignment >= src.size())], dst, src);
  }

  inline void v_store_fvec(const x86::Mem& dst, const x86::Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovups, Movups, Z), PACK_AVX_SSE(Vmovaps, Movaps, Z) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= src.size())], dst, src);
  }

  inline void v_store_dvec(const x86::Mem& dst, const x86::Vec& src, Alignment alignment) noexcept {
    static const uint32_t kMovTable[2] = { PACK_AVX_SSE(Vmovupd, Movupd, Z), PACK_AVX_SSE(Vmovapd, Movapd, Z) };
    v_emit_vv_vv(kMovTable[size_t(alignment >= src.size())], dst, src);
  }

  //! \}

  //! \name Memory Loads & Stores with Parameterized Size
  //! \{

  BL_NOINLINE void v_load_iany(const x86::Vec& dst, const x86::Mem& src, uint32_t nBytes, Alignment alignment) noexcept {
    switch (nBytes) {
      case 1: v_load_i8(dst, src); break;
      case 2: v_load_i16(dst, src); break;
      case 4: v_load_i32(dst, src); break;
      case 8: v_load_i64(dst, src); break;
      case 16: v_load_i128(dst, src, alignment); break;
      case 32: v_load_i256(dst, src, alignment); break;

      default:
        BL_NOT_REACHED();
    }
  }

  BL_NOINLINE void v_store_iany(const x86::Mem& dst, const x86::Vec& src, uint32_t nBytes, Alignment alignment) noexcept {
    switch (nBytes) {
      case 1: v_store_i8(dst, src); break;
      case 2: v_store_i16(dst, src); break;
      case 4: v_store_i32(dst, src); break;
      case 8: v_store_i64(dst, src); break;
      case 16: v_store_i128(dst, src, alignment); break;
      case 32: v_store_i256(dst, src, alignment); break;

      default:
        BL_NOT_REACHED();
    }
  }

  //! \}

  //! \name Memory Loads & Stores with Predicate
  //! \{

  BL_NOINLINE void v_load_predicated_v8(const x86::Vec& dst, const PixelPredicate& pred, const x86::Mem& src) noexcept {
    BL_ASSERT(pred.k.isValid());
    cc->k(pred.k).z().vmovdqu8(dst, src);
  }

  BL_NOINLINE void v_load_predicated_v32(const x86::Vec& dst, const PixelPredicate& pred, const x86::Mem& src) noexcept {
    if (pred.k.isValid())
      cc->k(pred.k).z().vmovdqu32(dst, src);
    else if (hasAVX2())
      cc->vpmaskmovd(dst, pred.v32, src);
    else
      cc->vmaskmovps(dst, pred.v32, src);
  }

  BL_NOINLINE void v_load_predicated_v64(const x86::Vec& dst, const PixelPredicate& pred, const x86::Mem& src) noexcept {
    if (pred.k.isValid())
      cc->k(pred.k).z().vmovdqu64(dst, src);
    else if (hasAVX2())
      cc->vpmaskmovq(dst, pred.v64, src);
    else
      cc->vmaskmovpd(dst, pred.v64, src);
  }

  BL_NOINLINE void v_store_predicated_v8(const x86::Mem& dst, const PixelPredicate& pred, const x86::Vec& src) noexcept {
    BL_ASSERT(pred.k.isValid());
    cc->k(pred.k).vmovdqu8(dst, src);
  }

  BL_NOINLINE void v_store_predicated_v32(const x86::Mem& dst, const PixelPredicate& pred, const x86::Vec& src) noexcept {
    if (pred.k.isValid())
      cc->k(pred.k).vmovdqu32(dst, src);
    else if (hasAVX2())
      cc->vpmaskmovd(dst, pred.v32, src);
    else
      cc->vmaskmovps(dst, pred.v32, src);
  }

  BL_NOINLINE void v_store_predicated_v64(const x86::Mem& dst, const PixelPredicate& pred, const x86::Vec& src) noexcept {
    if (pred.k.isValid())
      cc->k(pred.k).vmovdqu64(dst, src);
    else if (hasAVX2())
      cc->vpmaskmovq(dst, pred.v64, src);
    else
      cc->vmaskmovpd(dst, pred.v64, src);
  }

  //! \}

  // Intrinsics:
  //
  //   - v_mov{x}{y}   - Move with sign or zero extension from {x} to {y}. Similar to instructions like `pmovzx..`,
  //                     `pmovsx..`, and `punpckl..`
  //
  //   - v_swap{x}     - Swap low and high elements. If the vector has more than 2 elements it's divided into 2
  //                     element vectors in which the operation is performed separately.
  //
  //   - v_dup{l|h}{x} - Duplicate either low or high element into both. If there are more than 2 elements in the
  //                     vector it's considered they are separate units. For example a 4-element vector can be
  //                     considered as 2 2-element vectors on which the duplication operation is performed.

  template<typename DstT, typename SrcT>
  inline void v_mov_u8_u16(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vmovu8u16), dst, src); }

  template<typename DstT, typename SrcT>
  inline void v_mov_u8_u32(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vmovu8u32), dst, src); }

  template<typename DstT, typename SrcT>
  inline void v_mov_u16_u32(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vmovu16u32), dst, src); }

  template<typename DstT, typename SrcT>
  inline void v_abs_i8(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vabsi8), dst, src); }

  template<typename DstT, typename SrcT>
  inline void v_abs_i16(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vabsi16), dst, src); }

  template<typename DstT, typename SrcT>
  inline void v_abs_i32(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vabsi32), dst, src); }

  template<typename DstT, typename SrcT>
  inline void v_abs_i64(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vabsi64), dst, src); }

  template<typename DstT, typename SrcT>
  inline void v_swap_i32(const DstT& dst, const SrcT& src) noexcept { v_swizzle_i32(dst, src, x86::shuffleImm(2, 3, 0, 1)); }

  template<typename DstT, typename SrcT>
  inline void v_swap_i64(const DstT& dst, const SrcT& src) noexcept { v_swizzle_i32(dst, src, x86::shuffleImm(1, 0, 3, 2)); }

  template<typename DstT, typename SrcT>
  inline void v_dupl_i32(const DstT& dst, const SrcT& src) noexcept { v_swizzle_i32(dst, src, x86::shuffleImm(2, 2, 0, 0)); }

  template<typename DstT, typename SrcT>
  inline void v_duph_i32(const DstT& dst, const SrcT& src) noexcept { v_swizzle_i32(dst, src, x86::shuffleImm(3, 3, 1, 1)); }

  template<typename DstT, typename SrcT>
  inline void v_dupl_i64(const DstT& dst, const SrcT& src) noexcept { v_swizzle_i32(dst, src, x86::shuffleImm(1, 0, 1, 0)); }

  template<typename DstT, typename SrcT>
  inline void v_duph_i64(const DstT& dst, const SrcT& src) noexcept { v_swizzle_i32(dst, src, x86::shuffleImm(3, 2, 3, 2)); }

  // Dst = (CondBit == 0) ? Src1 : Src2;
  template<typename DstT, typename Src1T, typename Src2T, typename CondT>
  inline void v_blendv_u8(const DstT& dst, const Src1T& src1, const Src2T& src2, const CondT& cond) noexcept {
    v_emit_vvvv_vvv(PackedInst::packIntrin(kIntrin4Vpblendvb), dst, src1, src2, cond);
  }

  // Dst = (CondBit == 0) ? Src1 : Src2;
  template<typename DstT, typename Src1T, typename Src2T, typename CondT>
  inline void v_blendv_u8_destructive(const DstT& dst, const Src1T& src1, const Src2T& src2, const CondT& cond) noexcept {
    v_emit_vvvv_vvv(PackedInst::packIntrin(kIntrin4VpblendvbDestructive), dst, src1, src2, cond);
  }

  template<typename DstT, typename SrcT>
  inline void v_inv255_u16(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vinv255u16), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_inv256_u16(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vinv256u16), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_inv255_u32(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vinv255u32), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_inv256_u32(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vinv256u32), dst, src); }

  template<typename DstT, typename SrcT>
  inline void v_dupl_f64(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vduplpd), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_duph_f64(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2Vduphpd), dst, src); }

  template<typename DstT, typename Src1T, typename Src2T>
  inline void v_hadd_f64(const DstT& dst, const Src1T& src1, const Src2T& src2) noexcept { v_emit_vvv_vv(PackedInst::packIntrin(kIntrin3Vhaddpd), dst, src1, src2); }

  template<typename DstT, typename SrcT>
  inline void v_expand_lo_i32(const DstT& dst, const SrcT& src) noexcept {
    v_swizzle_i32(dst, src, x86::shuffleImm(0, 0, 0, 0));
  }

  // dst.u64[0] = src1.u64[1];
  // dst.u64[1] = src2.u64[0];
  template<typename DstT, typename Src1T, typename Src2T>
  inline void v_combine_hl_i64(const DstT& dst, const Src1T& src1, const Src2T& src2) noexcept {
    v_emit_vvv_vv(PackedInst::packIntrin(kIntrin3Vcombhli64), dst, src1, src2);
  }

  // dst.d64[0] = src1.d64[1];
  // dst.d64[1] = src2.d64[0];
  template<typename DstT, typename Src1T, typename Src2T>
  inline void v_combine_hl_f64(const DstT& dst, const Src1T& src1, const Src2T& src2) noexcept {
    v_emit_vvv_vv(PackedInst::packIntrin(kIntrin3Vcombhld64), dst, src1, src2);
  }

  template<typename DstT, typename Src1T, typename Src2T>
  inline void v_min_u16(const DstT& dst, const Src1T& src1, const Src2T& src2) noexcept {
    v_emit_vvv_vv(PackedInst::packIntrin(kIntrin3Vminu16), dst, src1, src2);
  }

  template<typename DstT, typename Src1T, typename Src2T>
  inline void v_max_u16(const DstT& dst, const Src1T& src1, const Src2T& src2) noexcept {
    v_emit_vvv_vv(PackedInst::packIntrin(kIntrin3Vmaxu16), dst, src1, src2);
  }

  // Multiplies packed uint64_t in `src1` with packed low uint32_t int `src2`.
  template<typename DstT, typename Src1T, typename Src2T>
  inline void v_mul_u64_u32_lo(const DstT& dst, const Src1T& src1, const Src2T& src2) noexcept {
    v_emit_vvv_vv(PackedInst::packIntrin(kIntrin3Vmulu64x32), dst, src1, src2);
  }

  template<typename DstT, typename SrcT>
  BL_NOINLINE void v_mul257_hi_u16(const DstT& dst, const SrcT& src) {
    v_mulh_u16(dst, src, simdConst(&blCommonTable.i_0101010101010101, Bcst::kNA, dst));
  }

  // TODO: [PIPEGEN] Consolidate this to only one implementation.
  template<typename DstSrcT>
  BL_NOINLINE void v_div255_u16(const DstSrcT& x) {
    v_add_i16(x, x, simdConst(&blCommonTable.i_0080008000800080, Bcst::kNA, x));
    v_mul257_hi_u16(x, x);
  }

  template<typename DstSrcT>
  BL_NOINLINE void v_div255_u16_2x(const DstSrcT& v0, const DstSrcT& v1) noexcept {
    Operand i_0080008000800080 = simdConst(&blCommonTable.i_0080008000800080, Bcst::kNA, v0);
    Operand i_0101010101010101 = simdConst(&blCommonTable.i_0101010101010101, Bcst::kNA, v0);

    v_add_i16(v0, v0, i_0080008000800080);
    v_add_i16(v1, v1, i_0080008000800080);

    v_mulh_u16(v0, v0, i_0101010101010101);
    v_mulh_u16(v1, v1, i_0101010101010101);
  }

  template<typename DstT, typename SrcT>
  inline void v_expand_lo_ps(const DstT& dst, const SrcT& src) noexcept {
    v_expand_lo_i32(dst, src);
  }

  template<typename DstT, typename SrcT>
  inline void v_swizzle_f32(const DstT& dst, const SrcT& src, uint32_t imm) noexcept { v_emit_vvi_vi(PackedInst::packIntrin(kIntrin2iVswizps), dst, src, imm); }
  template<typename DstT, typename SrcT>
  inline void v_swizzle_f64(const DstT& dst, const SrcT& src, uint32_t imm) noexcept { v_emit_vvi_vi(PackedInst::packIntrin(kIntrin2iVswizpd), dst, src, imm); }

  template<typename DstT, typename SrcT>
  inline void v_swap_f32(const DstT& dst, const SrcT& src) noexcept { v_swizzle_f32(dst, src, x86::shuffleImm(2, 3, 0, 1)); }
  template<typename DstT, typename SrcT>
  inline void v_swap_f64(const DstT& dst, const SrcT& src) noexcept { v_swizzle_f64(dst, src, x86::shuffleImm(0, 1)); }

  template<typename DstT, typename SrcT>
  inline void v_broadcast_u8(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2VBroadcastU8), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_broadcast_u16(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2VBroadcastU16), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_broadcast_u32(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2VBroadcastU32), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_broadcast_u64(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2VBroadcastU64), dst, src); }

  template<typename DstT, typename SrcT>
  inline void v_broadcast_i32x4(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2VBroadcastI32x4), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_broadcast_i64x2(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2VBroadcastI64x2), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_broadcast_f32x4(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2VBroadcastF32x4), dst, src); }
  template<typename DstT, typename SrcT>
  inline void v_broadcast_f64x2(const DstT& dst, const SrcT& src) noexcept { v_emit_vv_vv(PackedInst::packIntrin(kIntrin2VBroadcastF64x2), dst, src); }


  template<typename DstT, typename Src1T, typename Src2T>
  BL_INLINE void v_min_or_max_u8(const DstT& dst, const Src1T& src1, const Src2T& src2, bool isMin) noexcept {
    if (isMin)
      v_min_u8(dst, src1, src2);
    else
      v_max_u8(dst, src1, src2);
  }

  // d = int(floor(a / b) * b).
  template<typename VecOrMem>
  BL_NOINLINE void v_mod_pd(const x86::Vec& d, const x86::Vec& a, const VecOrMem& b) noexcept {
    if (hasSSE4_1()) {
      v_div_f64(d, a, b);
      v_round_f64_(d, d, x86::RoundImm::kTrunc | x86::RoundImm::kSuppress);
      v_mul_f64(d, d, b);
    }
    else {
      x86::Xmm t = cc->newXmm("vModTmp");

      v_div_f64(d, a, b);
      v_cvtt_f64_i32(t, d);
      v_cvt_i32_f64(t, t);
      v_cmp_f64(d, d, t, x86::VCmpImm::kLT_OS);
      v_and_f64(d, d, simdMemConst(&blCommonTable.f64_m1, Bcst::k64, d));
      v_add_f64(d, d, t);
      v_mul_f64(d, d, b);
    }
  }

  //! \}

  //! \name Emit - 'X' High Level Functionality
  //! \{

  void x_make_predicate_v32(const x86::Vec& vmask, const x86::Gp& count) noexcept;

  void x_ensure_predicate_8(PixelPredicate& predicate, uint32_t width) noexcept;
  void x_ensure_predicate_32(PixelPredicate& predicate, uint32_t width) noexcept;

  // Kind of a hack - if we don't have SSE4.1 we have to load the byte into GP register first and then we use 'PINSRW',
  // which is provided by baseline SSE2. If we have SSE4.1 then it's much easier as we can load the byte by 'PINSRB'.
  void x_insert_word_or_byte(const x86::Vec& dst, const x86::Mem& src, uint32_t wordIndex) noexcept {
    x86::Mem m = src;
    m.setSize(1);

    if (hasSSE4_1()) {
      v_insert_u8_(dst, dst, m, wordIndex * 2u);
    }
    else {
      x86::Gp tmp = cc->newUInt32("@tmp");
      cc->movzx(tmp, m);
      v_insert_u16(dst, dst, tmp, wordIndex);
    }
  }

  void x_inline_pixel_fill_loop(x86::Gp& dst, x86::Vec& src, x86::Gp& i, uint32_t mainLoopSize, uint32_t itemSize, uint32_t itemGranularity) noexcept;
  void x_inline_pixel_copy_loop(x86::Gp& dst, x86::Gp& src, x86::Gp& i, uint32_t mainLoopSize, uint32_t itemSize, uint32_t itemGranularity, BLInternalFormat format) noexcept;

  void _x_inline_memcpy_sequence_xmm(
    const x86::Mem& dPtr, bool dstAligned,
    const x86::Mem& sPtr, bool srcAligned, uint32_t numBytes, const x86::Vec& fillMask) noexcept;

  BL_NOINLINE void x_storea_fill(x86::Mem dst, const x86::Vec& src, uint32_t n) noexcept {
    if (src.isZmm() && n >= 64) {
      for (uint32_t j = 0; j < n; j += 64u) {
        v_storea_i512(dst, src);
        dst.addOffsetLo32(64);
      }
    }
    else if (src.isYmm() && n >= 32) {
      for (uint32_t j = 0; j < n; j += 32u) {
        v_storea_i256(dst, src);
        dst.addOffsetLo32(32);
      }
    }
    else {
      x86::Xmm srcXmm = src.xmm();
      for (uint32_t j = 0; j < n; j += 16u) {
        v_storea_i128(dst, srcXmm);
        dst.addOffsetLo32(16);
      }
    }
  }

  BL_NOINLINE void x_storeu_fill(x86::Mem dst, const x86::Vec& src_, uint32_t n) noexcept {
    x86::Vec src = src_;

    if (src.size() > 32 && n <= 32)
      src = src.ymm();

    if (src.size() > 16 && n <= 16)
      src = src.xmm();

    uint32_t vecSize = src.size();
    for (uint32_t i = 0; i < n; i += vecSize) {
      v_storeu_ivec(dst, src);
      dst.addOffsetLo32(vecSize);
    }
  }

  //! \}

  //! \name Emit - Pixel Fetch & Store Utilities
  //! \{

  void x_fetch_mask_a8_advance(VecArray& vm, PixelCount n, PixelType pixelType, const x86::Gp& mPtr, const x86::Vec& globalAlpha) noexcept;

  //! Fetches `n` pixels to vector register(s) in `p` from memory location `src_`.
  void x_fetch_pixel(Pixel& p, PixelCount n, PixelFlags flags, BLInternalFormat format, const x86::Mem& src_, Alignment alignment) noexcept;
  void x_fetch_pixel(Pixel& p, PixelCount n, PixelFlags flags, BLInternalFormat format, const x86::Mem& src_, Alignment alignment, PixelPredicate& predicate) noexcept;

  void _x_fetch_pixel_a8(Pixel& p, PixelCount n, PixelFlags flags, BLInternalFormat format, const x86::Mem& src_, Alignment alignment, PixelPredicate& predicate) noexcept;
  void _x_fetch_pixel_rgba32(Pixel& p, PixelCount n, PixelFlags flags, BLInternalFormat format, const x86::Mem& src_, Alignment alignment, PixelPredicate& predicate) noexcept;

  //! Makes sure that the given pixel `p` has all the requrements as specified by `flags`.
  void x_satisfy_pixel(Pixel& p, PixelFlags flags) noexcept;

  void _x_satisfy_pixel_a8(Pixel& p, PixelFlags flags) noexcept;
  void _x_satisfy_pixel_rgba32(Pixel& p, PixelFlags flags) noexcept;

  //! Makes sure that the given pixel `p` has all the requrements as specified by `flags` (solid source only).
  void x_satisfy_solid(Pixel& p, PixelFlags flags) noexcept;

  void _x_satisfy_solid_a8(Pixel& p, PixelFlags flags) noexcept;
  void _x_satisfy_solid_rgba32(Pixel& p, PixelFlags flags) noexcept;

  void _x_pack_pixel(VecArray& px, VecArray& ux, uint32_t n, const char* prefix, const char* pxName) noexcept;
  void _x_unpack_pixel(VecArray& ux, VecArray& px, uint32_t n, const char* prefix, const char* uxName) noexcept;

  void x_fetch_unpacked_a8_2x(const x86::Xmm& dst, BLInternalFormat format, const x86::Mem& src1, const x86::Mem& src0) noexcept;

  void x_assign_unpacked_alpha_values(Pixel& p, PixelFlags flags, x86::Xmm& vec) noexcept;

  //! Fills alpha channel with 1.
  void x_fill_pixel_alpha(Pixel& p) noexcept;

  void x_store_pixel_advance(const x86::Gp& dPtr, Pixel& p, PixelCount n, uint32_t bpp, Alignment alignment, PixelPredicate& predicate) noexcept;

  //! \}

  //! \name Emit - Pixel Processing Utilities
  //! \{

  //! Pack 16-bit integers to unsigned 8-bit integers in an AVX2 and AVX512 aware way.
  template<typename Dst, typename Src1, typename Src2>
  BL_NOINLINE void x_packs_i16_u8(const Dst& d, const Src1& s1, const Src2& s2) noexcept {
    if (JitUtils::isXmm(s1)) {
      v_packs_i16_u8(d, s1, s2);
    }
    else {
      const x86::Vec& vType = JitUtils::firstOp(s1).template as<x86::Vec>();
      v_packs_i16_u8(d, s1, s2);
      v_perm_i64(d.cloneAs(vType), d.cloneAs(vType), x86::shuffleImm(3, 1, 2, 0));
    }
  }

  BL_NOINLINE void xStorePixel(const x86::Gp& dPtr, const x86::Vec& vSrc, uint32_t count, uint32_t bpp, Alignment alignment) noexcept {
    v_store_iany(x86::ptr(dPtr), vSrc, count * bpp, alignment);
  }

  inline void xStore32_ARGB(const x86::Mem& dst, const x86::Vec& vSrc) noexcept {
    v_store_i32(dst, vSrc);
  }

  BL_NOINLINE void xMovzxBW_LoHi(const x86::Vec& d0, const x86::Vec& d1, const x86::Vec& s) noexcept {
    BL_ASSERT(d0.id() != d1.id());

    if (hasSSE4_1()) {
      if (d0.id() == s.id()) {
        v_swizzle_i32(d1, d0, x86::shuffleImm(1, 0, 3, 2));
        v_mov_u8_u16_(d0, d0);
        v_mov_u8_u16_(d1, d1);
      }
      else {
        v_mov_u8_u16(d0, s);
        v_swizzle_i32(d1, s, x86::shuffleImm(1, 0, 3, 2));
        v_mov_u8_u16(d1, d1);
      }
    }
    else {
      x86::Vec zero = simdVecConst(&blCommonTable.i_0000000000000000, s);
      if (d1.id() != s.id()) {
        v_interleave_hi_i8(d1, s, zero);
        v_interleave_lo_i8(d0, s, zero);
      }
      else {
        v_interleave_lo_i8(d0, s, zero);
        v_interleave_hi_i8(d1, s, zero);
      }
    }
  }

  template<typename Dst, typename Src>
  inline void vExpandAlphaLo16(const Dst& d, const Src& s) noexcept { v_swizzle_lo_i16(d, s, x86::shuffleImm(3, 3, 3, 3)); }

  template<typename Dst, typename Src>
  inline void vExpandAlphaHi16(const Dst& d, const Src& s) noexcept { v_swizzle_hi_i16(d, s, x86::shuffleImm(3, 3, 3, 3)); }

  template<typename Dst, typename Src>
  inline void vExpandAlpha16(const Dst& d, const Src& s, uint32_t useHiPart = 1) noexcept {
    vExpandAlphaLo16(d, s);
    if (useHiPart)
      vExpandAlphaHi16(d, d);
  }

  template<typename Dst, typename Src>
  inline void vExpandAlphaPS(const Dst& d, const Src& s) noexcept { v_swizzle_i32(d, s, x86::shuffleImm(3, 3, 3, 3)); }

  template<typename DstT, typename SrcT>
  inline void vFillAlpha255B(const DstT& dst, const SrcT& src) noexcept { v_or_i32(dst, src, simdConst(&blCommonTable.i_FF000000FF000000, Bcst::k32, dst)); }
  template<typename DstT, typename SrcT>
  inline void vFillAlpha255W(const DstT& dst, const SrcT& src) noexcept { v_or_i64(dst, src, simdConst(&blCommonTable.i_00FF000000000000, Bcst::k64, dst)); }

  template<typename DstT, typename SrcT>
  BL_NOINLINE void vZeroAlphaB(const DstT& dst, const SrcT& src) noexcept { v_and_i32(dst, src, simdMemConst(&blCommonTable.i_00FFFFFF00FFFFFF, Bcst::k32, dst)); }

  template<typename DstT, typename SrcT>
  BL_NOINLINE void vZeroAlphaW(const DstT& dst, const SrcT& src) noexcept { v_and_i64(dst, src, simdMemConst(&blCommonTable.i_0000FFFFFFFFFFFF, Bcst::k64, dst)); }

  template<typename DstT, typename SrcT>
  inline void vNegAlpha8B(const DstT& dst, const SrcT& src) noexcept { v_xor_i32(dst, src, simdConst(&blCommonTable.i_FF000000FF000000, Bcst::k32, dst)); }
  template<typename DstT, typename SrcT>
  inline void vNegAlpha8W(const DstT& dst, const SrcT& src) noexcept { v_xor_i64(dst, src, simdConst(&blCommonTable.i_00FF000000000000, Bcst::k64, dst)); }

  template<typename DstT, typename SrcT>
  inline void vNegRgb8B(const DstT& dst, const SrcT& src) noexcept { v_xor_i32(dst, src, simdConst(&blCommonTable.i_00FFFFFF00FFFFFF, Bcst::k32, dst)); }
  template<typename DstT, typename SrcT>
  inline void vNegRgb8W(const DstT& dst, const SrcT& src) noexcept { v_xor_i64(dst, src, simdConst(&blCommonTable.i_000000FF00FF00FF, Bcst::k64, dst)); }

  // Performs 32-bit unsigned modulo of 32-bit `a` (hi DWORD) with 32-bit `b` (lo DWORD).
  template<typename VecOrMem_A, typename VecOrMem_B>
  BL_NOINLINE void xModI64HIxU64LO(const x86::Xmm& d, const VecOrMem_A& a, const VecOrMem_B& b) noexcept {
    x86::Xmm t0 = cc->newXmm("t0");
    x86::Xmm t1 = cc->newXmm("t1");

    v_swizzle_i32(t1, b, x86::shuffleImm(3, 3, 2, 0));
    v_swizzle_i32(d , a, x86::shuffleImm(2, 0, 3, 1));

    v_cvt_i32_f64(t1, t1);
    v_cvt_i32_f64(t0, d);
    v_mod_pd(t0, t0, t1);
    v_cvtt_f64_i32(t0, t0);

    v_sub_i32(d, d, t0);
    v_swizzle_i32(d, d, x86::shuffleImm(1, 3, 0, 2));
  }

  // Performs 32-bit unsigned modulo of 32-bit `a` (hi DWORD) with 64-bit `b` (DOUBLE).
  template<typename VecOrMem_A, typename VecOrMem_B>
  BL_NOINLINE void xModI64HIxDouble(const x86::Xmm& d, const VecOrMem_A& a, const VecOrMem_B& b) noexcept {
    x86::Xmm t0 = cc->newXmm("t0");

    v_swizzle_i32(d, a, x86::shuffleImm(2, 0, 3, 1));
    v_cvt_i32_f64(t0, d);
    v_mod_pd(t0, t0, b);
    v_cvtt_f64_i32(t0, t0);

    v_sub_i32(d, d, t0);
    v_swizzle_i32(d, d, x86::shuffleImm(1, 3, 0, 2));
  }

  BL_NOINLINE void xExtractUnpackedAFromPackedARGB32_1(const x86::Vec& d, const x86::Vec& s) noexcept {
    v_swizzle_lo_i16(d, s, x86::shuffleImm(1, 1, 1, 1));
    v_srl_i16(d, d, 8);
  }

  BL_NOINLINE void xExtractUnpackedAFromPackedARGB32_2(const x86::Vec& d, const x86::Vec& s) noexcept {
    if (hasSSSE3()) {
      v_shuffle_i8(d, s, simdConst(&blCommonTable.pshufb_xxxxxxxx1xxx0xxx_to_z1z1z1z1z0z0z0z0, Bcst::kNA, d));
    }
    else {
      v_swizzle_lo_i16(d, s, x86::shuffleImm(3, 3, 1, 1));
      v_swizzle_i32(d, d, x86::shuffleImm(1, 1, 0, 0));
      v_srl_i16(d, d, 8);
    }
  }

  BL_NOINLINE void xExtractUnpackedAFromPackedARGB32_4(const x86::Vec& d0, const x86::Vec& d1, const x86::Vec& s) noexcept {
    BL_ASSERT(d0.id() != d1.id());

    const BLCommonTable& c = blCommonTable;

    if (hasSSSE3()) {
      if (d0.id() == s.id()) {
        v_shuffle_i8(d1, s, simdConst(&c.pshufb_1xxx0xxxxxxxxxxx_to_z1z1z1z1z0z0z0z0, Bcst::kNA, d1));
        v_shuffle_i8(d0, s, simdConst(&c.pshufb_xxxxxxxx1xxx0xxx_to_z1z1z1z1z0z0z0z0, Bcst::kNA, d0));
      }
      else {
        v_shuffle_i8(d0, s, simdConst(&c.pshufb_xxxxxxxx1xxx0xxx_to_z1z1z1z1z0z0z0z0, Bcst::kNA, d0));
        v_shuffle_i8(d1, s, simdConst(&c.pshufb_1xxx0xxxxxxxxxxx_to_z1z1z1z1z0z0z0z0, Bcst::kNA, d1));
      }
    }
    else {
      if (d1.id() != s.id()) {
        v_swizzle_hi_i16(d1, s, x86::shuffleImm(3, 3, 1, 1));
        v_swizzle_lo_i16(d0, s, x86::shuffleImm(3, 3, 1, 1));

        v_swizzle_i32(d1, d1, x86::shuffleImm(3, 3, 2, 2));
        v_swizzle_i32(d0, d0, x86::shuffleImm(1, 1, 0, 0));

        v_srl_i16(d1, d1, 8);
        v_srl_i16(d0, d0, 8);
      }
      else {
        v_swizzle_lo_i16(d0, s, x86::shuffleImm(3, 3, 1, 1));
        v_swizzle_hi_i16(d1, s, x86::shuffleImm(3, 3, 1, 1));

        v_swizzle_i32(d0, d0, x86::shuffleImm(1, 1, 0, 0));
        v_swizzle_i32(d1, d1, x86::shuffleImm(3, 3, 2, 2));

        v_srl_i16(d0, d0, 8);
        v_srl_i16(d1, d1, 8);
      }
    }
  }

  BL_NOINLINE void xPackU32ToU16Lo(const x86::Vec& d0, const x86::Vec& s0) noexcept {
    if (hasSSE4_1()) {
      v_packs_i32_u16_(d0, s0, s0);
    }
    else if (hasSSSE3()) {
      v_shuffle_i8(d0, s0, simdConst(&blCommonTable.pshufb_xx76xx54xx32xx10_to_7654321076543210, Bcst::kNA, d0));
    }
    else {
      // Sign extend and then use `packssdw()`.
      v_sll_i32(d0, s0, 16);
      v_sra_i32(d0, d0, 16);
      v_packs_i32_i16(d0, d0, d0);
    }
  }

  BL_NOINLINE void xPackU32ToU16Lo(const VecArray& d0, const VecArray& s0) noexcept {
    for (uint32_t i = 0; i < d0.size(); i++)
      xPackU32ToU16Lo(d0[i], s0[i]);
  }

  #undef PACK_AVX_SSE
  #undef V_EMIT_VVVV_VVV
  #undef V_EMIT_VVVi_VVi
  #undef V_EMIT_VVVI_VVI
  #undef V_EMIT_VVV_VV
  #undef V_EMIT_VVi_VVi
  #undef V_EMIT_VVI_VVI
  #undef V_EMIT_VVI_VVI
  #undef V_EMIT_VVI_VI
  #undef V_EMIT_VV_VV
  #undef I_EMIT_3
  #undef I_EMIT_2i
  #undef I_EMIT_2
};

class PipeInjectAtTheEnd {
public:
  ScopedInjector _injector;

  BL_INLINE PipeInjectAtTheEnd(PipeCompiler* pc) noexcept
    : _injector(pc->cc, &pc->_funcEnd) {}
};

//! Provides unpacked global alpha mask; can be used by \ref FillPart and \ref CompOpPart as a global alpha abstraction.
class GlobalAlpha {
public:
  //! Pipeline compiler.
  PipeCompiler* _pc = nullptr;
  //! Node where to emit additional code in case `sm` is not initialized, but required.
  asmjit::BaseNode* _hook = nullptr;

  //! Global alpha as scalar (only used by scalar alpha-only processing operations).
  x86::Gp _sm;
  //! Unpacked global alpha as vector.
  x86::Vec _vm;

  BL_INLINE void initFromMem(PipeCompiler* pc, const x86::Mem& mem) noexcept {
    _pc = pc;
    _vm = pc->newVec("ga.vm");
    _pc->v_broadcast_u16(_vm, mem);
    _hook = pc->cc->cursor();
  }

  BL_INLINE void initFromVec(PipeCompiler* pc, const x86::Vec& vm) noexcept {
    _pc = pc;
    _hook = pc->cc->cursor();
    _vm = vm;
  }

  //! Returns whether global alpha is initialized and should be applied
  BL_INLINE bool isInitialized() const noexcept { return _hook != nullptr; }

  BL_NOINLINE const x86::Gp& sm() noexcept {
    if (_vm.isValid() && !_sm.isValid()) {
      ScopedInjector injector(_pc->cc, &_hook);
      _sm = _pc->cc->newUInt32("ga.sm");
      _pc->v_extract_u16(_sm, _vm, 0u);
    }

    return _sm;
  }

  BL_INLINE const x86::Vec& vm() const noexcept {
    return _vm;
  }
};

} // {JIT}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_PIPECOMPILER_P_H_INCLUDED
