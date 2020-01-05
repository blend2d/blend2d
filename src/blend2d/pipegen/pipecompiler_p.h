// [Blend2D]
// 2D Vector Graphics Powered by a JIT Compiler.
//
// [License]
// Zlib - See LICENSE.md file in the package.

#ifndef BLEND2D_PIPEGEN_PIPECOMPILER_P_H
#define BLEND2D_PIPEGEN_PIPECOMPILER_P_H

#include "../pipegen/pipegencore_p.h"
#include "../pipegen/piperegusage_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_pipegen
//! \{

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::PipeCompiler]
// ============================================================================

//! Pipeline compiler.
class PipeCompiler {
public:
  BL_NONCOPYABLE(PipeCompiler)

  //! AsmJit compiler.
  x86::Compiler* cc;
  //! Target CPU features.
  x86::Features _features;

  //! SIMD width.
  uint32_t _simdWidth;
  //! Number of registers available to the pipeline compiler.
  PipeRegUsage _availableRegs;
  //! Estimation of registers used by the pipeline temporarily.
  PipeRegUsage _temporaryRegs;
  //! Estimation of registers used by the pipeline permanently.
  PipeRegUsage _persistentRegs;

  //! Function node.
  asmjit::FuncNode* _funcNode;
  //! Function initialization hook.
  asmjit::BaseNode* _funcInit;
  //! Function end hook (to add 'unlikely' branches).
  asmjit::BaseNode* _funcEnd;

  //! Invalid GP register.
  x86::Gp _gpNone;
  //! Holds `BLPipeFillFunc::ctxData` argument.
  x86::Gp _ctxData;
  //! Holds `BLPipeFillFunc::fillData` argument.
  x86::Gp _fillData;
  //! Holds `BLPipeFillFunc::fetchData` argument.
  x86::Gp _fetchData;
  //! Temporary stack used to transfer SIMD regs to GP/MM.
  x86::Mem _tmpStack;

  //! Offset to get real ctx-data from the passed pointer.
  int _ctxDataOffset;
  //! Offset to get real fill-data from the passed pointer.
  int _fillDataOffset;
  //! Offset to get real fetch-data from the passed pointer.
  int _fetchDataOffset;

  //! Offset to the first constant to the `blCommonTable` global.
  int32_t _commonTableOff;
  //! Pointer to the `blCommonTable` constant pool (only used in 64-bit mode).
  x86::Gp _commonTablePtr;
  //! XMM constants.
  x86::Xmm _constantsXmm[4];

  // --------------------------------------------------------------------------
  // [PackedInst]
  // --------------------------------------------------------------------------

  //! Packing generic instructions and SSE+AVX instructions into a single 32-bit
  //! integer.
  //!
  //! AsmJit has around 1400 instructions for X86|X64, which means that we need
  //! at least 11 bits to represent each. Typically we need just one instruction
  //! ID at a time, however, since SSE and AVX instructions use different IDs
  //! we need a way to pack both SSE and AVX instruction ID into one integer as
  //! it's much easier to use unified instruction set rather than using specific
  //! paths for SSE and AVX code.
  //!
  //! PackedInst allows to specify the following:
  //!
  //!   - SSE instruction ID for up to SSE4.2 code generation.
  //!   - AVX instruction ID for AVX+ code generation.
  //!   - Maximum operation width aka 0 (XMM), 1 (YMM) and 2 (ZMM).
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

    static inline uint32_t packIntrin(uint32_t intrinId, uint32_t width = kWidthZ) noexcept {
      return (intrinId << kSseIdShift ) |
             (width    << kWidthShift ) |
             (1u       << kIntrinShift) ;
    }

    static inline uint32_t packAvxSse(uint32_t avxId, uint32_t sseId, uint32_t width = kWidthZ) noexcept {
      return (avxId << kAvxIdShift) |
             (sseId << kSseIdShift) |
             (width << kWidthShift) ;
    }

    static inline uint32_t avxId(uint32_t packedId) noexcept { return (packedId >> kAvxIdShift) & kAvxIdBits; }
    static inline uint32_t sseId(uint32_t packedId) noexcept { return (packedId >> kSseIdShift) & kSseIdBits; }
    static inline uint32_t width(uint32_t packedId) noexcept { return (packedId >> kWidthShift) & kWidthBits; }

    static inline uint32_t isIntrin(uint32_t packedId) noexcept { return (packedId & (kIntrinBits << kIntrinShift)) != 0; }
    static inline uint32_t intrinId(uint32_t packedId) noexcept { return (packedId >> kSseIdShift) & kSseIdBits; }
  };

  //! Intrinsic ID.
  //!
  //! Some operations are not available as a single instruction or are part
  //! of CPU extensions outside of the baseline instruction set. These are
  //! handled as intrinsics.
  enum IntrinId {
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

    kIntrin2VBroadcastU16,
    kIntrin2VBroadcastU32,
    kIntrin2VBroadcastU64,

    kIntrin2iVswizps,
    kIntrin2iVswizpd,

    kIntrin3Vcombhli64,
    kIntrin3Vcombhld64,
    kIntrin3Vminu16,
    kIntrin3Vmaxu16,
    kIntrin3Vmulu64x32,
    kIntrin3Vhaddpd,

    kIntrin4Vpblendvb,
    kIntrin4VpblendvbDestructive
  };

  enum {
    //! Number of reserved GP registers for general use.
    //!
    //! \note In 32-bit mode constants are absolutely addressed, however, in
    //! 64-bit mode we can't address arbitrary 64-bit pointers, so one more
    //! register is reserved as a compensation.
    kReservedGpRegs = 1 + uint32_t(BL_TARGET_ARCH_BITS >= 64),
    //! Number of spare MM registers to always reserve (not used).
    kReservedMmRegs = 0,
    //! Number of spare XMM|YMM|ZMM registers to always reserve.
    kReservedVecRegs = 1
  };

  // --------------------------------------------------------------------------
  // [Construction / Destruction]
  // --------------------------------------------------------------------------

  PipeCompiler(x86::Compiler* cc, const asmjit::x86::Features& features) noexcept;
  ~PipeCompiler() noexcept;

  // --------------------------------------------------------------------------
  // [Reset]
  // --------------------------------------------------------------------------

  void reset() noexcept;

  // --------------------------------------------------------------------------
  // [CPU SIMD Width]
  // --------------------------------------------------------------------------

  //! Returns the current SIMD width that this compiler and all its parts use.
  //!
  //! \note The returned width is in bytes.
  inline uint32_t simdWidth() const noexcept { return _simdWidth; }

  void initSimdWidth() noexcept;

  // --------------------------------------------------------------------------
  // [CPU Features]
  // --------------------------------------------------------------------------

  inline bool hasSSE2() const noexcept { return _features.hasSSE2(); }
  inline bool hasSSE3() const noexcept { return _features.hasSSE3(); }
  inline bool hasSSSE3() const noexcept { return _features.hasSSSE3(); }
  inline bool hasSSE4_1() const noexcept { return _features.hasSSE4_1(); }
  inline bool hasSSE4_2() const noexcept { return _features.hasSSE4_2(); }
  inline bool hasAVX() const noexcept { return _features.hasAVX(); }
  inline bool hasAVX2() const noexcept { return _features.hasAVX2(); }
  inline bool hasAVX512_F() const noexcept { return _features.hasAVX512_F(); }
  inline bool hasAVX512_BW() const noexcept { return _features.hasAVX512_BW(); }

  inline bool hasADX() const noexcept { return _features.hasADX(); }
  inline bool hasBMI() const noexcept { return _features.hasBMI(); }
  inline bool hasBMI2() const noexcept { return _features.hasBMI2(); }
  inline bool hasLZCNT() const noexcept { return _features.hasLZCNT(); }
  inline bool hasPOPCNT() const noexcept { return _features.hasPOPCNT(); }

  // --------------------------------------------------------------------------
  // [Data Offsets]
  // --------------------------------------------------------------------------

  inline int ctxDataOffset() const noexcept { return _ctxDataOffset; }
  inline int fillDataOffset() const noexcept { return _fillDataOffset; }
  inline int fetchDataOffset() const noexcept { return _fetchDataOffset; }

  inline void setCtxDataOffset(int offset) noexcept { _ctxDataOffset = offset; }
  inline void setFillDataOffset(int offset) noexcept { _fillDataOffset = offset; }
  inline void setFetchDataOffset(int offset) noexcept { _fetchDataOffset = offset; }

  // --------------------------------------------------------------------------
  // [Compilation]
  // --------------------------------------------------------------------------

  void beginFunction() noexcept;
  void endFunction() noexcept;

  // --------------------------------------------------------------------------
  // [Parts Management]
  // --------------------------------------------------------------------------

  // TODO: [PIPEGEN] There should be a getter on asmjit side that will return
  // the `ZoneAllocator` object that can be used for these kind of purposes.
  // It doesn't make sense to create another ZoneAllocator.
  template<typename T>
  inline T* newPartT() noexcept {
    void* p = cc->_codeZone.alloc(sizeof(T), 8);
    if (BL_UNLIKELY(!p))
      return nullptr;
    return new(p) T(this);
  }

  template<typename T, typename... Args>
  inline T* newPartT(Args&&... args) noexcept {
    void* p = cc->_codeZone.alloc(sizeof(T), 8);
    if (BL_UNLIKELY(!p))
      return nullptr;
    return new(p) T(this, std::forward<Args>(args)...);
  }

  FillPart* newFillPart(uint32_t fillType, FetchPart* dstPart, CompOpPart* compOpPart) noexcept;
  FetchPart* newFetchPart(uint32_t fetchType, uint32_t fetchPayload, uint32_t format) noexcept;
  CompOpPart* newCompOpPart(uint32_t compOp, FetchPart* dstPart, FetchPart* srcPart) noexcept;

  // --------------------------------------------------------------------------
  // [Init]
  // --------------------------------------------------------------------------

  void initPipeline(PipePart* root) noexcept;

  void onPreInitPart(PipePart* part) noexcept;
  void onPostInitPart(PipePart* part) noexcept;

  //! Generate a function of the given `signature`.
  void compileFunc(uint32_t signature) noexcept;

  // --------------------------------------------------------------------------
  // [Miscellaneous]
  // --------------------------------------------------------------------------

  BL_INLINE void rename(const OpArray& opArray, const char* name) noexcept {
    for (uint32_t i = 0; i < opArray.size(); i++)
      cc->rename(opArray[i].as<asmjit::BaseReg>(), "%s%u", name, unsigned(i));
  }

  // --------------------------------------------------------------------------
  // [Constants]
  // --------------------------------------------------------------------------

  void _initCommonTablePtr() noexcept;

  x86::Mem constAsMem(const void* c) noexcept;
  x86::Xmm constAsXmm(const void* c) noexcept;

  // --------------------------------------------------------------------------
  // [Registers / Memory]
  // --------------------------------------------------------------------------

  BL_NOINLINE void newRegArray(OpArray& dst, uint32_t n, uint32_t regType, const char* name) noexcept {
    BL_ASSERT(n <= OpArray::kMaxSize);
    dst._size = n;
    for (uint32_t i = 0; i < n; i++) {
      cc->_newRegFmt(dst[i].as<asmjit::BaseReg>(), regType, "%s%i", name, i);
    }
  }

  BL_INLINE void newXmmArray(OpArray& dst, uint32_t n, const char* name) noexcept {
    newRegArray(dst, n, asmjit::x86::Reg::kTypeXmm, name);
  }

  BL_INLINE void newYmmArray(OpArray& dst, uint32_t n, const char* name) noexcept {
    newRegArray(dst, n, asmjit::x86::Reg::kTypeYmm, name);
  }

  x86::Mem tmpStack(uint32_t size) noexcept;

  // --------------------------------------------------------------------------
  // [Emit - Commons]
  // --------------------------------------------------------------------------

  // Emit helpers used by GP.
  void iemit2(uint32_t instId, const Operand_& op1, int imm) noexcept;
  void iemit2(uint32_t instId, const Operand_& op1, const Operand_& op2) noexcept;
  void iemit3(uint32_t instId, const Operand_& op1, const Operand_& op2, int imm) noexcept;

  // Emit helpers to emit MOVE from SrcT to DstT, used by pre-AVX instructions.
  // The `width` parameter is important as it describes how many bytes to read
  // in case that `src` is a memory location. It's important as some instructions
  // like PMOVZXBW read only 8 bytes, but to make the same operation in pre-SSE4.1
  // code we need to read 8 bytes from memory and use PUNPCKLBW to interleave that
  // bytes with zero. PUNPCKLBW would read 16 bytes from memory and would require
  // them to be aligned to 16 bytes, if used with memory operand.
  void vemit_xmov(const Operand_& dst, const Operand_& src, uint32_t width) noexcept;
  void vemit_xmov(const OpArray& dst, const Operand_& src, uint32_t width) noexcept;
  void vemit_xmov(const OpArray& dst, const OpArray& src, uint32_t width) noexcept;

  // Emit helpers used by SSE|AVX intrinsics.
  void vemit_vv_vv(uint32_t packedId, const Operand_& dst_, const Operand_& src_) noexcept;
  void vemit_vv_vv(uint32_t packedId, const OpArray& dst_, const Operand_& src_) noexcept;
  void vemit_vv_vv(uint32_t packedId, const OpArray& dst_, const OpArray& src_) noexcept;

  void vemit_vvi_vi(uint32_t packedId, const Operand_& dst_, const Operand_& src_, uint32_t imm) noexcept;
  void vemit_vvi_vi(uint32_t packedId, const OpArray& dst_, const Operand_& src_, uint32_t imm) noexcept;
  void vemit_vvi_vi(uint32_t packedId, const OpArray& dst_, const OpArray& src_, uint32_t imm) noexcept;

  void vemit_vvi_vvi(uint32_t packedId, const Operand_& dst_, const Operand_& src_, uint32_t imm) noexcept;
  void vemit_vvi_vvi(uint32_t packedId, const OpArray& dst_, const Operand_& src_, uint32_t imm) noexcept;
  void vemit_vvi_vvi(uint32_t packedId, const OpArray& dst_, const OpArray& src_, uint32_t imm) noexcept;

  void vemit_vvv_vv(uint32_t packedId, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_) noexcept;
  void vemit_vvv_vv(uint32_t packedId, const OpArray& dst_, const Operand_& src1_, const OpArray& src2_) noexcept;
  void vemit_vvv_vv(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const Operand_& src2_) noexcept;
  void vemit_vvv_vv(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_) noexcept;

  void vemit_vvvi_vvi(uint32_t packedId, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_, uint32_t imm) noexcept;
  void vemit_vvvi_vvi(uint32_t packedId, const OpArray& dst_, const Operand_& src1_, const OpArray& src2_, uint32_t imm) noexcept;
  void vemit_vvvi_vvi(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const Operand_& src2_, uint32_t imm) noexcept;
  void vemit_vvvi_vvi(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_, uint32_t imm) noexcept;

  void vemit_vvvv_vvv(uint32_t packedId, const Operand_& dst_, const Operand_& src1_, const Operand_& src2_, const Operand_& src3_) noexcept;
  void vemit_vvvv_vvv(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_, const Operand_& src3_) noexcept;
  void vemit_vvvv_vvv(uint32_t packedId, const OpArray& dst_, const OpArray& src1_, const OpArray& src2_, const OpArray& src3_) noexcept;

  #define I_EMIT_2(NAME, INST_ID)                                             \
  template<typename Op1, typename Op2>                                        \
  inline void NAME(const Op1& o1,                                             \
                   const Op2& o2) noexcept {                                  \
    iemit2(x86::Inst::kId##INST_ID, o1, o2);                                  \
  }

  #define I_EMIT_3(NAME, INST_ID)                                             \
  template<typename Op1, typename Op2, typename Op3>                          \
  inline void NAME(const Op1& o1,                                             \
                   const Op2& o2,                                             \
                   const Op3& o3) noexcept {                                  \
    iemit3(x86::Inst::kId##INST_ID, o1, o2, o3);                              \
  }

  #define V_EMIT_VV_VV(NAME, PACKED_ID)                                       \
  template<typename DstT, typename SrcT>                                      \
  inline void NAME(const DstT& dst,                                           \
                   const SrcT& src) noexcept {                                \
    vemit_vv_vv(PACKED_ID, dst, src);                                         \
  }

  #define V_EMIT_VVI_VI(NAME, PACKED_ID)                                      \
  template<typename DstT, typename SrcT>                                      \
  inline void NAME(const DstT& dst,                                           \
                   const SrcT& src,                                           \
                   uint32_t imm) noexcept {                                   \
    vemit_vvi_vi(PACKED_ID, dst, src, imm);                                   \
  }

  #define V_EMIT_VVI_VVI(NAME, PACKED_ID)                                     \
  template<typename DstT, typename SrcT>                                      \
  inline void NAME(const DstT& dst,                                           \
                   const SrcT& src,                                           \
                   uint32_t imm) noexcept {                                   \
    vemit_vvi_vvi(PACKED_ID, dst, src, imm);                                  \
  }

  #define V_EMIT_VVi_VVi(NAME, PACKED_ID, IMM_VALUE)                          \
  template<typename DstT, typename SrcT>                                      \
  inline void NAME(const DstT& dst,                                           \
                   const SrcT& src) noexcept {                                \
    vemit_vvi_vvi(PACKED_ID, dst, src, IMM_VALUE);                            \
  }

  #define V_EMIT_VVV_VV(NAME, PACKED_ID)                                      \
  template<typename DstT, typename Src1T, typename Src2T>                     \
  inline void NAME(const DstT& dst,                                           \
                   const Src1T& src1,                                         \
                   const Src2T& src2) noexcept {                              \
    vemit_vvv_vv(PACKED_ID, dst, src1, src2);                                 \
  }

  #define V_EMIT_VVVI_VVI(NAME, PACKED_ID)                                    \
  template<typename DstT, typename Src1T, typename Src2T>                     \
  inline void NAME(const DstT& dst,                                           \
                   const Src1T& src1,                                         \
                   const Src2T& src2,                                         \
                   uint32_t imm) noexcept {                                   \
    vemit_vvvi_vvi(PACKED_ID, dst, src1, src2, imm);                          \
  }

  #define V_EMIT_VVVi_VVi(NAME, PACKED_ID, IMM_VALUE)                         \
  template<typename DstT, typename Src1T, typename Src2T>                     \
  inline void NAME(const DstT& dst,                                           \
                   const Src1T& src1,                                         \
                   const Src2T& src2) noexcept {                              \
    vemit_vvvi_vvi(PACKED_ID, dst, src1, src2, IMM_VALUE);                    \
  }

  #define V_EMIT_VVVV_VVV(NAME, PACKED_ID)                                    \
  template<typename DstT, typename Src1T, typename Src2T, typename Src3T>     \
  inline void NAME(const DstT& dst,                                           \
                   const Src1T& src1,                                         \
                   const Src2T& src2,                                         \
                   const Src3T& src3) noexcept {                              \
    vemit_vvvv_vvv(PACKED_ID, dst, src1, src2, src3);                         \
  }

  #define PACK_AVX_SSE(AVX_ID, SSE_ID, W) \
    PackedInst::packAvxSse(x86::Inst::kId##AVX_ID, x86::Inst::kId##SSE_ID, PackedInst::kWidth##W)

  // --------------------------------------------------------------------------
  // [Emit - 'I' General Purpose Instructions]
  // --------------------------------------------------------------------------

  BL_NOINLINE void load8(const x86::Gp& dst, const x86::Mem& src) noexcept {
    x86::Mem src8 = src;
    src8.setSize(1);
    cc->movzx(dst.r32(), src8);
  }

  BL_NOINLINE void load16(const x86::Gp& dst, const x86::Mem& src) noexcept {
    x86::Mem src16 = src;
    src16.setSize(2);
    cc->movzx(dst.r32(), src16);
  }

  BL_NOINLINE void store8(const x86::Mem& dst, const x86::Gp& src) noexcept {
    x86::Mem dst8 = dst;
    dst8.setSize(1);
    cc->mov(dst8, src.r8());
  }

  BL_NOINLINE void store16(const x86::Mem& dst, const x86::Gp& src) noexcept {
    x86::Mem dst16 = dst;
    dst16.setSize(2);
    cc->mov(dst16, src.r16());
  }

  BL_INLINE void uMov(const x86::Gp& dst, const x86::Gp& src) noexcept {
    cc->mov(dst, src);
  }

  BL_INLINE void uMov(const x86::Gp& dst, const x86::Mem& src) noexcept {
    cc->mov(dst, src);
  }

  //! dst = src1 + src2.
  BL_NOINLINE void uAdd(const x86::Gp& dst, const x86::Gp& src1, const x86::Gp& src2) noexcept {
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
  BL_NOINLINE void uAdd(const x86::Gp& dst, const x86::Gp& src1, const x86::Mem& src2) noexcept {
    if (dst.id() != src1.id())
      cc->mov(dst, src1);
    cc->add(dst, src2);
  }

  //! dst = src1 + [src2].
  BL_NOINLINE void uAdd(const x86::Gp& dst, const x86::Gp& src1, const Imm& src2) noexcept {
    if (dst.id() != src1.id() && src2.isInt32()) {
      cc->lea(dst, x86::ptr(src1, src2.i32()));
    }
    else {
      if (dst.id() != src1.id())
        cc->mov(dst, src1);
      cc->add(dst, src2);
    }
  }

  BL_NOINLINE void uAddsU8(const x86::Gp& dst, const x86::Gp& src1, const x86::Gp& src2) noexcept {
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
  BL_NOINLINE void uSub(const x86::Gp& dst, const x86::Gp& src1, const x86::Gp& src2) noexcept {
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
  BL_NOINLINE void uSub(const x86::Gp& dst, const x86::Gp& src1, const x86::Mem& src2) noexcept {
    if (dst.id() != src1.id())
      cc->mov(dst, src1);
    cc->sub(dst, src2);
  }

  //! dst = src1 + [src2].
  BL_NOINLINE void uSub(const x86::Gp& dst, const x86::Gp& src1, const Imm& src2) noexcept {
    if (dst.id() != src1.id())
      cc->mov(dst, src1);
    cc->sub(dst, src2);
  }

  //! dst = src1 * src2.
  BL_NOINLINE void uMul(const x86::Gp& dst, const x86::Gp& src1, const x86::Gp& src2) noexcept {
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
  BL_NOINLINE void uMul(const x86::Gp& dst, const x86::Gp& src1, const x86::Mem& src2) noexcept {
    BL_ASSERT(dst.size() == src1.size());
    BL_ASSERT(dst.size() == src2.size());

    if (dst.id() != src1.id())
      cc->mov(dst, src1);
    cc->imul(dst, src2);
  }

  //! dst = src1 * src2.
  BL_NOINLINE void uMul(const x86::Gp& dst, const x86::Gp& src1, int src2) noexcept {
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

  BL_NOINLINE void uInv8(const x86::Gp& dst, const x86::Gp& src) noexcept {
    if (dst.id() != src.id())
      cc->mov(dst, src);
    cc->xor_(dst.r8(), 0xFF);
  }

  //! Integer division by 255 with correct rounding semantics.
  BL_NOINLINE void uDiv255(const x86::Gp& dst, const x86::Gp& src) noexcept {
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

  BL_NOINLINE void uMul257hu16(const x86::Gp& dst, const x86::Gp& src) {
    BL_ASSERT(dst.size() == src.size());
    cc->imul(dst, src, 257);
    cc->shr(dst, 16);
  }

  template<typename A, typename B>
  BL_NOINLINE void uZeroIfEq(const A& a, const B& b) noexcept {
    Label L = cc->newLabel();

    cc->cmp(a, b);
    cc->jne(L);
    cc->mov(a, 0);
    cc->bind(L);
  }

  BL_NOINLINE void uJumpIfNotOpaqueMask(const x86::Gp& msk, const Label& target) noexcept {
    cc->cmp(msk.r8(), 255);
    cc->jnz(target);
  }

  // dst = abs(src)
  template<typename DstT, typename SrcT>
  BL_NOINLINE void uAbs(const DstT& dst, const SrcT& src) noexcept {
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
  BL_NOINLINE void uBound0ToN(const DstT& dst, const ValueT& value, const LimitT& limit) noexcept {
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
  BL_NOINLINE void uReflect(const DstT& dst, const SrcT& src) noexcept {
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
  BL_NOINLINE void uMod(const DstT& dst, const SrcT& src) noexcept {
    x86::Gp mod = cc->newSimilarReg(dst, "@mod");

    cc->xor_(mod, mod);
    cc->div(mod, dst, src);
    cc->mov(dst, mod);
  }

  BL_NOINLINE void uAdvanceAndDecrement(const x86::Gp& p, int pAdd, const x86::Gp& i, int iDec) noexcept {
    cc->add(p, pAdd);
    cc->sub(i, iDec);
  }

  //! dst += a * b.
  BL_NOINLINE void uAddMulImm(const x86::Gp& dst, const x86::Gp& a, int b) noexcept {
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

  BL_NOINLINE void uLeaBpp(const x86::Gp& dst, const x86::Gp& src_, const x86::Gp& idx_, uint32_t scale, int32_t disp = 0) noexcept {
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

  BL_NOINLINE void uShl(const x86::Gp& dst, const x86::Gp& src1, const x86::Gp& src2) noexcept {
    if (hasBMI2()) {
      cc->shlx(dst, src1, src2.cloneAs(dst));
    }
    else {
      if (dst.id() != src1.id())
        cc->mov(dst, src1);
      cc->shl(dst, src2.r8());
    }
  }

  BL_NOINLINE void uShl(const x86::Gp& dst, const x86::Gp& src1, const Imm& src2) noexcept {
    if (dst.id() != src1.id())
      cc->mov(dst, src1);
    cc->shl(dst, src2);
  }

  BL_NOINLINE void uShr(const x86::Gp& dst, const x86::Gp& src1, const x86::Gp& src2) noexcept {
    if (hasBMI2()) {
      cc->shrx(dst, src1, src2.cloneAs(dst));
    }
    else {
      if (dst.id() != src1.id())
        cc->mov(dst, src1);
      cc->shr(dst, src2.r8());
    }
  }

  BL_NOINLINE void uShr(const x86::Gp& dst, const x86::Gp& src1, const Imm& src2) noexcept {
    if (dst.id() != src1.id())
      cc->mov(dst, src1);
    cc->shr(dst, src2);
  }

  inline void uCTZ(const Operand& dst, const Operand& src) noexcept {
    // INTEL - No difference, `bsf` and `tzcnt` both have latency ~2.5 cycles.
    // AMD   - Big difference, `tzcnt` has only ~1.5 cycle latency while `bsf` has ~2.5 cycles.
    cc->emit(hasBMI() ? x86::Inst::kIdTzcnt : x86::Inst::kIdBsf, dst, src);
  }

  inline void uTest(const x86::Gp& ptr, uint32_t mask) noexcept {
    if (mask <= 0xFF && cc->is64Bit()) {
      // Shorter, but limits as to use AL|BL|CL|DL, so we don't wanna use this
      // construct in 32-bit mode as it would limit the register allocator.
      cc->test(ptr.r8(), mask);
    }
    else {
      cc->test(ptr, mask);
    }
  }

  inline void uPrefetch(const x86::Mem& mem) noexcept {
    cc->prefetcht0(mem);
  }

  // --------------------------------------------------------------------------
  // [Emit - 'V' Vector Instructions (128..512-bit SSE|AVX|AVX512)]
  // --------------------------------------------------------------------------

  // To make the code generation easier and more parametrizable we support both
  // SSE|AVX through the same interface (always non-destructive source form) and
  // each intrinsic can accept either `Operand_` or `OpArray`, which can hold up
  // to 4 registers to form scalars, pairs and quads. Each 'V' instruction maps
  // directly to the ISA so check the optimization level before using them or use
  // instructions starting with 'x' that are generic and designed to map to the
  // best instruction(s) possible.
  //
  // Also, multiple overloads are provided for convenience, similarly to AsmJit
  // design, we don't want to inline expansion of `OpArray(op)` here so these
  // overloads are implemented in pipecompiler.cpp.

  // SSE instructions that require SSE3+ are suffixed with `_` to make it clear
  // that they are not part of the baseline instruction set. Some instructions
  // like that are always provided don't have such suffix, and will be emulated

  // Integer SIMD - Core.

  V_EMIT_VV_VV(vmov          , PACK_AVX_SSE(Vmovaps    , Movaps    , Z))       // AVX  | SSE2
  V_EMIT_VV_VV(vmov64        , PACK_AVX_SSE(Vmovq      , Movq      , X))       // AVX  | SSE2

  V_EMIT_VV_VV(vmovi8i16_    , PACK_AVX_SSE(Vpmovsxbw  , Pmovsxbw  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(vmovu8u16_    , PACK_AVX_SSE(Vpmovzxbw  , Pmovzxbw  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(vmovi8i32_    , PACK_AVX_SSE(Vpmovsxbd  , Pmovsxbd  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(vmovu8u32_    , PACK_AVX_SSE(Vpmovzxbd  , Pmovzxbd  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(vmovi8i64_    , PACK_AVX_SSE(Vpmovsxbq  , Pmovsxbq  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(vmovu8u64_    , PACK_AVX_SSE(Vpmovzxbq  , Pmovzxbq  , Z))       // AVX2 | SSE4.1

  V_EMIT_VV_VV(vmovi16i32_   , PACK_AVX_SSE(Vpmovsxwd  , Pmovsxwd  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(vmovu16u32_   , PACK_AVX_SSE(Vpmovzxwd  , Pmovzxwd  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(vmovi16i64_   , PACK_AVX_SSE(Vpmovsxwq  , Pmovsxwq  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(vmovu16u64_   , PACK_AVX_SSE(Vpmovzxwq  , Pmovzxwq  , Z))       // AVX2 | SSE4.1

  V_EMIT_VV_VV(vmovi32i64_   , PACK_AVX_SSE(Vpmovsxdq  , Pmovsxdq  , Z))       // AVX2 | SSE4.1
  V_EMIT_VV_VV(vmovu32u64_   , PACK_AVX_SSE(Vpmovzxdq  , Pmovzxdq  , Z))       // AVX2 | SSE4.1

  V_EMIT_VV_VV(vmovmsku8     , PACK_AVX_SSE(Vpmovmskb  , Pmovmskb  , Z))       // AVX2 | SSE2

  V_EMIT_VVVI_VVI(vinsertu8_ , PACK_AVX_SSE(Vpinsrb    , Pinsrb    , X))       // AVX2 | SSE4_1
  V_EMIT_VVVI_VVI(vinsertu16 , PACK_AVX_SSE(Vpinsrw    , Pinsrw    , X))       // AVX2 | SSE2
  V_EMIT_VVVI_VVI(vinsertu32_, PACK_AVX_SSE(Vpinsrd    , Pinsrd    , X))       // AVX2 | SSE4_1
  V_EMIT_VVVI_VVI(vinsertu64_, PACK_AVX_SSE(Vpinsrq    , Pinsrq    , X))       // AVX2 | SSE4_1

  V_EMIT_VVI_VVI(vextractu8_ , PACK_AVX_SSE(Vpextrb    , Pextrb    , X))       // AVX2 | SSE4_1
  V_EMIT_VVI_VVI(vextractu16 , PACK_AVX_SSE(Vpextrw    , Pextrw    , X))       // AVX2 | SSE2
  V_EMIT_VVI_VVI(vextractu32_, PACK_AVX_SSE(Vpextrd    , Pextrd    , X))       // AVX2 | SSE4_1
  V_EMIT_VVI_VVI(vextractu64_, PACK_AVX_SSE(Vpextrq    , Pextrq    , X))       // AVX2 | SSE4_1

  V_EMIT_VVV_VV(vunpackli8   , PACK_AVX_SSE(Vpunpcklbw , Punpcklbw , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vunpackhi8   , PACK_AVX_SSE(Vpunpckhbw , Punpckhbw , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vunpackli16  , PACK_AVX_SSE(Vpunpcklwd , Punpcklwd , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vunpackhi16  , PACK_AVX_SSE(Vpunpckhwd , Punpckhwd , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vunpackli32  , PACK_AVX_SSE(Vpunpckldq , Punpckldq , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vunpackhi32  , PACK_AVX_SSE(Vpunpckhdq , Punpckhdq , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vunpackli64  , PACK_AVX_SSE(Vpunpcklqdq, Punpcklqdq, Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vunpackhi64  , PACK_AVX_SSE(Vpunpckhqdq, Punpckhqdq, Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(vpacki32i16  , PACK_AVX_SSE(Vpackssdw  , Packssdw  , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vpacki32u16_ , PACK_AVX_SSE(Vpackusdw  , Packusdw  , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(vpacki16i8   , PACK_AVX_SSE(Vpacksswb  , Packsswb  , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vpacki16u8   , PACK_AVX_SSE(Vpackuswb  , Packuswb  , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(vswizi8v_    , PACK_AVX_SSE(Vpshufb    , Pshufb    , Z))       // AVX2 | SSSE3
  V_EMIT_VVI_VVI(vswizli16   , PACK_AVX_SSE(Vpshuflw   , Pshuflw   , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VVI(vswizhi16   , PACK_AVX_SSE(Vpshufhw   , Pshufhw   , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VVI(vswizi32    , PACK_AVX_SSE(Vpshufd    , Pshufd    , Z))       // AVX2 | SSE2

  V_EMIT_VVVI_VVI(vshufi32   , PACK_AVX_SSE(Vshufps    , Shufps    , Z))       // AVX  | SSE
  V_EMIT_VVVI_VVI(vshufi64   , PACK_AVX_SSE(Vshufpd    , Shufpd    , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(vand         , PACK_AVX_SSE(Vpand      , Pand      , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vandnot_a    , PACK_AVX_SSE(Vpandn     , Pandn     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vor          , PACK_AVX_SSE(Vpor       , Por       , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vxor         , PACK_AVX_SSE(Vpxor      , Pxor      , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(vavgu8       , PACK_AVX_SSE(Vpavgb     , Pavgb     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vavgu16      , PACK_AVX_SSE(Vpavgw     , Pavgw     , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(vsigni8_     , PACK_AVX_SSE(Vpsignb    , Psignb    , Z))       // AVX2 | SSSE3
  V_EMIT_VVV_VV(vsigni16_    , PACK_AVX_SSE(Vpsignw    , Psignw    , Z))       // AVX2 | SSSE3
  V_EMIT_VVV_VV(vsigni32_    , PACK_AVX_SSE(Vpsignd    , Psignd    , Z))       // AVX2 | SSSE3

  V_EMIT_VVV_VV(vaddi8       , PACK_AVX_SSE(Vpaddb     , Paddb     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vaddi16      , PACK_AVX_SSE(Vpaddw     , Paddw     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vaddi32      , PACK_AVX_SSE(Vpaddd     , Paddd     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vaddi64      , PACK_AVX_SSE(Vpaddq     , Paddq     , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(vaddsi8      , PACK_AVX_SSE(Vpaddsb    , Paddsb    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vaddsu8      , PACK_AVX_SSE(Vpaddusb   , Paddusb   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vaddsi16     , PACK_AVX_SSE(Vpaddsw    , Paddsw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vaddsu16     , PACK_AVX_SSE(Vpaddusw   , Paddusw   , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(vsubi8       , PACK_AVX_SSE(Vpsubb     , Psubb     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vsubi16      , PACK_AVX_SSE(Vpsubw     , Psubw     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vsubi32      , PACK_AVX_SSE(Vpsubd     , Psubd     , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vsubi64      , PACK_AVX_SSE(Vpsubq     , Psubq     , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(vsubsi8      , PACK_AVX_SSE(Vpsubsb    , Psubsb    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vsubsi16     , PACK_AVX_SSE(Vpsubsw    , Psubsw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vsubsu8      , PACK_AVX_SSE(Vpsubusb   , Psubusb   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vsubsu16     , PACK_AVX_SSE(Vpsubusw   , Psubusw   , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(vmuli16      , PACK_AVX_SSE(Vpmullw    , Pmullw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vmulu16      , PACK_AVX_SSE(Vpmullw    , Pmullw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vmulhi16     , PACK_AVX_SSE(Vpmulhw    , Pmulhw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vmulhu16     , PACK_AVX_SSE(Vpmulhuw   , Pmulhuw   , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(vmuli32_     , PACK_AVX_SSE(Vpmulld    , Pmulld    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(vmulu32_     , PACK_AVX_SSE(Vpmulld    , Pmulld    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(vmulxlli32_  , PACK_AVX_SSE(Vpmuldq    , Pmuldq    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(vmulxllu32   , PACK_AVX_SSE(Vpmuludq   , Pmuludq   , Z))       // AVX2 | SSE2

  V_EMIT_VVVi_VVi(vmulxllu64_, PACK_AVX_SSE(Vpclmulqdq , Pclmulqdq , Z), 0x00) // AVX2 | PCLMULQDQ
  V_EMIT_VVVi_VVi(vmulxhlu64_, PACK_AVX_SSE(Vpclmulqdq , Pclmulqdq , Z), 0x01) // AVX2 | PCLMULQDQ
  V_EMIT_VVVi_VVi(vmulxlhu64_, PACK_AVX_SSE(Vpclmulqdq , Pclmulqdq , Z), 0x10) // AVX2 | PCLMULQDQ
  V_EMIT_VVVi_VVi(vmulxhhu64_, PACK_AVX_SSE(Vpclmulqdq , Pclmulqdq , Z), 0x11) // AVX2 | PCLMULQDQ

  V_EMIT_VVV_VV(vmini8_      , PACK_AVX_SSE(Vpminsb    , Pminsb    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(vmaxi8_      , PACK_AVX_SSE(Vpmaxsb    , Pmaxsb    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(vminu8       , PACK_AVX_SSE(Vpminub    , Pminub    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vmaxu8       , PACK_AVX_SSE(Vpmaxub    , Pmaxub    , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(vmini16      , PACK_AVX_SSE(Vpminsw    , Pminsw    , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vmaxi16      , PACK_AVX_SSE(Vpmaxsw    , Pmaxsw    , Z))       // AVX2 | SSE2

  V_EMIT_VVV_VV(vmini32_     , PACK_AVX_SSE(Vpminsd    , Pminsd    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(vmaxi32_     , PACK_AVX_SSE(Vpmaxsd    , Pmaxsd    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(vminu32_     , PACK_AVX_SSE(Vpminud    , Pminud    , Z))       // AVX2 | SSE4.1
  V_EMIT_VVV_VV(vmaxu32_     , PACK_AVX_SSE(Vpmaxud    , Pmaxud    , Z))       // AVX2 | SSE4.1

  V_EMIT_VVV_VV(vcmpeqi8     , PACK_AVX_SSE(Vpcmpeqb   , Pcmpeqb   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vcmpeqi16    , PACK_AVX_SSE(Vpcmpeqw   , Pcmpeqw   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vcmpeqi32    , PACK_AVX_SSE(Vpcmpeqd   , Pcmpeqd   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vcmpeqi64_   , PACK_AVX_SSE(Vpcmpeqq   , Pcmpeqq   , Z))       // AVX2 | SSE4.1

  V_EMIT_VVV_VV(vcmpgti8     , PACK_AVX_SSE(Vpcmpgtb   , Pcmpgtb   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vcmpgti16    , PACK_AVX_SSE(Vpcmpgtw   , Pcmpgtw   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vcmpgti32    , PACK_AVX_SSE(Vpcmpgtd   , Pcmpgtd   , Z))       // AVX2 | SSE2
  V_EMIT_VVV_VV(vcmpgti64_   , PACK_AVX_SSE(Vpcmpgtq   , Pcmpgtq   , Z))       // AVX2 | SSE4.2

  V_EMIT_VVI_VI(vslli16      , PACK_AVX_SSE(Vpsllw     , Psllw     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(vsrli16      , PACK_AVX_SSE(Vpsrlw     , Psrlw     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(vsrai16      , PACK_AVX_SSE(Vpsraw     , Psraw     , Z))       // AVX2 | SSE2

  V_EMIT_VVI_VI(vslli32      , PACK_AVX_SSE(Vpslld     , Pslld     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(vsrli32      , PACK_AVX_SSE(Vpsrld     , Psrld     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(vsrai32      , PACK_AVX_SSE(Vpsrad     , Psrad     , Z))       // AVX2 | SSE2

  V_EMIT_VVI_VI(vslli64      , PACK_AVX_SSE(Vpsllq     , Psllq     , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(vsrli64      , PACK_AVX_SSE(Vpsrlq     , Psrlq     , Z))       // AVX2 | SSE2

  V_EMIT_VVI_VI(vslli128b    , PACK_AVX_SSE(Vpslldq    , Pslldq    , Z))       // AVX2 | SSE2
  V_EMIT_VVI_VI(vsrli128b    , PACK_AVX_SSE(Vpsrldq    , Psrldq    , Z))       // AVX2 | SSE2

  V_EMIT_VVVV_VVV(vblendv8_  , PACK_AVX_SSE(Vpblendvb  , Pblendvb  , Z))       // AVX2 | SSE4.1
  V_EMIT_VVVI_VVI(vblend16_  , PACK_AVX_SSE(Vpblendw   , Pblendw   , Z))       // AVX2 | SSE4.1

  V_EMIT_VVV_VV(vhaddi16_    , PACK_AVX_SSE(Vphaddw    , Phaddw    , Z))       // AVX2 | SSSE3
  V_EMIT_VVV_VV(vhaddi32_    , PACK_AVX_SSE(Vphaddd    , Phaddd    , Z))       // AVX2 | SSSE3

  V_EMIT_VVV_VV(vhsubi16_    , PACK_AVX_SSE(Vphsubw    , Phsubw    , Z))       // AVX2 | SSSE3
  V_EMIT_VVV_VV(vhsubi32_    , PACK_AVX_SSE(Vphsubd    , Phsubd    , Z))       // AVX2 | SSSE3

  V_EMIT_VVV_VV(vhaddsi16_   , PACK_AVX_SSE(Vphaddsw   , Phaddsw   , Z))       // AVX2 | SSSE3
  V_EMIT_VVV_VV(vhsubsi16_   , PACK_AVX_SSE(Vphsubsw   , Phsubsw   , Z))       // AVX2 | SSSE3

  // Integer SIMD - Miscellaneous.

  V_EMIT_VV_VV(vtest_        , PACK_AVX_SSE(Vptest     , Ptest     , Z))       // AVX2 | SSE4_1

  // Integer SIMD - Consult X86 manual before using these...

  V_EMIT_VVV_VV(vsadu8       , PACK_AVX_SSE(Vpsadbw    , Psadbw    , Z))       // AVX2 | SSE2      [dst.u64[0..X] = SUM{0.7}(ABS(src1.u8[N] - src2.u8[N]))))]
  V_EMIT_VVV_VV(vmulrhi16_   , PACK_AVX_SSE(Vpmulhrsw  , Pmulhrsw  , Z))       // AVX2 | SSSE3     [dst.i16[0..X] = ((((src1.i16[0] * src2.i16[0])) >> 14)) + 1)) >> 1))]
  V_EMIT_VVV_VV(vmaddsu8i8_  , PACK_AVX_SSE(Vpmaddubsw , Pmaddubsw , Z))       // AVX2 | SSSE3     [dst.i16[0..X] = SAT(src1.u8[0] * src2.i8[0] + src1.u8[1] * src2.i8[1]))
  V_EMIT_VVV_VV(vmaddi16     , PACK_AVX_SSE(Vpmaddwd   , Pmaddwd   , Z))       // AVX2 | SSE2      [dst.i32[0..X] = (src1.i16[0] * src2.i16[0] + src1.i16[1] * src2.i16[1]))
  V_EMIT_VVVI_VVI(vmpsadu8_  , PACK_AVX_SSE(Vmpsadbw   , Mpsadbw   , Z))       // AVX2 | SSE4.1
  V_EMIT_VVVI_VVI(valignr8_  , PACK_AVX_SSE(Vpalignr   , Palignr   , Z))       // AVX2 | SSSE3
  V_EMIT_VV_VV(vhminposu16_  , PACK_AVX_SSE(Vphminposuw, Phminposuw, Z))       // AVX2 | SSE4_1

  // Floating Point - Core.

  V_EMIT_VV_VV(vmovaps       , PACK_AVX_SSE(Vmovaps    , Movaps    , Z))       // AVX  | SSE
  V_EMIT_VV_VV(vmovapd       , PACK_AVX_SSE(Vmovapd    , Movapd    , Z))       // AVX  | SSE2
  V_EMIT_VV_VV(vmovups       , PACK_AVX_SSE(Vmovups    , Movups    , Z))       // AVX  | SSE
  V_EMIT_VV_VV(vmovupd       , PACK_AVX_SSE(Vmovupd    , Movupd    , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(vmovlps2x    , PACK_AVX_SSE(Vmovlps    , Movlps    , X))       // AVX  | SSE
  V_EMIT_VVV_VV(vmovhps2x    , PACK_AVX_SSE(Vmovhps    , Movhps    , X))       // AVX  | SSE

  V_EMIT_VVV_VV(vmovlhps2x   , PACK_AVX_SSE(Vmovlhps   , Movlhps   , X))       // AVX  | SSE
  V_EMIT_VVV_VV(vmovhlps2x   , PACK_AVX_SSE(Vmovhlps   , Movhlps   , X))       // AVX  | SSE

  V_EMIT_VVV_VV(vmovlpd      , PACK_AVX_SSE(Vmovlpd    , Movlpd    , X))       // AVX  | SSE
  V_EMIT_VVV_VV(vmovhpd      , PACK_AVX_SSE(Vmovhpd    , Movhpd    , X))       // AVX  | SSE

  V_EMIT_VV_VV(vmovduplps_   , PACK_AVX_SSE(Vmovsldup  , Movsldup  , Z))       // AVX  | SSE3
  V_EMIT_VV_VV(vmovduphps_   , PACK_AVX_SSE(Vmovshdup  , Movshdup  , Z))       // AVX  | SSE3

  V_EMIT_VV_VV(vmovduplpd_   , PACK_AVX_SSE(Vmovddup   , Movddup   , Z))       // AVX  | SSE3

  V_EMIT_VV_VV(vmovmskps     , PACK_AVX_SSE(Vmovmskps  , Movmskps  , Z))       // AVX  | SSE
  V_EMIT_VV_VV(vmovmskpd     , PACK_AVX_SSE(Vmovmskpd  , Movmskpd  , Z))       // AVX  | SSE2

  V_EMIT_VVI_VVI(vinsertss_  , PACK_AVX_SSE(Vinsertps  , Insertps  , X))       // AVX  | SSE4_1
  V_EMIT_VVI_VVI(vextractss_ , PACK_AVX_SSE(Vextractps , Extractps , X))       // AVX  | SSE4_1

  V_EMIT_VVV_VV(vunpacklps   , PACK_AVX_SSE(Vunpcklps  , Unpcklps  , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(vunpacklpd   , PACK_AVX_SSE(Vunpcklpd  , Unpcklpd  , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(vunpackhps   , PACK_AVX_SSE(Vunpckhps  , Unpckhps  , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(vunpackhpd   , PACK_AVX_SSE(Vunpckhpd  , Unpckhpd  , Z))       // AVX  | SSE2

  V_EMIT_VVVI_VVI(vshufps    , PACK_AVX_SSE(Vshufps    , Shufps    , Z))       // AVX  | SSE
  V_EMIT_VVVI_VVI(vshufpd    , PACK_AVX_SSE(Vshufpd    , Shufpd    , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(vandps       , PACK_AVX_SSE(Vandps     , Andps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(vandpd       , PACK_AVX_SSE(Vandpd     , Andpd     , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(vandnot_aps  , PACK_AVX_SSE(Vandnps    , Andnps    , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(vandnot_apd  , PACK_AVX_SSE(Vandnpd    , Andnpd    , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(vorps        , PACK_AVX_SSE(Vorps      , Orps      , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(vorpd        , PACK_AVX_SSE(Vorpd      , Orpd      , Z))       // AVX  | SSE2
  V_EMIT_VVV_VV(vxorps       , PACK_AVX_SSE(Vxorps     , Xorps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(vxorpd       , PACK_AVX_SSE(Vxorpd     , Xorpd     , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(vaddss       , PACK_AVX_SSE(Vaddss     , Addss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(vaddsd       , PACK_AVX_SSE(Vaddsd     , Addsd     , X))       // AVX  | SSE2
  V_EMIT_VVV_VV(vaddps       , PACK_AVX_SSE(Vaddps     , Addps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(vaddpd       , PACK_AVX_SSE(Vaddpd     , Addpd     , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(vsubss       , PACK_AVX_SSE(Vsubss     , Subss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(vsubsd       , PACK_AVX_SSE(Vsubsd     , Subsd     , X))       // AVX  | SSE2
  V_EMIT_VVV_VV(vsubps       , PACK_AVX_SSE(Vsubps     , Subps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(vsubpd       , PACK_AVX_SSE(Vsubpd     , Subpd     , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(vaddsubps_   , PACK_AVX_SSE(Vaddsubps  , Addsubps  , Z))       // AVX  | SSE3
  V_EMIT_VVV_VV(vaddsubpd_   , PACK_AVX_SSE(Vaddsubpd  , Addsubpd  , Z))       // AVX  | SSE3

  V_EMIT_VVV_VV(vmulss       , PACK_AVX_SSE(Vmulss     , Mulss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(vmulsd       , PACK_AVX_SSE(Vmulsd     , Mulsd     , X))       // AVX  | SSE2
  V_EMIT_VVV_VV(vmulps       , PACK_AVX_SSE(Vmulps     , Mulps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(vmulpd       , PACK_AVX_SSE(Vmulpd     , Mulpd     , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(vdivss       , PACK_AVX_SSE(Vdivss     , Divss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(vdivsd       , PACK_AVX_SSE(Vdivsd     , Divsd     , X))       // AVX  | SSE2
  V_EMIT_VVV_VV(vdivps       , PACK_AVX_SSE(Vdivps     , Divps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(vdivpd       , PACK_AVX_SSE(Vdivpd     , Divpd     , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(vminss       , PACK_AVX_SSE(Vminss     , Minss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(vminsd       , PACK_AVX_SSE(Vminsd     , Minsd     , X))       // AVX  | SSE2
  V_EMIT_VVV_VV(vminps       , PACK_AVX_SSE(Vminps     , Minps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(vminpd       , PACK_AVX_SSE(Vminpd     , Minpd     , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(vmaxss       , PACK_AVX_SSE(Vmaxss     , Maxss     , X))       // AVX  | SSE
  V_EMIT_VVV_VV(vmaxsd       , PACK_AVX_SSE(Vmaxsd     , Maxsd     , X))       // AVX  | SSE2
  V_EMIT_VVV_VV(vmaxps       , PACK_AVX_SSE(Vmaxps     , Maxps     , Z))       // AVX  | SSE
  V_EMIT_VVV_VV(vmaxpd       , PACK_AVX_SSE(Vmaxpd     , Maxpd     , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(vsqrtss      , PACK_AVX_SSE(Vsqrtss    , Sqrtss    , X))       // AVX  | SSE
  V_EMIT_VVV_VV(vsqrtsd      , PACK_AVX_SSE(Vsqrtsd    , Sqrtsd    , X))       // AVX  | SSE2
  V_EMIT_VV_VV(vsqrtps       , PACK_AVX_SSE(Vsqrtps    , Sqrtps    , Z))       // AVX  | SSE
  V_EMIT_VV_VV(vsqrtpd       , PACK_AVX_SSE(Vsqrtpd    , Sqrtpd    , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(vrcpss       , PACK_AVX_SSE(Vrcpss     , Rcpss     , X))       // AVX  | SSE
  V_EMIT_VV_VV(vrcpps        , PACK_AVX_SSE(Vrcpps     , Rcpps     , Z))       // AVX  | SSE

  V_EMIT_VVV_VV(vrsqrtss     , PACK_AVX_SSE(Vrsqrtss   , Rsqrtss   , X))       // AVX  | SSE
  V_EMIT_VV_VV(vrsqrtps      , PACK_AVX_SSE(Vrsqrtps   , Rsqrtps   , Z))       // AVX  | SSE

  V_EMIT_VVVI_VVI(vdpps_     , PACK_AVX_SSE(Vdpps      , Dpps      , Z))       // AVX  | SSE4.1
  V_EMIT_VVVI_VVI(vdppd_     , PACK_AVX_SSE(Vdppd      , Dppd      , Z))       // AVX  | SSE4.1

  V_EMIT_VVVI_VVI(vroundss_   , PACK_AVX_SSE(Vroundss  , Roundss   , X))       // AVX  | SSE4.1
  V_EMIT_VVVI_VVI(vroundsd_   , PACK_AVX_SSE(Vroundsd  , Roundsd   , X))       // AVX  | SSE4.1
  V_EMIT_VVI_VVI(vroundps_    , PACK_AVX_SSE(Vroundps  , Roundps   , Z))       // AVX  | SSE4.1
  V_EMIT_VVI_VVI(vroundpd_    , PACK_AVX_SSE(Vroundpd  , Roundpd   , Z))       // AVX  | SSE4.1

  V_EMIT_VVVI_VVI(vcmpss      , PACK_AVX_SSE(Vcmpss    , Cmpss     , X))       // AVX  | SSE
  V_EMIT_VVVI_VVI(vcmpsd      , PACK_AVX_SSE(Vcmpsd    , Cmpsd     , X))       // AVX  | SSE2
  V_EMIT_VVVI_VVI(vcmpps      , PACK_AVX_SSE(Vcmpps    , Cmpps     , Z))       // AVX  | SSE
  V_EMIT_VVVI_VVI(vcmppd      , PACK_AVX_SSE(Vcmppd    , Cmppd     , Z))       // AVX  | SSE2

  V_EMIT_VVVV_VVV(vblendvps_ , PACK_AVX_SSE(Vblendvps  , Blendvps  , Z))       // AVX  | SSE4.1
  V_EMIT_VVVV_VVV(vblendvpd_ , PACK_AVX_SSE(Vblendvpd  , Blendvpd  , Z))       // AVX  | SSE4.1
  V_EMIT_VVVI_VVI(vblendps_  , PACK_AVX_SSE(Vblendps   , Blendps   , Z))       // AVX  | SSE4.1
  V_EMIT_VVVI_VVI(vblendpd_  , PACK_AVX_SSE(Vblendpd   , Blendpd   , Z))       // AVX  | SSE4.1

  V_EMIT_VV_VV(vcvti32ps     , PACK_AVX_SSE(Vcvtdq2ps  , Cvtdq2ps  , Z))       // AVX  | SSE2
  V_EMIT_VV_VV(vcvtpdps      , PACK_AVX_SSE(Vcvtpd2ps  , Cvtpd2ps  , Z))       // AVX  | SSE2

  V_EMIT_VV_VV(vcvti32pd     , PACK_AVX_SSE(Vcvtdq2pd  , Cvtdq2pd  , Z))       // AVX  | SSE2
  V_EMIT_VV_VV(vcvtpspd      , PACK_AVX_SSE(Vcvtps2pd  , Cvtps2pd  , Z))       // AVX  | SSE2

  V_EMIT_VV_VV(vcvtpsi32     , PACK_AVX_SSE(Vcvtps2dq  , Cvtps2dq  , Z))       // AVX  | SSE2
  V_EMIT_VV_VV(vcvtpdi32     , PACK_AVX_SSE(Vcvtpd2dq  , Cvtpd2dq  , Z))       // AVX  | SSE2

  V_EMIT_VV_VV(vcvttpsi32    , PACK_AVX_SSE(Vcvttps2dq , Cvttps2dq , Z))       // AVX  | SSE2
  V_EMIT_VV_VV(vcvttpdi32    , PACK_AVX_SSE(Vcvttpd2dq , Cvttpd2dq , Z))       // AVX  | SSE2

  V_EMIT_VVV_VV(vcvtsdss     , PACK_AVX_SSE(Vcvtsd2ss  , Cvtsd2ss  , X))       // AVX  | SSE2
  V_EMIT_VVV_VV(vcvtsssd     , PACK_AVX_SSE(Vcvtss2sd  , Cvtss2sd  , X))       // AVX  | SSE2

  V_EMIT_VVV_VV(vcvtsiss     , PACK_AVX_SSE(Vcvtsi2ss  , Cvtsi2ss  , X))       // AVX  | SSE
  V_EMIT_VVV_VV(vcvtsisd     , PACK_AVX_SSE(Vcvtsi2sd  , Cvtsi2sd  , X))       // AVX  | SSE2

  V_EMIT_VV_VV(vcvtsssi      , PACK_AVX_SSE(Vcvtss2si  , Cvtss2si  , X))       // AVX  | SSE
  V_EMIT_VV_VV(vcvtsdsi      , PACK_AVX_SSE(Vcvtsd2si  , Cvtsd2si  , X))       // AVX  | SSE2

  V_EMIT_VV_VV(vcvttsssi     , PACK_AVX_SSE(Vcvttss2si , Cvttss2si , X))       // AVX  | SSE
  V_EMIT_VV_VV(vcvttsdsi     , PACK_AVX_SSE(Vcvttsd2si , Cvttsd2si , X))       // AVX  | SSE2

  V_EMIT_VVV_VV(vhaddps_     , PACK_AVX_SSE(Vhaddps    , Haddps    , Z))       // AVX  | SSE3
  V_EMIT_VVV_VV(vhaddpd_     , PACK_AVX_SSE(Vhaddpd    , Haddpd    , Z))       // AVX  | SSE3
  V_EMIT_VVV_VV(vhsubps_     , PACK_AVX_SSE(Vhsubps    , Hsubps    , Z))       // AVX  | SSE3
  V_EMIT_VVV_VV(vhsubpd_     , PACK_AVX_SSE(Vhsubpd    , Hsubpd    , Z))       // AVX  | SSE3

  // Floating Point - Miscellaneous.

  V_EMIT_VV_VV(vcomiss       , PACK_AVX_SSE(Vcomiss    , Comiss    , X))       // AVX  | SSE
  V_EMIT_VV_VV(vcomisd       , PACK_AVX_SSE(Vcomisd    , Comisd    , X))       // AVX  | SSE2
  V_EMIT_VV_VV(vucomiss      , PACK_AVX_SSE(Vucomiss   , Ucomiss   , X))       // AVX  | SSE
  V_EMIT_VV_VV(vucomisd      , PACK_AVX_SSE(Vucomisd   , Ucomisd   , X))       // AVX  | SSE2

  // Initialization.

  inline void vzeropi(const Operand_& dst) noexcept { vemit_vvv_vv(PACK_AVX_SSE(Vpxor , Pxor , Z), dst, dst, dst); }
  inline void vzerops(const Operand_& dst) noexcept { vemit_vvv_vv(PACK_AVX_SSE(Vxorps, Xorps, Z), dst, dst, dst); }
  inline void vzeropd(const Operand_& dst) noexcept { vemit_vvv_vv(PACK_AVX_SSE(Vxorpd, Xorpd, Z), dst, dst, dst); }

  inline void vzeropi(const OpArray& dst) noexcept { for (uint32_t i = 0; i < dst.size(); i++) vzeropi(dst[i]); }
  inline void vzerops(const OpArray& dst) noexcept { for (uint32_t i = 0; i < dst.size(); i++) vzerops(dst[i]); }
  inline void vzeropd(const OpArray& dst) noexcept { for (uint32_t i = 0; i < dst.size(); i++) vzeropd(dst[i]); }

  // Conversion.

  inline void vmovsi32(const x86::Vec& dst, const x86::Gp& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovd, Movd, X), dst, src); }
  inline void vmovsi64(const x86::Vec& dst, const x86::Gp& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovq, Movq, X), dst, src); }

  inline void vmovsi32(const x86::Gp& dst, const x86::Vec& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovd, Movd, X), dst, src); }
  inline void vmovsi64(const x86::Gp& dst, const x86::Vec& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovq, Movq, X), dst, src); }

  // Memory Load & Store.

  BL_NOINLINE void vloadi8(const Operand_& dst, const x86::Mem& src) noexcept {
    if (hasSSE4_1()) {
      vzeropi(dst);
      vinsertu8_(dst, dst, src, 0);
    }
    else {
      x86::Gp tmp = cc->newUInt32("@tmp");
      load8(tmp, src);
      vmovsi32(dst.as<x86::Xmm>(), tmp);
    }
  }

  BL_NOINLINE void vloadu8_u16_2x(const Operand_& dst, const x86::Mem& lo, const x86::Mem& hi) noexcept {
    x86::Gp reg = cc->newUInt32("@tmp");
    x86::Mem mLo(lo);
    x86::Mem mHi(hi);

    mLo.setSize(1);
    mHi.setSize(1);

    cc->movzx(reg, mHi);
    cc->shl(reg, 16);
    cc->mov(reg.r8(), mLo);
    vmovsi32(dst.as<x86::Xmm>(), reg);
  }

  BL_NOINLINE void vloadi16(const Operand_& dst, const x86::Mem& src) noexcept {
    if (hasSSE4_1()) {
      vzeropi(dst);
      vinsertu16(dst, dst, src, 0);
    }
    else {
      x86::Gp tmp = cc->newUInt32("@tmp");
      load16(tmp, src);
      vmovsi32(dst.as<x86::Xmm>(), tmp);
    }
  }

  inline void vloadi32(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovd, Movd, X), dst, src); }
  inline void vloadi64(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovq, Movq, X), dst, src); }

  inline void vloadi128a(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovdqa, Movaps, X), dst, src); }
  inline void vloadi128u(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovdqu, Movups, X), dst, src); }
  inline void vloadi128u_ro(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PackedInst::packIntrin(kIntrin2Vloadi128uRO), dst, src); }

  inline void vloadi256a(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovdqa, Movaps, Y), dst, src); }
  inline void vloadi256u(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovdqu, Movups, Y), dst, src); }
  inline void vloadi256u_ro(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PackedInst::packIntrin(kIntrin2Vloadi128uRO), dst, src); }

  inline void vloadi64_u8u16_(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vpmovzxbw, Pmovzxbw, X), dst, src); }
  inline void vloadi32_u8u32_(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vpmovzxbd, Pmovzxbd, X), dst, src); }
  inline void vloadi16_u8u64_(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vpmovzxbq, Pmovzxbq, X), dst, src); }
  inline void vloadi64_u16u32_(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vpmovzxwd, Pmovzxwd, X), dst, src); }
  inline void vloadi32_u16u64_(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vpmovzxwq, Pmovzxwq, X), dst, src); }
  inline void vloadi64_u32u64_(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vpmovzxdq, Pmovzxdq, X), dst, src); }

  inline void vloadi64_i8i16_(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vpmovsxbw, Pmovsxbw, X), dst, src); }
  inline void vloadi32_i8i32_(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vpmovsxbd, Pmovsxbd, X), dst, src); }
  inline void vloadi16_i8i64_(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vpmovsxbq, Pmovsxbq, X), dst, src); }
  inline void vloadi64_i16i32_(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vpmovsxwd, Pmovsxwd, X), dst, src); }
  inline void vloadi32_i16i64_(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vpmovsxwq, Pmovsxwq, X), dst, src); }
  inline void vloadi64_i32i64_(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vpmovsxdq, Pmovsxdq, X), dst, src); }

  inline void vstorei32(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovd, Movd, X), dst, src); }
  inline void vstorei64(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovq, Movq, X), dst, src); }

  inline void vstorei128a(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovdqa, Movaps, X), dst, src); }
  inline void vstorei128u(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovdqu, Movups, X), dst, src); }

  inline void vstorei128x(const x86::Mem& dst, const x86::Vec& src, uint32_t alignment) noexcept {
    if (alignment >= 16)
      vstorei128a(dst, src);
    else
      vstorei128u(dst, src);
  }

  inline void vstorei256a(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovdqa, Movaps, Y), dst, src); }
  inline void vstorei256u(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovdqu, Movups, Y), dst, src); }

  inline void vstorei256x(const x86::Mem& dst, const x86::Vec& src, uint32_t alignment) noexcept {
    if (alignment >= 32)
      vstorei256a(dst, src);
    else
      vstorei256u(dst, src);
  }

  inline void vloadss(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovss, Movss, X), dst, src); }
  inline void vloadsd(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovsd, Movsd, X), dst, src); }

  inline void vloadps_64l(const Operand_& dst, const Operand_& src1, const x86::Mem& src2) noexcept { vemit_vvv_vv(PACK_AVX_SSE(Vmovlps, Movlps, X), dst, src1, src2); }
  inline void vloadps_64h(const Operand_& dst, const Operand_& src1, const x86::Mem& src2) noexcept { vemit_vvv_vv(PACK_AVX_SSE(Vmovhps, Movhps, X), dst, src1, src2); }
  inline void vloadpd_64l(const Operand_& dst, const Operand_& src1, const x86::Mem& src2) noexcept { vemit_vvv_vv(PACK_AVX_SSE(Vmovlpd, Movlpd, X), dst, src1, src2); }
  inline void vloadpd_64h(const Operand_& dst, const Operand_& src1, const x86::Mem& src2) noexcept { vemit_vvv_vv(PACK_AVX_SSE(Vmovhpd, Movhpd, X), dst, src1, src2); }

  inline void vloadps_128a(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovaps, Movaps, X), dst, src); }
  inline void vloadps_128u(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovups, Movups, X), dst, src); }
  inline void vloadpd_128a(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovapd, Movaps, X), dst, src); }
  inline void vloadpd_128u(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovupd, Movups, X), dst, src); }

  inline void vloadps_256a(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovaps, Movaps, Y), dst, src); }
  inline void vloadps_256u(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovups, Movups, Y), dst, src); }
  inline void vloadpd_256a(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovapd, Movaps, Y), dst, src); }
  inline void vloadpd_256u(const Operand_& dst, const x86::Mem& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovupd, Movups, Y), dst, src); }

  inline void vstoress(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovss, Movss, X), dst, src); }
  inline void vstoresd(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovsd, Movsd, X), dst, src); }

  inline void vstoreps_64l(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovlps, Movlps, X), dst, src); }
  inline void vstoreps_64h(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovhps, Movhps, X), dst, src); }

  inline void vstorepd_64l(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovsd , Movsd , X), dst, src); }
  inline void vstorepd_64h(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovhpd, Movhpd, X), dst, src); }

  inline void vstoreps_128a(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovaps, Movaps, X), dst, src); }
  inline void vstoreps_128u(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovups, Movups, X), dst, src); }
  inline void vstorepd_128a(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovapd, Movaps, X), dst, src); }
  inline void vstorepd_128u(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovupd, Movups, X), dst, src); }

  inline void vstoreps_256a(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovaps, Movaps, Y), dst, src); }
  inline void vstoreps_256u(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovups, Movups, Y), dst, src); }
  inline void vstorepd_256a(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovapd, Movaps, Y), dst, src); }
  inline void vstorepd_256u(const x86::Mem& dst, const Operand_& src) noexcept { vemit_vv_vv(PACK_AVX_SSE(Vmovupd, Movups, Y), dst, src); }

  // Intrinsics:
  //
  //   - vmov{x}{y}   - Move with sign or zero extension from {x} to {y}. Similar
  //                    to instructions like `pmovzx..`, `pmovsx..`, and `punpckl..`
  //
  //   - vswap{x}     - Swap low and high elements. If the vector has more than 2
  //                    elements it's divided into 2 element vectors in which the
  //                    operation is performed separately.
  //
  //   - vdup{l|h}{x} - Duplicate either low or high element into both. If there
  //                    are more than 2 elements in the vector it's considered
  //                    they are separate units. For example a 4-element vector
  //                    can be considered as 2 2-element vectors on which the
  //                    duplication operation is performed.

  template<typename DstT, typename SrcT>
  inline void vmovu8u16(const DstT& dst, const SrcT& src) noexcept {
    vemit_vv_vv(PackedInst::packIntrin(kIntrin2Vmovu8u16), dst, src);
  }

  template<typename DstT, typename SrcT>
  inline void vmovu8u32(const DstT& dst, const SrcT& src) noexcept {
    vemit_vv_vv(PackedInst::packIntrin(kIntrin2Vmovu8u32), dst, src);
  }

  template<typename DstT, typename SrcT>
  inline void vmovu16u32(const DstT& dst, const SrcT& src) noexcept {
    vemit_vv_vv(PackedInst::packIntrin(kIntrin2Vmovu16u32), dst, src);
  }

  template<typename DstT, typename SrcT>
  inline void vabsi8(const DstT& dst, const SrcT& src) noexcept {
    vemit_vv_vv(PackedInst::packIntrin(kIntrin2Vabsi8), dst, src);
  }

  template<typename DstT, typename SrcT>
  inline void vabsi16(const DstT& dst, const SrcT& src) noexcept {
    vemit_vv_vv(PackedInst::packIntrin(kIntrin2Vabsi16), dst, src);
  }

  template<typename DstT, typename SrcT>
  inline void vabsi32(const DstT& dst, const SrcT& src) noexcept {
    vemit_vv_vv(PackedInst::packIntrin(kIntrin2Vabsi32), dst, src);
  }

  template<typename DstT, typename SrcT>
  inline void vabsi64(const DstT& dst, const SrcT& src) noexcept {
    vemit_vv_vv(PackedInst::packIntrin(kIntrin2Vabsi64), dst, src);
  }

  template<typename DstT, typename SrcT>
  inline void vswapi32(const DstT& dst, const SrcT& src) noexcept {
    vswizi32(dst, src, x86::Predicate::shuf(2, 3, 0, 1));
  }

  template<typename DstT, typename SrcT>
  inline void vswapi64(const DstT& dst, const SrcT& src) noexcept {
    vswizi32(dst, src, x86::Predicate::shuf(1, 0, 3, 2));
  }

  template<typename DstT, typename SrcT>
  inline void vdupli32(const DstT& dst, const SrcT& src) noexcept {
    vswizi32(dst, src, x86::Predicate::shuf(2, 2, 0, 0));
  }

  template<typename DstT, typename SrcT>
  inline void vduphi32(const DstT& dst, const SrcT& src) noexcept {
    vswizi32(dst, src, x86::Predicate::shuf(3, 3, 1, 1));
  }

  template<typename DstT, typename SrcT>
  inline void vdupli64(const DstT& dst, const SrcT& src) noexcept {
    vswizi32(dst, src, x86::Predicate::shuf(1, 0, 1, 0));
  }

  template<typename DstT, typename SrcT>
  inline void vduphi64(const DstT& dst, const SrcT& src) noexcept {
    vswizi32(dst, src, x86::Predicate::shuf(3, 2, 3, 2));
  }

  // Dst = (CondBit == 0) ? Src1 : Src2;
  template<typename DstT, typename Src1T, typename Src2T, typename CondT>
  inline void vblendv8(const DstT& dst, const Src1T& src1, const Src2T& src2, const CondT& cond) noexcept {
    vemit_vvvv_vvv(PackedInst::packIntrin(kIntrin4Vpblendvb), dst, src1, src2, cond);
  }

  // Dst = (CondBit == 0) ? Src1 : Src2;
  template<typename DstT, typename Src1T, typename Src2T, typename CondT>
  inline void vblendv8_destructive(const DstT& dst, const Src1T& src1, const Src2T& src2, const CondT& cond) noexcept {
    vemit_vvvv_vvv(PackedInst::packIntrin(kIntrin4VpblendvbDestructive), dst, src1, src2, cond);
  }

  template<typename DstT, typename SrcT>
  inline void vinv255u16(const DstT& dst, const SrcT& src) noexcept {
    vemit_vv_vv(PackedInst::packIntrin(kIntrin2Vinv255u16), dst, src);
  }

  template<typename DstT, typename SrcT>
  inline void vinv256u16(const DstT& dst, const SrcT& src) noexcept {
    vemit_vv_vv(PackedInst::packIntrin(kIntrin2Vinv256u16), dst, src);
  }

  template<typename DstT, typename SrcT>
  inline void vinv255u32(const DstT& dst, const SrcT& src) noexcept {
    vemit_vv_vv(PackedInst::packIntrin(kIntrin2Vinv255u32), dst, src);
  }

  template<typename DstT, typename SrcT>
  inline void vinv256u32(const DstT& dst, const SrcT& src) noexcept {
    vemit_vv_vv(PackedInst::packIntrin(kIntrin2Vinv256u32), dst, src);
  }

  template<typename DstT, typename SrcT>
  inline void vduplpd(const DstT& dst, const SrcT& src) noexcept {
    vemit_vv_vv(PackedInst::packIntrin(kIntrin2Vduplpd), dst, src);
  }

  template<typename DstT, typename SrcT>
  inline void vduphpd(const DstT& dst, const SrcT& src) noexcept {
    vemit_vv_vv(PackedInst::packIntrin(kIntrin2Vduphpd), dst, src);
  }

  template<typename DstT, typename Src1T, typename Src2T>
  inline void vhaddpd(const DstT& dst, const Src1T& src1, const Src2T& src2) noexcept {
    vemit_vvv_vv(PackedInst::packIntrin(kIntrin3Vhaddpd), dst, src1, src2);
  }

  template<typename DstT, typename SrcT>
  inline void vexpandli32(const DstT& dst, const SrcT& src) noexcept {
    vswizi32(dst, src, x86::Predicate::shuf(0, 0, 0, 0));
  }

  // dst.u64[0] = src1.u64[1];
  // dst.u64[1] = src2.u64[0];
  template<typename DstT, typename Src1T, typename Src2T>
  inline void vcombhli64(const DstT& dst, const Src1T& src1, const Src2T& src2) noexcept {
    vemit_vvv_vv(PackedInst::packIntrin(kIntrin3Vcombhli64), dst, src1, src2);
  }

  // dst.d64[0] = src1.d64[1];
  // dst.d64[1] = src2.d64[0];
  template<typename DstT, typename Src1T, typename Src2T>
  inline void vcombhld64(const DstT& dst, const Src1T& src1, const Src2T& src2) noexcept {
    vemit_vvv_vv(PackedInst::packIntrin(kIntrin3Vcombhld64), dst, src1, src2);
  }

  template<typename DstT, typename Src1T, typename Src2T>
  inline void vminu16(const DstT& dst, const Src1T& src1, const Src2T& src2) noexcept {
    vemit_vvv_vv(PackedInst::packIntrin(kIntrin3Vminu16), dst, src1, src2);
  }

  template<typename DstT, typename Src1T, typename Src2T>
  inline void vmaxu16(const DstT& dst, const Src1T& src1, const Src2T& src2) noexcept {
    vemit_vvv_vv(PackedInst::packIntrin(kIntrin3Vmaxu16), dst, src1, src2);
  }

  // Multiplies packed uint64_t in `src1` with packed low uint32_t int `src2`.
  template<typename DstT, typename Src1T, typename Src2T>
  inline void vMulU64xU32Lo(const DstT& dst, const Src1T& src1, const Src2T& src2) noexcept {
    vemit_vvv_vv(PackedInst::packIntrin(kIntrin3Vmulu64x32), dst, src1, src2);
  }

  template<typename DstT, typename SrcT>
  BL_NOINLINE void vmul257hu16(const DstT& dst, const SrcT& src) {
    vmulhu16(dst, src, constAsXmm(blCommonTable.i128_0101010101010101));
  }

  // TODO: [PIPEGEN] Consolidate this to only one implementation.
  template<typename DstSrcT>
  BL_NOINLINE void vdiv255u16(const DstSrcT& x) {
    vaddi16(x, x, constAsXmm(blCommonTable.i128_0080008000800080));
    vmul257hu16(x, x);
  }

  template<typename DstSrcT>
  BL_NOINLINE void vdiv255u16_2x(
    const DstSrcT& v0,
    const DstSrcT& v1) noexcept {

    x86::Xmm i128_0080008000800080 = constAsXmm(blCommonTable.i128_0080008000800080);
    x86::Xmm i128_0101010101010101 = constAsXmm(blCommonTable.i128_0101010101010101);

    vaddi16(v0, v0, i128_0080008000800080);
    vaddi16(v1, v1, i128_0080008000800080);

    vmulhu16(v0, v0, i128_0101010101010101);
    vmulhu16(v1, v1, i128_0101010101010101);
  }

  template<typename DstSrcT>
  BL_NOINLINE void vdiv255u16_3x(
    const DstSrcT& v0,
    const DstSrcT& v1,
    const DstSrcT& v2) noexcept {

    x86::Xmm i128_0080008000800080 = constAsXmm(blCommonTable.i128_0080008000800080);
    x86::Xmm i128_0101010101010101 = constAsXmm(blCommonTable.i128_0101010101010101);

    vaddi16(v0, v0, i128_0080008000800080);
    vaddi16(v1, v1, i128_0080008000800080);

    vmulhu16(v0, v0, i128_0101010101010101);
    vmulhu16(v1, v1, i128_0101010101010101);

    vaddi16(v2, v2, i128_0080008000800080);
    vmulhu16(v2, v2, i128_0101010101010101);
  }


  template<typename DstT, typename SrcT>
  inline void vexpandlps(const DstT& dst, const SrcT& src) noexcept {
    vexpandli32(dst, src);
  }

  template<typename DstT, typename SrcT>
  inline void vswizps(const DstT& dst, const SrcT& src, uint32_t imm) noexcept { vemit_vvi_vi(PackedInst::packIntrin(kIntrin2iVswizps), dst, src, imm); }

  template<typename DstT, typename SrcT>
  inline void vswizpd(const DstT& dst, const SrcT& src, uint32_t imm) noexcept { vemit_vvi_vi(PackedInst::packIntrin(kIntrin2iVswizpd), dst, src, imm); }

  template<typename DstT, typename SrcT>
  inline void vswapps(const DstT& dst, const SrcT& src) noexcept { vswizps(dst, src, x86::Predicate::shuf(2, 3, 0, 1)); }

  template<typename DstT, typename SrcT>
  inline void vswappd(const DstT& dst, const SrcT& src) noexcept { vswizpd(dst, src, x86::Predicate::shuf(0, 1)); }

  template<typename DstT, typename SrcT>
  inline void vbroadcast_u16(const DstT& dst, const SrcT& src) noexcept {
    vemit_vv_vv(PackedInst::packIntrin(kIntrin2VBroadcastU16), dst, src);
  }

  template<typename DstT, typename SrcT>
  inline void vbroadcast_u32(const DstT& dst, const SrcT& src) noexcept {
    vemit_vv_vv(PackedInst::packIntrin(kIntrin2VBroadcastU32), dst, src);
  }

  template<typename DstT, typename SrcT>
  inline void vbroadcast_u64(const DstT& dst, const SrcT& src) noexcept {
    vemit_vv_vv(PackedInst::packIntrin(kIntrin2VBroadcastU64), dst, src);
  }

  template<typename DstT, typename Src1T, typename Src2T>
  BL_INLINE void vminmaxu8(const DstT& dst, const Src1T& src1, const Src2T& src2, bool isMin) noexcept {
    if (isMin)
      vminu8(dst, src1, src2);
    else
      vmaxu8(dst, src1, src2);
  }

  // --------------------------------------------------------------------------
  // [X-Emit - High-Level]
  // --------------------------------------------------------------------------

  // Kind of a hack - if we don't have SSE4.1 we have to load the byte into GP
  // register first and then we use 'PINSRW', which is provided by baseline SSE2.
  // If we have SSE4.1 then it's much easier as we can load the byte by 'PINSRB'.
  void xInsertWordOrByte(const x86::Vec& dst, const x86::Mem& src, uint32_t wordIndex) noexcept {
    x86::Mem m = src;
    m.setSize(1);

    if (hasSSE4_1()) {
      vinsertu8_(dst, dst, m, wordIndex * 2u);
    }
    else {
      x86::Gp tmp = cc->newUInt32("@tmp");
      cc->movzx(tmp, m);
      vinsertu16(dst, dst, tmp, wordIndex);
    }
  }

  void xInlinePixelFillLoop(x86::Gp& dst, x86::Vec& src, x86::Gp& i, uint32_t mainLoopSize, uint32_t itemSize, uint32_t itemGranularity) noexcept;
  void xInlinePixelCopyLoop(x86::Gp& dst, x86::Gp& src, x86::Gp& i, uint32_t mainLoopSize, uint32_t itemSize, uint32_t itemGranularity, uint32_t format) noexcept;

  void _xInlineMemCopySequenceXmm(
    const x86::Mem& dPtr, bool dstAligned,
    const x86::Mem& sPtr, bool srcAligned, uint32_t numBytes, const x86::Vec& fillMask) noexcept;

  // --------------------------------------------------------------------------
  // [Fetch Utilities]
  // --------------------------------------------------------------------------

  //! Fetches 1 pixel to XMM register(s) in `p` from memory location `sMem`.
  void xFetchPixel_1x(Pixel& p, uint32_t flags, uint32_t sFormat, const x86::Mem& sMem, uint32_t sAlignment) noexcept;
  //! Fetches 4 pixels to XMM register(s) in `p` from memory location `sMem`.
  void xFetchPixel_4x(Pixel& p, uint32_t flags, uint32_t sFormat, const x86::Mem& sMem, uint32_t sAlignment) noexcept;
  //! Fetches 8 pixels to XMM register(s) in `p` from memory location `sMem`.
  void xFetchPixel_8x(Pixel& p, uint32_t flags, uint32_t sFormat, const x86::Mem& sMem, uint32_t sAlignment) noexcept;

  //! Makes sure that the given pixel `p` has all the `flags`.
  void xSatisfyPixel(Pixel& p, uint32_t flags) noexcept;
  //! Makes sure that the given pixel `p` has all the `flags` (solid source only).
  void xSatisfySolid(Pixel& p, uint32_t flags) noexcept;

  void _xSatisfyPixelRGBA(Pixel& p, uint32_t flags) noexcept;
  void _xSatisfyPixelAlpha(Pixel& p, uint32_t flags) noexcept;

  void _xSatisfySolidRGBA(Pixel& p, uint32_t flags) noexcept;
  void _xSatisfySolidAlpha(Pixel& p, uint32_t flags) noexcept;

  void xFetchUnpackedA8_2x(const x86::Xmm& dst, uint32_t format, const x86::Mem& src1, const x86::Mem& src0) noexcept;

  void xAssignUnpackedAlphaValues(Pixel& p, uint32_t flags, x86::Xmm& vec) noexcept;

  //! Fills alpha channel with 1.
  void vFillAlpha(Pixel& p) noexcept;

  // --------------------------------------------------------------------------
  // [Utilities - SIMD]
  // --------------------------------------------------------------------------

  BL_NOINLINE void xStorePixel(const x86::Gp& dPtr, const x86::Vec& vSrc, uint32_t count, uint32_t bpp, uint32_t dAlignment) noexcept {
    x86::Mem dMem = x86::ptr(dPtr);
    uint32_t numBytes = bpp * count;

    switch (numBytes) {
      case 4:
        vstorei32(dMem, vSrc);
        break;

      case 8:
        vstorei64(dMem, vSrc);
        break;

      case 16:
        if (dAlignment == 16)
          vstorei128a(dMem, vSrc);
        else
          vstorei128u(dMem, vSrc);
        break;

      default:
        BL_NOT_REACHED();
    }
  }

  inline void xStore32_ARGB(const x86::Gp& dPtr, const x86::Vec& vSrc) noexcept {
    vstorei32(x86::ptr_32(dPtr), vSrc);
  }

  BL_NOINLINE void xMovzxBW_LoHi(const x86::Vec& d0, const x86::Vec& d1, const x86::Vec& s) noexcept {
    BL_ASSERT(d0.id() != d1.id());

    if (hasSSE4_1()) {
      if (d0.id() == s.id()) {
        vswizi32(d1, d0, x86::Predicate::shuf(1, 0, 3, 2));
        vmovu8u16_(d0, d0);
        vmovu8u16_(d1, d1);
      }
      else {
        vmovu8u16(d0, s);
        vswizi32(d1, s, x86::Predicate::shuf(1, 0, 3, 2));
        vmovu8u16(d1, d1);
      }
    }
    else {
      x86::Xmm i128_0000000000000000 = constAsXmm(blCommonTable.i128_0000000000000000);
      if (d1.id() != s.id()) {
        vunpackhi8(d1, s, i128_0000000000000000);
        vunpackli8(d0, s, i128_0000000000000000);
      }
      else {
        vunpackli8(d0, s, i128_0000000000000000);
        vunpackhi8(d1, s, i128_0000000000000000);
      }
    }
  }

  template<typename Dst, typename Src>
  inline void vExpandAlphaLo16(const Dst& d, const Src& s) noexcept { vswizli16(d, s, x86::Predicate::shuf(3, 3, 3, 3)); }

  template<typename Dst, typename Src>
  inline void vExpandAlphaHi16(const Dst& d, const Src& s) noexcept { vswizhi16(d, s, x86::Predicate::shuf(3, 3, 3, 3)); }

  template<typename Dst, typename Src>
  inline void vExpandAlpha16(const Dst& d, const Src& s, uint32_t useHiPart = 1) noexcept {
    vExpandAlphaLo16(d, s);
    if (useHiPart)
      vExpandAlphaHi16(d, d);
  }

  template<typename Dst, typename Src>
  inline void vExpandAlphaPS(const Dst& d, const Src& s) noexcept { vswizi32(d, s, x86::Predicate::shuf(3, 3, 3, 3)); }

  template<typename DstT, typename SrcT>
  inline void vFillAlpha255B(const DstT& dst, const SrcT& src) noexcept { vor(dst, src, constAsXmm(blCommonTable.i128_FF000000FF000000)); }
  template<typename DstT, typename SrcT>
  inline void vFillAlpha255W(const DstT& dst, const SrcT& src) noexcept { vor(dst, src, constAsMem(blCommonTable.i128_00FF000000000000)); }

  template<typename DstT, typename SrcT>
  inline void vZeroAlphaB(const DstT& dst, const SrcT& src) noexcept { vand(dst, src, constAsMem(blCommonTable.i128_00FFFFFF00FFFFFF)); }
  template<typename DstT, typename SrcT>
  inline void vZeroAlphaW(const DstT& dst, const SrcT& src) noexcept { vand(dst, src, constAsMem(blCommonTable.i128_0000FFFFFFFFFFFF)); }

  template<typename DstT, typename SrcT>
  inline void vNegAlpha8B(const DstT& dst, const SrcT& src) noexcept { vxor(dst, src, constAsMem(blCommonTable.i128_FF000000FF000000)); }
  template<typename DstT, typename SrcT>
  inline void vNegAlpha8W(const DstT& dst, const SrcT& src) noexcept { vxor(dst, src, constAsMem(blCommonTable.i128_00FF000000000000)); }

  template<typename DstT, typename SrcT>
  inline void vNegRgb8B(const DstT& dst, const SrcT& src) noexcept { vxor(dst, src, constAsMem(blCommonTable.i128_00FFFFFF00FFFFFF)); }
  template<typename DstT, typename SrcT>
  inline void vNegRgb8W(const DstT& dst, const SrcT& src) noexcept { vxor(dst, src, constAsMem(blCommonTable.i128_000000FF00FF00FF)); }

  // d = int(floor(a / b) * b).
  template<typename XmmOrMem>
  BL_NOINLINE void vmodpd(const x86::Xmm& d, const x86::Xmm& a, const XmmOrMem& b) noexcept {
    if (hasSSE4_1()) {
      vdivpd(d, a, b);
      vroundpd_(d, d, x86::Predicate::kRoundTrunc | x86::Predicate::kRoundInexact);
      vmulpd(d, d, b);
    }
    else {
      x86::Xmm t = cc->newXmm("vmodpdTmp");

      vdivpd(d, a, b);
      vcvttpdi32(t, d);
      vcvti32pd(t, t);
      vcmppd(d, d, t, x86::Predicate::kCmpLT | x86::Predicate::kCmpUNORD);
      vandpd(d, d, constAsMem(blCommonTable.d128_m1));
      vaddpd(d, d, t);
      vmulpd(d, d, b);
    }
  }

  // Performs 32-bit unsigned modulo of 32-bit `a` (hi DWORD) with 32-bit `b` (lo DWORD).
  template<typename XmmOrMem_A, typename XmmOrMem_B>
  BL_NOINLINE void xModI64HIxU64LO(const x86::Xmm& d, const XmmOrMem_A& a, const XmmOrMem_B& b) noexcept {
    x86::Xmm t0 = cc->newXmm("t0");
    x86::Xmm t1 = cc->newXmm("t1");

    vswizi32(t1, b, x86::Predicate::shuf(3, 3, 2, 0));
    vswizi32(d , a, x86::Predicate::shuf(2, 0, 3, 1));

    vcvti32pd(t1, t1);
    vcvti32pd(t0, d);
    vmodpd(t0, t0, t1);
    vcvttpdi32(t0, t0);

    vsubi32(d, d, t0);
    vswizi32(d, d, x86::Predicate::shuf(1, 3, 0, 2));
  }

  // Performs 32-bit unsigned modulo of 32-bit `a` (hi DWORD) with 64-bit `b` (DOUBLE).
  template<typename XmmOrMem_A, typename XmmOrMem_B>
  BL_NOINLINE void xModI64HIxDouble(const x86::Xmm& d, const XmmOrMem_A& a, const XmmOrMem_B& b) noexcept {
    x86::Xmm t0 = cc->newXmm("t0");

    vswizi32(d, a, x86::Predicate::shuf(2, 0, 3, 1));
    vcvti32pd(t0, d);
    vmodpd(t0, t0, b);
    vcvttpdi32(t0, t0);

    vsubi32(d, d, t0);
    vswizi32(d, d, x86::Predicate::shuf(1, 3, 0, 2));
  }

  BL_NOINLINE void xExtractUnpackedAFromPackedARGB32_1(const x86::Xmm& d, const x86::Xmm& s) noexcept {
    vswizli16(d, s, x86::Predicate::shuf(1, 1, 1, 1));
    vsrli16(d, d, 8);
  }

  BL_NOINLINE void xExtractUnpackedAFromPackedARGB32_2(const x86::Xmm& d, const x86::Xmm& s) noexcept {
    if (hasSSSE3()) {
      vswizi8v_(d, s, constAsMem(blCommonTable.i128_pshufb_packed_argb32_2x_lo_to_unpacked_a8));
    }
    else {
      vswizli16(d, s, x86::Predicate::shuf(3, 3, 1, 1));
      vswizi32(d, d, x86::Predicate::shuf(1, 1, 0, 0));
      vsrli16(d, d, 8);
    }
  }

  BL_NOINLINE void xExtractUnpackedAFromPackedARGB32_4(const x86::Vec& d0, const x86::Vec& d1, const x86::Vec& s) noexcept {
    BL_ASSERT(d0.id() != d1.id());

    if (hasSSSE3()) {
      if (d0.id() == s.id()) {
        vswizi8v_(d1, s, constAsMem(blCommonTable.i128_pshufb_packed_argb32_2x_hi_to_unpacked_a8));
        vswizi8v_(d0, s, constAsMem(blCommonTable.i128_pshufb_packed_argb32_2x_lo_to_unpacked_a8));
      }
      else {
        vswizi8v_(d0, s, constAsMem(blCommonTable.i128_pshufb_packed_argb32_2x_lo_to_unpacked_a8));
        vswizi8v_(d1, s, constAsMem(blCommonTable.i128_pshufb_packed_argb32_2x_hi_to_unpacked_a8));
      }
    }
    else {
      if (d1.id() != s.id()) {
        vswizhi16(d1, s, x86::Predicate::shuf(3, 3, 1, 1));
        vswizli16(d0, s, x86::Predicate::shuf(3, 3, 1, 1));

        vswizi32(d1, d1, x86::Predicate::shuf(3, 3, 2, 2));
        vswizi32(d0, d0, x86::Predicate::shuf(1, 1, 0, 0));

        vsrli16(d1, d1, 8);
        vsrli16(d0, d0, 8);
      }
      else {
        vswizli16(d0, s, x86::Predicate::shuf(3, 3, 1, 1));
        vswizhi16(d1, s, x86::Predicate::shuf(3, 3, 1, 1));

        vswizi32(d0, d0, x86::Predicate::shuf(1, 1, 0, 0));
        vswizi32(d1, d1, x86::Predicate::shuf(3, 3, 2, 2));

        vsrli16(d0, d0, 8);
        vsrli16(d1, d1, 8);
      }
    }
  }

  BL_NOINLINE void xPackU32ToU16Lo(const x86::Vec& d0, const x86::Vec& s0) noexcept {
    if (hasSSE4_1()) {
      vpacki32u16_(d0, s0, s0);
    }
    else if (hasSSSE3()) {
      vswizi8v_(d0, s0, constAsMem(blCommonTable.i128_pshufb_u32_to_u16_lo));
    }
    else {
      // Sign extend and then use `packssdw()`.
      vslli32(d0, s0, 16);
      vsrai32(d0, d0, 16);
      vpacki32i16(d0, d0, d0);
    }
  }

  BL_NOINLINE void xPackU32ToU16Lo(const VecArray& d0, const VecArray& s0) noexcept {
    for (uint32_t i = 0; i < d0.size(); i++)
      xPackU32ToU16Lo(d0[i], s0[i]);
  }

  // --------------------------------------------------------------------------
  // [Emit - End]
  // --------------------------------------------------------------------------

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

// ============================================================================
// [BLPipeGen::PipeInjectAtTheEnd]
// ============================================================================

class PipeInjectAtTheEnd {
public:
  ScopedInjector _injector;

  BL_INLINE PipeInjectAtTheEnd(PipeCompiler* pc) noexcept
    : _injector(pc->cc, &pc->_funcEnd) {}
};

} // {BLPipeGen}

//! \}
//! \endcond

#endif // BLEND2D_PIPEGEN_PIPECOMPILER_P_H
