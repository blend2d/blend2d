// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#ifndef BLEND2D_PIPELINE_JIT_PIPEDEBUG_P_H_INCLUDED
#define BLEND2D_PIPELINE_JIT_PIPEDEBUG_P_H_INCLUDED

#include "../../pipeline/jit/pipegencore_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_pipeline_jit
//! \{

namespace BLPipeline {
namespace JIT {

//! Pipeline debugging.
struct PipeDebug {
  static void printGp(x86::Compiler& cc, const char* key, const x86::Gp& reg) noexcept {
    asmjit::InvokeNode* invokeNode;

    if (reg.size() <= 4)
      cc.invoke(&invokeNode, imm(_printGp32Cb), asmjit::FuncSignatureT<void, void*, int32_t>(asmjit::CallConvId::kHost));
    else
      cc.invoke(&invokeNode, imm(_printGp64Cb), asmjit::FuncSignatureT<void, void*, int64_t>(asmjit::CallConvId::kHost));

    invokeNode->setArg(0, imm(key));
    invokeNode->setArg(1, reg);
  }

  static void _printGp32Cb(const char* key, int32_t value) noexcept {
    printf("%s=%d\n", key, (int)value);
  }

  static void _printGp64Cb(const char* key, int64_t value) noexcept {
    printf("%s=%lld\n", key, (long long)value);
  }

  static void printXmmPi(x86::Compiler& cc, const char* key, const x86::Xmm& reg) noexcept {
    x86::Mem m = cc.newStack(16, 4, "dump_mem");
    x86::Gp a = cc.newIntPtr("dump_tmp");

    cc.movupd(m, reg);
    cc.lea(a, m);

    asmjit::InvokeNode* invokeNode;
    cc.invoke(&invokeNode,
      imm(_printXmmPiCb),
      asmjit::FuncSignatureT<void, void*, int>(asmjit::CallConvId::kHost));

    invokeNode->setArg(0, imm(key));
    invokeNode->setArg(1, a);
  }

  static void _printXmmPiCb(const char* key, const void* data) noexcept {
    const uint32_t* u = static_cast<const uint32_t*>(data);

    printf("%s=[0x%08X | 0x%08X | 0x%08X | 0x%08X] (%d %d %d %d)\n",
      key,
      (unsigned)u[0], (unsigned)u[1], (unsigned)u[2], (unsigned)u[3],
      (unsigned)u[0], (unsigned)u[1], (unsigned)u[2], (unsigned)u[3]);
  }

  static void printXmmPs(x86::Compiler& cc, const char* key, const x86::Xmm& reg) noexcept {
    x86::Mem m = cc.newStack(16, 4, "dump_mem");
    x86::Gp a = cc.newIntPtr("dump_tmp");

    cc.movupd(m, reg);
    cc.lea(a, m);

    asmjit::InvokeNode* invokeNode;
    cc.invoke(&invokeNode,
      imm(_printXmmPsCb),
      asmjit::FuncSignatureT<void, void*, int>(asmjit::CallConvId::kHost));

    invokeNode->setArg(0, imm(key));
    invokeNode->setArg(1, a);
  }

  static void _printXmmPsCb(const char* key, const void* data) noexcept {
    const float* f = static_cast<const float*>(data);
    const uint32_t* u = static_cast<const uint32_t*>(data);

    printf("%s=[0x%08X (%3.9g)  |  0x%08X (%3.9g)  |  0x%08X (%3.9g)  |  0x%08X (%3.9g)]\n",
      key,
      (unsigned)u[0], f[0],
      (unsigned)u[1], f[1],
      (unsigned)u[2], f[2],
      (unsigned)u[3], f[3]);
  }

  static void printXmmPd(x86::Compiler& cc, const char* key, const x86::Xmm& reg) noexcept {
    x86::Mem m = cc.newStack(16, 4, "dump_mem");
    x86::Gp a = cc.newIntPtr("dump_tmp");

    cc.movupd(m, reg);
    cc.lea(a, m);

    asmjit::InvokeNode* invokeNode;
    cc.invoke(&invokeNode,
      imm(_printXmmPdCb),
      asmjit::FuncSignatureT<void, void*, int>(asmjit::CallConvId::kHost));

    invokeNode->setArg(0, imm(key));
    invokeNode->setArg(1, a);
  }

  static void _printXmmPdCb(const char* key, const void* data) noexcept {
    const double* d = static_cast<const double*>(data);
    const uint64_t* u = static_cast<const uint64_t*>(data);

    printf("%s=[0x%016llX (%3.9g)  |  0x%016llX (%3.9g)]\n",
      key,
      (unsigned long long)u[0], d[0],
      (unsigned long long)u[1], d[1]);
  }
};

} // {JIT}
} // {BLPipeline}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_PIPEDEBUG_P_H_INCLUDED
