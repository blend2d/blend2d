// Blend2D - 2D Vector Graphics Powered by a JIT Compiler
//
//  * Official Blend2D Home Page: https://blend2d.com
//  * Official Github Repository: https://github.com/blend2d/blend2d
//
// Copyright (c) 2017-2020 The Blend2D Authors
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#ifndef BLEND2D_PIPEGEN_PIPEDEBUG_P_H_INCLUDED
#define BLEND2D_PIPEGEN_PIPEDEBUG_P_H_INCLUDED

#include "../pipegen/pipegencore_p.h"

//! \cond INTERNAL
//! \addtogroup blend2d_internal_pipegen
//! \{

namespace BLPipeGen {

// ============================================================================
// [BLPipeGen::PipeDebug]
// ============================================================================

//! Pipeline debugging.
struct PipeDebug {
  static void printGp(x86::Compiler& cc, const char* key, const x86::Gp& reg) noexcept {
    asmjit::FuncCallNode* x;

    if (reg.size() <= 4)
      x = cc.call(imm(_printGp32Cb), asmjit::FuncSignatureT<void, void*, int32_t>(asmjit::CallConv::kIdHost));
    else
      x = cc.call(imm(_printGp64Cb), asmjit::FuncSignatureT<void, void*, int64_t>(asmjit::CallConv::kIdHost));

    x->setArg(0, imm(key));
    x->setArg(1, reg);
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

    asmjit::FuncCallNode* x = cc.call(
      imm(_printXmmPiCb),
      asmjit::FuncSignatureT<void, void*, int>(asmjit::CallConv::kIdHost));

    x->setArg(0, imm(key));
    x->setArg(1, a);
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

    asmjit::FuncCallNode* x = cc.call(
      imm(_printXmmPsCb),
      asmjit::FuncSignatureT<void, void*, int>(asmjit::CallConv::kIdHost));

    x->setArg(0, imm(key));
    x->setArg(1, a);
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

    asmjit::FuncCallNode* x = cc.call(
      imm(_printXmmPdCb),
      asmjit::FuncSignatureT<void, void*, int>(asmjit::CallConv::kIdHost));

    x->setArg(0, imm(key));
    x->setArg(1, a);
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

} // {BLPipeGen}

//! \}
//! \endcond

#endif // BLEND2D_PIPEGEN_PIPEDEBUG_P_H_INCLUDED
