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

namespace bl {
namespace Pipeline {
namespace JIT {

//! Pipeline debugging.
struct PipeDebug {
  static void printGp(AsmCompiler& cc, const char* key, const Gp& reg) noexcept {
    asmjit::InvokeNode* invokeNode;

    if (reg.size() <= 4)
      cc.invoke(&invokeNode, imm(_printGp32Cb), asmjit::FuncSignature::build<void, void*, int32_t>());
    else
      cc.invoke(&invokeNode, imm(_printGp64Cb), asmjit::FuncSignature::build<void, void*, int64_t>());

    invokeNode->setArg(0, imm(key));
    invokeNode->setArg(1, reg);
  }

  static void _printGp32Cb(const char* key, int32_t value) noexcept {
    printf("%s=%d\n", key, (int)value);
  }

  static void _printGp64Cb(const char* key, int64_t value) noexcept {
    printf("%s=%lld\n", key, (long long)value);
  }

  static void printXmmPi(AsmCompiler& cc, const char* key, const Vec& reg) noexcept {
    Mem m = cc.newStack(16, 4, "dump_mem");
    Gp a = cc.newIntPtr("dump_tmp");

    cc.movupd(m, reg);
    cc.lea(a, m);

    asmjit::InvokeNode* invokeNode;
    cc.invoke(&invokeNode,
      imm(_printXmmPiCb),
      asmjit::FuncSignature::build<void, void*, int>());

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

  static void printXmmPs(AsmCompiler& cc, const char* key, const Vec& reg) noexcept {
    Mem m = cc.newStack(16, 4, "dump_mem");
    Gp a = cc.newIntPtr("dump_tmp");

    cc.movupd(m, reg);
    cc.lea(a, m);

    asmjit::InvokeNode* invokeNode;
    cc.invoke(&invokeNode,
      imm(_printXmmPsCb),
      asmjit::FuncSignature::build<void, void*, int>());

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

  static void printXmmPd(AsmCompiler& cc, const char* key, const Vec& reg) noexcept {
    Mem m = cc.newStack(16, 4, "dump_mem");
    Gp a = cc.newIntPtr("dump_tmp");

    cc.movupd(m, reg);
    cc.lea(a, m);

    asmjit::InvokeNode* invokeNode;
    cc.invoke(&invokeNode,
      imm(_printXmmPdCb),
      asmjit::FuncSignature::build<void, void*, int>());

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
} // {Pipeline}
} // {bl}

//! \}
//! \endcond

#endif // BLEND2D_PIPELINE_JIT_PIPEDEBUG_P_H_INCLUDED
