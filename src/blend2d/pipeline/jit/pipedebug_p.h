// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_PIPEDEBUG_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_PIPEDEBUG_P_H_INCLUDED

#include "../../pipeline/jit/pipecompiler_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace bl {
namespace Pipeline {
namespace JIT {

//! Pipeline debugging.
struct PipeDebug {
  static void printGp(PipeCompiler* pc, const char* key, const Gp& reg) noexcept {
    asmjit::InvokeNode* invokeNode;
    Gp func_ptr = pc->newGpPtr("func_ptr");

    if (reg.size() <= 4) {
      pc->mov(func_ptr, Imm((uintptr_t)_printGp32Cb));
      pc->cc->invoke(&invokeNode, func_ptr, asmjit::FuncSignature::build<void, void*, int32_t>());
    }
    else {
      pc->mov(func_ptr, Imm((uintptr_t)_printGp64Cb));
      pc->cc->invoke(&invokeNode, func_ptr, asmjit::FuncSignature::build<void, void*, int64_t>());
    }

    invokeNode->setArg(0, imm(key));
    invokeNode->setArg(1, reg);
  }

  static void loadAddress(PipeCompiler* pc, const Gp& dst, const Mem& mem) noexcept {
#if defined(BL_JIT_ARCH_X86)
    pc->cc->lea(dst, mem);
#else
    pc->cc->loadAddressOf(dst, mem);
#endif
  }

  static void printVecU32(PipeCompiler* pc, const char* key, const Vec& reg) noexcept {
    Mem m = pc->cc->newStack(16, 16, "dump_mem");
    Gp a = pc->cc->newIntPtr("dump_tmp");

    pc->v_storeu128(m, reg);
    loadAddress(pc, a, m);

    Gp func_ptr = pc->newGpPtr("func_ptr");
    pc->mov(func_ptr, Imm((uintptr_t)_printXmmPiCb));

    asmjit::InvokeNode* invokeNode;
    pc->cc->invoke(&invokeNode, func_ptr, asmjit::FuncSignature::build<void, void*, int>());

    invokeNode->setArg(0, imm(key));
    invokeNode->setArg(1, a);
  }

  static void printScalarF32(PipeCompiler* pc, const char* key, const Vec& reg) noexcept {
    Mem m = pc->cc->newStack(16, 16, "dump_mem");
    Gp a = pc->cc->newIntPtr("dump_tmp");

    pc->v_storeu32(m, reg);
    loadAddress(pc, a, m);

    Gp func_ptr = pc->newGpPtr("func_ptr");
    pc->mov(func_ptr, Imm((uintptr_t)_printScalarF32));

    asmjit::InvokeNode* invokeNode;
    pc->cc->invoke(&invokeNode, func_ptr, asmjit::FuncSignature::build<void, void*, int>());

    invokeNode->setArg(0, imm(key));
    invokeNode->setArg(1, a);
  }

  static void printVecF32(PipeCompiler* pc, const char* key, const Vec& reg) noexcept {
    Mem m = pc->cc->newStack(16, 16, "dump_mem");
    Gp a = pc->cc->newIntPtr("dump_tmp");

    pc->v_storeu128(m, reg);
    loadAddress(pc, a, m);

    Gp func_ptr = pc->newGpPtr("func_ptr");
    pc->mov(func_ptr, Imm((uintptr_t)_printXmmPsCb));

    asmjit::InvokeNode* invokeNode;
    pc->cc->invoke(&invokeNode, func_ptr, asmjit::FuncSignature::build<void, void*, int>());

    invokeNode->setArg(0, imm(key));
    invokeNode->setArg(1, a);
  }

  static void printVecF64(PipeCompiler* pc, const char* key, const Vec& reg) noexcept {
    Mem m = pc->cc->newStack(16, 16, "dump_mem");
    Gp a = pc->cc->newIntPtr("dump_tmp");

    pc->v_storeu128(m, reg);
    loadAddress(pc, a, m);

    Gp func_ptr = pc->newGpPtr("func_ptr");
    pc->mov(func_ptr, Imm((uintptr_t)_printXmmPdCb));

    asmjit::InvokeNode* invokeNode;
    pc->cc->invoke(&invokeNode, func_ptr, asmjit::FuncSignature::build<void, void*, int>());

    invokeNode->setArg(0, imm(key));
    invokeNode->setArg(1, a);
  }

  static void _printGp32Cb(const char* key, int32_t value) noexcept {
    blRuntimeMessageFmt("%s=%08X (%u) (%d)\n", key, (unsigned int)value, (unsigned int)value, (int)value);
  }

  static void _printGp64Cb(const char* key, int64_t value) noexcept {
    blRuntimeMessageFmt("%s=%016X (%llu) (%lld)\n", key, (unsigned long long)value, (unsigned long long)value, (long long)value);
  }

  static void _printXmmPiCb(const char* key, const void* data) noexcept {
    const uint32_t* u = static_cast<const uint32_t*>(data);

    blRuntimeMessageFmt("%s=[0x%08X | 0x%08X | 0x%08X | 0x%08X] (%d %d %d %d)\n",
      key,
      (unsigned)u[0], (unsigned)u[1], (unsigned)u[2], (unsigned)u[3],
      (unsigned)u[0], (unsigned)u[1], (unsigned)u[2], (unsigned)u[3]);
  }

  static void _printScalarF32(const char* key, const void* data) noexcept {
    const float* f = static_cast<const float*>(data);
    const uint32_t* u = static_cast<const uint32_t*>(data);

    blRuntimeMessageFmt("%s=[0x%08X (%3.9g)]\n",
      key,
      (unsigned)u[0], f[0]);
  }

  static void _printXmmPsCb(const char* key, const void* data) noexcept {
    const float* f = static_cast<const float*>(data);
    const uint32_t* u = static_cast<const uint32_t*>(data);

    blRuntimeMessageFmt("%s=[0x%08X (%3.9g)  |  0x%08X (%3.9g)  |  0x%08X (%3.9g)  |  0x%08X (%3.9g)]\n",
      key,
      (unsigned)u[0], f[0],
      (unsigned)u[1], f[1],
      (unsigned)u[2], f[2],
      (unsigned)u[3], f[3]);
  }

  static void _printXmmPdCb(const char* key, const void* data) noexcept {
    const double* d = static_cast<const double*>(data);
    const uint64_t* u = static_cast<const uint64_t*>(data);

    blRuntimeMessageFmt("%s=[0x%016llX (%3.9g)  |  0x%016llX (%3.9g)]\n",
      key,
      (unsigned long long)u[0], d[0],
      (unsigned long long)u[1], d[1]);
  }
};

} // {JIT}
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_PIPEDEBUG_P_H_INCLUDED
