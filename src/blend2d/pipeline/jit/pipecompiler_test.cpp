// This file is part of Blend2D project <https://blend2d.com>
//
// See blend2d.h or LICENSE.md for license and copyright information
// SPDX-License-Identifier: Zlib

#include "../../api-build_test_p.h"
#if defined(BL_TEST) && !defined(BL_BUILD_NO_JIT)

#include "../../random_p.h"
#include "../../pipeline/jit/pipecompiler_p.h"

#include <cmath>

// bl::Pipeline::JIT - Tests
// =========================

namespace bl {
namespace Pipeline {
namespace JIT {
namespace Tests {

// bl::Pipeline::JIT - Tests - Constants
// =====================================

static constexpr uint64_t kRandomSeed = 0x1234u;
static constexpr uint32_t kTestIterCount = 1000u;

static BL_INLINE_NODEBUG constexpr uint32_t byteWidthFromVecWidth(VecWidth vw) noexcept {
  return 16u << uint32_t(vw);
}

// bl::Pipeline::JIT - Tests - MulAdd
// ==================================

#if defined(BL_JIT_ARCH_X86)

float madd_nofma_ref_f32(float a, float b, float c) noexcept;
double madd_nofma_ref_f64(double a, double b, double c) noexcept;

float madd_fma_ref_f32(float a, float b, float c) noexcept;
double madd_fma_ref_f64(double a, double b, double c) noexcept;

void madd_fma_check_valgrind_bug(const float a[4], const float b[4], const float c[4], float dst[4]) noexcept;

static float madd_nofma_ref(float a, float b, float c) noexcept { return madd_nofma_ref_f32(a, b, c); }
static double madd_nofma_ref(double a, double b, double c) noexcept { return madd_nofma_ref_f64(a, b, c); }

static float madd_fma_ref(float a, float b, float c) noexcept { return madd_fma_ref_f32(a, b, c); }
static double madd_fma_ref(double a, double b, double c) noexcept { return madd_fma_ref_f64(a, b, c); }

#else

static float madd_nofma_ref(float a, float b, float c) noexcept { return a * b + c; }
static double madd_nofma_ref(double a, double b, double c) noexcept { return a * b + c; }

static float madd_fma_ref(float a, float b, float c) noexcept { return std::fma(a, b, c); }
static double madd_fma_ref(double a, double b, double c) noexcept { return std::fma(a, b, c); }

#endif

// bl::Pipeline::JIT - Tests - Types
// =================================

struct Variation {
  uint32_t value;

  BL_INLINE_NODEBUG bool operator==(uint32_t v) const noexcept { return value == v; }
  BL_INLINE_NODEBUG bool operator!=(uint32_t v) const noexcept { return value != v; }
  BL_INLINE_NODEBUG bool operator<=(uint32_t v) const noexcept { return value <= v; }
  BL_INLINE_NODEBUG bool operator< (uint32_t v) const noexcept { return value <  v; }
  BL_INLINE_NODEBUG bool operator>=(uint32_t v) const noexcept { return value >= v; }
  BL_INLINE_NODEBUG bool operator> (uint32_t v) const noexcept { return value >  v; }
};

// bl::Pipeline::JIT - Tests - JIT Function Prototypes
// ===================================================

typedef uint32_t (*TestCondRRFunc)(int32_t a, int32_t b);
typedef uint32_t (*TestCondRIFunc)(int32_t a);

typedef void (*TestMFunc)(void* ptr);
typedef uintptr_t (*TestRMFunc)(uintptr_t reg, void* ptr);
typedef void (*TestMRFunc)(void* ptr, uintptr_t reg);

typedef uint32_t (*TestRRFunc)(uint32_t a);
typedef uint32_t (*TestRRRFunc)(uint32_t a, uint32_t b);
typedef uint32_t (*TestRRIFunc)(uint32_t a);

typedef void (*TestVVFunc)(void* dst, const void* src);
typedef void (*TestVVVFunc)(void* dst, const void* src1, const void* src2);
typedef void (*TestVVVVFunc)(void* dst, const void* src1, const void* src2, const void* src3);

// bl::Pipeline::JIT - Tests - JIT Context Error Handler
// =====================================================

class TestErrorHandler : public asmjit::ErrorHandler {
public:
  TestErrorHandler() noexcept {}
  ~TestErrorHandler() noexcept override {}

  void handleError(asmjit::Error err, const char* message, asmjit::BaseEmitter* origin) override {
    blUnused(origin);
    EXPECT_EQ(err, asmjit::kErrorOk)
      .message("AsmJit Error: %s", message);
  }
};
// bl::Pipeline::JIT - Tests - JIT Context for Testing
// ===================================================

class JitContext {
public:
  asmjit::JitRuntime rt;
  asmjit::CpuFeatures features {};
  PipeOptFlags optFlags {};

  asmjit::StringLogger logger;
  // asmjit::FileLogger fl;

  TestErrorHandler eh;
  asmjit::CodeHolder code;
  AsmCompiler cc;

  void prepare() noexcept {
    logger.clear();

    code.reset();
    code.init(rt.environment());
    code.setErrorHandler(&eh);

    // fl.setFile(stdout);
    // fl.addFlags(asmjit::FormatFlags::kMachineCode);
    // code.setLogger(&fl);
    code.setLogger(&logger);

    code.attach(&cc);
    cc.addDiagnosticOptions(asmjit::DiagnosticOptions::kRAAnnotate);
    cc.addDiagnosticOptions(asmjit::DiagnosticOptions::kValidateAssembler);
    cc.addDiagnosticOptions(asmjit::DiagnosticOptions::kValidateIntermediate);
  }

  template<typename Fn>
  Fn finish() noexcept {
    Fn fn;
    EXPECT_EQ(cc.finalize(), asmjit::kErrorOk);
    EXPECT_EQ(rt.add(&fn, &code), asmjit::kErrorOk);
    code.reset();
    return fn;
  }
};

// bl::Pipeline::JIT - Tests - Conditional Operations - Functions
// ==============================================================

static TestCondRRFunc create_func_cond_rr(JitContext& ctx, OpcodeCond op, CondCode condCode, uint32_t variation) noexcept {
  ctx.prepare();
  PipeCompiler pc(&ctx.cc, ctx.features, ctx.optFlags);

  asmjit::FuncNode* node = ctx.cc.newFunc(asmjit::FuncSignature::build<uint32_t, int32_t, int32_t>());
  EXPECT_NOT_NULL(node);

  pc.initVecWidth(VecWidth::k128);
  pc.initFunction(node);

  Gp a = pc.newGp32("a");
  Gp b = pc.newGp32("b");
  Gp result = pc.newGp32("result");

  node->setArg(0, a);
  node->setArg(1, b);

  switch (variation) {
    case 0: {
      // Test a conditional branch based on the given condition.
      Label done = pc.newLabel();
      pc.mov(result, 1);
      pc.j(done, Condition(op, condCode, a, b));
      pc.mov(result, 0);
      pc.bind(done);
      break;
    }

    case 1: {
      // Test a cmov functionality.
      Gp trueValue = pc.newGp32("trueValue");
      pc.mov(result, 0);
      pc.mov(trueValue, 1);
      pc.cmov(result, trueValue, Condition(op, condCode, a, b));
      break;
    }

    case 2: {
      // Test a select functionality.
      Gp falseValue = pc.newGp32("falseValue");
      Gp trueValue = pc.newGp32("trueValue");
      pc.mov(falseValue, 0);
      pc.mov(trueValue, 1);
      pc.select(result, trueValue, falseValue, Condition(op, condCode, a, b));
      break;
    }
  }

  ctx.cc.ret(result);
  ctx.cc.endFunc();

  return ctx.finish<TestCondRRFunc>();
}

static TestCondRIFunc create_func_cond_ri(JitContext& ctx, OpcodeCond op, CondCode condCode, Imm bImm) noexcept {
  ctx.prepare();
  PipeCompiler pc(&ctx.cc, ctx.features, ctx.optFlags);

  asmjit::FuncNode* node = ctx.cc.newFunc(asmjit::FuncSignature::build<uint32_t, int32_t>());
  EXPECT_NOT_NULL(node);

  pc.initVecWidth(VecWidth::k128);
  pc.initFunction(node);

  Gp a = pc.newGp32("a");
  Gp result = pc.newGp32("result");
  Label done = pc.newLabel();

  node->setArg(0, a);
  pc.mov(result, 1);
  pc.j(done, Condition(op, condCode, a, bImm));
  pc.mov(result, 0);
  pc.bind(done);
  ctx.cc.ret(result);

  ctx.cc.endFunc();
  return ctx.finish<TestCondRIFunc>();
}

// bl::Pipeline::JIT - Tests - Conditional Operations - Runner
// ===========================================================

static BL_NOINLINE void test_conditional_op(JitContext& ctx, OpcodeCond op, CondCode condCode, int32_t a, int32_t b, bool expected) noexcept {
  for (uint32_t variation = 0; variation < 3; variation++) {
    TestCondRRFunc fn_rr = create_func_cond_rr(ctx, op, condCode, variation);
    TestCondRIFunc fn_ri = create_func_cond_ri(ctx, op, condCode, b);

    uint32_t observed_rr = fn_rr(a, b);
    EXPECT_EQ(observed_rr, uint32_t(expected))
      .message("Operation failed (RR):\n"
              "      Input #1: %d\n"
              "      Input #2: %d\n"
              "      Expected: %d\n"
              "      Observed: %d\n"
              "Assembly:\n%s",
              a,
              b,
              uint32_t(expected),
              observed_rr,
              ctx.logger.data());

    uint32_t observed_ri = fn_ri(a);
    EXPECT_EQ(observed_ri, uint32_t(expected))
      .message("Operation failed (RI):\n"
              "      Input #1: %d\n"
              "      Input #2: %d\n"
              "      Expected: %d\n"
              "      Observed: %d\n"
              "Assembly:\n%s",
              a,
              b,
              uint32_t(expected),
              observed_ri,
              ctx.logger.data());

    ctx.rt.reset();
  }
}

static BL_NOINLINE void test_cond_ops(JitContext& ctx) noexcept {
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kEqual, 0, 0, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kEqual, 1, 1, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kEqual, 1, 2, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kEqual, 100, 31, false);

  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kNotEqual, 0, 0, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kNotEqual, 1, 1, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kNotEqual, 1, 2, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kNotEqual, 100, 31, true);

  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedGT, 0, 0, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedGT, 1, 0, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedGT, 111111, 0, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedGT, 111111, 222, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedGT, 222, 111111, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedGT, 222, 111, true);

  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedGE, 0, 0, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedGE, 1, 0, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedGE, 111111, 0, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedGE, 111111, 111111, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedGE, 111111, 222, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedGE, 222, 111111, false);

  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedLT, 0, 0, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedLT, 1, 0, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedLT, 0, 1, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedLT, 111111, 0, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedLT, 111111, 222, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedLT, 222, 111111, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedLT, 222, 111, false);

  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedLE, 0, 0, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedLE, 1, 0, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedLE, 0, 1, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedLE, 111111, 0, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedLE, 111111, 222, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedLE, 222, 111111, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kUnsignedLE, 22222, 22222, true);

  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedGT, 0, 0, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedGT, 1, 0, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedGT, 111111, 0, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedGT, 111111, -222, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedGT, -222, 111111, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedGT, -222, -111, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedGT, -111, -1, false);

  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedGE, 0, 0, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedGE, 1, 0, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedGE, 111111, 0, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedGE, 111111, 111111, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedGE, 111111, -222, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedGE, -222, 111111, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedGE, -111, -1, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedGE, -111, -111, true);

  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedLT, 0, 0, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedLT, 1, 0, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedLT, 111111, 0, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedLT, 111111, -222, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedLT, -222, 111111, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedLT, -222, -111, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedLT, -111, -1, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedLT, -1, -1, false);

  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedLE, 0, 0, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedLE, 1, 0, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedLE, 111111, 0, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedLE, 111111, -222, false);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedLE, -222, 111111, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedLE, -222, -111, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedLE, -111, -1, true);
  test_conditional_op(ctx, OpcodeCond::kCompare, CondCode::kSignedLE, -1, -1, true);

  test_conditional_op(ctx, OpcodeCond::kTest, CondCode::kZero, 0, 0, true);
  test_conditional_op(ctx, OpcodeCond::kTest, CondCode::kZero, 1, 0, true);
  test_conditional_op(ctx, OpcodeCond::kTest, CondCode::kZero, 111111, 0, true);
  test_conditional_op(ctx, OpcodeCond::kTest, CondCode::kZero, 111111, -222, false);
  test_conditional_op(ctx, OpcodeCond::kTest, CondCode::kZero, -222, 111111, false);

  test_conditional_op(ctx, OpcodeCond::kTest, CondCode::kNotZero, 0, 0, false);
  test_conditional_op(ctx, OpcodeCond::kTest, CondCode::kNotZero, 1, 0, false);
  test_conditional_op(ctx, OpcodeCond::kTest, CondCode::kNotZero, 111111, 0, false);
  test_conditional_op(ctx, OpcodeCond::kTest, CondCode::kNotZero, 111111, -222, true);
  test_conditional_op(ctx, OpcodeCond::kTest, CondCode::kNotZero, -222, 111111, true);

  test_conditional_op(ctx, OpcodeCond::kBitTest, CondCode::kBTZero, 0x0, 0, true);
  test_conditional_op(ctx, OpcodeCond::kBitTest, CondCode::kBTZero, 0x1, 0, false);
  test_conditional_op(ctx, OpcodeCond::kBitTest, CondCode::kBTZero, 0xFF, 7, false);
  test_conditional_op(ctx, OpcodeCond::kBitTest, CondCode::kBTZero, 0xFF, 9, true);
  test_conditional_op(ctx, OpcodeCond::kBitTest, CondCode::kBTZero, 0xFFFFFFFF, 31, false);
  test_conditional_op(ctx, OpcodeCond::kBitTest, CondCode::kBTZero, 0x7FFFFFFF, 31, true);

  test_conditional_op(ctx, OpcodeCond::kBitTest, CondCode::kBTNotZero, 0x0, 0, false);
  test_conditional_op(ctx, OpcodeCond::kBitTest, CondCode::kBTNotZero, 0x1, 0, true);
  test_conditional_op(ctx, OpcodeCond::kBitTest, CondCode::kBTNotZero, 0xFF, 7, true);
  test_conditional_op(ctx, OpcodeCond::kBitTest, CondCode::kBTNotZero, 0xFF, 9, false);
  test_conditional_op(ctx, OpcodeCond::kBitTest, CondCode::kBTNotZero, 0xFFFFFFFF, 31, true);
  test_conditional_op(ctx, OpcodeCond::kBitTest, CondCode::kBTNotZero, 0x7FFFFFFF, 31, false);

  test_conditional_op(ctx, OpcodeCond::kAssignAnd, CondCode::kZero, 0x00000000, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAnd, CondCode::kZero, 0x00000001, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAnd, CondCode::kZero, 0x000000FF, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAnd, CondCode::kZero, 0x000000FF, 0x000000FF, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAnd, CondCode::kZero, 0xFFFFFFFF, 0xFF000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAnd, CondCode::kZero, 0x7FFFFFFF, 0x80000000, true);

  test_conditional_op(ctx, OpcodeCond::kAssignAnd, CondCode::kNotZero, 0x00000000, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAnd, CondCode::kNotZero, 0x00000001, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAnd, CondCode::kNotZero, 0x000000FF, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAnd, CondCode::kNotZero, 0x000000FF, 0x000000FF, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAnd, CondCode::kNotZero, 0xFFFFFFFF, 0xFF000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAnd, CondCode::kNotZero, 0x7FFFFFFF, 0x80000000, false);

  test_conditional_op(ctx, OpcodeCond::kAssignOr, CondCode::kZero, 0x00000000, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignOr, CondCode::kZero, 0x00000001, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignOr, CondCode::kZero, 0x000000FF, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignOr, CondCode::kZero, 0x000000FF, 0x000000FF, false);
  test_conditional_op(ctx, OpcodeCond::kAssignOr, CondCode::kZero, 0xFFFFFFFF, 0xFF000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignOr, CondCode::kZero, 0x7FFFFFFF, 0x80000000, false);

  test_conditional_op(ctx, OpcodeCond::kAssignOr, CondCode::kNotZero, 0x00000000, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignOr, CondCode::kNotZero, 0x00000001, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignOr, CondCode::kNotZero, 0x000000FF, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignOr, CondCode::kNotZero, 0x000000FF, 0x000000FF, true);
  test_conditional_op(ctx, OpcodeCond::kAssignOr, CondCode::kNotZero, 0xFFFFFFFF, 0xFF000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignOr, CondCode::kNotZero, 0x7FFFFFFF, 0x80000000, true);

  test_conditional_op(ctx, OpcodeCond::kAssignXor, CondCode::kZero, 0x00000000, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignXor, CondCode::kZero, 0x00000001, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignXor, CondCode::kZero, 0x000000FF, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignXor, CondCode::kZero, 0x000000FF, 0x000000FF, true);
  test_conditional_op(ctx, OpcodeCond::kAssignXor, CondCode::kZero, 0xFFFFFFFF, 0xFF000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignXor, CondCode::kZero, 0x7FFFFFFF, 0x80000000, false);

  test_conditional_op(ctx, OpcodeCond::kAssignXor, CondCode::kNotZero, 0x00000000, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignXor, CondCode::kNotZero, 0x00000001, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignXor, CondCode::kNotZero, 0x000000FF, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignXor, CondCode::kNotZero, 0x000000FF, 0x000000FF, false);
  test_conditional_op(ctx, OpcodeCond::kAssignXor, CondCode::kNotZero, 0xFFFFFFFF, 0xFF000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignXor, CondCode::kNotZero, 0x7FFFFFFF, 0x80000000, true);

  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kZero, 0x00000000, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kZero, 0xFF000000, 0x01000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kZero, 0x000000FF, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kZero, 0x000000FF, 0x000000FF, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kZero, 0xFFFFFFFF, 0xFF000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kZero, 0x7FFFFFFF, 0x80000000, false);

  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kNotZero, 0x00000000, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kNotZero, 0xFF000000, 0x01000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kNotZero, 0x000000FF, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kNotZero, 0x000000FF, 0x000000FF, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kNotZero, 0xFFFFFFFF, 0xFF000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kNotZero, 0x7FFFFFFF, 0x80000000, true);

  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kCarry, 0x00000000, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kCarry, 0xFF000000, 0x01000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kCarry, 0x000000FF, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kCarry, 0x000000FF, 0x000000FF, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kCarry, 0xFFFFFFFF, 0xFF000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kCarry, 0x7FFFFFFF, 0x80000000, false);

  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kNotCarry, 0x00000000, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kNotCarry, 0xFF000000, 0x01000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kNotCarry, 0x000000FF, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kNotCarry, 0x000000FF, 0x000000FF, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kNotCarry, 0xFFFFFFFF, 0xFF000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kNotCarry, 0x7FFFFFFF, 0x80000000, true);

  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kSign, 0x00000000, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kSign, 0xFF000000, 0x01000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kSign, 0x000000FF, 0x80000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kSign, 0x000000FF, 0x800000FF, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kSign, 0xFFFFFFFF, 0xFF000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kSign, 0x7FFFFFFF, 0x80000000, true);

  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kNotSign, 0x00000000, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kNotSign, 0xFF000000, 0x01000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kNotSign, 0x000000FF, 0x80000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kNotSign, 0x000000FF, 0x800000FF, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kNotSign, 0xFFFFFFFF, 0xFF000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignAdd, CondCode::kNotSign, 0x7FFFFFFF, 0x80000000, false);

  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kZero, 0x00000000, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kZero, 0xFF000000, 0x01000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kZero, 0x000000FF, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kZero, 0x000000FF, 0x000000FF, true);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kZero, 0xFFFFFFFF, 0xFF000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kZero, 0x7FFFFFFF, 0x80000000, false);

  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kNotZero, 0x00000000, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kNotZero, 0xFF000000, 0x01000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kNotZero, 0x000000FF, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kNotZero, 0x000000FF, 0x000000FF, false);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kNotZero, 0xFFFFFFFF, 0xFF000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kNotZero, 0x7FFFFFFF, 0x80000000, true);

  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedLT, 0x00000000, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedLT, 0xFF000000, 0x01000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedLT, 0x000000FF, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedLT, 0x000000FF, 0x000000FF, false);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedLT, 0xFFFFFFFF, 0xFF000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedLT, 0x7FFFFFFF, 0x80000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedLT, 0x00000111, 0x0000F0FF, true);

  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedGE, 0x00000000, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedGE, 0xFF000000, 0x01000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedGE, 0x000000FF, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedGE, 0x000000FF, 0x000000FF, true);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedGE, 0xFFFFFFFF, 0xFF000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedGE, 0x7FFFFFFF, 0x80000000, false);

  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kSign, 0x00000000, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kSign, 0x00000000, 0xFFFFFFFF, false);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kSign, 0x00000000, 0x00000001, true);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kSign, 0x00000001, 0x00000010, true);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kSign, 0xFFFFFFFF, 0xFF000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kSign, 0x7FFFFFFF, 0x80000000, true);

  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kNotSign, 0x00000000, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kNotSign, 0x00000000, 0xFFFFFFFF, true);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kNotSign, 0x00000000, 0x00000001, false);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kNotSign, 0x00000001, 0x00000010, false);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kNotSign, 0xFFFFFFFF, 0xFF000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kNotSign, 0x7FFFFFFF, 0x80000000, false);

  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedGT, 0x00000000, 0x00000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedGT, 0xFF000000, 0x01000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedGT, 0x000000FF, 0x00000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedGT, 0x000000FF, 0x000000FF, false);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedGT, 0xFFFFFFFF, 0xFF000000, true);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedGT, 0x7FFFFFFF, 0x80000000, false);
  test_conditional_op(ctx, OpcodeCond::kAssignSub, CondCode::kUnsignedGT, 0x00000111, 0x0000F0FF, false);

  test_conditional_op(ctx, OpcodeCond::kAssignShr, CondCode::kZero, 0x00000000, 1, true);
  test_conditional_op(ctx, OpcodeCond::kAssignShr, CondCode::kZero, 0x000000FF, 8, true);
  test_conditional_op(ctx, OpcodeCond::kAssignShr, CondCode::kZero, 0x000000FF, 7, false);
  test_conditional_op(ctx, OpcodeCond::kAssignShr, CondCode::kZero, 0xFFFFFFFF, 31, false);
  test_conditional_op(ctx, OpcodeCond::kAssignShr, CondCode::kZero, 0x7FFFFFFF, 31, true);

  test_conditional_op(ctx, OpcodeCond::kAssignShr, CondCode::kNotZero, 0x00000000, 1, false);
  test_conditional_op(ctx, OpcodeCond::kAssignShr, CondCode::kNotZero, 0x000000FF, 8, false);
  test_conditional_op(ctx, OpcodeCond::kAssignShr, CondCode::kNotZero, 0x000000FF, 7, true);
  test_conditional_op(ctx, OpcodeCond::kAssignShr, CondCode::kNotZero, 0xFFFFFFFF, 31, true);
  test_conditional_op(ctx, OpcodeCond::kAssignShr, CondCode::kNotZero, 0x7FFFFFFF, 31, false);
}

// bl::Pipeline::JIT - Tests - M Operations - Functions
// ====================================================

static TestMFunc create_func_m(JitContext& ctx, OpcodeM op) noexcept {
  ctx.prepare();
  PipeCompiler pc(&ctx.cc, ctx.features, ctx.optFlags);

  asmjit::FuncNode* node = ctx.cc.newFunc(asmjit::FuncSignature::build<void, void*>());
  EXPECT_NOT_NULL(node);

  pc.initVecWidth(VecWidth::k128);
  pc.initFunction(node);

  Gp ptr = pc.newGpPtr("ptr");
  node->setArg(0, ptr);
  pc.emit_m(op, mem_ptr(ptr));

  ctx.cc.endFunc();
  return ctx.finish<TestMFunc>();
}

// bl::Pipeline::JIT - Tests - M Operations - Runner
// =================================================

static BL_NOINLINE void test_m_ops(JitContext& ctx) noexcept {
  uint8_t buffer[8];

  TestMFunc fn_zero_u8 = create_func_m(ctx, OpcodeM::kStoreZeroU8);
  memcpy(buffer, "ABCDEFGH", 8);
  fn_zero_u8(buffer + 0);
  EXPECT_EQ(memcmp(buffer, "\0BCDEFGH", 8), 0);
  fn_zero_u8(buffer + 5);
  EXPECT_EQ(memcmp(buffer, "\0BCDE\0GH", 8), 0);

  TestMFunc fn_zero_u16 = create_func_m(ctx, OpcodeM::kStoreZeroU16);
  memcpy(buffer, "ABCDEFGH", 8);
  fn_zero_u16(buffer + 0);
  EXPECT_EQ(memcmp(buffer, "\0\0CDEFGH", 8), 0);
  fn_zero_u16(buffer + 4);
  EXPECT_EQ(memcmp(buffer, "\0\0CD\0\0GH", 8), 0);

  TestMFunc fn_zero_u32 = create_func_m(ctx, OpcodeM::kStoreZeroU32);
  memcpy(buffer, "ABCDEFGH", 8);
  fn_zero_u32(buffer + 0);
  EXPECT_EQ(memcmp(buffer, "\0\0\0\0EFGH", 8), 0);
  fn_zero_u32(buffer + 4);
  EXPECT_EQ(memcmp(buffer, "\0\0\0\0\0\0\0\0", 8), 0);

#if BL_TARGET_ARCH_BITS >= 64
  TestMFunc fn_zero_u64 = create_func_m(ctx, OpcodeM::kStoreZeroU64);
  memcpy(buffer, "ABCDEFGH", 8);
  fn_zero_u64(buffer + 0);
  EXPECT_EQ(memcmp(buffer, "\0\0\0\0\0\0\0\0", 8), 0);
#endif

  TestMFunc fn_zero_reg = create_func_m(ctx, OpcodeM::kStoreZeroReg);
  memcpy(buffer, "ABCDEFGH", 8);
  fn_zero_reg(buffer + 0);
#if BL_TARGET_ARCH_BITS >= 64
  EXPECT_EQ(memcmp(buffer, "\0\0\0\0\0\0\0\0", 8), 0);
#else
  EXPECT_EQ(memcmp(buffer, "\0\0\0\0EFGH", 8), 0);
#endif

  ctx.rt.reset();
}

// bl::Pipeline::JIT - Tests - RM Operations - Functions
// =====================================================

static TestRMFunc create_func_rm(JitContext& ctx, OpcodeRM op) noexcept {
  ctx.prepare();
  PipeCompiler pc(&ctx.cc, ctx.features, ctx.optFlags);

  asmjit::FuncNode* node = ctx.cc.newFunc(asmjit::FuncSignature::build<uintptr_t, uintptr_t, void*>());
  EXPECT_NOT_NULL(node);

  pc.initVecWidth(VecWidth::k128);
  pc.initFunction(node);

  Gp reg = pc.newGpPtr("reg");
  Gp ptr = pc.newGpPtr("ptr");

  node->setArg(0, reg);
  node->setArg(1, ptr);

  pc.emit_rm(op, reg, mem_ptr(ptr));
  ctx.cc.ret(reg);

  ctx.cc.endFunc();
  return ctx.finish<TestRMFunc>();
}

// bl::Pipeline::JIT - Tests - RM Operations - Runner
// ==================================================

static BL_NOINLINE void test_rm_ops(JitContext& ctx) noexcept {
  union Mem {
    uint8_t buffer[8];
    uint16_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
  };

  Mem mem{};

  TestRMFunc fn_load_i8 = create_func_rm(ctx, OpcodeRM::kLoadI8);
  mem.u8 = uint8_t(int8_t(6));
  EXPECT_EQ(fn_load_i8(0, mem.buffer), uintptr_t(intptr_t(6)));

  mem.u8 = uint8_t(int8_t(-6));
  EXPECT_EQ(fn_load_i8(0, mem.buffer), uintptr_t(intptr_t(-6)));

  TestRMFunc fn_load_u8 = create_func_rm(ctx, OpcodeRM::kLoadU8);
  mem.u8 = uint8_t(0x80);
  EXPECT_EQ(fn_load_u8(0, mem.buffer), 0x80u);

  mem.u8 = uint8_t(0xFF);
  EXPECT_EQ(fn_load_u8(0, mem.buffer), 0xFFu);

  TestRMFunc fn_load_i16 = create_func_rm(ctx, OpcodeRM::kLoadI16);
  mem.u16 = uint16_t(int16_t(666));
  EXPECT_EQ(fn_load_i16(0, mem.buffer), uintptr_t(intptr_t(666)));

  mem.u16 = uint16_t(int16_t(-666));
  EXPECT_EQ(fn_load_i16(0, mem.buffer), uintptr_t(intptr_t(-666)));

  TestRMFunc fn_load_u16 = create_func_rm(ctx, OpcodeRM::kLoadU16);
  mem.u16 = uint16_t(0x8000);
  EXPECT_EQ(fn_load_u16(0, mem.buffer), 0x8000u);

  mem.u16 = uint16_t(0xFEED);
  EXPECT_EQ(fn_load_u16(0, mem.buffer), 0xFEEDu);

  TestRMFunc fn_load_i32 = create_func_rm(ctx, OpcodeRM::kLoadI32);
  mem.u32 = uint32_t(int32_t(666666));
  EXPECT_EQ(fn_load_i32(0, mem.buffer), uintptr_t(intptr_t(666666)));

  mem.u32 = uint32_t(int32_t(-666666));
  EXPECT_EQ(fn_load_i32(0, mem.buffer), uintptr_t(intptr_t(-666666)));

  TestRMFunc fn_load_u32 = create_func_rm(ctx, OpcodeRM::kLoadU32);
  mem.u32 = 0x12345678;
  EXPECT_EQ(fn_load_u32(0, mem.buffer), uint32_t(0x12345678));

#if BL_TARGET_ARCH_BITS >= 64
  TestRMFunc fn_load_i64 = create_func_rm(ctx, OpcodeRM::kLoadI64);
  mem.u64 = 0xF123456789ABCDEFu;
  EXPECT_EQ(fn_load_i64(0, mem.buffer), 0xF123456789ABCDEFu);

  TestRMFunc fn_load_u64 = create_func_rm(ctx, OpcodeRM::kLoadU64);
  mem.u64 = 0xF123456789ABCDEFu;
  EXPECT_EQ(fn_load_u64(0, mem.buffer), 0xF123456789ABCDEFu);
#endif

  TestRMFunc fn_load_reg = create_func_rm(ctx, OpcodeRM::kLoadReg);
  mem.u64 = 0xF123456789ABCDEFu;
#if BL_TARGET_ARCH_BITS >= 64
  EXPECT_EQ(fn_load_reg(0, mem.buffer), 0xF123456789ABCDEFu);
#else
  EXPECT_EQ(fn_load_reg(0, mem.buffer), 0x89ABCDEFu);
#endif

  TestRMFunc fn_load_merge_u8 = create_func_rm(ctx, OpcodeRM::kLoadMergeU8);
  mem.u8 = uint8_t(0xAA);
  EXPECT_EQ(fn_load_merge_u8(0x1F2FFF00, mem.buffer), 0x1F2FFFAAu);

  TestRMFunc fn_load_shift_u8 = create_func_rm(ctx, OpcodeRM::kLoadShiftU8);
  mem.u8 = uint8_t(0xAA);
  EXPECT_EQ(fn_load_shift_u8(0x002FFF00, mem.buffer), 0x2FFF00AAu);

  TestRMFunc fn_load_merge_u16 = create_func_rm(ctx, OpcodeRM::kLoadMergeU16);
  mem.u16 = uint16_t(0xAABB);
  EXPECT_EQ(fn_load_merge_u16(0x1F2F0000, mem.buffer), 0x1F2FAABBu);

  TestRMFunc fn_load_shift_u16 = create_func_rm(ctx, OpcodeRM::kLoadShiftU16);
  mem.u16 = uint16_t(0xAABB);
  EXPECT_EQ(fn_load_shift_u16(0x00001F2F, mem.buffer), 0x1F2FAABBu);

  ctx.rt.reset();
}

// bl::Pipeline::JIT - Tests - MR Operations - Functions
// =====================================================

static TestMRFunc create_func_mr(JitContext& ctx, OpcodeMR op) noexcept {
  ctx.prepare();
  PipeCompiler pc(&ctx.cc, ctx.features, ctx.optFlags);

  asmjit::FuncNode* node = ctx.cc.newFunc(asmjit::FuncSignature::build<void, void*, uintptr_t>());
  EXPECT_NOT_NULL(node);

  pc.initVecWidth(VecWidth::k128);
  pc.initFunction(node);

  Gp ptr = pc.newGpPtr("ptr");
  Gp reg = pc.newGpPtr("reg");

  node->setArg(0, ptr);
  node->setArg(1, reg);

  pc.emit_mr(op, mem_ptr(ptr), reg);

  ctx.cc.endFunc();
  return ctx.finish<TestMRFunc>();
}

// bl::Pipeline::JIT - Tests - MR Operations - Runner
// ==================================================

static BL_NOINLINE void test_mr_ops(JitContext& ctx) noexcept {
  union Mem {
    uint8_t buffer[8];
    uint16_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
  };

  Mem mem{};

  TestMRFunc fn_store_u8 = create_func_mr(ctx, OpcodeMR::kStoreU8);
  memcpy(mem.buffer, "ABCDEFGH", 8);
  fn_store_u8(mem.buffer, 0x7A);
  EXPECT_EQ(memcmp(mem.buffer, "zBCDEFGH", 8), 0);

  TestMRFunc fn_store_u16 = create_func_mr(ctx, OpcodeMR::kStoreU16);
  memcpy(mem.buffer, "ABCDEFGH", 8);
  fn_store_u16(mem.buffer, 0x7A7A);
  EXPECT_EQ(memcmp(mem.buffer, "zzCDEFGH", 8), 0);

  TestMRFunc fn_store_u32 = create_func_mr(ctx, OpcodeMR::kStoreU32);
  memcpy(mem.buffer, "ABCDEFGH", 8);
  fn_store_u32(mem.buffer, 0x7A7A7A7A);
  EXPECT_EQ(memcmp(mem.buffer, "zzzzEFGH", 8), 0);

#if BL_TARGET_ARCH_BITS >= 64
  TestMRFunc fn_store_u64 = create_func_mr(ctx, OpcodeMR::kStoreU64);
  memcpy(mem.buffer, "ABCDEFGH", 8);
  fn_store_u64(mem.buffer, 0x7A7A7A7A7A7A7A7A);
  EXPECT_EQ(memcmp(mem.buffer, "zzzzzzzz", 8), 0);
#endif

  TestMRFunc fn_store_reg = create_func_mr(ctx, OpcodeMR::kStoreReg);
  memcpy(mem.buffer, "ABCDEFGH", 8);
#if BL_TARGET_ARCH_BITS >= 64
  fn_store_reg(mem.buffer, 0x7A7A7A7A7A7A7A7A);
  EXPECT_EQ(memcmp(mem.buffer, "zzzzzzzz", 8), 0);
#else
  fn_store_reg(mem.buffer, 0x7A7A7A7A);
  EXPECT_EQ(memcmp(mem.buffer, "zzzzEFGH", 8), 0);
#endif

  TestMRFunc fn_add_u8 = create_func_mr(ctx, OpcodeMR::kAddU8);
  mem.u64 = 0;
  mem.u8 = 42;
  fn_add_u8(mem.buffer, 13);
  EXPECT_EQ(mem.u8, 55u);
  EXPECT_EQ(memcmp(mem.buffer + 1, "\0\0\0\0\0\0\0", 7), 0);

  TestMRFunc fn_add_u16 = create_func_mr(ctx, OpcodeMR::kAddU16);
  mem.u64 = 0;
  mem.u16 = 442;
  fn_add_u16(mem.buffer, 335);
  EXPECT_EQ(mem.u16, 777u);
  EXPECT_EQ(memcmp(mem.buffer + 2, "\0\0\0\0\0\0", 6), 0);

  TestMRFunc fn_add_u32 = create_func_mr(ctx, OpcodeMR::kAddU32);
  mem.u64 = 0;
  mem.u32 = 442332;
  fn_add_u32(mem.buffer, 335223);
  EXPECT_EQ(mem.u32, 777555u);
  EXPECT_EQ(memcmp(mem.buffer + 2, "\0\0\0\0\0\0", 6), 0);

#if BL_TARGET_ARCH_BITS >= 64
  TestMRFunc fn_add_u64 = create_func_mr(ctx, OpcodeMR::kAddU64);
  mem.u64 = 0xF123456789ABCDEFu;
  fn_add_u64(mem.buffer, 0x0102030405060708u);
  EXPECT_EQ(mem.u64, 0xF225486B8EB1D4F7u);
#endif

  TestMRFunc fn_add_reg = create_func_mr(ctx, OpcodeMR::kAddReg);
  mem.u64 = 0xFFFFFFFFFFFFFFFF;
#if BL_TARGET_ARCH_BITS >= 64
  fn_add_reg(mem.buffer, 1);
  EXPECT_EQ(mem.u64, 0u);
#else
  mem.u32 = 0x01020304;
  fn_add_reg(mem.buffer, 0x02030405);
  EXPECT_EQ(mem.u32, 0x03050709u);
  EXPECT_EQ(memcmp(mem.buffer + 4, "\255\255\255\255", 4), 0);
#endif

  ctx.rt.reset();
}

// bl::Pipeline::JIT - Tests - RR Operations - Functions
// =====================================================

static TestRRFunc create_func_rr(JitContext& ctx, OpcodeRR op) noexcept {
  ctx.prepare();
  PipeCompiler pc(&ctx.cc, ctx.features, ctx.optFlags);

  asmjit::FuncNode* node = ctx.cc.newFunc(asmjit::FuncSignature::build<uint32_t, uint32_t>());
  EXPECT_NOT_NULL(node);

  pc.initVecWidth(VecWidth::k128);
  pc.initFunction(node);

  Gp r = pc.newGp32("r");
  node->setArg(0, r);
  pc.emit_2i(op, r, r);
  ctx.cc.ret(r);

  ctx.cc.endFunc();
  return ctx.finish<TestRRFunc>();
}

// bl::Pipeline::JIT - Tests - RR Operations - Runner
// ==================================================

static BL_NOINLINE void test_rr_ops(JitContext& ctx) noexcept {
  TestRRFunc fn_abs = create_func_rr(ctx, OpcodeRR::kAbs);
  EXPECT_EQ(fn_abs(0u), 0u);
  EXPECT_EQ(fn_abs(1u), 1u);
  EXPECT_EQ(fn_abs(uint32_t(-1)), 1u);
  EXPECT_EQ(fn_abs(uint32_t(-333)), 333u);
  EXPECT_EQ(fn_abs(0x80000000u), 0x80000000u);

  TestRRFunc fn_neg = create_func_rr(ctx, OpcodeRR::kNeg);
  EXPECT_EQ(fn_neg(0u), 0u);
  EXPECT_EQ(fn_neg(1u), uint32_t(-1));
  EXPECT_EQ(fn_neg(uint32_t(-1)), 1u);
  EXPECT_EQ(fn_neg(uint32_t(-333)), 333u);
  EXPECT_EQ(fn_neg(333u), uint32_t(-333));
  EXPECT_EQ(fn_neg(0x80000000u), 0x80000000u);

  TestRRFunc fn_not = create_func_rr(ctx, OpcodeRR::kNot);
  EXPECT_EQ(fn_not(0u), 0xFFFFFFFFu);
  EXPECT_EQ(fn_not(1u), 0xFFFFFFFEu);
  EXPECT_EQ(fn_not(0xFFFFFFFF), 0u);
  EXPECT_EQ(fn_not(0x12333245), ~0x12333245u);
  EXPECT_EQ(fn_not(0x80000000u), 0x7FFFFFFFu);

  TestRRFunc fn_bswap32 = create_func_rr(ctx, OpcodeRR::kBSwap);
  EXPECT_EQ(fn_bswap32(0x11223344u), 0x44332211u);
  EXPECT_EQ(fn_bswap32(0xFFFF0000u), 0x0000FFFFu);
  EXPECT_EQ(fn_bswap32(0x00000000u), 0x00000000u);

  TestRRFunc fn_clz32 = create_func_rr(ctx, OpcodeRR::kCLZ);
  EXPECT_EQ(fn_clz32(0x80000000u), 0u);
  EXPECT_EQ(fn_clz32(0x40000000u), 1u);
  EXPECT_EQ(fn_clz32(0x00800000u), 8u);
  EXPECT_EQ(fn_clz32(0x00008000u), 16u);
  EXPECT_EQ(fn_clz32(0x00000080u), 24u);
  EXPECT_EQ(fn_clz32(0x00000001u), 31u);

  TestRRFunc fn_ctz32 = create_func_rr(ctx, OpcodeRR::kCTZ);
  EXPECT_EQ(fn_ctz32(0x80000000u), 31u);
  EXPECT_EQ(fn_ctz32(0x40000000u), 30u);
  EXPECT_EQ(fn_ctz32(0x00800000u), 23u);
  EXPECT_EQ(fn_ctz32(0x00008000u), 15u);
  EXPECT_EQ(fn_ctz32(0x00000080u), 7u);
  EXPECT_EQ(fn_ctz32(0x00000001u), 0u);

  TestRRFunc fn_reflect = create_func_rr(ctx, OpcodeRR::kReflect);
  EXPECT_EQ(fn_reflect(0x00000000u), 0x00000000u);
  EXPECT_EQ(fn_reflect(0x00FF0000u), 0x00FF0000u);
  EXPECT_EQ(fn_reflect(0x000000FFu), 0x000000FFu);
  EXPECT_EQ(fn_reflect(0x80000000u), 0x7FFFFFFFu);
  EXPECT_EQ(fn_reflect(0xFFFFFFFFu), 0x00000000u);
  EXPECT_EQ(fn_reflect(0x88FF0000u), 0x7700FFFFu);

  ctx.rt.reset();
}

// bl::Pipeline::JIT - Tests - RRR Operations - Functions
// ======================================================

static TestRRRFunc create_func_rrr(JitContext& ctx, OpcodeRRR op) noexcept {
  ctx.prepare();
  PipeCompiler pc(&ctx.cc, ctx.features, ctx.optFlags);

  asmjit::FuncNode* node = ctx.cc.newFunc(asmjit::FuncSignature::build<uint32_t, uint32_t, uint32_t>());
  EXPECT_NOT_NULL(node);

  pc.initVecWidth(VecWidth::k128);
  pc.initFunction(node);

  Gp a = pc.newGp32("a");
  Gp b = pc.newGp32("b");
  Gp result = pc.newGp32("result");

  node->setArg(0, a);
  node->setArg(1, b);

  pc.emit_3i(op, result, a, b);
  ctx.cc.ret(result);

  ctx.cc.endFunc();
  return ctx.finish<TestRRRFunc>();
}

static TestRRIFunc create_func_rri(JitContext& ctx, OpcodeRRR op, Imm bImm) noexcept {
  ctx.prepare();
  PipeCompiler pc(&ctx.cc, ctx.features, ctx.optFlags);

  asmjit::FuncNode* node = ctx.cc.newFunc(asmjit::FuncSignature::build<uint32_t, uint32_t>());
  EXPECT_NOT_NULL(node);

  pc.initVecWidth(VecWidth::k128);
  pc.initFunction(node);

  Gp a = pc.newGp32("a");
  Gp result = pc.newGp32("result");

  node->setArg(0, a);

  pc.emit_3i(op, result, a, bImm);
  ctx.cc.ret(result);

  ctx.cc.endFunc();
  return ctx.finish<TestRRIFunc>();
}

// bl::Pipeline::JIT - Tests - RRR Operations - Runner
// ===================================================

static BL_NOINLINE void test_rrr_op(JitContext& ctx, OpcodeRRR op, uint32_t a, uint32_t b, uint32_t expected) noexcept {
  TestRRRFunc fn_rrr = create_func_rrr(ctx, op);
  uint32_t observed_rrr = fn_rrr(a, b);
  EXPECT_EQ(observed_rrr, expected)
    .message("Operation failed (RRR):\n"
            "      Input #1: %d\n"
            "      Input #2: %d\n"
            "      Expected: %d\n"
            "      Observed: %d\n"
            "Assembly:\n%s",
            a,
            b,
            uint32_t(expected),
            observed_rrr,
            ctx.logger.data());

  TestRRIFunc fn_rri = create_func_rri(ctx, op, Imm(b));
  uint32_t observed_rri = fn_rri(a);
  EXPECT_EQ(observed_rri, expected)
    .message("Operation failed (RRI):\n"
            "      Input #1: %d\n"
            "      Input #2: %d\n"
            "      Expected: %d\n"
            "      Observed: %d\n"
            "Assembly:\n%s",
            a,
            b,
            uint32_t(expected),
            observed_rri,
            ctx.logger.data());

  ctx.rt.reset();
}

static BL_NOINLINE void test_rrr_ops(JitContext& ctx) noexcept {
  test_rrr_op(ctx, OpcodeRRR::kAnd, 0u, 0u, 0u);
  test_rrr_op(ctx, OpcodeRRR::kAnd, 0xFFu, 0x11u, 0x11u);
  test_rrr_op(ctx, OpcodeRRR::kAnd, 0x11u, 0xFFu, 0x11u);
  test_rrr_op(ctx, OpcodeRRR::kAnd, 0xFF11u, 0x1111u, 0x1111u);
  test_rrr_op(ctx, OpcodeRRR::kAnd, 0x1111u, 0xFF11u, 0x1111u);
  test_rrr_op(ctx, OpcodeRRR::kAnd, 0x0000FFFFu, 0xFFFF0000u, 0u);
  test_rrr_op(ctx, OpcodeRRR::kAnd, 0xFFFFFFFFu, 0xFFFF0000u, 0xFFFF0000u);
  test_rrr_op(ctx, OpcodeRRR::kAnd, 0x11111111u, 0x11223344u, 0x11001100u);

  test_rrr_op(ctx, OpcodeRRR::kOr, 0u, 0u, 0u);
  test_rrr_op(ctx, OpcodeRRR::kOr, 0xFFu, 0x11u, 0xFFu);
  test_rrr_op(ctx, OpcodeRRR::kOr, 0x11u, 0xFFu, 0xFFu);
  test_rrr_op(ctx, OpcodeRRR::kOr, 0xFF11u, 0x1111u, 0xFF11u);
  test_rrr_op(ctx, OpcodeRRR::kOr, 0x1111u, 0xFF11u, 0xFF11u);
  test_rrr_op(ctx, OpcodeRRR::kOr, 0x0000FFFFu, 0xFFFF0001u, 0xFFFFFFFFu);
  test_rrr_op(ctx, OpcodeRRR::kOr, 0xFFFFFFFFu, 0xFF000000u, 0xFFFFFFFFu);
  test_rrr_op(ctx, OpcodeRRR::kOr, 0x11111111u, 0x00223344u, 0x11333355u);

  test_rrr_op(ctx, OpcodeRRR::kXor, 0u, 0u, 0u);
  test_rrr_op(ctx, OpcodeRRR::kXor, 0xFFu, 0x11u, 0xEEu);
  test_rrr_op(ctx, OpcodeRRR::kXor, 0x11u, 0xFFu, 0xEEu);
  test_rrr_op(ctx, OpcodeRRR::kXor, 0xFF11u, 0x1111u, 0xEE00u);
  test_rrr_op(ctx, OpcodeRRR::kXor, 0x1111u, 0xFF11u, 0xEE00u);
  test_rrr_op(ctx, OpcodeRRR::kXor, 0x0000FFFFu, 0xFFFF0001u, 0xFFFFFFFEu);
  test_rrr_op(ctx, OpcodeRRR::kXor, 0xFFFFFFFFu, 0xFF000000u, 0x00FFFFFFu);
  test_rrr_op(ctx, OpcodeRRR::kXor, 0x11111111u, 0x00223344u, 0x11332255u);

  test_rrr_op(ctx, OpcodeRRR::kBic, 0u, 0u, 0u);
  test_rrr_op(ctx, OpcodeRRR::kBic, 0xFFu, 0x11u, 0xEEu);
  test_rrr_op(ctx, OpcodeRRR::kBic, 0x11u, 0xFFu, 0x00u);
  test_rrr_op(ctx, OpcodeRRR::kBic, 0xFF11u, 0x1111u, 0xEE00u);
  test_rrr_op(ctx, OpcodeRRR::kBic, 0x1111u, 0xFF11u, 0x0000u);
  test_rrr_op(ctx, OpcodeRRR::kBic, 0x0000FFFFu, 0xFFFF0000u, 0x0000FFFFu);
  test_rrr_op(ctx, OpcodeRRR::kBic, 0xFFFFFFFFu, 0xFFFF0000u, 0x0000FFFFu);
  test_rrr_op(ctx, OpcodeRRR::kBic, 0x11111111u, 0x11223344u, 0x00110011u);

  test_rrr_op(ctx, OpcodeRRR::kAdd, 0u, 0u, 0u);
  test_rrr_op(ctx, OpcodeRRR::kAdd, 1u, 2u, 3u);
  test_rrr_op(ctx, OpcodeRRR::kAdd, 0xFF000000u, 0x00FFFFFFu, 0xFFFFFFFFu);
  test_rrr_op(ctx, OpcodeRRR::kAdd, 1u, 0xFFFu, 0x1000u);
  test_rrr_op(ctx, OpcodeRRR::kAdd, 1u, 0xFFF000u, 0xFFF001u);

  test_rrr_op(ctx, OpcodeRRR::kSub, 1u, 2u, 0xFFFFFFFFu);

  test_rrr_op(ctx, OpcodeRRR::kMul, 1000u, 999u, 999000u);
  test_rrr_op(ctx, OpcodeRRR::kMul, 0xFFFFu, 0x00010001u, 0xFFFFFFFFu);

  test_rrr_op(ctx, OpcodeRRR::kUDiv, 100000u, 1000u, 100u);

  test_rrr_op(ctx, OpcodeRRR::kUMod, 1999u, 1000u, 999u);

  test_rrr_op(ctx, OpcodeRRR::kSMin, uint32_t(1111), uint32_t(0), uint32_t(0));
  test_rrr_op(ctx, OpcodeRRR::kSMin, uint32_t(-1111), uint32_t(0), uint32_t(-1111));
  test_rrr_op(ctx, OpcodeRRR::kSMin, uint32_t(1), uint32_t(22), uint32_t(1));
  test_rrr_op(ctx, OpcodeRRR::kSMin, uint32_t(1), uint32_t(0), uint32_t(0));
  test_rrr_op(ctx, OpcodeRRR::kSMin, uint32_t(100101033), uint32_t(999), uint32_t(999));
  test_rrr_op(ctx, OpcodeRRR::kSMin, uint32_t(100101033), uint32_t(112), uint32_t(112));
  test_rrr_op(ctx, OpcodeRRR::kSMin, uint32_t(112), uint32_t(1125532), uint32_t(112));
  test_rrr_op(ctx, OpcodeRRR::kSMin, uint32_t(1111), uint32_t(-1), uint32_t(-1));
  test_rrr_op(ctx, OpcodeRRR::kSMin, uint32_t(-1111), uint32_t(-1), uint32_t(-1111));
  test_rrr_op(ctx, OpcodeRRR::kSMin, uint32_t(-1), uint32_t(-22), uint32_t(-22));
  test_rrr_op(ctx, OpcodeRRR::kSMin, uint32_t(-1), uint32_t(-128), uint32_t(-128));
  test_rrr_op(ctx, OpcodeRRR::kSMin, uint32_t(-128), uint32_t(-1), uint32_t(-128));
  test_rrr_op(ctx, OpcodeRRR::kSMin, uint32_t(-128), uint32_t(9), uint32_t(-128));
  test_rrr_op(ctx, OpcodeRRR::kSMin, uint32_t(12444), uint32_t(-1), uint32_t(-1));

  test_rrr_op(ctx, OpcodeRRR::kSMax, uint32_t(1), uint32_t(22), uint32_t(22));
  test_rrr_op(ctx, OpcodeRRR::kSMax, uint32_t(1), uint32_t(0), uint32_t(1));
  test_rrr_op(ctx, OpcodeRRR::kSMax, uint32_t(100101033), uint32_t(999), uint32_t(100101033));
  test_rrr_op(ctx, OpcodeRRR::kSMax, uint32_t(100101033), uint32_t(112), uint32_t(100101033));
  test_rrr_op(ctx, OpcodeRRR::kSMax, uint32_t(112), uint32_t(1125532), uint32_t(1125532));
  test_rrr_op(ctx, OpcodeRRR::kSMax, uint32_t(1111), uint32_t(-1), uint32_t(1111));
  test_rrr_op(ctx, OpcodeRRR::kSMax, uint32_t(-1111), uint32_t(-1), uint32_t(-1));
  test_rrr_op(ctx, OpcodeRRR::kSMax, uint32_t(-1), uint32_t(-22), uint32_t(-1));
  test_rrr_op(ctx, OpcodeRRR::kSMax, uint32_t(-1), uint32_t(-128), uint32_t(-1));
  test_rrr_op(ctx, OpcodeRRR::kSMax, uint32_t(-128), uint32_t(-1), uint32_t(-1));
  test_rrr_op(ctx, OpcodeRRR::kSMax, uint32_t(-128), uint32_t(9), uint32_t(9));
  test_rrr_op(ctx, OpcodeRRR::kSMax, uint32_t(12444), uint32_t(-1), uint32_t(12444));

  test_rrr_op(ctx, OpcodeRRR::kUMin, 1, 22, 1);
  test_rrr_op(ctx, OpcodeRRR::kUMin, 22, 1, 1);
  test_rrr_op(ctx, OpcodeRRR::kUMin, 1, 255, 1);
  test_rrr_op(ctx, OpcodeRRR::kUMin, 255, 1, 1);
  test_rrr_op(ctx, OpcodeRRR::kUMin, 1023, 255, 255);
  test_rrr_op(ctx, OpcodeRRR::kUMin, 255, 1023, 255);
  test_rrr_op(ctx, OpcodeRRR::kUMin, 0xFFFFFFFFu, 255, 255);
  test_rrr_op(ctx, OpcodeRRR::kUMin, 255, 0xFFFFFFFFu, 255);
  test_rrr_op(ctx, OpcodeRRR::kUMin, 0xFFFFFFFFu, 0xFFFFFF00u, 0xFFFFFF00u);
  test_rrr_op(ctx, OpcodeRRR::kUMin, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu);

  test_rrr_op(ctx, OpcodeRRR::kUMax, 1, 22, 22);
  test_rrr_op(ctx, OpcodeRRR::kUMax, 22, 1, 22);
  test_rrr_op(ctx, OpcodeRRR::kUMax, 1, 255, 255);
  test_rrr_op(ctx, OpcodeRRR::kUMax, 255, 1, 255);
  test_rrr_op(ctx, OpcodeRRR::kUMax, 1023, 255, 1023);
  test_rrr_op(ctx, OpcodeRRR::kUMax, 255, 1023, 1023);
  test_rrr_op(ctx, OpcodeRRR::kUMax, 0xFFFFFFFFu, 255, 0xFFFFFFFFu);
  test_rrr_op(ctx, OpcodeRRR::kUMax, 255, 0xFFFFFFFFu, 0xFFFFFFFFu);
  test_rrr_op(ctx, OpcodeRRR::kUMax, 0xFFFFFFFFu, 0xFFFFFF00u, 0xFFFFFFFFu);
  test_rrr_op(ctx, OpcodeRRR::kUMax, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu);

  test_rrr_op(ctx, OpcodeRRR::kSll, 1u, 1u, 1u << 1);
  test_rrr_op(ctx, OpcodeRRR::kSll, 1u, 22u, 1u << 22);
  test_rrr_op(ctx, OpcodeRRR::kSll, 1u, 31u, 1u << 31);
  test_rrr_op(ctx, OpcodeRRR::kSll, 0x7FFFFFFFu, 1u, 0xFFFFFFFEu);

  test_rrr_op(ctx, OpcodeRRR::kSrl, 1u, 1u, 1u >> 1);
  test_rrr_op(ctx, OpcodeRRR::kSrl, 1u, 22u, 1u >> 22);
  test_rrr_op(ctx, OpcodeRRR::kSrl, 1u, 31u, 1u >> 31);
  test_rrr_op(ctx, OpcodeRRR::kSrl, 0x7FFFFFFFu, 1u, 0x7FFFFFFFu >> 1);

  test_rrr_op(ctx, OpcodeRRR::kSra, 1u, 1u, 1u >> 1);
  test_rrr_op(ctx, OpcodeRRR::kSra, 1u, 22u, 1u >> 22);
  test_rrr_op(ctx, OpcodeRRR::kSra, 1u, 31u, 1u >> 31);
  test_rrr_op(ctx, OpcodeRRR::kSra, 0x7FFFFFFFu, 1u, 0x7FFFFFFFu >> 1);
  test_rrr_op(ctx, OpcodeRRR::kSra, 0xF0000000u, 4u, 0xFF000000u);
  test_rrr_op(ctx, OpcodeRRR::kSra, 0x80000000u, 31u, 0xFFFFFFFFu);

  test_rrr_op(ctx, OpcodeRRR::kRol, 0x11223344u, 8u, 0x22334411u);
  test_rrr_op(ctx, OpcodeRRR::kRol, 0x11223344u, 16u, 0x33441122u);
  test_rrr_op(ctx, OpcodeRRR::kRol, 0xFCFFDABBu, 1u, 0xF9FFB577u);

  test_rrr_op(ctx, OpcodeRRR::kRor, 0x11223344u, 8u, 0x44112233u);
  test_rrr_op(ctx, OpcodeRRR::kRor, 0x11223344u, 16u, 0x33441122u);
  test_rrr_op(ctx, OpcodeRRR::kRor, 0xF0000000u, 1u, 0x78000000u);

  test_rrr_op(ctx, OpcodeRRR::kSBound, 0, 244u, 0);
  test_rrr_op(ctx, OpcodeRRR::kSBound, 42, 244u, 42u);
  test_rrr_op(ctx, OpcodeRRR::kSBound, 1111, 244u, 244u);
  test_rrr_op(ctx, OpcodeRRR::kSBound, 9999999, 111244u, 111244u);
  test_rrr_op(ctx, OpcodeRRR::kSBound, uint32_t(int32_t(-1)), 1000u, 0u);
  test_rrr_op(ctx, OpcodeRRR::kSBound, uint32_t(INT32_MIN), 100000u, 0u);
  test_rrr_op(ctx, OpcodeRRR::kSBound, uint32_t(INT32_MAX), 0u, 0u);
  test_rrr_op(ctx, OpcodeRRR::kSBound, uint32_t(INT32_MAX), 100000u, 100000u);
  test_rrr_op(ctx, OpcodeRRR::kSBound, uint32_t(INT32_MAX), uint32_t(INT32_MAX), uint32_t(INT32_MAX));
}

// bl::Pipeline::JIT - Tests - SIMD - Functions
// ============================================

// The following variations are supported:
//   - 0 - separate destination & source registers
//   - 1 - destination register is a source register as well
//   - 2 - source is a memory operand
//   - 3 - source register is a GP register (only for broadcasts from a GP register, otherwise maps to 0)
static constexpr uint32_t kNumVariationsVV = 3;
static constexpr uint32_t kNumVariationsVV_Broadcast = 4;

static TestVVFunc create_func_vv(JitContext& ctx, VecWidth vw, OpcodeVV op, Variation variation = Variation{0}) noexcept {
  ctx.prepare();
  PipeCompiler pc(&ctx.cc, ctx.features, ctx.optFlags);

  asmjit::FuncNode* node = ctx.cc.newFunc(asmjit::FuncSignature::build<void, void*, const void*>());
  EXPECT_NOT_NULL(node);

  pc.initVecWidth(vw);
  pc.initFunction(node);

  Gp dstPtr = pc.newGpPtr("dstPtr");
  Gp srcPtr = pc.newGpPtr("srcPtr");

  node->setArg(0, dstPtr);
  node->setArg(1, srcPtr);

  Vec dstVec = pc.newVec(vw, "dstVec");

  // There are some instructions that fill the high part of the register, so just zero the destination to make
  // sure that we can test this function (that the low part is actually zeroed and doesn't contain garbage).
  pc.v_zero_i(dstVec);

  if (variation == 3u && (op == OpcodeVV::kBroadcastU8  ||
                          op == OpcodeVV::kBroadcastU8Z ||
                          op == OpcodeVV::kBroadcastU32 ||
                          op == OpcodeVV::kBroadcastU64 ||
                          op == OpcodeVV::kBroadcastF32 ||
                          op == OpcodeVV::kBroadcastF64)) {
    // This is used to test broadcasts from a GP register to a vector register.
    Gp srcGp = pc.newGpPtr("srcGp");

    switch (op) {
      case OpcodeVV::kBroadcastU8:
      case OpcodeVV::kBroadcastU8Z:
        pc.load_u8(srcGp, mem_ptr(srcPtr));
        pc.emit_2v(op, dstVec, srcGp);
        break;

      case OpcodeVV::kBroadcastU16:
      case OpcodeVV::kBroadcastU16Z:
        pc.load_u16(srcGp, mem_ptr(srcPtr));
        pc.emit_2v(op, dstVec, srcGp);
        break;

      case OpcodeVV::kBroadcastU32:
      case OpcodeVV::kBroadcastF32:
        pc.load_u32(srcGp, mem_ptr(srcPtr));
        pc.emit_2v(op, dstVec, srcGp);
        break;

      case OpcodeVV::kBroadcastU64:
      case OpcodeVV::kBroadcastF64:
        // Prevent using 64-bit registers on 32-bit architectures (that would fail).
        if (pc.is64Bit()) {
          pc.load_u64(srcGp, mem_ptr(srcPtr));
          pc.emit_2v(op, dstVec, srcGp);
        }
        else {
          pc.emit_2v(op, dstVec, mem_ptr(srcPtr));
        }
        break;

      default:
        BL_NOT_REACHED();
    }
  }
  else if (variation == 2u) {
    pc.emit_2v(op, dstVec, mem_ptr(srcPtr));
  }
  else if (variation == 1u) {
    pc.v_loaduvec(dstVec, mem_ptr(srcPtr));
    pc.emit_2v(op, dstVec, dstVec);
  }
  else {
    Vec srcVec = pc.newVec(vw, "srcVec");
    pc.v_loaduvec(srcVec, mem_ptr(srcPtr));
    pc.emit_2v(op, dstVec, srcVec);
  }

  pc.v_storeuvec(mem_ptr(dstPtr), dstVec);

  ctx.cc.endFunc();
  return ctx.finish<TestVVFunc>();
}

// The following variations are supported:
//   - 0 - separate destination & source registers
//   - 1 - destination register is a source register as well
//   - 2 - source is a memory operand
static constexpr uint32_t kNumVariationsVVI = 3;

static TestVVFunc create_func_vvi(JitContext& ctx, VecWidth vw, OpcodeVVI op, uint32_t imm, Variation variation = Variation{0}) noexcept {
  ctx.prepare();
  PipeCompiler pc(&ctx.cc, ctx.features, ctx.optFlags);

  asmjit::FuncNode* node = ctx.cc.newFunc(asmjit::FuncSignature::build<void, void*, const void*>());
  EXPECT_NOT_NULL(node);

  pc.initVecWidth(vw);
  pc.initFunction(node);

  Gp dstPtr = pc.newGpPtr("dstPtr");
  Gp srcPtr = pc.newGpPtr("srcPtr");

  node->setArg(0, dstPtr);
  node->setArg(1, srcPtr);

  Vec srcVec = pc.newVec(vw, "srcVec");

  switch (variation.value) {
    default:
    case 0: {
      // There are some instructions that fill the high part of the register, so just zero the destination to make
      // sure that we can test this function (that the low part is actually zeroed and doesn't contain garbage).
      Vec dstVec = pc.newVec(vw, "dstVec");
      pc.v_zero_i(dstVec);

      pc.v_loaduvec(srcVec, mem_ptr(srcPtr));
      pc.emit_2vi(op, dstVec, srcVec, imm);
      pc.v_storeuvec(mem_ptr(dstPtr), dstVec);

      break;
    }

    case 1: {
      pc.v_loaduvec(srcVec, mem_ptr(srcPtr));
      pc.emit_2vi(op, srcVec, srcVec, imm);
      pc.v_storeuvec(mem_ptr(dstPtr), srcVec);
      break;
    }

    case 2: {
      Vec dstVec = pc.newVec(vw, "dstVec");
      pc.emit_2vi(op, dstVec, mem_ptr(srcPtr), imm);
      pc.v_storeuvec(mem_ptr(dstPtr), dstVec);
      break;
    }
  }

  ctx.cc.endFunc();
  return ctx.finish<TestVVFunc>();
}

// The following variations are supported:
//   - 0 - separate destination & source registers
//   - 1 - destination register is the same as the first source register
//   - 2 - destination register is the same as the second source register
//   - 3 - separate destination & source registers, the second source is a memory operand
//   - 4 - destination register is the same as the first source register, second source is a memory operand
static constexpr uint32_t kNumVariationsVVV = 5;

static TestVVVFunc create_func_vvv(JitContext& ctx, VecWidth vw, OpcodeVVV op, Variation variation = Variation{0}) noexcept {
  ctx.prepare();
  PipeCompiler pc(&ctx.cc, ctx.features, ctx.optFlags);

  asmjit::FuncNode* node = ctx.cc.newFunc(asmjit::FuncSignature::build<void, void*, const void*, const void*>());
  EXPECT_NOT_NULL(node);

  pc.initVecWidth(vw);
  pc.initFunction(node);

  Gp dstPtr = pc.newGpPtr("dstPtr");
  Gp src1Ptr = pc.newGpPtr("src1Ptr");
  Gp src2Ptr = pc.newGpPtr("src2Ptr");

  node->setArg(0, dstPtr);
  node->setArg(1, src1Ptr);
  node->setArg(2, src2Ptr);

  Vec src1Vec = pc.newVec(vw, "src1Vec");
  Vec src2Vec = pc.newVec(vw, "src2Vec");

  switch (variation.value) {
    default:
    case 0: {
      // There are some instructions that fill the high part of the register, so just zero the destination to make
      // sure that we can test this function (that the low part is actually zeroed and doesn't contain garbage).
      Vec dstVec = pc.newVec(vw, "dstVec");
      pc.v_zero_i(dstVec);

      pc.v_loaduvec(src1Vec, mem_ptr(src1Ptr));
      pc.v_loaduvec(src2Vec, mem_ptr(src2Ptr));
      pc.emit_3v(op, dstVec, src1Vec, src2Vec);
      pc.v_storeuvec(mem_ptr(dstPtr), dstVec);

      break;
    }

    case 1: {
      pc.v_loaduvec(src1Vec, mem_ptr(src1Ptr));
      pc.v_loaduvec(src2Vec, mem_ptr(src2Ptr));
      pc.emit_3v(op, src1Vec, src1Vec, src2Vec);
      pc.v_storeuvec(mem_ptr(dstPtr), src1Vec);

      break;
    }

    case 2: {
      pc.v_loaduvec(src1Vec, mem_ptr(src1Ptr));
      pc.v_loaduvec(src2Vec, mem_ptr(src2Ptr));
      pc.emit_3v(op, src2Vec, src1Vec, src2Vec);
      pc.v_storeuvec(mem_ptr(dstPtr), src2Vec);

      break;
    }

    case 3: {
      Vec dstVec = pc.newVec(vw, "dstVec");
      pc.v_zero_i(dstVec);

      pc.v_loaduvec(src1Vec, mem_ptr(src1Ptr));
      pc.emit_3v(op, dstVec, src1Vec, mem_ptr(src2Ptr));
      pc.v_storeuvec(mem_ptr(dstPtr), dstVec);

      break;
    }

    case 4: {
      pc.v_loaduvec(src1Vec, mem_ptr(src1Ptr));
      pc.emit_3v(op, src1Vec, src1Vec, mem_ptr(src2Ptr));
      pc.v_storeuvec(mem_ptr(dstPtr), src1Vec);

      break;
    }
  }

  ctx.cc.endFunc();
  return ctx.finish<TestVVVFunc>();
}

static constexpr uint32_t kNumVariationsVVVI = 5;

static TestVVVFunc create_func_vvvi(JitContext& ctx, VecWidth vw, OpcodeVVVI op, uint32_t imm, Variation variation = Variation{0}) noexcept {
  ctx.prepare();
  PipeCompiler pc(&ctx.cc, ctx.features, ctx.optFlags);

  asmjit::FuncNode* node = ctx.cc.newFunc(asmjit::FuncSignature::build<void, void*, const void*, const void*>());
  EXPECT_NOT_NULL(node);

  pc.initVecWidth(vw);
  pc.initFunction(node);

  Gp dstPtr = pc.newGpPtr("dstPtr");
  Gp src1Ptr = pc.newGpPtr("src1Ptr");
  Gp src2Ptr = pc.newGpPtr("src2Ptr");

  node->setArg(0, dstPtr);
  node->setArg(1, src1Ptr);
  node->setArg(2, src2Ptr);

  Vec src1Vec = pc.newVec(vw, "src1Vec");
  Vec src2Vec = pc.newVec(vw, "src2Vec");

  switch (variation.value) {
    default:
    case 0: {
      // There are some instructions that fill the high part of the register, so just zero the destination to make
      // sure that we can test this function (that the low part is actually zeroed and doesn't contain garbage).
      Vec dstVec = pc.newVec(vw, "dstVec");
      pc.v_zero_i(dstVec);

      pc.v_loaduvec(src1Vec, mem_ptr(src1Ptr));
      pc.v_loaduvec(src2Vec, mem_ptr(src2Ptr));
      pc.emit_3vi(op, dstVec, src1Vec, src2Vec, imm);
      pc.v_storeuvec(mem_ptr(dstPtr), dstVec);

      break;
    }

    case 1: {
      pc.v_loaduvec(src1Vec, mem_ptr(src1Ptr));
      pc.v_loaduvec(src2Vec, mem_ptr(src2Ptr));
      pc.emit_3vi(op, src1Vec, src1Vec, src2Vec, imm);
      pc.v_storeuvec(mem_ptr(dstPtr), src1Vec);

      break;
    }

    case 2: {
      pc.v_loaduvec(src1Vec, mem_ptr(src1Ptr));
      pc.v_loaduvec(src2Vec, mem_ptr(src2Ptr));
      pc.emit_3vi(op, src2Vec, src1Vec, src2Vec, imm);
      pc.v_storeuvec(mem_ptr(dstPtr), src2Vec);

      break;
    }

    case 3: {
      Vec dstVec = pc.newVec(vw, "dstVec");
      pc.v_zero_i(dstVec);

      pc.v_loaduvec(src1Vec, mem_ptr(src1Ptr));
      pc.emit_3vi(op, dstVec, src1Vec, mem_ptr(src2Ptr), imm);
      pc.v_storeuvec(mem_ptr(dstPtr), dstVec);

      break;
    }

    case 4: {
      pc.v_loaduvec(src1Vec, mem_ptr(src1Ptr));
      pc.emit_3vi(op, src1Vec, src1Vec, mem_ptr(src2Ptr), imm);
      pc.v_storeuvec(mem_ptr(dstPtr), src1Vec);

      break;
    }
  }

  ctx.cc.endFunc();
  return ctx.finish<TestVVVFunc>();
}

// The following variations are supported:
//   - 0 - separate destination & source registers
//   - 1 - destination register is the first source register
//   - 2 - destination register is the second source register
//   - 3 - destination register is the third source register
static constexpr uint32_t kNumVariationsVVVV = 4;

static TestVVVVFunc create_func_vvvv(JitContext& ctx, VecWidth vw, OpcodeVVVV op, Variation variation = Variation{0}) noexcept {
  ctx.prepare();
  PipeCompiler pc(&ctx.cc, ctx.features, ctx.optFlags);

  asmjit::FuncNode* node = ctx.cc.newFunc(asmjit::FuncSignature::build<void, void*, const void*, const void*, const void*>());
  EXPECT_NOT_NULL(node);

  pc.initVecWidth(vw);
  pc.initFunction(node);

  Gp dstPtr = pc.newGpPtr("dstPtr");
  Gp src1Ptr = pc.newGpPtr("src1Ptr");
  Gp src2Ptr = pc.newGpPtr("src2Ptr");
  Gp src3Ptr = pc.newGpPtr("src3Ptr");

  node->setArg(0, dstPtr);
  node->setArg(1, src1Ptr);
  node->setArg(2, src2Ptr);
  node->setArg(3, src3Ptr);

  Vec src1Vec = pc.newVec(vw, "src1Vec");
  Vec src2Vec = pc.newVec(vw, "src2Vec");
  Vec src3Vec = pc.newVec(vw, "src3Vec");

  pc.v_loaduvec(src1Vec, mem_ptr(src1Ptr));
  pc.v_loaduvec(src2Vec, mem_ptr(src2Ptr));
  pc.v_loaduvec(src3Vec, mem_ptr(src3Ptr));

  switch (variation.value) {
    default:
    case 0: {
      // There are some instructions that fill the high part of the register, so just zero the destination to make
      // sure that we can test this function (that the low part is actually zeroed and doesn't contain garbage).
      Vec dstVec = pc.newVec(vw, "dstVec");
      pc.v_zero_i(dstVec);

      pc.emit_4v(op, dstVec, src1Vec, src2Vec, src3Vec);
      pc.v_storeuvec(mem_ptr(dstPtr), dstVec);

      break;
    }

    case 1: {
      pc.emit_4v(op, src1Vec, src1Vec, src2Vec, src3Vec);
      pc.v_storeuvec(mem_ptr(dstPtr), src1Vec);

      break;
    }

    case 2: {
      pc.emit_4v(op, src2Vec, src1Vec, src2Vec, src3Vec);
      pc.v_storeuvec(mem_ptr(dstPtr), src2Vec);

      break;
    }

    case 3: {
      pc.emit_4v(op, src3Vec, src1Vec, src2Vec, src3Vec);
      pc.v_storeuvec(mem_ptr(dstPtr), src3Vec);

      break;
    }
  }

  ctx.cc.endFunc();
  return ctx.finish<TestVVVVFunc>();
}
// bl::Pipeline::JIT - Tests - SIMD - Vector Overlay
// =================================================

enum class VecElementType : uint8_t {
  kInt8,
  kInt16,
  kInt32,
  kInt64,
  kUInt8,
  kUInt16,
  kUInt32,
  kUInt64,
  kFloat32,
  kFloat64
};

struct VecOpInfo {
  uint32_t _data;

  BL_INLINE_NODEBUG uint32_t count() const noexcept { return _data >> 28; }

  BL_INLINE_NODEBUG VecElementType ret() const noexcept { return VecElementType((_data >> 24) & 0xFu); }
  BL_INLINE_NODEBUG VecElementType arg(uint32_t i) const noexcept { return VecElementType((_data >> (i * 4)) & 0xFu); }

  static BL_INLINE_NODEBUG VecOpInfo make(VecElementType ret, VecElementType arg0) noexcept {
    return VecOpInfo{(1u << 28) | (uint32_t(ret) << 24) | uint32_t(arg0)};
  }

  static BL_INLINE_NODEBUG VecOpInfo make(VecElementType ret, VecElementType arg0, VecElementType arg1) noexcept {
    return VecOpInfo{(1u << 28) | (uint32_t(ret) << 24) | uint32_t(arg0) | (uint32_t(arg1) << 4)};
  }

  static BL_INLINE_NODEBUG VecOpInfo make(VecElementType ret, VecElementType arg0, VecElementType arg1, VecElementType arg2) noexcept {
    return VecOpInfo{(1u << 28) | (uint32_t(ret) << 24) | uint32_t(arg0) | (uint32_t(arg1) << 4) | (uint32_t(arg2) << 8)};
  }

  static BL_INLINE_NODEBUG VecOpInfo make(VecElementType ret, VecElementType arg0, VecElementType arg1, VecElementType arg2, VecElementType arg3) noexcept {
    return VecOpInfo{(1u << 28) | (uint32_t(ret) << 24) | uint32_t(arg0) | (uint32_t(arg1) << 4) | (uint32_t(arg2) << 8) | (uint32_t(arg3) << 12)};
  }
};

template<uint32_t kW>
struct alignas(16) VecOverlay {
  union {
    int8_t data_i8[kW];
    uint8_t data_u8[kW];

    int16_t data_i16[kW / 2u];
    uint16_t data_u16[kW / 2u];

    int32_t data_i32[kW / 4u];
    uint32_t data_u32[kW / 4u];

    int64_t data_i64[kW / 8u];
    uint64_t data_u64[kW / 8u];

    float data_f32[kW / 4u];
    double data_f64[kW / 8u];
  };

  template<typename T>
  BL_INLINE_NODEBUG T* data() noexcept;

  template<typename T>
  BL_INLINE_NODEBUG const T* data() const noexcept;

  template<typename T>
  BL_INLINE_NODEBUG T get(size_t index) const noexcept;

  template<typename T>
  BL_INLINE_NODEBUG void set(size_t index, const T& value) noexcept;

  template<uint32_t kOtherW>
  BL_INLINE_NODEBUG void copy16bFrom(const VecOverlay<kOtherW>& other) noexcept {
    data_u64[0] = other.data_u64[0];
    data_u64[1] = other.data_u64[1];
  }
};

template<typename T>
struct VecAccess;

template<>
struct VecAccess<int8_t> {
  template<uint32_t kW> static BL_INLINE_NODEBUG int8_t* data(VecOverlay<kW>& vec) noexcept { return vec.data_i8; }
  template<uint32_t kW> static BL_INLINE_NODEBUG const int8_t* data(const VecOverlay<kW>& vec) noexcept { return vec.data_i8; }

  template<uint32_t kW> static BL_INLINE_NODEBUG int8_t get(const VecOverlay<kW>& vec, size_t index) noexcept { return vec.data_i8[index]; }
  template<uint32_t kW> static BL_INLINE_NODEBUG void set(VecOverlay<kW>& vec, size_t index, int8_t value) noexcept { vec.data_i8[index] = value; }
};

template<>
struct VecAccess<int16_t> {
  template<uint32_t kW> static BL_INLINE_NODEBUG int16_t* data(VecOverlay<kW>& vec) noexcept { return vec.data_i16; }
  template<uint32_t kW> static BL_INLINE_NODEBUG const int16_t* data(const VecOverlay<kW>& vec) noexcept { return vec.data_i16; }

  template<uint32_t kW> static BL_INLINE_NODEBUG int16_t get(const VecOverlay<kW>& vec, size_t index) noexcept { return vec.data_i16[index]; }
  template<uint32_t kW> static BL_INLINE_NODEBUG void set(VecOverlay<kW>& vec, size_t index, int16_t value) noexcept { vec.data_i16[index] = value; }
};

template<>
struct VecAccess<int32_t> {
  template<uint32_t kW> static BL_INLINE_NODEBUG int32_t* data(VecOverlay<kW>& vec) noexcept { return vec.data_i32; }
  template<uint32_t kW> static BL_INLINE_NODEBUG const int32_t* data(const VecOverlay<kW>& vec) noexcept { return vec.data_i32; }

  template<uint32_t kW> static BL_INLINE_NODEBUG int32_t get(const VecOverlay<kW>& vec, size_t index) noexcept { return vec.data_i32[index]; }
  template<uint32_t kW> static BL_INLINE_NODEBUG void set(VecOverlay<kW>& vec, size_t index, int32_t value) noexcept { vec.data_i32[index] = value; }
};

template<>
struct VecAccess<int64_t> {
  template<uint32_t kW> static BL_INLINE_NODEBUG int64_t* data(VecOverlay<kW>& vec) noexcept { return vec.data_i64; }
  template<uint32_t kW> static BL_INLINE_NODEBUG const int64_t* data(const VecOverlay<kW>& vec) noexcept { return vec.data_i64; }

  template<uint32_t kW> static BL_INLINE_NODEBUG int64_t get(const VecOverlay<kW>& vec, size_t index) noexcept { return vec.data_i64[index]; }
  template<uint32_t kW> static BL_INLINE_NODEBUG void set(VecOverlay<kW>& vec, size_t index, int64_t value) noexcept { vec.data_i64[index] = value; }
};

template<>
struct VecAccess<uint8_t> {
  template<uint32_t kW> static BL_INLINE_NODEBUG uint8_t* data(VecOverlay<kW>& vec) noexcept { return vec.data_u8; }
  template<uint32_t kW> static BL_INLINE_NODEBUG const uint8_t* data(const VecOverlay<kW>& vec) noexcept { return vec.data_u8; }

  template<uint32_t kW> static BL_INLINE_NODEBUG uint8_t get(const VecOverlay<kW>& vec, size_t index) noexcept { return vec.data_u8[index]; }
  template<uint32_t kW> static BL_INLINE_NODEBUG void set(VecOverlay<kW>& vec, size_t index, uint8_t value) noexcept { vec.data_u8[index] = value; }
};

template<>
struct VecAccess<uint16_t> {
  template<uint32_t kW> static BL_INLINE_NODEBUG uint16_t* data(VecOverlay<kW>& vec) noexcept { return vec.data_u16; }
  template<uint32_t kW> static BL_INLINE_NODEBUG const uint16_t* data(const VecOverlay<kW>& vec) noexcept { return vec.data_u16; }

  template<uint32_t kW> static BL_INLINE_NODEBUG uint16_t get(const VecOverlay<kW>& vec, size_t index) noexcept { return vec.data_u16[index]; }
  template<uint32_t kW> static BL_INLINE_NODEBUG void set(VecOverlay<kW>& vec, size_t index, uint16_t value) noexcept { vec.data_u16[index] = value; }
};

template<>
struct VecAccess<uint32_t> {
  template<uint32_t kW> static BL_INLINE_NODEBUG uint32_t* data(VecOverlay<kW>& vec) noexcept { return vec.data_u32; }
  template<uint32_t kW> static BL_INLINE_NODEBUG const uint32_t* data(const VecOverlay<kW>& vec) noexcept { return vec.data_u32; }

  template<uint32_t kW> static BL_INLINE_NODEBUG uint32_t get(const VecOverlay<kW>& vec, size_t index) noexcept { return vec.data_u32[index]; }
  template<uint32_t kW> static BL_INLINE_NODEBUG void set(VecOverlay<kW>& vec, size_t index, uint32_t value) noexcept { vec.data_u32[index] = value; }
};

template<>
struct VecAccess<uint64_t> {
  template<uint32_t kW> static BL_INLINE_NODEBUG uint64_t* data(VecOverlay<kW>& vec) noexcept { return vec.data_u64; }
  template<uint32_t kW> static BL_INLINE_NODEBUG const uint64_t* data(const VecOverlay<kW>& vec) noexcept { return vec.data_u64; }

  template<uint32_t kW> static BL_INLINE_NODEBUG uint64_t get(const VecOverlay<kW>& vec, size_t index) noexcept { return vec.data_u64[index]; }
  template<uint32_t kW> static BL_INLINE_NODEBUG void set(VecOverlay<kW>& vec, size_t index, uint64_t value) noexcept { vec.data_u64[index] = value; }
};

template<>
struct VecAccess<float> {
  template<uint32_t kW> static BL_INLINE_NODEBUG float* data(VecOverlay<kW>& vec) noexcept { return vec.data_f32; }
  template<uint32_t kW> static BL_INLINE_NODEBUG const float* data(const VecOverlay<kW>& vec) noexcept { return vec.data_f32; }

  template<uint32_t kW> static BL_INLINE_NODEBUG float get(const VecOverlay<kW>& vec, size_t index) noexcept { return vec.data_f32[index]; }
  template<uint32_t kW> static BL_INLINE_NODEBUG void set(VecOverlay<kW>& vec, size_t index, float value) noexcept { vec.data_f32[index] = value; }
};

template<>
struct VecAccess<double> {
  template<uint32_t kW> static BL_INLINE_NODEBUG double* data(VecOverlay<kW>& vec) noexcept { return vec.data_f64; }
  template<uint32_t kW> static BL_INLINE_NODEBUG const double* data(const VecOverlay<kW>& vec) noexcept { return vec.data_f64; }

  template<uint32_t kW> static BL_INLINE_NODEBUG double get(const VecOverlay<kW>& vec, size_t index) noexcept { return vec.data_f64[index]; }
  template<uint32_t kW> static BL_INLINE_NODEBUG void set(VecOverlay<kW>& vec, size_t index, double value) noexcept { vec.data_f64[index] = value; }
};

template<uint32_t kW>
template<typename T>
BL_INLINE_NODEBUG T* VecOverlay<kW>::data() noexcept { return VecAccess<T>::data(*this); }

template<uint32_t kW>
template<typename T>
BL_INLINE_NODEBUG const T* VecOverlay<kW>::data() const noexcept { return VecAccess<T>::data(*this); }

template<uint32_t kW>
template<typename T>
BL_INLINE_NODEBUG T VecOverlay<kW>::get(size_t index) const noexcept { return VecAccess<T>::get(*this, index); }

template<uint32_t kW>
template<typename T>
BL_INLINE_NODEBUG void VecOverlay<kW>::set(size_t index, const T& value) noexcept { return VecAccess<T>::set(*this, index, value); }

template<typename T> struct TypeNameToString {};
template<> struct TypeNameToString<int8_t  > { static BL_INLINE_NODEBUG const char* get() noexcept { return "int8"; } };
template<> struct TypeNameToString<int16_t > { static BL_INLINE_NODEBUG const char* get() noexcept { return "int16"; } };
template<> struct TypeNameToString<int32_t > { static BL_INLINE_NODEBUG const char* get() noexcept { return "int32"; } };
template<> struct TypeNameToString<int64_t > { static BL_INLINE_NODEBUG const char* get() noexcept { return "int64"; } };
template<> struct TypeNameToString<uint8_t > { static BL_INLINE_NODEBUG const char* get() noexcept { return "uint8"; } };
template<> struct TypeNameToString<uint16_t> { static BL_INLINE_NODEBUG const char* get() noexcept { return "uint16"; } };
template<> struct TypeNameToString<uint32_t> { static BL_INLINE_NODEBUG const char* get() noexcept { return "uint32"; } };
template<> struct TypeNameToString<uint64_t> { static BL_INLINE_NODEBUG const char* get() noexcept { return "uint64"; } };
template<> struct TypeNameToString<float   > { static BL_INLINE_NODEBUG const char* get() noexcept { return "float32"; } };
template<> struct TypeNameToString<double  > { static BL_INLINE_NODEBUG const char* get() noexcept { return "float64"; } };

template<uint32_t kW>
static bool vec_eq(const VecOverlay<kW>& a, const VecOverlay<kW>& b) noexcept {
  return memcmp(a.data_u8, b.data_u8, kW) == 0;
}

template<typename T>
static bool float_eq(const T& a, const T& b) noexcept {
  return a == b || (Math::isNaN(a) && Math::isNaN(b));
}

template<uint32_t kW>
static bool vec_eq(const VecOverlay<kW>& a, const VecOverlay<kW>& b, VecElementType elementType) noexcept {
  if (elementType == VecElementType::kFloat32) {
    size_t count = kW / sizeof(float);
    for (size_t i = 0; i < count; i++)
      if (!float_eq(a.data_f32[i], b.data_f32[i]))
        return false;
    return true;
  }
  else if (elementType == VecElementType::kFloat64) {
    size_t count = kW / sizeof(double);
    for (size_t i = 0; i < count; i++)
      if (!float_eq(a.data_f64[i], b.data_f64[i]))
        return false;
    return true;
  }
  else {
    return vec_eq(a, b);
  }
}

template<uint32_t kW>
static BL_NOINLINE BLString vec_stringify(const VecOverlay<kW>& vec, VecElementType elementType) noexcept {
  BLString s;
  s.append('{');

  switch (elementType) {
    case VecElementType::kInt8   : { for (uint32_t i = 0; i < kW    ; i++) s.appendFormat("%s%d"  , i == 0 ? "" : ", ", vec.data_i8[i]); break; }
    case VecElementType::kInt16  : { for (uint32_t i = 0; i < kW / 2; i++) s.appendFormat("%s%d"  , i == 0 ? "" : ", ", vec.data_i16[i]); break; }
    case VecElementType::kInt32  : { for (uint32_t i = 0; i < kW / 4; i++) s.appendFormat("%s%d"  , i == 0 ? "" : ", ", vec.data_i32[i]); break; }
    case VecElementType::kInt64  : { for (uint32_t i = 0; i < kW / 8; i++) s.appendFormat("%s%lld", i == 0 ? "" : ", ", (long long)vec.data_i64[i]); break; }
    case VecElementType::kUInt8  : { for (uint32_t i = 0; i < kW    ; i++) s.appendFormat("%s%u"  , i == 0 ? "" : ", ", vec.data_u8[i]); break; }
    case VecElementType::kUInt16 : { for (uint32_t i = 0; i < kW / 2; i++) s.appendFormat("%s%u"  , i == 0 ? "" : ", ", vec.data_u16[i]); break; }
    case VecElementType::kUInt32 : { for (uint32_t i = 0; i < kW / 4; i++) s.appendFormat("%s%u"  , i == 0 ? "" : ", ", vec.data_u32[i]); break; }
    case VecElementType::kUInt64 : { for (uint32_t i = 0; i < kW / 8; i++) s.appendFormat("%s%llu", i == 0 ? "" : ", ", (unsigned long long)vec.data_u64[i]); break; }
    case VecElementType::kFloat32: { for (uint32_t i = 0; i < kW / 4; i++) s.appendFormat("%s%.20f"  , i == 0 ? "" : ", ", vec.data_f32[i]); break; }
    case VecElementType::kFloat64: { for (uint32_t i = 0; i < kW / 8; i++) s.appendFormat("%s%.20f"  , i == 0 ? "" : ", ", vec.data_f64[i]); break; }

    default:
      BL_NOT_REACHED();
  }

  s.append('}');
  return s;
}

// bl::Pipeline::JIT - Tests - SIMD - Metadata
// ===========================================

static const char* vec_op_name_vv(OpcodeVV op) noexcept {
  switch (op) {
    case OpcodeVV::kMov               : return "v_mov";
    case OpcodeVV::kMovU64            : return "v_mov_u64";
    case OpcodeVV::kBroadcastU8Z      : return "v_broadcast_u8z";
    case OpcodeVV::kBroadcastU16Z     : return "v_broadcast_u16z";
    case OpcodeVV::kBroadcastU8       : return "v_broadcast_u8";
    case OpcodeVV::kBroadcastU16      : return "v_broadcast_u16";
    case OpcodeVV::kBroadcastU32      : return "v_broadcast_u32";
    case OpcodeVV::kBroadcastU64      : return "v_broadcast_u64";
    case OpcodeVV::kBroadcastF32      : return "v_broadcast_f32";
    case OpcodeVV::kBroadcastF64      : return "v_broadcast_f64";
    case OpcodeVV::kBroadcastV128_U32 : return "v_broadcast_v128_u32";
    case OpcodeVV::kBroadcastV128_U64 : return "v_broadcast_v128_u64";
    case OpcodeVV::kBroadcastV128_F32 : return "v_broadcast_v128_f32";
    case OpcodeVV::kBroadcastV128_F64 : return "v_broadcast_v128_f64";
    case OpcodeVV::kBroadcastV256_U32 : return "v_broadcast_v256_u32";
    case OpcodeVV::kBroadcastV256_U64 : return "v_broadcast_v256_u64";
    case OpcodeVV::kBroadcastV256_F32 : return "v_broadcast_v256_f32";
    case OpcodeVV::kBroadcastV256_F64 : return "v_broadcast_v256_f64";
    case OpcodeVV::kAbsI8             : return "v_abs_i8";
    case OpcodeVV::kAbsI16            : return "v_abs_i16";
    case OpcodeVV::kAbsI32            : return "v_abs_i32";
    case OpcodeVV::kAbsI64            : return "v_abs_i64";
    case OpcodeVV::kNotU32            : return "v_not_u32";
    case OpcodeVV::kNotU64            : return "v_not_u64";
    case OpcodeVV::kCvtI8LoToI16      : return "v_cvt_i8_lo_to_i16";
    case OpcodeVV::kCvtI8HiToI16      : return "v_cvt_i8_hi_to_i16";
    case OpcodeVV::kCvtU8LoToU16      : return "v_cvt_u8_lo_to_u16";
    case OpcodeVV::kCvtU8HiToU16      : return "v_cvt_u8_hi_to_u16";
    case OpcodeVV::kCvtI8ToI32        : return "v_cvt_i8_to_i32";
    case OpcodeVV::kCvtU8ToU32        : return "v_cvt_u8_to_u32";
    case OpcodeVV::kCvtI16LoToI32     : return "v_cvt_i16_lo_to_i32";
    case OpcodeVV::kCvtI16HiToI32     : return "v_cvt_i16_hi_to_i32";
    case OpcodeVV::kCvtU16LoToU32     : return "v_cvt_u16_lo_to_u32";
    case OpcodeVV::kCvtU16HiToU32     : return "v_cvt_u16_hi_to_u32";
    case OpcodeVV::kCvtI32LoToI64     : return "v_cvt_i32_lo_to_i64";
    case OpcodeVV::kCvtI32HiToI64     : return "v_cvt_i32_hi_to_i64";
    case OpcodeVV::kCvtU32LoToU64     : return "v_cvt_u32_lo_to_u64";
    case OpcodeVV::kCvtU32HiToU64     : return "v_cvt_u32_hi_to_u64";
    case OpcodeVV::kAbsF32            : return "v_abs_f32";
    case OpcodeVV::kAbsF64            : return "v_abs_f64";
    case OpcodeVV::kNotF32            : return "v_not_f32";
    case OpcodeVV::kNotF64            : return "v_not_f64";
    case OpcodeVV::kTruncF32S         : return "v_trunc_f32s";
    case OpcodeVV::kTruncF64S         : return "v_trunc_f64s";
    case OpcodeVV::kTruncF32          : return "v_trunc_f32";
    case OpcodeVV::kTruncF64          : return "v_trunc_f64";
    case OpcodeVV::kFloorF32S         : return "v_floor_f32s";
    case OpcodeVV::kFloorF64S         : return "v_floor_f64s";
    case OpcodeVV::kFloorF32          : return "v_floor_f32";
    case OpcodeVV::kFloorF64          : return "v_floor_f64";
    case OpcodeVV::kCeilF32S          : return "v_ceil_f32s";
    case OpcodeVV::kCeilF64S          : return "v_ceil_f64s";
    case OpcodeVV::kCeilF32           : return "v_ceil_f32";
    case OpcodeVV::kCeilF64           : return "v_ceil_f64";
    case OpcodeVV::kRoundF32S         : return "v_round_f32s";
    case OpcodeVV::kRoundF64S         : return "v_round_f64s";
    case OpcodeVV::kRoundF32          : return "v_round_f32";
    case OpcodeVV::kRoundF64          : return "v_round_f64";
    case OpcodeVV::kRcpF32            : return "v_rcp_f32";
    case OpcodeVV::kRcpF64            : return "v_rcp_f64";
    case OpcodeVV::kSqrtF32S          : return "v_sqrt_f32s";
    case OpcodeVV::kSqrtF64S          : return "v_sqrt_f64s";
    case OpcodeVV::kSqrtF32           : return "v_sqrt_f32";
    case OpcodeVV::kSqrtF64           : return "v_sqrt_f64";
    case OpcodeVV::kCvtF32ToF64S      : return "v_cvt_f32_to_f64s";
    case OpcodeVV::kCvtF64ToF32S      : return "v_cvt_f64_to_f32s";
    case OpcodeVV::kCvtI32ToF32       : return "v_cvt_i32_to_f32";
    case OpcodeVV::kCvtF32LoToF64     : return "v_cvt_f32_lo_to_f64";
    case OpcodeVV::kCvtF32HiToF64     : return "v_cvt_f32_hi_to_f64";
    case OpcodeVV::kCvtF64ToF32Lo     : return "v_cvt_f64_to_f32_lo";
    case OpcodeVV::kCvtF64ToF32Hi     : return "v_cvt_f64_to_f32_hi";
    case OpcodeVV::kCvtI32LoToF64     : return "v_cvt_i32_lo_to_f64";
    case OpcodeVV::kCvtI32HiToF64     : return "v_cvt_i32_hi_to_f64";
    case OpcodeVV::kCvtTruncF32ToI32  : return "v_cvt_trunc_f32_to_i32";
    case OpcodeVV::kCvtTruncF64ToI32Lo: return "v_cvt_trunc_f64_to_i32_lo";
    case OpcodeVV::kCvtTruncF64ToI32Hi: return "v_cvt_trunc_f64_to_i32_hi";
    case OpcodeVV::kCvtRoundF32ToI32  : return "v_cvt_round_f32_to_i32";
    case OpcodeVV::kCvtRoundF64ToI32Lo: return "v_cvt_round_f64_to_i32_lo";
    case OpcodeVV::kCvtRoundF64ToI32Hi: return "v_cvt_round_f64_to_i32_hi";

    default:
      BL_NOT_REACHED();
  }
}

static VecOpInfo vec_op_info_vv(OpcodeVV op) noexcept {
  using VE = VecElementType;

  switch (op) {
    case OpcodeVV::kMov               : return VecOpInfo::make(VE::kUInt8, VE::kUInt8);
    case OpcodeVV::kMovU64            : return VecOpInfo::make(VE::kUInt8, VE::kUInt8);
    case OpcodeVV::kBroadcastU8Z      : return VecOpInfo::make(VE::kUInt8, VE::kUInt8);
    case OpcodeVV::kBroadcastU16Z     : return VecOpInfo::make(VE::kUInt16, VE::kUInt16);
    case OpcodeVV::kBroadcastU8       : return VecOpInfo::make(VE::kUInt8, VE::kUInt8);
    case OpcodeVV::kBroadcastU16      : return VecOpInfo::make(VE::kUInt16, VE::kUInt16);
    case OpcodeVV::kBroadcastU32      : return VecOpInfo::make(VE::kUInt32, VE::kUInt32);
    case OpcodeVV::kBroadcastU64      : return VecOpInfo::make(VE::kUInt64, VE::kUInt64);
    case OpcodeVV::kBroadcastF32      : return VecOpInfo::make(VE::kFloat32, VE::kFloat32);
    case OpcodeVV::kBroadcastF64      : return VecOpInfo::make(VE::kFloat64, VE::kFloat64);
    case OpcodeVV::kBroadcastV128_U32 : return VecOpInfo::make(VE::kUInt32, VE::kUInt32);
    case OpcodeVV::kBroadcastV128_U64 : return VecOpInfo::make(VE::kUInt64, VE::kUInt64);
    case OpcodeVV::kBroadcastV128_F32 : return VecOpInfo::make(VE::kFloat32, VE::kFloat32);
    case OpcodeVV::kBroadcastV128_F64 : return VecOpInfo::make(VE::kFloat64, VE::kFloat64);
    case OpcodeVV::kBroadcastV256_U32 : return VecOpInfo::make(VE::kUInt32, VE::kUInt32);
    case OpcodeVV::kBroadcastV256_U64 : return VecOpInfo::make(VE::kUInt64, VE::kUInt64);
    case OpcodeVV::kBroadcastV256_F32 : return VecOpInfo::make(VE::kFloat32, VE::kFloat32);
    case OpcodeVV::kBroadcastV256_F64 : return VecOpInfo::make(VE::kFloat64, VE::kFloat64);
    case OpcodeVV::kAbsI8             : return VecOpInfo::make(VE::kUInt8, VE::kInt8);
    case OpcodeVV::kAbsI16            : return VecOpInfo::make(VE::kUInt16, VE::kInt16);
    case OpcodeVV::kAbsI32            : return VecOpInfo::make(VE::kUInt32, VE::kInt32);
    case OpcodeVV::kAbsI64            : return VecOpInfo::make(VE::kUInt64, VE::kInt64);
    case OpcodeVV::kNotU32            : return VecOpInfo::make(VE::kUInt32, VE::kInt32);
    case OpcodeVV::kNotU64            : return VecOpInfo::make(VE::kUInt64, VE::kInt64);
    case OpcodeVV::kCvtI8LoToI16      : return VecOpInfo::make(VE::kInt16, VE::kInt8);
    case OpcodeVV::kCvtI8HiToI16      : return VecOpInfo::make(VE::kInt16, VE::kInt8);
    case OpcodeVV::kCvtU8LoToU16      : return VecOpInfo::make(VE::kUInt16, VE::kUInt8);
    case OpcodeVV::kCvtU8HiToU16      : return VecOpInfo::make(VE::kUInt16, VE::kUInt8);
    case OpcodeVV::kCvtI8ToI32        : return VecOpInfo::make(VE::kInt32, VE::kInt8);
    case OpcodeVV::kCvtU8ToU32        : return VecOpInfo::make(VE::kUInt32, VE::kUInt8);
    case OpcodeVV::kCvtI16LoToI32     : return VecOpInfo::make(VE::kInt32, VE::kInt16);
    case OpcodeVV::kCvtI16HiToI32     : return VecOpInfo::make(VE::kInt32, VE::kInt16);
    case OpcodeVV::kCvtU16LoToU32     : return VecOpInfo::make(VE::kUInt32, VE::kUInt16);
    case OpcodeVV::kCvtU16HiToU32     : return VecOpInfo::make(VE::kUInt32, VE::kUInt16);
    case OpcodeVV::kCvtI32LoToI64     : return VecOpInfo::make(VE::kInt64, VE::kInt32);
    case OpcodeVV::kCvtI32HiToI64     : return VecOpInfo::make(VE::kInt64, VE::kInt32);
    case OpcodeVV::kCvtU32LoToU64     : return VecOpInfo::make(VE::kUInt64, VE::kUInt32);
    case OpcodeVV::kCvtU32HiToU64     : return VecOpInfo::make(VE::kUInt64, VE::kUInt32);
    case OpcodeVV::kAbsF32            : return VecOpInfo::make(VE::kFloat32, VE::kFloat32);
    case OpcodeVV::kAbsF64            : return VecOpInfo::make(VE::kFloat64, VE::kFloat64);
    case OpcodeVV::kNotF32            : return VecOpInfo::make(VE::kUInt32, VE::kUInt32);
    case OpcodeVV::kNotF64            : return VecOpInfo::make(VE::kUInt64, VE::kUInt64);
    case OpcodeVV::kTruncF32S         : return VecOpInfo::make(VE::kFloat32, VE::kFloat32);
    case OpcodeVV::kTruncF64S         : return VecOpInfo::make(VE::kFloat64, VE::kFloat64);
    case OpcodeVV::kTruncF32          : return VecOpInfo::make(VE::kFloat32, VE::kFloat32);
    case OpcodeVV::kTruncF64          : return VecOpInfo::make(VE::kFloat64, VE::kFloat64);
    case OpcodeVV::kFloorF32S         : return VecOpInfo::make(VE::kFloat32, VE::kFloat32);
    case OpcodeVV::kFloorF64S         : return VecOpInfo::make(VE::kFloat64, VE::kFloat64);
    case OpcodeVV::kFloorF32          : return VecOpInfo::make(VE::kFloat32, VE::kFloat32);
    case OpcodeVV::kFloorF64          : return VecOpInfo::make(VE::kFloat64, VE::kFloat64);
    case OpcodeVV::kCeilF32S          : return VecOpInfo::make(VE::kFloat32, VE::kFloat32);
    case OpcodeVV::kCeilF64S          : return VecOpInfo::make(VE::kFloat64, VE::kFloat64);
    case OpcodeVV::kCeilF32           : return VecOpInfo::make(VE::kFloat32, VE::kFloat32);
    case OpcodeVV::kCeilF64           : return VecOpInfo::make(VE::kFloat64, VE::kFloat64);
    case OpcodeVV::kRoundF32S         : return VecOpInfo::make(VE::kFloat32, VE::kFloat32);
    case OpcodeVV::kRoundF64S         : return VecOpInfo::make(VE::kFloat64, VE::kFloat64);
    case OpcodeVV::kRoundF32          : return VecOpInfo::make(VE::kFloat32, VE::kFloat32);
    case OpcodeVV::kRoundF64          : return VecOpInfo::make(VE::kFloat64, VE::kFloat64);
    case OpcodeVV::kRcpF32            : return VecOpInfo::make(VE::kFloat32, VE::kFloat32);
    case OpcodeVV::kRcpF64            : return VecOpInfo::make(VE::kFloat64, VE::kFloat64);
    case OpcodeVV::kSqrtF32S          : return VecOpInfo::make(VE::kFloat32, VE::kFloat32);
    case OpcodeVV::kSqrtF64S          : return VecOpInfo::make(VE::kFloat64, VE::kFloat64);
    case OpcodeVV::kSqrtF32           : return VecOpInfo::make(VE::kFloat32, VE::kFloat32);
    case OpcodeVV::kSqrtF64           : return VecOpInfo::make(VE::kFloat64, VE::kFloat64);
    case OpcodeVV::kCvtF32ToF64S      : return VecOpInfo::make(VE::kFloat64, VE::kFloat32);
    case OpcodeVV::kCvtF64ToF32S      : return VecOpInfo::make(VE::kFloat32, VE::kFloat64);
    case OpcodeVV::kCvtI32ToF32       : return VecOpInfo::make(VE::kFloat32, VE::kInt32);
    case OpcodeVV::kCvtF32LoToF64     : return VecOpInfo::make(VE::kFloat64, VE::kFloat32);
    case OpcodeVV::kCvtF32HiToF64     : return VecOpInfo::make(VE::kFloat64, VE::kFloat32);
    case OpcodeVV::kCvtF64ToF32Lo     : return VecOpInfo::make(VE::kFloat32, VE::kFloat64);
    case OpcodeVV::kCvtF64ToF32Hi     : return VecOpInfo::make(VE::kFloat32, VE::kFloat64);
    case OpcodeVV::kCvtI32LoToF64     : return VecOpInfo::make(VE::kFloat64, VE::kInt32);
    case OpcodeVV::kCvtI32HiToF64     : return VecOpInfo::make(VE::kFloat64, VE::kInt32);
    case OpcodeVV::kCvtTruncF32ToI32  : return VecOpInfo::make(VE::kInt32, VE::kFloat32);
    case OpcodeVV::kCvtTruncF64ToI32Lo: return VecOpInfo::make(VE::kInt32, VE::kFloat64);
    case OpcodeVV::kCvtTruncF64ToI32Hi: return VecOpInfo::make(VE::kInt32, VE::kFloat64);
    case OpcodeVV::kCvtRoundF32ToI32  : return VecOpInfo::make(VE::kInt32, VE::kFloat32);
    case OpcodeVV::kCvtRoundF64ToI32Lo: return VecOpInfo::make(VE::kInt32, VE::kFloat64);
    case OpcodeVV::kCvtRoundF64ToI32Hi: return VecOpInfo::make(VE::kInt32, VE::kFloat64);

    default:
      BL_NOT_REACHED();
  }
}

static const char* vec_op_name_vvi(OpcodeVVI op) noexcept {
  switch (op) {
    case OpcodeVVI::kSllU16         : return "v_sll_u16";
    case OpcodeVVI::kSllU32         : return "v_sll_u32";
    case OpcodeVVI::kSllU64         : return "v_sll_u64";
    case OpcodeVVI::kSrlU16         : return "v_srl_u16";
    case OpcodeVVI::kSrlU32         : return "v_srl_u32";
    case OpcodeVVI::kSrlU64         : return "v_srl_u64";
    case OpcodeVVI::kSraI16         : return "v_sra_i16";
    case OpcodeVVI::kSraI32         : return "v_sra_i32";
    case OpcodeVVI::kSraI64         : return "v_sra_i64";
    case OpcodeVVI::kSllbU128       : return "v_sllb_u128";
    case OpcodeVVI::kSrlbU128       : return "v_srlb_u128";
    case OpcodeVVI::kSwizzleU16x4   : return "v_swizzle_u16x4";
    case OpcodeVVI::kSwizzleLoU16x4 : return "v_swizzle_lo_u16x4";
    case OpcodeVVI::kSwizzleHiU16x4 : return "v_swizzle_hi_u16x4";
    case OpcodeVVI::kSwizzleU32x4   : return "v_swizzle_u32x4";
    case OpcodeVVI::kSwizzleU64x2   : return "v_swizzle_u64x2";
    case OpcodeVVI::kSwizzleF32x4   : return "v_swizzle_f32x4";
    case OpcodeVVI::kSwizzleF64x2   : return "v_swizzle_f64x2";
    case OpcodeVVI::kSwizzleU64x4   : return "v_swizzle_u64x4";
    case OpcodeVVI::kSwizzleF64x4   : return "v_swizzle_f64x4";
    case OpcodeVVI::kExtractV128_I32: return "v_extract_v128_i32";
    case OpcodeVVI::kExtractV128_I64: return "v_extract_v128_i64";
    case OpcodeVVI::kExtractV128_F32: return "v_extract_v128_f32";
    case OpcodeVVI::kExtractV128_F64: return "v_extract_v128_f64";
    case OpcodeVVI::kExtractV256_I32: return "v_extract_v256_i32";
    case OpcodeVVI::kExtractV256_I64: return "v_extract_v256_i64";
    case OpcodeVVI::kExtractV256_F32: return "v_extract_v256_f32";
    case OpcodeVVI::kExtractV256_F64: return "v_extract_v256_f64";

#if defined(BL_JIT_ARCH_A64)
    case OpcodeVVI::kSrlRndU16      : return "v_srl_rnd_u16";
    case OpcodeVVI::kSrlRndU32      : return "v_srl_rnd_u32";
    case OpcodeVVI::kSrlRndU64      : return "v_srl_rnd_u64";
    case OpcodeVVI::kSrlAccU16      : return "v_srl_acc_u16";
    case OpcodeVVI::kSrlAccU32      : return "v_srl_acc_u32";
    case OpcodeVVI::kSrlAccU64      : return "v_srl_acc_u64";
    case OpcodeVVI::kSrlRndAccU16   : return "v_srl_rnd_acc_u16";
    case OpcodeVVI::kSrlRndAccU32   : return "v_srl_rnd_acc_u32";
    case OpcodeVVI::kSrlRndAccU64   : return "v_srl_rnd_acc_u64";
    case OpcodeVVI::kSrlnLoU16      : return "v_srln_lo_u16";
    case OpcodeVVI::kSrlnHiU16      : return "v_srln_hi_u16";
    case OpcodeVVI::kSrlnLoU32      : return "v_srln_lo_u32";
    case OpcodeVVI::kSrlnHiU32      : return "v_srln_hi_u32";
    case OpcodeVVI::kSrlnLoU64      : return "v_srln_lo_u64";
    case OpcodeVVI::kSrlnHiU64      : return "v_srln_hi_u64";
#endif // BL_JIT_ARCH_A64

    default:
      BL_NOT_REACHED();
  }
}

static VecOpInfo vec_op_info_vvi(OpcodeVVI op) noexcept {
  using VE = VecElementType;

  switch (op) {
    case OpcodeVVI::kSllU16         : return VecOpInfo::make(VE::kUInt16, VE::kUInt16);
    case OpcodeVVI::kSllU32         : return VecOpInfo::make(VE::kUInt32, VE::kUInt32);
    case OpcodeVVI::kSllU64         : return VecOpInfo::make(VE::kUInt64, VE::kUInt64);
    case OpcodeVVI::kSrlU16         : return VecOpInfo::make(VE::kUInt16, VE::kUInt16);
    case OpcodeVVI::kSrlU32         : return VecOpInfo::make(VE::kUInt32, VE::kUInt32);
    case OpcodeVVI::kSrlU64         : return VecOpInfo::make(VE::kUInt64, VE::kUInt64);
    case OpcodeVVI::kSraI16         : return VecOpInfo::make(VE::kInt16, VE::kInt16);
    case OpcodeVVI::kSraI32         : return VecOpInfo::make(VE::kInt32, VE::kInt32);
    case OpcodeVVI::kSraI64         : return VecOpInfo::make(VE::kInt64, VE::kInt64);
    case OpcodeVVI::kSllbU128       : return VecOpInfo::make(VE::kUInt8, VE::kUInt8);
    case OpcodeVVI::kSrlbU128       : return VecOpInfo::make(VE::kUInt8, VE::kUInt8);
    case OpcodeVVI::kSwizzleU16x4   : return VecOpInfo::make(VE::kUInt16, VE::kUInt16);
    case OpcodeVVI::kSwizzleLoU16x4 : return VecOpInfo::make(VE::kUInt16, VE::kUInt16);
    case OpcodeVVI::kSwizzleHiU16x4 : return VecOpInfo::make(VE::kUInt16, VE::kUInt16);
    case OpcodeVVI::kSwizzleU32x4   : return VecOpInfo::make(VE::kUInt32, VE::kUInt32);
    case OpcodeVVI::kSwizzleU64x2   : return VecOpInfo::make(VE::kUInt64, VE::kUInt64);
    case OpcodeVVI::kSwizzleF32x4   : return VecOpInfo::make(VE::kFloat32, VE::kFloat32);
    case OpcodeVVI::kSwizzleF64x2   : return VecOpInfo::make(VE::kFloat64, VE::kFloat64);
    case OpcodeVVI::kSwizzleU64x4   : return VecOpInfo::make(VE::kUInt64, VE::kUInt64);
    case OpcodeVVI::kSwizzleF64x4   : return VecOpInfo::make(VE::kFloat64, VE::kFloat64);
    case OpcodeVVI::kExtractV128_I32: return VecOpInfo::make(VE::kInt32, VE::kInt32);
    case OpcodeVVI::kExtractV128_I64: return VecOpInfo::make(VE::kInt64, VE::kInt64);
    case OpcodeVVI::kExtractV128_F32: return VecOpInfo::make(VE::kFloat32, VE::kFloat32);
    case OpcodeVVI::kExtractV128_F64: return VecOpInfo::make(VE::kFloat64, VE::kFloat64);
    case OpcodeVVI::kExtractV256_I32: return VecOpInfo::make(VE::kUInt32, VE::kUInt32);
    case OpcodeVVI::kExtractV256_I64: return VecOpInfo::make(VE::kUInt64, VE::kUInt64);
    case OpcodeVVI::kExtractV256_F32: return VecOpInfo::make(VE::kFloat32, VE::kFloat32);
    case OpcodeVVI::kExtractV256_F64: return VecOpInfo::make(VE::kFloat64, VE::kFloat64);

#if defined(BL_JIT_ARCH_A64)
    case OpcodeVVI::kSrlRndU16      : return VecOpInfo::make(VE::kUInt16, VE::kUInt16);
    case OpcodeVVI::kSrlRndU32      : return VecOpInfo::make(VE::kUInt32, VE::kUInt32);
    case OpcodeVVI::kSrlRndU64      : return VecOpInfo::make(VE::kUInt64, VE::kUInt64);
    case OpcodeVVI::kSrlAccU16      : return VecOpInfo::make(VE::kUInt16, VE::kUInt16);
    case OpcodeVVI::kSrlAccU32      : return VecOpInfo::make(VE::kUInt32, VE::kUInt32);
    case OpcodeVVI::kSrlAccU64      : return VecOpInfo::make(VE::kUInt64, VE::kUInt64);
    case OpcodeVVI::kSrlRndAccU16   : return VecOpInfo::make(VE::kUInt16, VE::kUInt16);
    case OpcodeVVI::kSrlRndAccU32   : return VecOpInfo::make(VE::kUInt32, VE::kUInt32);
    case OpcodeVVI::kSrlRndAccU64   : return VecOpInfo::make(VE::kUInt64, VE::kUInt64);
    case OpcodeVVI::kSrlnLoU16      : return VecOpInfo::make(VE::kUInt16, VE::kUInt16);
    case OpcodeVVI::kSrlnHiU16      : return VecOpInfo::make(VE::kUInt32, VE::kUInt32);
    case OpcodeVVI::kSrlnLoU32      : return VecOpInfo::make(VE::kUInt64, VE::kUInt64);
    case OpcodeVVI::kSrlnHiU32      : return VecOpInfo::make(VE::kUInt16, VE::kUInt16);
    case OpcodeVVI::kSrlnLoU64      : return VecOpInfo::make(VE::kUInt32, VE::kUInt32);
    case OpcodeVVI::kSrlnHiU64      : return VecOpInfo::make(VE::kUInt64, VE::kUInt64);
#endif // BL_JIT_ARCH_A64

    default:
      BL_NOT_REACHED();
  }
}

static const char* vec_op_name_vvv(OpcodeVVV op) noexcept {
  switch (op) {
    case OpcodeVVV::kAndU32         : return "v_and_u32";
    case OpcodeVVV::kAndU64         : return "v_and_u64";
    case OpcodeVVV::kOrU32          : return "v_or_u32";
    case OpcodeVVV::kOrU64          : return "v_or_u64";
    case OpcodeVVV::kXorU32         : return "v_xor_u32";
    case OpcodeVVV::kXorU64         : return "v_xor_u64";
    case OpcodeVVV::kAndnU32        : return "v_andn_u32";
    case OpcodeVVV::kAndnU64        : return "v_andn_u64";
    case OpcodeVVV::kBicU32         : return "v_bic_u32";
    case OpcodeVVV::kBicU64         : return "v_bic_u64";
    case OpcodeVVV::kAvgrU8         : return "v_avgr_u8";
    case OpcodeVVV::kAvgrU16        : return "v_avgr_u16";
    case OpcodeVVV::kAddU8          : return "v_add_u8";
    case OpcodeVVV::kAddU16         : return "v_add_u16";
    case OpcodeVVV::kAddU32         : return "v_add_u32";
    case OpcodeVVV::kAddU64         : return "v_add_u64";
    case OpcodeVVV::kSubU8          : return "v_sub_u8";
    case OpcodeVVV::kSubU16         : return "v_sub_u16";
    case OpcodeVVV::kSubU32         : return "v_sub_u32";
    case OpcodeVVV::kSubU64         : return "v_sub_u64";
    case OpcodeVVV::kAddsI8         : return "v_adds_i8";
    case OpcodeVVV::kAddsU8         : return "v_adds_u8";
    case OpcodeVVV::kAddsI16        : return "v_adds_i16";
    case OpcodeVVV::kAddsU16        : return "v_adds_u16";
    case OpcodeVVV::kSubsI8         : return "v_subs_i8";
    case OpcodeVVV::kSubsU8         : return "v_subs_u8";
    case OpcodeVVV::kSubsI16        : return "v_subs_i16";
    case OpcodeVVV::kSubsU16        : return "v_subs_u16";
    case OpcodeVVV::kMulU16         : return "v_mul_u16";
    case OpcodeVVV::kMulU32         : return "v_mul_u32";
    case OpcodeVVV::kMulU64         : return "v_mul_u64";
    case OpcodeVVV::kMulhI16        : return "v_mulh_i16";
    case OpcodeVVV::kMulhU16        : return "v_mulh_u16";
    case OpcodeVVV::kMulU64_LoU32   : return "v_mul_u64_lo_u32";
    case OpcodeVVV::kMHAddI16_I32   : return "v_mhadd_i16_i32";
    case OpcodeVVV::kMinI8          : return "v_min_i8";
    case OpcodeVVV::kMinU8          : return "v_min_u8";
    case OpcodeVVV::kMinI16         : return "v_min_i16";
    case OpcodeVVV::kMinU16         : return "v_min_u16";
    case OpcodeVVV::kMinI32         : return "v_min_i32";
    case OpcodeVVV::kMinU32         : return "v_min_u32";
    case OpcodeVVV::kMinI64         : return "v_min_i64";
    case OpcodeVVV::kMinU64         : return "v_min_u64";
    case OpcodeVVV::kMaxI8          : return "v_max_i8";
    case OpcodeVVV::kMaxU8          : return "v_max_u8";
    case OpcodeVVV::kMaxI16         : return "v_max_i16";
    case OpcodeVVV::kMaxU16         : return "v_max_u16";
    case OpcodeVVV::kMaxI32         : return "v_max_i32";
    case OpcodeVVV::kMaxU32         : return "v_max_u32";
    case OpcodeVVV::kMaxI64         : return "v_max_i64";
    case OpcodeVVV::kMaxU64         : return "v_max_u64";
    case OpcodeVVV::kCmpEqU8        : return "v_cmp_eq_u8";
    case OpcodeVVV::kCmpEqU16       : return "v_cmp_eq_u16";
    case OpcodeVVV::kCmpEqU32       : return "v_cmp_eq_u32";
    case OpcodeVVV::kCmpEqU64       : return "v_cmp_eq_u64";
    case OpcodeVVV::kCmpGtI8        : return "v_cmp_gt_i8";
    case OpcodeVVV::kCmpGtU8        : return "v_cmp_gt_u8";
    case OpcodeVVV::kCmpGtI16       : return "v_cmp_gt_i16";
    case OpcodeVVV::kCmpGtU16       : return "v_cmp_gt_u16";
    case OpcodeVVV::kCmpGtI32       : return "v_cmp_gt_i32";
    case OpcodeVVV::kCmpGtU32       : return "v_cmp_gt_u32";
    case OpcodeVVV::kCmpGtI64       : return "v_cmp_gt_i64";
    case OpcodeVVV::kCmpGtU64       : return "v_cmp_gt_u64";
    case OpcodeVVV::kCmpGeI8        : return "v_cmp_ge_i8";
    case OpcodeVVV::kCmpGeU8        : return "v_cmp_ge_u8";
    case OpcodeVVV::kCmpGeI16       : return "v_cmp_ge_i16";
    case OpcodeVVV::kCmpGeU16       : return "v_cmp_ge_u16";
    case OpcodeVVV::kCmpGeI32       : return "v_cmp_ge_i32";
    case OpcodeVVV::kCmpGeU32       : return "v_cmp_ge_u32";
    case OpcodeVVV::kCmpGeI64       : return "v_cmp_ge_i64";
    case OpcodeVVV::kCmpGeU64       : return "v_cmp_ge_u64";
    case OpcodeVVV::kCmpLtI8        : return "v_cmp_lt_i8";
    case OpcodeVVV::kCmpLtU8        : return "v_cmp_lt_u8";
    case OpcodeVVV::kCmpLtI16       : return "v_cmp_lt_i16";
    case OpcodeVVV::kCmpLtU16       : return "v_cmp_lt_u16";
    case OpcodeVVV::kCmpLtI32       : return "v_cmp_lt_i32";
    case OpcodeVVV::kCmpLtU32       : return "v_cmp_lt_u32";
    case OpcodeVVV::kCmpLtI64       : return "v_cmp_lt_i64";
    case OpcodeVVV::kCmpLtU64       : return "v_cmp_lt_u64";
    case OpcodeVVV::kCmpLeI8        : return "v_cmp_le_i8";
    case OpcodeVVV::kCmpLeU8        : return "v_cmp_le_u8";
    case OpcodeVVV::kCmpLeI16       : return "v_cmp_le_i16";
    case OpcodeVVV::kCmpLeU16       : return "v_cmp_le_u16";
    case OpcodeVVV::kCmpLeI32       : return "v_cmp_le_i32";
    case OpcodeVVV::kCmpLeU32       : return "v_cmp_le_u32";
    case OpcodeVVV::kCmpLeI64       : return "v_cmp_le_i64";
    case OpcodeVVV::kCmpLeU64       : return "v_cmp_le_u64";
    case OpcodeVVV::kAndF32         : return "v_and_f32";
    case OpcodeVVV::kAndF64         : return "v_and_f64";
    case OpcodeVVV::kOrF32          : return "v_or_f32";
    case OpcodeVVV::kOrF64          : return "v_or_f64";
    case OpcodeVVV::kXorF32         : return "v_xor_f32";
    case OpcodeVVV::kXorF64         : return "v_xor_f64";
    case OpcodeVVV::kAndnF32        : return "v_andn_f32";
    case OpcodeVVV::kAndnF64        : return "v_andn_f64";
    case OpcodeVVV::kBicF32         : return "v_bic_f32";
    case OpcodeVVV::kBicF64         : return "v_bic_f64";
    case OpcodeVVV::kAddF32S        : return "v_add_f32s";
    case OpcodeVVV::kAddF64S        : return "v_add_f64s";
    case OpcodeVVV::kAddF32         : return "v_add_f32";
    case OpcodeVVV::kAddF64         : return "v_add_f64";
    case OpcodeVVV::kSubF32S        : return "v_sub_f32s";
    case OpcodeVVV::kSubF64S        : return "v_sub_f64s";
    case OpcodeVVV::kSubF32         : return "v_sub_f32";
    case OpcodeVVV::kSubF64         : return "v_sub_f64";
    case OpcodeVVV::kMulF32S        : return "v_mul_f32s";
    case OpcodeVVV::kMulF64S        : return "v_mul_f64s";
    case OpcodeVVV::kMulF32         : return "v_mul_f32";
    case OpcodeVVV::kMulF64         : return "v_mul_f64";
    case OpcodeVVV::kDivF32S        : return "v_div_f32s";
    case OpcodeVVV::kDivF64S        : return "v_div_f64s";
    case OpcodeVVV::kDivF32         : return "v_div_f32";
    case OpcodeVVV::kDivF64         : return "v_div_f64";
    case OpcodeVVV::kMinF32S        : return "v_min_f32s";
    case OpcodeVVV::kMinF64S        : return "v_min_f64s";
    case OpcodeVVV::kMinF32         : return "v_min_f32";
    case OpcodeVVV::kMinF64         : return "v_min_f64";
    case OpcodeVVV::kMaxF32S        : return "v_max_f32s";
    case OpcodeVVV::kMaxF64S        : return "v_max_f64s";
    case OpcodeVVV::kMaxF32         : return "v_max_f32";
    case OpcodeVVV::kMaxF64         : return "v_max_f64";
    case OpcodeVVV::kCmpEqF32S      : return "v_cmp_eq_f32s";
    case OpcodeVVV::kCmpEqF64S      : return "v_cmp_eq_f64s";
    case OpcodeVVV::kCmpEqF32       : return "v_cmp_eq_f32";
    case OpcodeVVV::kCmpEqF64       : return "v_cmp_eq_f64";
    case OpcodeVVV::kCmpNeF32S      : return "v_cmp_ne_f32s";
    case OpcodeVVV::kCmpNeF64S      : return "v_cmp_ne_f64s";
    case OpcodeVVV::kCmpNeF32       : return "v_cmp_ne_f32";
    case OpcodeVVV::kCmpNeF64       : return "v_cmp_ne_f64";
    case OpcodeVVV::kCmpGtF32S      : return "v_cmp_gt_f32s";
    case OpcodeVVV::kCmpGtF64S      : return "v_cmp_gt_f64s";
    case OpcodeVVV::kCmpGtF32       : return "v_cmp_gt_f32";
    case OpcodeVVV::kCmpGtF64       : return "v_cmp_gt_f64";
    case OpcodeVVV::kCmpGeF32S      : return "v_cmp_ge_f32s";
    case OpcodeVVV::kCmpGeF64S      : return "v_cmp_ge_f64s";
    case OpcodeVVV::kCmpGeF32       : return "v_cmp_ge_f32";
    case OpcodeVVV::kCmpGeF64       : return "v_cmp_ge_f64";
    case OpcodeVVV::kCmpLtF32S      : return "v_cmp_lt_f32s";
    case OpcodeVVV::kCmpLtF64S      : return "v_cmp_lt_f64s";
    case OpcodeVVV::kCmpLtF32       : return "v_cmp_lt_f32";
    case OpcodeVVV::kCmpLtF64       : return "v_cmp_lt_f64";
    case OpcodeVVV::kCmpLeF32S      : return "v_cmp_le_f32s";
    case OpcodeVVV::kCmpLeF64S      : return "v_cmp_le_f64s";
    case OpcodeVVV::kCmpLeF32       : return "v_cmp_le_f32";
    case OpcodeVVV::kCmpLeF64       : return "v_cmp_le_f64";
    case OpcodeVVV::kCmpOrdF32S     : return "v_cmp_ord_f32s";
    case OpcodeVVV::kCmpOrdF64S     : return "v_cmp_ord_f64s";
    case OpcodeVVV::kCmpOrdF32      : return "v_cmp_ord_f32";
    case OpcodeVVV::kCmpOrdF64      : return "v_cmp_ord_f64";
    case OpcodeVVV::kCmpUnordF32S   : return "v_cmp_unord_f32s";
    case OpcodeVVV::kCmpUnordF64S   : return "v_cmp_unord_f64s";
    case OpcodeVVV::kCmpUnordF32    : return "v_cmp_unord_f32";
    case OpcodeVVV::kCmpUnordF64    : return "v_cmp_unord_f64";
    case OpcodeVVV::kHAddF64        : return "v_hadd_f64";
    case OpcodeVVV::kCombineLoHiU64 : return "v_combine_lo_hi_u64";
    case OpcodeVVV::kCombineLoHiF64 : return "v_combine_lo_hi_f64";
    case OpcodeVVV::kCombineHiLoU64 : return "v_combine_hi_lo_u64";
    case OpcodeVVV::kCombineHiLoF64 : return "v_combine_hi_lo_f64";
    case OpcodeVVV::kInterleaveLoU8 : return "v_interleave_lo_u8";
    case OpcodeVVV::kInterleaveHiU8 : return "v_interleave_hi_u8";
    case OpcodeVVV::kInterleaveLoU16: return "v_interleave_lo_u16";
    case OpcodeVVV::kInterleaveHiU16: return "v_interleave_hi_u16";
    case OpcodeVVV::kInterleaveLoU32: return "v_interleave_lo_u32";
    case OpcodeVVV::kInterleaveHiU32: return "v_interleave_hi_u32";
    case OpcodeVVV::kInterleaveLoU64: return "v_interleave_lo_u64";
    case OpcodeVVV::kInterleaveHiU64: return "v_interleave_hi_u64";
    case OpcodeVVV::kInterleaveLoF32: return "v_interleave_lo_f32";
    case OpcodeVVV::kInterleaveHiF32: return "v_interleave_hi_f32";
    case OpcodeVVV::kInterleaveLoF64: return "v_interleave_lo_f64";
    case OpcodeVVV::kInterleaveHiF64: return "v_interleave_hi_f64";
    case OpcodeVVV::kPacksI16_I8    : return "v_packs_i16_i8";
    case OpcodeVVV::kPacksI16_U8    : return "v_packs_i16_u8";
    case OpcodeVVV::kPacksI32_I16   : return "v_packs_i32_i16";
    case OpcodeVVV::kPacksI32_U16   : return "v_packs_i32_u16";
    case OpcodeVVV::kSwizzlev_U8    : return "v_swizzlev_u8";

#if defined(BL_JIT_ARCH_A64)
    case OpcodeVVV::kMulwLoI8       : return "v_mulw_lo_i8";
    case OpcodeVVV::kMulwLoU8       : return "v_mulw_lo_u8";
    case OpcodeVVV::kMulwHiI8       : return "v_mulw_hi_i8";
    case OpcodeVVV::kMulwHiU8       : return "v_mulw_hi_u8";
    case OpcodeVVV::kMulwLoI16      : return "v_mulw_lo_i16";
    case OpcodeVVV::kMulwLoU16      : return "v_mulw_lo_u16";
    case OpcodeVVV::kMulwHiI16      : return "v_mulw_hi_i16";
    case OpcodeVVV::kMulwHiU16      : return "v_mulw_hi_u16";
    case OpcodeVVV::kMulwLoI32      : return "v_mulw_lo_i32";
    case OpcodeVVV::kMulwLoU32      : return "v_mulw_lo_u32";
    case OpcodeVVV::kMulwHiI32      : return "v_mulw_hi_i32";
    case OpcodeVVV::kMulwHiU32      : return "v_mulw_hi_u32";
    case OpcodeVVV::kMAddwLoI8      : return "v_maddw_lo_i8";
    case OpcodeVVV::kMAddwLoU8      : return "v_maddw_lo_u8";
    case OpcodeVVV::kMAddwHiI8      : return "v_maddw_hi_i8";
    case OpcodeVVV::kMAddwHiU8      : return "v_maddw_hi_u8";
    case OpcodeVVV::kMAddwLoI16     : return "v_maddw_lo_i16";
    case OpcodeVVV::kMAddwLoU16     : return "v_maddw_lo_u16";
    case OpcodeVVV::kMAddwHiI16     : return "v_maddw_hi_i16";
    case OpcodeVVV::kMAddwHiU16     : return "v_maddw_hi_u16";
    case OpcodeVVV::kMAddwLoI32     : return "v_maddw_lo_i32";
    case OpcodeVVV::kMAddwLoU32     : return "v_maddw_lo_u32";
    case OpcodeVVV::kMAddwHiI32     : return "v_maddw_hi_i32";
    case OpcodeVVV::kMAddwHiU32     : return "v_maddw_hi_u32";
#endif // BL_JIT_ARCH_A64

    default:
      BL_NOT_REACHED();
  }
}

static VecOpInfo vec_op_info_vvv(OpcodeVVV op) noexcept {
  using VE = VecElementType;

  switch (op) {
    case OpcodeVVV::kAndU32         : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kAndU64         : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kOrU32          : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kOrU64          : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kXorU32         : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kXorU64         : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kAndnU32        : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kAndnU64        : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kBicU32         : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kBicU64         : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kAvgrU8         : return VecOpInfo::make(VE::kUInt8, VE::kUInt8, VE::kUInt8);
    case OpcodeVVV::kAvgrU16        : return VecOpInfo::make(VE::kUInt16, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kAddU8          : return VecOpInfo::make(VE::kUInt8, VE::kUInt8, VE::kUInt8);
    case OpcodeVVV::kAddU16         : return VecOpInfo::make(VE::kUInt16, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kAddU32         : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kAddU64         : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kSubU8          : return VecOpInfo::make(VE::kUInt8, VE::kUInt8, VE::kUInt8);
    case OpcodeVVV::kSubU16         : return VecOpInfo::make(VE::kUInt16, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kSubU32         : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kSubU64         : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kAddsI8         : return VecOpInfo::make(VE::kInt8, VE::kInt8, VE::kInt8);
    case OpcodeVVV::kAddsU8         : return VecOpInfo::make(VE::kUInt8, VE::kUInt8, VE::kUInt8);
    case OpcodeVVV::kAddsI16        : return VecOpInfo::make(VE::kInt16, VE::kInt16, VE::kInt16);
    case OpcodeVVV::kAddsU16        : return VecOpInfo::make(VE::kUInt16, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kSubsI8         : return VecOpInfo::make(VE::kInt8, VE::kInt8, VE::kInt8);
    case OpcodeVVV::kSubsU8         : return VecOpInfo::make(VE::kUInt8, VE::kUInt8, VE::kUInt8);
    case OpcodeVVV::kSubsI16        : return VecOpInfo::make(VE::kInt16, VE::kInt16, VE::kInt16);
    case OpcodeVVV::kSubsU16        : return VecOpInfo::make(VE::kUInt16, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kMulU16         : return VecOpInfo::make(VE::kUInt16, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kMulU32         : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kMulU64         : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kMulhI16        : return VecOpInfo::make(VE::kInt16, VE::kInt16, VE::kInt16);
    case OpcodeVVV::kMulhU16        : return VecOpInfo::make(VE::kUInt16, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kMulU64_LoU32   : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt32);
    case OpcodeVVV::kMHAddI16_I32   : return VecOpInfo::make(VE::kInt32, VE::kInt16, VE::kInt16);
    case OpcodeVVV::kMinI8          : return VecOpInfo::make(VE::kInt8, VE::kInt8, VE::kInt8);
    case OpcodeVVV::kMinU8          : return VecOpInfo::make(VE::kUInt8, VE::kUInt8, VE::kUInt8);
    case OpcodeVVV::kMinI16         : return VecOpInfo::make(VE::kInt16, VE::kInt16, VE::kInt16);
    case OpcodeVVV::kMinU16         : return VecOpInfo::make(VE::kUInt16, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kMinI32         : return VecOpInfo::make(VE::kInt32, VE::kInt32, VE::kInt32);
    case OpcodeVVV::kMinU32         : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kMinI64         : return VecOpInfo::make(VE::kInt64, VE::kInt64, VE::kInt64);
    case OpcodeVVV::kMinU64         : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kMaxI8          : return VecOpInfo::make(VE::kInt8, VE::kInt8, VE::kInt8);
    case OpcodeVVV::kMaxU8          : return VecOpInfo::make(VE::kUInt8, VE::kUInt8, VE::kUInt8);
    case OpcodeVVV::kMaxI16         : return VecOpInfo::make(VE::kInt16, VE::kInt16, VE::kInt16);
    case OpcodeVVV::kMaxU16         : return VecOpInfo::make(VE::kUInt16, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kMaxI32         : return VecOpInfo::make(VE::kInt32, VE::kInt32, VE::kInt32);
    case OpcodeVVV::kMaxU32         : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kMaxI64         : return VecOpInfo::make(VE::kInt64, VE::kInt64, VE::kInt64);
    case OpcodeVVV::kMaxU64         : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kCmpEqU8        : return VecOpInfo::make(VE::kUInt8, VE::kUInt8, VE::kUInt8);
    case OpcodeVVV::kCmpEqU16       : return VecOpInfo::make(VE::kUInt16, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kCmpEqU32       : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kCmpEqU64       : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kCmpGtI8        : return VecOpInfo::make(VE::kInt8, VE::kInt8, VE::kInt8);
    case OpcodeVVV::kCmpGtU8        : return VecOpInfo::make(VE::kUInt8, VE::kUInt8, VE::kUInt8);
    case OpcodeVVV::kCmpGtI16       : return VecOpInfo::make(VE::kInt16, VE::kInt16, VE::kInt16);
    case OpcodeVVV::kCmpGtU16       : return VecOpInfo::make(VE::kUInt16, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kCmpGtI32       : return VecOpInfo::make(VE::kInt32, VE::kInt32, VE::kInt32);
    case OpcodeVVV::kCmpGtU32       : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kCmpGtI64       : return VecOpInfo::make(VE::kInt64, VE::kInt64, VE::kInt64);
    case OpcodeVVV::kCmpGtU64       : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kCmpGeI8        : return VecOpInfo::make(VE::kInt8, VE::kInt8, VE::kInt8);
    case OpcodeVVV::kCmpGeU8        : return VecOpInfo::make(VE::kUInt8, VE::kUInt8, VE::kUInt8);
    case OpcodeVVV::kCmpGeI16       : return VecOpInfo::make(VE::kInt16, VE::kInt16, VE::kInt16);
    case OpcodeVVV::kCmpGeU16       : return VecOpInfo::make(VE::kUInt16, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kCmpGeI32       : return VecOpInfo::make(VE::kInt32, VE::kInt32, VE::kInt32);
    case OpcodeVVV::kCmpGeU32       : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kCmpGeI64       : return VecOpInfo::make(VE::kInt64, VE::kInt64, VE::kInt64);
    case OpcodeVVV::kCmpGeU64       : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kCmpLtI8        : return VecOpInfo::make(VE::kInt8, VE::kInt8, VE::kInt8);
    case OpcodeVVV::kCmpLtU8        : return VecOpInfo::make(VE::kUInt8, VE::kUInt8, VE::kUInt8);
    case OpcodeVVV::kCmpLtI16       : return VecOpInfo::make(VE::kInt16, VE::kInt16, VE::kInt16);
    case OpcodeVVV::kCmpLtU16       : return VecOpInfo::make(VE::kUInt16, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kCmpLtI32       : return VecOpInfo::make(VE::kInt32, VE::kInt32, VE::kInt32);
    case OpcodeVVV::kCmpLtU32       : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kCmpLtI64       : return VecOpInfo::make(VE::kInt64, VE::kInt64, VE::kInt64);
    case OpcodeVVV::kCmpLtU64       : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kCmpLeI8        : return VecOpInfo::make(VE::kInt8, VE::kInt8, VE::kInt8);
    case OpcodeVVV::kCmpLeU8        : return VecOpInfo::make(VE::kUInt8, VE::kUInt8, VE::kUInt8);
    case OpcodeVVV::kCmpLeI16       : return VecOpInfo::make(VE::kInt16, VE::kInt16, VE::kInt16);
    case OpcodeVVV::kCmpLeU16       : return VecOpInfo::make(VE::kUInt16, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kCmpLeI32       : return VecOpInfo::make(VE::kInt32, VE::kInt32, VE::kInt32);
    case OpcodeVVV::kCmpLeU32       : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kCmpLeI64       : return VecOpInfo::make(VE::kInt64, VE::kInt64, VE::kInt64);
    case OpcodeVVV::kCmpLeU64       : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kAndF32         : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kAndF64         : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kOrF32          : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kOrF64          : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kXorF32         : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kXorF64         : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kAndnF32        : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kAndnF64        : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kBicF32         : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kBicF64         : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kAddF32S        : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kAddF64S        : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kAddF32         : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kAddF64         : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kSubF32S        : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kSubF64S        : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kSubF32         : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kSubF64         : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kMulF32S        : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kMulF64S        : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kMulF32         : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kMulF64         : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kDivF32S        : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kDivF64S        : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kDivF32         : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kDivF64         : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kMinF32S        : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kMinF64S        : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kMinF32         : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kMinF64         : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kMaxF32S        : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kMaxF64S        : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kMaxF32         : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kMaxF64         : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kCmpEqF32S      : return VecOpInfo::make(VE::kUInt32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kCmpEqF64S      : return VecOpInfo::make(VE::kUInt64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kCmpEqF32       : return VecOpInfo::make(VE::kUInt32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kCmpEqF64       : return VecOpInfo::make(VE::kUInt64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kCmpNeF32S      : return VecOpInfo::make(VE::kUInt32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kCmpNeF64S      : return VecOpInfo::make(VE::kUInt64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kCmpNeF32       : return VecOpInfo::make(VE::kUInt32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kCmpNeF64       : return VecOpInfo::make(VE::kUInt64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kCmpGtF32S      : return VecOpInfo::make(VE::kUInt32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kCmpGtF64S      : return VecOpInfo::make(VE::kUInt64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kCmpGtF32       : return VecOpInfo::make(VE::kUInt32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kCmpGtF64       : return VecOpInfo::make(VE::kUInt64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kCmpGeF32S      : return VecOpInfo::make(VE::kUInt32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kCmpGeF64S      : return VecOpInfo::make(VE::kUInt64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kCmpGeF32       : return VecOpInfo::make(VE::kUInt32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kCmpGeF64       : return VecOpInfo::make(VE::kUInt64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kCmpLtF32S      : return VecOpInfo::make(VE::kUInt32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kCmpLtF64S      : return VecOpInfo::make(VE::kUInt64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kCmpLtF32       : return VecOpInfo::make(VE::kUInt32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kCmpLtF64       : return VecOpInfo::make(VE::kUInt64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kCmpLeF32S      : return VecOpInfo::make(VE::kUInt32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kCmpLeF64S      : return VecOpInfo::make(VE::kUInt64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kCmpLeF32       : return VecOpInfo::make(VE::kUInt32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kCmpLeF64       : return VecOpInfo::make(VE::kUInt64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kCmpOrdF32S     : return VecOpInfo::make(VE::kUInt32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kCmpOrdF64S     : return VecOpInfo::make(VE::kUInt64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kCmpOrdF32      : return VecOpInfo::make(VE::kUInt32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kCmpOrdF64      : return VecOpInfo::make(VE::kUInt64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kCmpUnordF32S   : return VecOpInfo::make(VE::kUInt32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kCmpUnordF64S   : return VecOpInfo::make(VE::kUInt64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kCmpUnordF32    : return VecOpInfo::make(VE::kUInt32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kCmpUnordF64    : return VecOpInfo::make(VE::kUInt64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kHAddF64        : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kCombineLoHiU64 : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kCombineLoHiF64 : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kCombineHiLoU64 : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kCombineHiLoF64 : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kInterleaveLoU8 : return VecOpInfo::make(VE::kUInt8, VE::kUInt8, VE::kUInt8);
    case OpcodeVVV::kInterleaveHiU8 : return VecOpInfo::make(VE::kUInt8, VE::kUInt8, VE::kUInt8);
    case OpcodeVVV::kInterleaveLoU16: return VecOpInfo::make(VE::kUInt16, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kInterleaveHiU16: return VecOpInfo::make(VE::kUInt16, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kInterleaveLoU32: return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kInterleaveHiU32: return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kInterleaveLoU64: return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kInterleaveHiU64: return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVV::kInterleaveLoF32: return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kInterleaveHiF32: return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVV::kInterleaveLoF64: return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kInterleaveHiF64: return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVV::kPacksI16_I8    : return VecOpInfo::make(VE::kInt8, VE::kInt16, VE::kInt16);
    case OpcodeVVV::kPacksI16_U8    : return VecOpInfo::make(VE::kUInt8, VE::kInt16, VE::kInt16);
    case OpcodeVVV::kPacksI32_I16   : return VecOpInfo::make(VE::kInt16, VE::kInt32, VE::kInt32);
    case OpcodeVVV::kPacksI32_U16   : return VecOpInfo::make(VE::kUInt16, VE::kInt32, VE::kInt32);
    case OpcodeVVV::kSwizzlev_U8    : return VecOpInfo::make(VE::kUInt8, VE::kUInt8, VE::kUInt8);

#if defined(BL_JIT_ARCH_A64)
    case OpcodeVVV::kMulwLoI8       : return VecOpInfo::make(VE::kInt16, VE::kInt8, VE::kInt8);
    case OpcodeVVV::kMulwLoU8       : return VecOpInfo::make(VE::kUInt16, VE::kUInt8, VE::kUInt8);
    case OpcodeVVV::kMulwHiI8       : return VecOpInfo::make(VE::kInt16, VE::kInt8, VE::kInt8);
    case OpcodeVVV::kMulwHiU8       : return VecOpInfo::make(VE::kUInt16, VE::kUInt8, VE::kUInt8);
    case OpcodeVVV::kMulwLoI16      : return VecOpInfo::make(VE::kInt32, VE::kInt16, VE::kInt16);
    case OpcodeVVV::kMulwLoU16      : return VecOpInfo::make(VE::kUInt32, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kMulwHiI16      : return VecOpInfo::make(VE::kInt32, VE::kInt16, VE::kInt16);
    case OpcodeVVV::kMulwHiU16      : return VecOpInfo::make(VE::kUInt32, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kMulwLoI32      : return VecOpInfo::make(VE::kInt64, VE::kInt32, VE::kInt32);
    case OpcodeVVV::kMulwLoU32      : return VecOpInfo::make(VE::kUInt64, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kMulwHiI32      : return VecOpInfo::make(VE::kInt64, VE::kInt32, VE::kInt32);
    case OpcodeVVV::kMulwHiU32      : return VecOpInfo::make(VE::kUInt64, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kMAddwLoI8      : return VecOpInfo::make(VE::kInt16, VE::kInt8, VE::kInt8);
    case OpcodeVVV::kMAddwLoU8      : return VecOpInfo::make(VE::kUInt16, VE::kUInt8, VE::kUInt8);
    case OpcodeVVV::kMAddwHiI8      : return VecOpInfo::make(VE::kInt16, VE::kInt8, VE::kInt8);
    case OpcodeVVV::kMAddwHiU8      : return VecOpInfo::make(VE::kUInt16, VE::kUInt8, VE::kUInt8);
    case OpcodeVVV::kMAddwLoI16     : return VecOpInfo::make(VE::kInt32, VE::kInt16, VE::kInt16);
    case OpcodeVVV::kMAddwLoU16     : return VecOpInfo::make(VE::kUInt32, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kMAddwHiI16     : return VecOpInfo::make(VE::kInt32, VE::kInt16, VE::kInt16);
    case OpcodeVVV::kMAddwHiU16     : return VecOpInfo::make(VE::kUInt32, VE::kUInt16, VE::kUInt16);
    case OpcodeVVV::kMAddwLoI32     : return VecOpInfo::make(VE::kInt64, VE::kInt32, VE::kInt32);
    case OpcodeVVV::kMAddwLoU32     : return VecOpInfo::make(VE::kUInt64, VE::kUInt32, VE::kUInt32);
    case OpcodeVVV::kMAddwHiI32     : return VecOpInfo::make(VE::kInt64, VE::kInt32, VE::kInt32);
    case OpcodeVVV::kMAddwHiU32     : return VecOpInfo::make(VE::kUInt64, VE::kUInt32, VE::kUInt32);
#endif // BL_JIT_ARCH_A64

    default:
      BL_NOT_REACHED();
  }
}

static const char* vec_op_name_vvvi(OpcodeVVVI op) noexcept {
  switch (op) {
    case OpcodeVVVI::kAlignr_U128           : return "v_alignr_u128";
    case OpcodeVVVI::kInterleaveShuffleU32x4: return "v_interleave_shuffle_u32x4";
    case OpcodeVVVI::kInterleaveShuffleU64x2: return "v_interleave_shuffle_u64x2";
    case OpcodeVVVI::kInterleaveShuffleF32x4: return "v_interleave_shuffle_f32x4";
    case OpcodeVVVI::kInterleaveShuffleF64x2: return "v_interleave_shuffle_f64x2";
    case OpcodeVVVI::kInsertV128_U32        : return "v_insert_v128_u32";
    case OpcodeVVVI::kInsertV128_F32        : return "v_insert_v128_f32";
    case OpcodeVVVI::kInsertV128_U64        : return "v_insert_v128_u64";
    case OpcodeVVVI::kInsertV128_F64        : return "v_insert_v128_f64";
    case OpcodeVVVI::kInsertV256_U32        : return "v_insert_v256_u32";
    case OpcodeVVVI::kInsertV256_F32        : return "v_insert_v256_f32";
    case OpcodeVVVI::kInsertV256_U64        : return "v_insert_v256_u64";
    case OpcodeVVVI::kInsertV256_F64        : return "v_insert_v256_f64";

    default:
      BL_NOT_REACHED();
  }
}

static VecOpInfo vec_op_info_vvvi(OpcodeVVVI op) noexcept {
  using VE = VecElementType;

  switch (op) {
    case OpcodeVVVI::kAlignr_U128           : return VecOpInfo::make(VE::kUInt8, VE::kUInt8, VE::kUInt8);
    case OpcodeVVVI::kInterleaveShuffleU32x4: return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVVI::kInterleaveShuffleU64x2: return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVVI::kInterleaveShuffleF32x4: return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVVI::kInterleaveShuffleF64x2: return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVVI::kInsertV128_U32        : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVVI::kInsertV128_F32        : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVVI::kInsertV128_U64        : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVVI::kInsertV128_F64        : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVVI::kInsertV256_U32        : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVVI::kInsertV256_F32        : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVVI::kInsertV256_U64        : return VecOpInfo::make(VE::kUInt64, VE::kUInt64, VE::kUInt64);
    case OpcodeVVVI::kInsertV256_F64        : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64);

    default:
      BL_NOT_REACHED();
  }
}

static const char* vec_op_name_vvvv(OpcodeVVVV op) noexcept {
  switch (op) {
    case OpcodeVVVV::kBlendV_U8: return "v_blendv_u8";
    case OpcodeVVVV::kMAddU16  : return "v_madd_u16";
    case OpcodeVVVV::kMAddU32  : return "v_madd_u32";
    case OpcodeVVVV::kMAddF32S : return "v_madd_f32s";
    case OpcodeVVVV::kMAddF64S : return "v_madd_f64s";
    case OpcodeVVVV::kMAddF32  : return "v_madd_f32";
    case OpcodeVVVV::kMAddF64  : return "v_madd_f64";
    case OpcodeVVVV::kMSubF32S : return "v_msub_f32s";
    case OpcodeVVVV::kMSubF64S : return "v_msub_f64s";
    case OpcodeVVVV::kMSubF32  : return "v_msub_f32";
    case OpcodeVVVV::kMSubF64  : return "v_msub_f64";
    case OpcodeVVVV::kNMAddF32S: return "v_nmadd_f32s";
    case OpcodeVVVV::kNMAddF64S: return "v_nmadd_f64s";
    case OpcodeVVVV::kNMAddF32 : return "v_nmadd_f32";
    case OpcodeVVVV::kNMAddF64 : return "v_nmadd_f64";
    case OpcodeVVVV::kNMSubF32S: return "v_nmsub_f32s";
    case OpcodeVVVV::kNMSubF64S: return "v_nmsub_f64s";
    case OpcodeVVVV::kNMSubF32 : return "v_nmsub_f32";
    case OpcodeVVVV::kNMSubF64 : return "v_nmsub_f64";

    default:
      BL_NOT_REACHED();
  }
}

static VecOpInfo vec_op_info_vvvv(OpcodeVVVV op) noexcept {
  using VE = VecElementType;

  switch (op) {
    case OpcodeVVVV::kBlendV_U8: return VecOpInfo::make(VE::kUInt8, VE::kUInt8, VE::kUInt8, VE::kUInt8);
    case OpcodeVVVV::kMAddU16  : return VecOpInfo::make(VE::kUInt16, VE::kUInt16, VE::kUInt16, VE::kUInt16);
    case OpcodeVVVV::kMAddU32  : return VecOpInfo::make(VE::kUInt32, VE::kUInt32, VE::kUInt32, VE::kUInt32);
    case OpcodeVVVV::kMAddF32S : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVVV::kMAddF64S : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVVV::kMAddF32  : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVVV::kMAddF64  : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVVV::kMSubF32S : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVVV::kMSubF64S : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVVV::kMSubF32  : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVVV::kMSubF64  : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVVV::kNMAddF32S: return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVVV::kNMAddF64S: return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVVV::kNMAddF32 : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVVV::kNMAddF64 : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVVV::kNMSubF32S: return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVVV::kNMSubF64S: return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64, VE::kFloat64);
    case OpcodeVVVV::kNMSubF32 : return VecOpInfo::make(VE::kFloat32, VE::kFloat32, VE::kFloat32, VE::kFloat32);
    case OpcodeVVVV::kNMSubF64 : return VecOpInfo::make(VE::kFloat64, VE::kFloat64, VE::kFloat64, VE::kFloat64);

    default:
      BL_NOT_REACHED();
  }
}

// bl::Pipeline::JIT - Tests - SIMD - Float To Int - Machine Behavior
// ==================================================================

#if defined(BL_JIT_ARCH_X86)
static constexpr int32_t kNaNToInt32 = INT32_MIN;
static constexpr int32_t kPInfToInt32 = INT32_MIN;
static constexpr int32_t kNInfToInt32 = INT32_MIN;
#endif // BL_JIT_ARCH_X86

#if defined(BL_JIT_ARCH_A64)
static constexpr int32_t kNaNToInt32 = 0;
static constexpr int32_t kPInfToInt32 = INT32_MAX;
static constexpr int32_t kNInfToInt32 = INT32_MIN;
#endif // BL_JIT_ARCH_A64

static BL_INLINE_NODEBUG int32_t cvt_non_finite_f32_to_i32(float x) noexcept {
  if (x == Math::inf<float>())
    return kPInfToInt32;
  if (x == -Math::inf<float>())
    return kNInfToInt32;
  return kNaNToInt32;
}

static BL_INLINE_NODEBUG int32_t cvt_non_finite_f64_to_i32(double x) noexcept {
  if (x == Math::inf<double>())
    return kPInfToInt32;
  if (x == -Math::inf<double>())
    return kNInfToInt32;
  return kNaNToInt32;
}

// bl::Pipeline::JIT - Tests - SIMD - Data Generators & Constraints
// ================================================================

// Data generator, which is used to fill the content of SIMD registers.
class DataGenInt {
public:
  BLRandom rng;
  uint32_t step;

  struct Float32Data {
    uint32_t u32;
    float f32;
  };

  BL_INLINE explicit DataGenInt(uint64_t seed) noexcept
    : rng(seed),
      step(0) {}

  BL_NOINLINE uint64_t nextUInt64() noexcept {
    if (++step >= 256)
      step = 0;

    // NOTE: Nothing really elaborate - sometimes we want to test also numbers
    // that random number generators won't return often, so we hardcode some.
    switch (step) {
      case   0: return 0u;
      case   1: return 0u;
      case   2: return 0u;
      case   6: return 1u;
      case   7: return 0u;
      case  10: return 0u;
      case  11: return 0xFFu;
      case  15: return 0xFFFFu;
      case  17: return 0xFFFFFFFFu;
      case  21: return 0xFFFFFFFFFFFFFFFFu;
      case  24: return 1u;
      case  40: return 0xFFu;
      case  55: return 0x8080808080808080u;
      case  66: return 0x80000080u;
      case  69: return 1u;
      case  79: return 0x7F;
      case 122: return 0xFFFFu;
      case 123: return 0xFFFFu;
      case 124: return 0xFFFFu;
      case 127: return 1u;
      case 130: return 0xFFu;
      case 142: return 0x7FFFu;
      case 143: return 0x7FFFu;
      case 144: return 0u;
      case 145: return 0x7FFFu;
      default : return rng.nextUInt64();
    }
  }

  BL_NOINLINE float nextFloat32() noexcept {
    if (++step >= 256)
      step = 0;

    switch (step) {
      case   0: return 0.0f;
      case   1: return 0.0f;
      case   2: return 0.0f;
      case   6: return 1.0f;
      case   7: return 0.0f;
      case  10: return 0.00001f;
      case  11: return 2.0f;
      case  12: return -Math::inf<float>();
      case  15: return 3.0f;
      case  17: return 256.0f;
      case  21: return 0.5f;
      case  23: return Math::nan<float>();
      case  24: return 0.25f;
      case  27: return Math::nan<float>();
      case  29: return Math::inf<float>();
      case  31: return Math::nan<float>();
      case  35: return Math::nan<float>();
      case  40: return 5.12323f;
      case  45: return -Math::inf<float>();
      case  55: return 100.5f;
      case  66: return 0.1f;
      case  69: return 0.2f;
      case  79: return 0.3f;
      case  99: return -Math::inf<float>();
      case 100:
      case 102:
      case 104:
      case 106:
      case 108: return float(rng.nextDouble());
      case 110:
      case 112:
      case 114:
      case 116:
      case 118: return -float(rng.nextDouble());
      case 122: return 10.3f;
      case 123: return 20.3f;
      case 124: return -100.3f;
      case 127: return 1.3f;
      case 130: return Math::nan<float>();
      case 135: return -Math::inf<float>();
      case 142: return 1.0f;
      case 143: return 1.5f;
      case 144: return 2.0f;
      case 145: return Math::inf<float>();
      case 155: return -1.5f;
      case 165: return -0.5f;
      case 175: return -1.0f;
      case 245: return 2.5f;

      default: {
        float sign = rng.nextUInt32() < 0x7FFFFFF ? 1.0f : -1.0f;
        return float(rng.nextDouble() * double(rng.nextUInt32() & 0xFFFFFFu)) * sign;
      }
    }
  }

  BL_NOINLINE double nextFloat64() noexcept {
    if (++step >= 256)
      step = 0;

    switch (step) {
      case   0: return 0.0;
      case   1: return 0.0;
      case   2: return 0.0;
      case   6: return 1.0;
      case   7: return 0.0;
      case  10: return 0.00001;
      case  11: return 2.0;
      case  12: return -Math::inf<double>();
      case  15: return 3.0;
      case  17: return 256.0;
      case  21: return 0.5;
      case  23: return Math::nan<double>();
      case  24: return 0.25;
      case  27: return Math::nan<double>();
      case  29: return Math::inf<double>();
      case  31: return Math::nan<double>();
      case  35: return Math::nan<double>();
      case  40: return 5.12323;
      case  45: return -Math::inf<double>();
      case  55: return 100.5;
      case  66: return 0.1;
      case  69: return 0.2;
      case  79: return 0.3;
      case  99: return -Math::inf<double>();
      case 100:
      case 102:
      case 104:
      case 106:
      case 108: return rng.nextDouble();
      case 110:
      case 112:
      case 114:
      case 116:
      case 118: return -rng.nextDouble();
      case 122: return 10.3;
      case 123: return 20.3;
      case 124: return -100.3;
      case 127: return 1.3;
      case 130: return Math::nan<double>();
      case 135: return -Math::inf<double>();
      case 142: return 1.0;
      case 143: return 1.5;
      case 144: return 2.0;
      case 145: return Math::inf<double>();
      case 155: return -1.5;
      case 165: return -0.5;
      case 175: return -1.0;
      case 245: return 2.5;

      default: {
        double sign = rng.nextUInt32() < 0x7FFFFFF ? 1.0 : -1.0;
        return double(rng.nextDouble() * double(rng.nextUInt32() & 0x3FFFFFFFu)) * sign;
      }
    }
  }
};

// Some SIMD operations are constrained, especially those higher level. So, to successfully test these we
// have to model the constraints in a way that the SIMD instruction we test actually gets the correct input.
// Note that a constraint doesn't have to be always range based, it could be anything.
struct ConstraintNone {
  template<uint32_t kW>
  static BL_INLINE_NODEBUG void apply(VecOverlay<kW>& v) noexcept { blUnused(v); }
};

template<typename ElementT, typename Derived>
struct ConstraintBase {
  template<uint32_t kW>
  static BL_INLINE void apply(VecOverlay<kW>& v) noexcept {
    ElementT* elements = v.template data<ElementT>();
    for (size_t i = 0; i < kW / sizeof(ElementT); i++)
      elements[i] = Derived::apply_one(elements[i]);
  }
};

template<uint8_t kMin, uint8_t kMax>
struct ConstraintRangeU8 : public ConstraintBase<uint16_t, ConstraintRangeU8<kMin, kMax>> {
  static BL_INLINE_NODEBUG uint8_t apply_one(uint8_t x) noexcept { return blClamp(x, kMin, kMax); }
};

template<uint16_t kMin, uint16_t kMax>
struct ConstraintRangeU16 : public ConstraintBase<uint16_t, ConstraintRangeU16<kMin, kMax>> {
  static BL_INLINE_NODEBUG uint16_t apply_one(uint16_t x) noexcept { return blClamp(x, kMin, kMax); }
};

template<uint32_t kMin, uint32_t kMax>
struct ConstraintRangeU32 : public ConstraintBase<uint32_t, ConstraintRangeU32<kMin, kMax>> {
  static BL_INLINE_NODEBUG uint32_t apply_one(uint32_t x) noexcept { return blClamp(x, kMin, kMax); }
};

// bl::Pipeline::JIT - Tests - Generic Operations
// ==============================================

template<typename T>
static BL_INLINE_NODEBUG typename std::make_unsigned<T>::type cast_uint(const T& x) noexcept {
  return (typename std::make_unsigned<T>::type)x;
}

template<typename T>
static BL_INLINE_NODEBUG typename std::make_signed<T>::type cast_int(const T& x) noexcept {
  return (typename std::make_signed<T>::type)x;
}

static BL_INLINE_NODEBUG int8_t saturate_i16_to_i8(int16_t x) noexcept {
  return x < int16_t(-128) ? int8_t(-128) :
         x > int16_t( 127) ? int8_t( 127) : int8_t(x & 0xFF);
}

static BL_INLINE_NODEBUG uint8_t saturate_i16_to_u8(int16_t x) noexcept {
  return x < int16_t(0x00) ? uint8_t(0x00) :
         x > int16_t(0xFF) ? uint8_t(0xFF) : uint8_t(x & 0xFF);
}

static BL_INLINE_NODEBUG int16_t saturate_i32_to_i16(int32_t x) noexcept {
  return x < int32_t(-32768) ? int16_t(-32768) :
         x > int32_t( 32767) ? int16_t( 32767) : int16_t(x & 0xFFFF);
}

static BL_INLINE_NODEBUG uint16_t saturate_i32_to_u16(int32_t x) noexcept {
  return x < int32_t(0x0000) ? uint16_t(0x0000) :
         x > int32_t(0xFFFF) ? uint16_t(0xFFFF) : uint16_t(x & 0xFFFF);
}

template<typename T, typename Derived> struct op_each_vv {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t i = 0; i < kW / sizeof(T); i++)
      out.set(i, Derived::apply_one(a.template get<T>(i)));
    return out;
  }
};

template<typename T, typename Derived> struct op_each_vvi {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, uint32_t imm) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t i = 0; i < kW / sizeof(T); i++)
      out.set(i, Derived::apply_one(a.template get<T>(i), imm));
    return out;
  }
};

template<typename T, typename Derived> struct op_each_vvv {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t i = 0; i < kW / sizeof(T); i++)
      out.set(i, Derived::apply_one(a.template get<T>(i), b.template get<T>(i)));
    return out;
  }
};

template<typename T, typename Derived> struct op_each_vvvi {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b, uint32_t imm) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t i = 0; i < kW / sizeof(T); i++)
      out.set(i, Derived::apply_one(a.template get<T>(i), b.template get<T>(i), imm));
    return out;
  }
};

template<typename T, typename Derived> struct op_each_vvvv {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b, const VecOverlay<kW>& c) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t i = 0; i < kW / sizeof(T); i++)
      out.set(i, Derived::apply_one(a.template get<T>(i), b.template get<T>(i), c.template get<T>(i)));
    return out;
  }
};

template<ScalarOpBehavior kB, typename T, typename Derived> struct op_scalar_vv {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out {};
    if (kB == ScalarOpBehavior::kPreservingVec128)
      out.copy16bFrom(a);
    out.set(0, Derived::apply_one(a.template get<T>(0)));
    return out;
  }
};

template<ScalarOpBehavior kB, typename T, typename Derived> struct op_scalar_vvi {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, uint32_t imm) noexcept {
    VecOverlay<kW> out {};
    if (kB == ScalarOpBehavior::kPreservingVec128)
      out.copy16bFrom(a);
    out.set(0, Derived::apply_one(a.template get<T>(0), imm));
    return out;
  }
};

template<ScalarOpBehavior kB, typename T, typename Derived> struct op_scalar_vvv {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b) noexcept {
    VecOverlay<kW> out {};
    if (kB == ScalarOpBehavior::kPreservingVec128)
      out.copy16bFrom(a);
    out.set(0, Derived::apply_one(a.template get<T>(0), b.template get<T>(0)));
    return out;
  }
};

template<ScalarOpBehavior kB, typename T, typename Derived> struct op_scalar_vvvv {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b, const VecOverlay<kW>& c) noexcept {
    VecOverlay<kW> out {};
    if (kB == ScalarOpBehavior::kPreservingVec128)
      out.copy16bFrom(a);
    out.set(0, Derived::apply_one(a.template get<T>(0), b.template get<T>(0), c.template get<T>(0)));
    return out;
  }
};

// bl::Pipeline::JIT - Tests - Generic Operations - VV
// ===================================================

struct vec_op_mov : public op_each_vv<uint32_t, vec_op_mov> {
  static BL_INLINE_NODEBUG uint32_t apply_one(const uint32_t& a) noexcept { return a; }
};

struct vec_op_mov_u64 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    out.data_u64[0] = a.data_u64[0];
    return out;
  }
};

struct vec_op_broadcast_u8 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t i = 0; i < kW; i++)
      out.data_u8[i] = a.data_u8[0];
    return out;
  }
};

struct vec_op_broadcast_u16 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t i = 0; i < kW / 2u; i++)
      out.data_u16[i] = a.data_u16[0];
    return out;
  }
};

struct vec_op_broadcast_u32 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t i = 0; i < kW / 4u; i++)
      out.data_u32[i] = a.data_u32[0];
    return out;
  }
};

struct vec_op_broadcast_u64 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t i = 0; i < kW / 8u; i++)
      out.data_u64[i] = a.data_u64[0];
    return out;
  }
};

struct vec_op_broadcast_u128 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};

    for (uint32_t i = 0; i < kW / 8u; i += 2) {
      out.data_u64[i + 0] = a.data_u64[0];
      out.data_u64[i + 1] = a.data_u64[1];
    }
    return out;
  }
};

struct vec_op_broadcast_u256 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};

    if (kW < 32)
      return a;

    for (uint32_t i = 0; i < kW / 8u; i += 4) {
      out.data_u64[i + 0] = a.data_u64[0];
      out.data_u64[i + 1] = a.data_u64[1];
      out.data_u64[i + 2] = a.data_u64[2];
      out.data_u64[i + 3] = a.data_u64[3];
    }
    return out;
  }
};

template<typename T> struct vec_op_abs : public op_each_vv<T, vec_op_abs<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a) noexcept { return a < 0 ? T(cast_uint(T(0)) - cast_uint(a)) : a; }
};

template<typename T> struct vec_op_neg : public op_each_vv<T, vec_op_neg<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a) noexcept { return T(cast_uint(T(0)) - cast_uint(a)); }
};

template<typename T> struct vec_op_not : public op_each_vv<T, vec_op_not<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a) noexcept { return T(~a); }
};

struct vec_op_cvt_i8_lo_to_i16 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_i16[off / 2 + 0] = a.data_i8[off / 2 + 0];
      out.data_i16[off / 2 + 1] = a.data_i8[off / 2 + 1];
      out.data_i16[off / 2 + 2] = a.data_i8[off / 2 + 2];
      out.data_i16[off / 2 + 3] = a.data_i8[off / 2 + 3];
      out.data_i16[off / 2 + 4] = a.data_i8[off / 2 + 4];
      out.data_i16[off / 2 + 5] = a.data_i8[off / 2 + 5];
      out.data_i16[off / 2 + 6] = a.data_i8[off / 2 + 6];
      out.data_i16[off / 2 + 7] = a.data_i8[off / 2 + 7];
    }
    return out;
  }
};

struct vec_op_cvt_i8_hi_to_i16 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_i16[off / 2 + 0] = a.data_i8[kW / 2 + off / 2 + 0];
      out.data_i16[off / 2 + 1] = a.data_i8[kW / 2 + off / 2 + 1];
      out.data_i16[off / 2 + 2] = a.data_i8[kW / 2 + off / 2 + 2];
      out.data_i16[off / 2 + 3] = a.data_i8[kW / 2 + off / 2 + 3];
      out.data_i16[off / 2 + 4] = a.data_i8[kW / 2 + off / 2 + 4];
      out.data_i16[off / 2 + 5] = a.data_i8[kW / 2 + off / 2 + 5];
      out.data_i16[off / 2 + 6] = a.data_i8[kW / 2 + off / 2 + 6];
      out.data_i16[off / 2 + 7] = a.data_i8[kW / 2 + off / 2 + 7];
    }
    return out;
  }
};

struct vec_op_cvt_u8_lo_to_u16 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_u16[off / 2 + 0] = a.data_u8[off / 2 + 0];
      out.data_u16[off / 2 + 1] = a.data_u8[off / 2 + 1];
      out.data_u16[off / 2 + 2] = a.data_u8[off / 2 + 2];
      out.data_u16[off / 2 + 3] = a.data_u8[off / 2 + 3];
      out.data_u16[off / 2 + 4] = a.data_u8[off / 2 + 4];
      out.data_u16[off / 2 + 5] = a.data_u8[off / 2 + 5];
      out.data_u16[off / 2 + 6] = a.data_u8[off / 2 + 6];
      out.data_u16[off / 2 + 7] = a.data_u8[off / 2 + 7];
    }
    return out;
  }
};

struct vec_op_cvt_u8_hi_to_u16 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_u16[off / 2 + 0] = a.data_u8[kW / 2 + off / 2 + 0];
      out.data_u16[off / 2 + 1] = a.data_u8[kW / 2 + off / 2 + 1];
      out.data_u16[off / 2 + 2] = a.data_u8[kW / 2 + off / 2 + 2];
      out.data_u16[off / 2 + 3] = a.data_u8[kW / 2 + off / 2 + 3];
      out.data_u16[off / 2 + 4] = a.data_u8[kW / 2 + off / 2 + 4];
      out.data_u16[off / 2 + 5] = a.data_u8[kW / 2 + off / 2 + 5];
      out.data_u16[off / 2 + 6] = a.data_u8[kW / 2 + off / 2 + 6];
      out.data_u16[off / 2 + 7] = a.data_u8[kW / 2 + off / 2 + 7];
    }
    return out;
  }
};

struct vec_op_cvt_i8_to_i32 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_i32[off / 4 + 0] = a.data_i8[off / 4 + 0];
      out.data_i32[off / 4 + 1] = a.data_i8[off / 4 + 1];
      out.data_i32[off / 4 + 2] = a.data_i8[off / 4 + 2];
      out.data_i32[off / 4 + 3] = a.data_i8[off / 4 + 3];
    }
    return out;
  }
};

struct vec_op_cvt_u8_to_u32 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_u32[off / 4 + 0] = a.data_u8[off / 4 + 0];
      out.data_u32[off / 4 + 1] = a.data_u8[off / 4 + 1];
      out.data_u32[off / 4 + 2] = a.data_u8[off / 4 + 2];
      out.data_u32[off / 4 + 3] = a.data_u8[off / 4 + 3];
    }
    return out;
  }
};

struct vec_op_cvt_i16_lo_to_i32 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_i32[off / 4 + 0] = a.data_i16[off / 4 + 0];
      out.data_i32[off / 4 + 1] = a.data_i16[off / 4 + 1];
      out.data_i32[off / 4 + 2] = a.data_i16[off / 4 + 2];
      out.data_i32[off / 4 + 3] = a.data_i16[off / 4 + 3];
    }
    return out;
  }
};

struct vec_op_cvt_i16_hi_to_i32 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_i32[off / 4 + 0] = a.data_i16[kW / 4 + off / 4 + 0];
      out.data_i32[off / 4 + 1] = a.data_i16[kW / 4 + off / 4 + 1];
      out.data_i32[off / 4 + 2] = a.data_i16[kW / 4 + off / 4 + 2];
      out.data_i32[off / 4 + 3] = a.data_i16[kW / 4 + off / 4 + 3];
    }
    return out;
  }
};

struct vec_op_cvt_u16_lo_to_u32 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_u32[off / 4 + 0] = a.data_u16[off / 4 + 0];
      out.data_u32[off / 4 + 1] = a.data_u16[off / 4 + 1];
      out.data_u32[off / 4 + 2] = a.data_u16[off / 4 + 2];
      out.data_u32[off / 4 + 3] = a.data_u16[off / 4 + 3];
    }
    return out;
  }
};

struct vec_op_cvt_u16_hi_to_u32 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_u32[off / 4 + 0] = a.data_u16[kW / 4 + off / 4 + 0];
      out.data_u32[off / 4 + 1] = a.data_u16[kW / 4 + off / 4 + 1];
      out.data_u32[off / 4 + 2] = a.data_u16[kW / 4 + off / 4 + 2];
      out.data_u32[off / 4 + 3] = a.data_u16[kW / 4 + off / 4 + 3];
    }
    return out;
  }
};

struct vec_op_cvt_i32_lo_to_i64 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_i64[off / 8 + 0] = a.data_i32[off / 8 + 0];
      out.data_i64[off / 8 + 1] = a.data_i32[off / 8 + 1];
    }
    return out;
  }
};

struct vec_op_cvt_i32_hi_to_i64 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_i64[off / 8 + 0] = a.data_i32[kW / 8 + off / 8 + 0];
      out.data_i64[off / 8 + 1] = a.data_i32[kW / 8 + off / 8 + 1];
    }
    return out;
  }
};

struct vec_op_cvt_u32_lo_to_u64 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_u64[off / 8 + 0] = a.data_u32[off / 8 + 0];
      out.data_u64[off / 8 + 1] = a.data_u32[off / 8 + 1];
    }
    return out;
  }
};

struct vec_op_cvt_u32_hi_to_u64 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_u64[off / 8 + 0] = a.data_u32[kW / 8 + off / 8 + 0];
      out.data_u64[off / 8 + 1] = a.data_u32[kW / 8 + off / 8 + 1];
    }
    return out;
  }
};

template<typename T> struct vec_op_fabs : public op_each_vv<T, vec_op_fabs<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a) noexcept { return blAbs(a); }
};

template<typename T> struct vec_op_trunc : public op_each_vv<T, vec_op_trunc<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a) noexcept { return Math::trunc(a); }
};

template<typename T> struct vec_op_floor : public op_each_vv<T, vec_op_floor<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a) noexcept { return Math::floor(a); }
};

template<typename T> struct vec_op_ceil : public op_each_vv<T, vec_op_ceil<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a) noexcept { return Math::ceil(a); }
};

template<typename T> struct vec_op_round : public op_each_vv<T, vec_op_round<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a) noexcept { return Math::nearby(a); }
};

template<typename T> struct vec_op_sqrt : public op_each_vv<T, vec_op_sqrt<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a) noexcept { return Math::sqrt(a); }
};

template<typename T> struct vec_op_rcp : public op_each_vv<T, vec_op_rcp<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a) noexcept { return T(1) / a; }
};

struct vec_op_cvt_i32_to_f32 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_f32[off / 4 + 0] = float(a.data_i32[off / 4 + 0]);
      out.data_f32[off / 4 + 1] = float(a.data_i32[off / 4 + 1]);
      out.data_f32[off / 4 + 2] = float(a.data_i32[off / 4 + 2]);
      out.data_f32[off / 4 + 3] = float(a.data_i32[off / 4 + 3]);
    }
    return out;
  }
};

template<bool kHi>
struct vec_op_cvt_f32_to_f64_impl {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    uint32_t adj = kHi ? kW / 8 : 0u;
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_f64[off / 8 + 0] = a.data_f32[off / 8 + adj + 0];
      out.data_f64[off / 8 + 1] = a.data_f32[off / 8 + adj + 1];
    }
    return out;
  }
};

struct vec_op_cvt_f32_lo_to_f64 : public vec_op_cvt_f32_to_f64_impl<false> {};
struct vec_op_cvt_f32_hi_to_f64 : public vec_op_cvt_f32_to_f64_impl<true> {};

template<bool kHi>
struct vec_op_cvt_f64_to_f32_impl {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    uint32_t adj = kHi ? kW / 8 : 0u;
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_f32[off / 8 + adj + 0] = float(a.data_f64[off / 8 + 0]);
      out.data_f32[off / 8 + adj + 1] = float(a.data_f64[off / 8 + 1]);
    }
    return out;
  }
};

struct vec_op_cvt_f64_to_f32_lo : public vec_op_cvt_f64_to_f32_impl<false> {};
struct vec_op_cvt_f64_to_f32_hi : public vec_op_cvt_f64_to_f32_impl<true> {};

template<bool kHi>
struct vec_op_cvt_i32_to_f64_impl {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    uint32_t adj = kHi ? kW / 8 : 0u;
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_f64[off / 8 + 0] = double(a.data_i32[off / 8 + adj + 0]);
      out.data_f64[off / 8 + 1] = double(a.data_i32[off / 8 + adj + 1]);
    }
    return out;
  }
};

struct vec_op_cvt_i32_lo_to_f64 : public vec_op_cvt_i32_to_f64_impl<false> {};
struct vec_op_cvt_i32_hi_to_f64 : public vec_op_cvt_i32_to_f64_impl<true> {};

struct vec_op_cvt_trunc_f32_to_i32 {
  static BL_INLINE int32_t cvt(float val) noexcept {
    if (!Math::isFinite(val))
      return cvt_non_finite_f32_to_i32(val);

    if (val <= float(INT32_MIN))
      return INT32_MIN;
    else if (val >= float(INT32_MAX))
      return INT32_MAX;
    else
      return int32_t(val);
  }

  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_i32[off / 4 + 0] = cvt(a.data_f32[off / 4 + 0]);
      out.data_i32[off / 4 + 1] = cvt(a.data_f32[off / 4 + 1]);
      out.data_i32[off / 4 + 2] = cvt(a.data_f32[off / 4 + 2]);
      out.data_i32[off / 4 + 3] = cvt(a.data_f32[off / 4 + 3]);
    }
    return out;
  }
};

template<bool kHi>
struct vec_op_cvt_trunc_f64_to_i32_impl {
  static BL_INLINE int32_t cvt(double val) noexcept {
    if (!Math::isFinite(val))
      return cvt_non_finite_f64_to_i32(val);

    if (val <= double(INT32_MIN))
      return INT32_MIN;
    else if (val >= double(INT32_MAX))
      return INT32_MAX;
    else
      return int32_t(val);
  }

  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    uint32_t adj = kHi ? kW / 8 : 0u;
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_i32[off / 8 + adj + 0] = cvt(a.data_f64[off / 8 + 0]);
      out.data_i32[off / 8 + adj + 1] = cvt(a.data_f64[off / 8 + 1]);
    }
    return out;
  }
};

struct vec_op_cvt_trunc_f64_to_i32_lo : vec_op_cvt_trunc_f64_to_i32_impl<false> {};
struct vec_op_cvt_trunc_f64_to_i32_hi : vec_op_cvt_trunc_f64_to_i32_impl<true> {};

struct vec_op_cvt_round_f32_to_i32 {
  static BL_INLINE int32_t cvt(float val) noexcept {
    if (!Math::isFinite(val))
      return cvt_non_finite_f32_to_i32(val);

    if (val <= float(INT32_MIN))
      return INT32_MIN;
    else if (val >= float(INT32_MAX))
      return INT32_MAX;
    else
      return Math::nearbyToInt(val);
  }

  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_i32[off / 4 + 0] = cvt(a.data_f32[off / 4 + 0]);
      out.data_i32[off / 4 + 1] = cvt(a.data_f32[off / 4 + 1]);
      out.data_i32[off / 4 + 2] = cvt(a.data_f32[off / 4 + 2]);
      out.data_i32[off / 4 + 3] = cvt(a.data_f32[off / 4 + 3]);
    }
    return out;
  }
};

template<bool kHi>
struct vec_op_cvt_round_f64_to_i32_impl {
  static BL_INLINE int32_t cvt(double val) noexcept {
    if (!Math::isFinite(val))
      return cvt_non_finite_f64_to_i32(val);

    if (val <= double(INT32_MIN))
      return INT32_MIN;
    else if (val >= double(INT32_MAX))
      return INT32_MAX;
    else
      return Math::nearbyToInt(val);
  }

  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    uint32_t adj = kHi ? kW / 8 : 0u;
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_i32[off / 8 + adj + 0] = cvt(a.data_f64[off / 8 + 0]);
      out.data_i32[off / 8 + adj + 1] = cvt(a.data_f64[off / 8 + 1]);
    }
    return out;
  }
};

struct vec_op_cvt_round_f64_to_i32_lo : vec_op_cvt_round_f64_to_i32_impl<false> {};
struct vec_op_cvt_round_f64_to_i32_hi : vec_op_cvt_round_f64_to_i32_impl<true> {};

struct scalar_op_cvt_f32_to_f64 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    out.data_f64[0] = a.data_f32[0];
    return out;
  }
};

struct scalar_op_cvt_f64_to_f32 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a) noexcept {
    VecOverlay<kW> out{};
    out.data_f32[0] = a.data_f64[0];
    return out;
  }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_trunc : public op_scalar_vv<kB, T, scalar_op_trunc<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a) noexcept { return Math::trunc(a); }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_floor : public op_scalar_vv<kB, T, scalar_op_floor<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a) noexcept { return Math::floor(a); }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_ceil : public op_scalar_vv<kB, T, scalar_op_ceil<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a) noexcept { return Math::ceil(a); }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_round : public op_scalar_vv<kB, T, scalar_op_round<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a) noexcept { return Math::nearby(a); }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_sqrt : public op_scalar_vv<kB, T, scalar_op_sqrt<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a) noexcept { return Math::sqrt(a); }
};

// bl::Pipeline::JIT - Tests - Generic Operations - VVI
// ====================================================

template<typename T> struct vec_op_slli : public op_each_vvi<T, vec_op_slli<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, uint32_t imm) noexcept { return T(cast_uint(a) << imm); }
};

template<typename T> struct vec_op_srli : public op_each_vvi<T, vec_op_srli<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, uint32_t imm) noexcept { return T(cast_uint(a) >> imm); }
};

template<typename T> struct vec_op_rsrli : public op_each_vvi<T, vec_op_rsrli<T>> {
  static BL_INLINE T apply_one(const T& a, uint32_t imm) noexcept {
    T add = T((a & (T(1) << (imm - 1))) != 0);
    return T((cast_uint(a) >> imm) + cast_uint(add));
  }
};

template<typename T> struct vec_op_srai : public op_each_vvi<T, vec_op_srai<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, uint32_t imm) noexcept { return T(cast_int(a) >> imm); }
};

struct vec_op_sllb_u128 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, uint32_t imm) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      for (uint32_t i = 0; i < 16; i++) {
        out.data_u8[off + i] = i < imm ? uint8_t(0) : a.data_u8[off + i - imm];
      }
    }
    return out;
  }
};

struct vec_op_srlb_u128 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, uint32_t imm) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      for (uint32_t i = 0; i < 16; i++) {
        out.data_u8[off + i] = i + imm < 16u ? a.data_u8[off + i + imm] : uint8_t(0);
      }
    }
    return out;
  }
};

struct vec_op_swizzle_u16 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, uint32_t imm) noexcept {
    uint32_t D = (imm >> 24) & 0x3;
    uint32_t C = (imm >> 16) & 0x3;
    uint32_t B = (imm >>  8) & 0x3;
    uint32_t A = (imm >>  0) & 0x3;

    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_u16[off / 2 + 0] = a.data_u16[off / 2 + 0 + A];
      out.data_u16[off / 2 + 1] = a.data_u16[off / 2 + 0 + B];
      out.data_u16[off / 2 + 2] = a.data_u16[off / 2 + 0 + C];
      out.data_u16[off / 2 + 3] = a.data_u16[off / 2 + 0 + D];
      out.data_u16[off / 2 + 4] = a.data_u16[off / 2 + 4 + A];
      out.data_u16[off / 2 + 5] = a.data_u16[off / 2 + 4 + B];
      out.data_u16[off / 2 + 6] = a.data_u16[off / 2 + 4 + C];
      out.data_u16[off / 2 + 7] = a.data_u16[off / 2 + 4 + D];
    }
    return out;
  }
};

struct vec_op_swizzle_lo_u16x4 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, uint32_t imm) noexcept {
    uint32_t D = (imm >> 24) & 0x3;
    uint32_t C = (imm >> 16) & 0x3;
    uint32_t B = (imm >>  8) & 0x3;
    uint32_t A = (imm >>  0) & 0x3;

    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_u16[off / 2 + 0] = a.data_u16[off / 2 + A];
      out.data_u16[off / 2 + 1] = a.data_u16[off / 2 + B];
      out.data_u16[off / 2 + 2] = a.data_u16[off / 2 + C];
      out.data_u16[off / 2 + 3] = a.data_u16[off / 2 + D];
      memcpy(out.data_u8 + off + 8, a.data_u8 + off + 8, 8);
    }
    return out;
  }
};

struct vec_op_swizzle_hi_u16x4 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, uint32_t imm) noexcept {
    uint32_t D = (imm >> 24) & 0x3;
    uint32_t C = (imm >> 16) & 0x3;
    uint32_t B = (imm >>  8) & 0x3;
    uint32_t A = (imm >>  0) & 0x3;

    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      memcpy(out.data_u8 + off, a.data_u8 + off, 8);
      out.data_u16[off / 2 + 4] = a.data_u16[off / 2 + 4 + A];
      out.data_u16[off / 2 + 5] = a.data_u16[off / 2 + 4 + B];
      out.data_u16[off / 2 + 6] = a.data_u16[off / 2 + 4 + C];
      out.data_u16[off / 2 + 7] = a.data_u16[off / 2 + 4 + D];
    }
    return out;
  }
};

struct vec_op_swizzle_u32x4 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, uint32_t imm) noexcept {
    uint32_t D = (imm >> 24) & 0x3;
    uint32_t C = (imm >> 16) & 0x3;
    uint32_t B = (imm >>  8) & 0x3;
    uint32_t A = (imm >>  0) & 0x3;

    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_u32[off / 4 + 0] = a.data_u32[off / 4 + A];
      out.data_u32[off / 4 + 1] = a.data_u32[off / 4 + B];
      out.data_u32[off / 4 + 2] = a.data_u32[off / 4 + C];
      out.data_u32[off / 4 + 3] = a.data_u32[off / 4 + D];
    }
    return out;
  }
};

struct vec_op_swizzle_u64x2 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, uint32_t imm) noexcept {
    uint32_t B = (imm >>  8) & 0x1;
    uint32_t A = (imm >>  0) & 0x1;

    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_u64[off / 8 + 0] = a.data_u64[off / 8 + A];
      out.data_u64[off / 8 + 1] = a.data_u64[off / 8 + B];
    }
    return out;
  }
};

// bl::Pipeline::JIT - Tests - SIMD - Generic Operations - VVV
// ===========================================================

template<typename T> struct vec_op_and : public op_each_vvv<T, vec_op_and<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return T(a & b); }
};

template<typename T> struct vec_op_or : public op_each_vvv<T, vec_op_or<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return T(a | b); }
};

template<typename T> struct vec_op_xor : public op_each_vvv<T, vec_op_xor<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return T(a ^ b); }
};

template<typename T> struct vec_op_andn : public op_each_vvv<T, vec_op_andn<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return T(~a & b); }
};

template<typename T> struct vec_op_bic : public op_each_vvv<T, vec_op_bic<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return T(a & ~b); }
};

template<typename T> struct vec_op_add : public op_each_vvv<T, vec_op_add<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return T(cast_uint(a) + cast_uint(b)); }
};

template<typename T> struct vec_op_adds : public op_each_vvv<T, vec_op_adds<T>> {
  static BL_INLINE T apply_one(const T& a, const T& b) noexcept {
    bl::OverflowFlag of{};
    T result = IntOps::addOverflow(a, b, &of);

    if (!of)
      return result;

    if (Traits::isUnsigned<T>() || b > 0)
      return Traits::maxValue<T>();
    else
      return Traits::minValue<T>();
  }
};

template<typename T> struct vec_op_sub : public op_each_vvv<T, vec_op_sub<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return T(cast_uint(a) - cast_uint(b)); }
};

template<typename T> struct vec_op_subs : public op_each_vvv<T, vec_op_subs<T>> {
  static BL_INLINE T apply_one(const T& a, const T& b) noexcept {
    bl::OverflowFlag of{};
    T result = IntOps::subOverflow(a, b, &of);

    if (!of)
      return result;

    if (Traits::isUnsigned<T>() || b > 0)
      return Traits::minValue<T>();
    else
      return Traits::maxValue<T>();
  }
};

template<typename T> struct vec_op_mul : public op_each_vvv<T, vec_op_mul<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return T((uint64_t(a) * uint64_t(b)) & ~T(0)); }
};

template<typename T> struct vec_op_mulhi : public op_each_vvv<T, vec_op_mulhi<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept {
    uint64_t result = uint64_t(int64_t(cast_int(a))) * uint64_t(int64_t(cast_int(b)));
    return T((result >> (sizeof(T) * 8u)) & ~T(0));
  }
};

template<typename T> struct vec_op_mulhu : public op_each_vvv<T, vec_op_mulhu<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept {
    uint64_t result = uint64_t(a) * uint64_t(b);
    return T((result >> (sizeof(T) * 8u)) & ~T(0));
  }
};

struct vec_op_mul_u64_lo_u32 : public op_each_vvv<uint64_t, vec_op_mul_u64_lo_u32> {
  static BL_INLINE_NODEBUG uint64_t apply_one(const uint64_t& a, const uint64_t& b) noexcept {
    return uint64_t(a) * uint64_t(b & 0xFFFFFFFFu);
  }
};

struct vec_op_mhadd_i16_i32 : public op_each_vvv<uint32_t, vec_op_mhadd_i16_i32> {
  static BL_INLINE_NODEBUG uint32_t apply_one(const uint32_t& a, const uint32_t& b) noexcept {
    uint32_t al = uint32_t(int32_t(int16_t(a & 0xFFFF)));
    uint32_t ah = uint32_t(int32_t(int16_t(a >> 16)));

    uint32_t bl = uint32_t(int32_t(int16_t(b & 0xFFFF)));
    uint32_t bh = uint32_t(int32_t(int16_t(b >> 16)));

    return al * bl + ah * bh;
  }
};

template<typename T> struct vec_op_madd : public op_each_vvvv<T, vec_op_madd<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b, const T& c) noexcept { return T((uint64_t(a) * uint64_t(b) + uint64_t(c)) & ~T(0)); }
};

template<typename T> struct vec_op_min : public op_each_vvv<T, vec_op_min<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a < b ? a : b; }
};

template<typename T> struct vec_op_max : public op_each_vvv<T, vec_op_max<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a > b ? a : b; }
};

template<typename T> struct vec_op_cmp_eq : public op_each_vvv<T, vec_op_cmp_eq<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a == b ? IntOps::allOnes<T>() : T(0); }
};

template<typename T> struct vec_op_cmp_ne : public op_each_vvv<T, vec_op_cmp_ne<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a != b ? IntOps::allOnes<T>() : T(0); }
};

template<typename T> struct vec_op_cmp_gt : public op_each_vvv<T, vec_op_cmp_gt<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a >  b ? IntOps::allOnes<T>() : T(0); }
};

template<typename T> struct vec_op_cmp_ge : public op_each_vvv<T, vec_op_cmp_ge<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a >= b ? IntOps::allOnes<T>() : T(0); }
};

template<typename T> struct vec_op_cmp_lt : public op_each_vvv<T, vec_op_cmp_lt<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a <  b ? IntOps::allOnes<T>() : T(0); }
};

template<typename T> struct vec_op_cmp_le : public op_each_vvv<T, vec_op_cmp_le<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a <= b ? IntOps::allOnes<T>() : T(0); }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_fadd : public op_scalar_vvv<kB, T, scalar_op_fadd<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a + b; }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_fsub : public op_scalar_vvv<kB, T, scalar_op_fsub<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a - b; }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_fmul : public op_scalar_vvv<kB, T, scalar_op_fmul<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a * b; }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_fdiv : public op_scalar_vvv<kB, T, scalar_op_fdiv<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a / b; }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_fmin_ternary : public op_scalar_vvv<kB, T, scalar_op_fmin_ternary<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a < b ? a : b; }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_fmax_ternary : public op_scalar_vvv<kB, T, scalar_op_fmax_ternary<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a > b ? a : b; }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_fmin_finite : public op_scalar_vvv<kB, T, scalar_op_fmin_finite<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return Math::isNaN(a) ? b : Math::isNaN(b) ? a : blMin(a, b); }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_fmax_finite : public op_scalar_vvv<kB, T, scalar_op_fmax_finite<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return Math::isNaN(a) ? b : Math::isNaN(b) ? a : blMax(a, b); }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_fmadd_nofma : public op_scalar_vvvv<kB, T, scalar_op_fmadd_nofma<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b, const T& c) noexcept { return madd_nofma_ref(a, b, c); }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_fmsub_nofma : public op_scalar_vvvv<kB, T, scalar_op_fmsub_nofma<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b, const T& c) noexcept { return madd_nofma_ref(a, b, -c); }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_fnmadd_nofma : public op_scalar_vvvv<kB, T, scalar_op_fnmadd_nofma<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b, const T& c) noexcept { return madd_nofma_ref(-a, b, c); }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_fnmsub_nofma : public op_scalar_vvvv<kB, T, scalar_op_fnmsub_nofma<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b, const T& c) noexcept { return madd_nofma_ref(-a, b, -c); }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_fmadd_fma : public op_scalar_vvvv<kB, T, scalar_op_fmadd_fma<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b, const T& c) noexcept { return madd_fma_ref(a, b, c); }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_fmsub_fma : public op_scalar_vvvv<kB, T, scalar_op_fmsub_fma<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b, const T& c) noexcept { return madd_fma_ref(a, b, -c); }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_fnmadd_fma : public op_scalar_vvvv<kB, T, scalar_op_fnmadd_fma<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b, const T& c) noexcept { return madd_fma_ref(-a, b, c); }
};

template<ScalarOpBehavior kB, typename T> struct scalar_op_fnmsub_fma : public op_scalar_vvvv<kB, T, scalar_op_fnmsub_fma<kB, T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b, const T& c) noexcept { return madd_fma_ref(-a, b, -c); }
};

template<typename T> struct vec_op_fadd : public op_each_vvv<T, vec_op_fadd<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a + b; }
};

template<typename T> struct vec_op_fsub : public op_each_vvv<T, vec_op_fsub<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a - b; }
};

template<typename T> struct vec_op_fmul : public op_each_vvv<T, vec_op_fmul<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a * b; }
};

template<typename T> struct vec_op_fdiv : public op_each_vvv<T, vec_op_fdiv<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a / b; }
};

template<typename T> struct vec_op_fmin_ternary : public op_each_vvv<T, vec_op_fmin_ternary<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a < b ? a : b; }
};

template<typename T> struct vec_op_fmax_ternary : public op_each_vvv<T, vec_op_fmax_ternary<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return a > b ? a : b; }
};

template<typename T> struct vec_op_fmin_finite : public op_each_vvv<T, vec_op_fmin_finite<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return Math::isNaN(a) ? b : Math::isNaN(b) ? a : blMin(a, b); }
};

template<typename T> struct vec_op_fmax_finite : public op_each_vvv<T, vec_op_fmax_finite<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b) noexcept { return Math::isNaN(a) ? b : Math::isNaN(b) ? a : blMax(a, b); }
};

template<typename T> struct vec_op_fmadd_nofma : public op_each_vvvv<T, vec_op_fmadd_nofma<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b, const T& c) noexcept { return madd_nofma_ref(a, b, c); }
};

template<typename T> struct vec_op_fmsub_nofma : public op_each_vvvv<T, vec_op_fmsub_nofma<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b, const T& c) noexcept { return madd_nofma_ref(a, b, -c); }
};

template<typename T> struct vec_op_fnmadd_nofma : public op_each_vvvv<T, vec_op_fnmadd_nofma<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b, const T& c) noexcept { return madd_nofma_ref(-a, b, c); }
};

template<typename T> struct vec_op_fnmsub_nofma : public op_each_vvvv<T, vec_op_fnmsub_nofma<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b, const T& c) noexcept { return madd_nofma_ref(-a, b, -c); }
};

template<typename T> struct vec_op_fmadd_fma : public op_each_vvvv<T, vec_op_fmadd_fma<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b, const T& c) noexcept { return madd_fma_ref(a, b, c); }
};

template<typename T> struct vec_op_fmsub_fma : public op_each_vvvv<T, vec_op_fmsub_fma<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b, const T& c) noexcept { return madd_fma_ref(a, b, -c); }
};

template<typename T> struct vec_op_fnmadd_fma : public op_each_vvvv<T, vec_op_fnmadd_fma<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b, const T& c) noexcept { return madd_fma_ref(-a, b, c); }
};

template<typename T> struct vec_op_fnmsub_fma : public op_each_vvvv<T, vec_op_fnmsub_fma<T>> {
  static BL_INLINE_NODEBUG T apply_one(const T& a, const T& b, const T& c) noexcept { return madd_fma_ref(-a, b, -c); }
};

template<typename T>
struct cmp_result {
  typedef T Result;

  static BL_INLINE_NODEBUG Result make(bool result) noexcept { return result ? Result(~Result(0)) : Result(0); }
};

template<>
struct cmp_result<float> {
  typedef uint32_t Result;

  static BL_INLINE_NODEBUG Result make(bool result) noexcept { return result ? ~Result(0) : Result(0); }
};

template<>
struct cmp_result<double> {
  typedef uint64_t Result;

  static BL_INLINE_NODEBUG Result make(bool result) noexcept { return result ? ~Result(0) : Result(0); }
};

template<typename T> struct vec_op_fcmpo_eq : public op_each_vvv<T, vec_op_fcmpo_eq<T>> {
  static BL_INLINE_NODEBUG typename cmp_result<T>::Result apply_one(const T& a, const T& b) noexcept { return cmp_result<T>::make(a == b); }
};

template<typename T> struct vec_op_fcmpu_ne : public op_each_vvv<T, vec_op_fcmpu_ne<T>> {
  static BL_INLINE_NODEBUG typename cmp_result<T>::Result apply_one(const T& a, const T& b) noexcept { return cmp_result<T>::make(!(a == b)); }
};

template<typename T> struct vec_op_fcmpo_gt : public op_each_vvv<T, vec_op_fcmpo_gt<T>> {
  static BL_INLINE_NODEBUG typename cmp_result<T>::Result apply_one(const T& a, const T& b) noexcept { return cmp_result<T>::make(a > b); }
};

template<typename T> struct vec_op_fcmpo_ge : public op_each_vvv<T, vec_op_fcmpo_ge<T>> {
  static BL_INLINE_NODEBUG typename cmp_result<T>::Result apply_one(const T& a, const T& b) noexcept { return cmp_result<T>::make(a >= b); }
};

template<typename T> struct vec_op_fcmpo_lt : public op_each_vvv<T, vec_op_fcmpo_lt<T>> {
  static BL_INLINE_NODEBUG typename cmp_result<T>::Result apply_one(const T& a, const T& b) noexcept { return cmp_result<T>::make(a < b); }
};

template<typename T> struct vec_op_fcmpo_le : public op_each_vvv<T, vec_op_fcmpo_le<T>> {
  static BL_INLINE_NODEBUG typename cmp_result<T>::Result apply_one(const T& a, const T& b) noexcept { return cmp_result<T>::make(a <= b); }
};

template<typename T> struct vec_op_fcmp_ord : public op_each_vvv<T, vec_op_fcmp_ord<T>> {
  static BL_INLINE_NODEBUG typename cmp_result<T>::Result apply_one(const T& a, const T& b) noexcept { return cmp_result<T>::make(!Math::isNaN(a) && !Math::isNaN(b)); }
};

template<typename T> struct vec_op_fcmp_unord : public op_each_vvv<T, vec_op_fcmp_unord<T>> {
  static BL_INLINE_NODEBUG typename cmp_result<T>::Result apply_one(const T& a, const T& b) noexcept { return cmp_result<T>::make(Math::isNaN(a) || Math::isNaN(b)); }
};

struct vec_op_hadd_f64 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_f64[off / 8 + 0] = a.data_f64[off / 8 + 0] + a.data_f64[off / 8 + 1];
      out.data_f64[off / 8 + 1] = b.data_f64[off / 8 + 0] + b.data_f64[off / 8 + 1];
    }
    return out;
  }
};

struct vec_op_combine_lo_hi_u64 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_u64[off / 8 + 0] = b.data_u64[off / 8 + 1];
      out.data_u64[off / 8 + 1] = a.data_u64[off / 8 + 0];
    }
    return out;
  }
};

struct vec_op_combine_hi_lo_u64 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_u64[off / 8 + 0] = b.data_u64[off / 8 + 0];
      out.data_u64[off / 8 + 1] = a.data_u64[off / 8 + 1];
    }
    return out;
  }
};

struct vec_op_interleave_lo_u8 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      for (uint32_t i = 0; i < 8; i++) {
        out.data_u8[off + i * 2 + 0] = a.data_u8[off + i];
        out.data_u8[off + i * 2 + 1] = b.data_u8[off + i];
      }
    }
    return out;
  }
};

struct vec_op_interleave_hi_u8 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      for (uint32_t i = 0; i < 8; i++) {
        out.data_u8[off + i * 2 + 0] = a.data_u8[off + 8 + i];
        out.data_u8[off + i * 2 + 1] = b.data_u8[off + 8 + i];
      }
    }
    return out;
  }
};

struct vec_op_interleave_lo_u16 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      for (uint32_t i = 0; i < 4; i++) {
        out.data_u16[off / 2 + i * 2 + 0] = a.data_u16[off / 2 + i];
        out.data_u16[off / 2 + i * 2 + 1] = b.data_u16[off / 2 + i];
      }
    }
    return out;
  }
};

struct vec_op_interleave_hi_u16 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      for (uint32_t i = 0; i < 4; i++) {
        out.data_u16[off / 2 + i * 2 + 0] = a.data_u16[off / 2 + 4 + i];
        out.data_u16[off / 2 + i * 2 + 1] = b.data_u16[off / 2 + 4 + i];
      }
    }
    return out;
  }
};

struct vec_op_interleave_lo_u32 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      for (uint32_t i = 0; i < 2; i++) {
        out.data_u32[off / 4 + i * 2 + 0] = a.data_u32[off / 4 + i];
        out.data_u32[off / 4 + i * 2 + 1] = b.data_u32[off / 4 + i];
      }
    }
    return out;
  }
};

struct vec_op_interleave_hi_u32 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      for (uint32_t i = 0; i < 2; i++) {
        out.data_u32[off / 4 + i * 2 + 0] = a.data_u32[off / 4 + 2 + i];
        out.data_u32[off / 4 + i * 2 + 1] = b.data_u32[off / 4 + 2 + i];
      }
    }
    return out;
  }
};

struct vec_op_interleave_lo_u64 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_u64[off / 8 + 0] = a.data_u64[off / 8 + 0];
      out.data_u64[off / 8 + 1] = b.data_u64[off / 8 + 0];
    }
    return out;
  }
};

struct vec_op_interleave_hi_u64 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_u64[off / 8 + 0] = a.data_u64[off / 8 + 1];
      out.data_u64[off / 8 + 1] = b.data_u64[off / 8 + 1];
    }
    return out;
  }
};

// bl::Pipeline::JIT - Tests - SIMD - Generic Operations - VVVI
// ============================================================

struct vec_op_alignr_u128 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b, uint32_t imm) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      for (uint32_t i = 0; i < 16; i++) {
        out.data_u8[off + i] = i + imm < 16 ? b.data_u8[off + i + imm] : a.data_u8[off + i + imm - 16];
      }
    }
    return out;
  }
};

struct vec_op_interleave_shuffle_u32x4 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b, uint32_t imm) noexcept {
    uint32_t D = (imm >> 24) & 0x3;
    uint32_t C = (imm >> 16) & 0x3;
    uint32_t B = (imm >>  8) & 0x3;
    uint32_t A = (imm >>  0) & 0x3;

    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_u32[off / 4 + 0] = a.data_u32[off / 4 + A];
      out.data_u32[off / 4 + 1] = a.data_u32[off / 4 + B];
      out.data_u32[off / 4 + 2] = b.data_u32[off / 4 + C];
      out.data_u32[off / 4 + 3] = b.data_u32[off / 4 + D];
    }
    return out;
  }
};

struct vec_op_interleave_shuffle_u64x2 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b, uint32_t imm) noexcept {
    uint32_t B = (imm >>  8) & 0x1;
    uint32_t A = (imm >>  0) & 0x1;

    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_u64[off / 8 + 0] = a.data_u64[off / 8 + A];
      out.data_u64[off / 8 + 1] = b.data_u64[off / 8 + B];
    }
    return out;
  }
};

struct vec_op_packs_i16_i8 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_i8[off +  0] = saturate_i16_to_i8(a.data_i16[off / 2 + 0]);
      out.data_i8[off +  1] = saturate_i16_to_i8(a.data_i16[off / 2 + 1]);
      out.data_i8[off +  2] = saturate_i16_to_i8(a.data_i16[off / 2 + 2]);
      out.data_i8[off +  3] = saturate_i16_to_i8(a.data_i16[off / 2 + 3]);
      out.data_i8[off +  4] = saturate_i16_to_i8(a.data_i16[off / 2 + 4]);
      out.data_i8[off +  5] = saturate_i16_to_i8(a.data_i16[off / 2 + 5]);
      out.data_i8[off +  6] = saturate_i16_to_i8(a.data_i16[off / 2 + 6]);
      out.data_i8[off +  7] = saturate_i16_to_i8(a.data_i16[off / 2 + 7]);
      out.data_i8[off +  8] = saturate_i16_to_i8(b.data_i16[off / 2 + 0]);
      out.data_i8[off +  9] = saturate_i16_to_i8(b.data_i16[off / 2 + 1]);
      out.data_i8[off + 10] = saturate_i16_to_i8(b.data_i16[off / 2 + 2]);
      out.data_i8[off + 11] = saturate_i16_to_i8(b.data_i16[off / 2 + 3]);
      out.data_i8[off + 12] = saturate_i16_to_i8(b.data_i16[off / 2 + 4]);
      out.data_i8[off + 13] = saturate_i16_to_i8(b.data_i16[off / 2 + 5]);
      out.data_i8[off + 14] = saturate_i16_to_i8(b.data_i16[off / 2 + 6]);
      out.data_i8[off + 15] = saturate_i16_to_i8(b.data_i16[off / 2 + 7]);
    }
    return out;
  }
};

struct vec_op_packs_i16_u8 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_u8[off +  0] = saturate_i16_to_u8(a.data_i16[off / 2 + 0]);
      out.data_u8[off +  1] = saturate_i16_to_u8(a.data_i16[off / 2 + 1]);
      out.data_u8[off +  2] = saturate_i16_to_u8(a.data_i16[off / 2 + 2]);
      out.data_u8[off +  3] = saturate_i16_to_u8(a.data_i16[off / 2 + 3]);
      out.data_u8[off +  4] = saturate_i16_to_u8(a.data_i16[off / 2 + 4]);
      out.data_u8[off +  5] = saturate_i16_to_u8(a.data_i16[off / 2 + 5]);
      out.data_u8[off +  6] = saturate_i16_to_u8(a.data_i16[off / 2 + 6]);
      out.data_u8[off +  7] = saturate_i16_to_u8(a.data_i16[off / 2 + 7]);
      out.data_u8[off +  8] = saturate_i16_to_u8(b.data_i16[off / 2 + 0]);
      out.data_u8[off +  9] = saturate_i16_to_u8(b.data_i16[off / 2 + 1]);
      out.data_u8[off + 10] = saturate_i16_to_u8(b.data_i16[off / 2 + 2]);
      out.data_u8[off + 11] = saturate_i16_to_u8(b.data_i16[off / 2 + 3]);
      out.data_u8[off + 12] = saturate_i16_to_u8(b.data_i16[off / 2 + 4]);
      out.data_u8[off + 13] = saturate_i16_to_u8(b.data_i16[off / 2 + 5]);
      out.data_u8[off + 14] = saturate_i16_to_u8(b.data_i16[off / 2 + 6]);
      out.data_u8[off + 15] = saturate_i16_to_u8(b.data_i16[off / 2 + 7]);
    }
    return out;
  }
};

struct vec_op_packs_i32_i16 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_i16[off / 2 + 0] = saturate_i32_to_i16(a.data_i32[off / 4 + 0]);
      out.data_i16[off / 2 + 1] = saturate_i32_to_i16(a.data_i32[off / 4 + 1]);
      out.data_i16[off / 2 + 2] = saturate_i32_to_i16(a.data_i32[off / 4 + 2]);
      out.data_i16[off / 2 + 3] = saturate_i32_to_i16(a.data_i32[off / 4 + 3]);
      out.data_i16[off / 2 + 4] = saturate_i32_to_i16(b.data_i32[off / 4 + 0]);
      out.data_i16[off / 2 + 5] = saturate_i32_to_i16(b.data_i32[off / 4 + 1]);
      out.data_i16[off / 2 + 6] = saturate_i32_to_i16(b.data_i32[off / 4 + 2]);
      out.data_i16[off / 2 + 7] = saturate_i32_to_i16(b.data_i32[off / 4 + 3]);
    }
    return out;
  }
};

struct vec_op_packs_i32_u16 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      out.data_u16[off / 2 + 0] = saturate_i32_to_u16(a.data_i32[off / 4 + 0]);
      out.data_u16[off / 2 + 1] = saturate_i32_to_u16(a.data_i32[off / 4 + 1]);
      out.data_u16[off / 2 + 2] = saturate_i32_to_u16(a.data_i32[off / 4 + 2]);
      out.data_u16[off / 2 + 3] = saturate_i32_to_u16(a.data_i32[off / 4 + 3]);
      out.data_u16[off / 2 + 4] = saturate_i32_to_u16(b.data_i32[off / 4 + 0]);
      out.data_u16[off / 2 + 5] = saturate_i32_to_u16(b.data_i32[off / 4 + 1]);
      out.data_u16[off / 2 + 6] = saturate_i32_to_u16(b.data_i32[off / 4 + 2]);
      out.data_u16[off / 2 + 7] = saturate_i32_to_u16(b.data_i32[off / 4 + 3]);
    }
    return out;
  }
};

// bl::Pipeline::JIT - Tests - SIMD - Generic Operations - VVVV
// ============================================================

struct vec_op_blendv_bits : public op_each_vvvv<uint32_t, vec_op_blendv_bits> {
  static BL_INLINE_NODEBUG uint32_t apply_one(const uint32_t& a, const uint32_t& b, const uint32_t& c) noexcept { return ((a & ~c) | (b & c)); }
};

struct vec_op_swizzlev_u8 {
  template<uint32_t kW>
  static BL_INLINE VecOverlay<kW> apply(const VecOverlay<kW>& a, const VecOverlay<kW>& b) noexcept {
    VecOverlay<kW> out{};
    for (uint32_t off = 0; off < kW; off += 16) {
      for (uint32_t i = 0; i < 16; i++) {
        size_t sel = b.data_u8[off + i] & (0x8F); // 3 bits ignored.
        out.data_u8[off + i] = sel & 0x80 ? uint8_t(0) : a.data_u8[off + sel];
      }
    }
    return out;
  }
};

struct vec_op_div255_u16 : public op_each_vv<uint16_t, vec_op_div255_u16> {
  static BL_INLINE uint16_t apply_one(const uint16_t& a) noexcept {
    uint32_t x = a + 0x80u;
    return uint16_t((x + (x >> 8)) >> 8);
  }
};

struct vec_op_div65535_u32 : public op_each_vv<uint32_t, vec_op_div65535_u32> {
  static BL_INLINE uint32_t apply_one(const uint32_t& a) noexcept {
    uint32_t x = a + 0x8000u;
    return uint32_t((x + (x >> 16)) >> 16);
  }
};

// bl::Pipeline::JIT - Tests - SIMD - Utilities
// ============================================

template<uint32_t kW>
static void fill_random_bytes(DataGenInt& dg, VecOverlay<kW>& dst) noexcept {
  for (uint32_t i = 0; i < kW / 8u; i++)
    dst.data_u64[i] = dg.nextUInt64();
}

template<uint32_t kW>
static void fill_random_f32(DataGenInt& dg, VecOverlay<kW>& dst) noexcept {
  for (uint32_t i = 0; i < kW / 4u; i++)
    dst.data_f32[i] = dg.nextFloat32();
}

template<uint32_t kW>
static void fill_random_f64(DataGenInt& dg, VecOverlay<kW>& dst) noexcept {
  for (uint32_t i = 0; i < kW / 8u; i++)
    dst.data_f64[i] = dg.nextFloat64();
}

template<uint32_t kW>
static void fill_random_data(DataGenInt& dg, VecOverlay<kW>& dst, VecElementType elementType) noexcept {
  switch (elementType) {
    case VecElementType::kFloat32:
      fill_random_f32(dg, dst);
      break;

    case VecElementType::kFloat64:
      fill_random_f64(dg, dst);
      break;

    default:
      fill_random_bytes(dg, dst);
      break;
  }
}

// bl::Pipeline::JIT - Tests - SIMD - Verification
// ===============================================

template<uint32_t kW>
static BL_NOINLINE void test_vecop_vv_failed(OpcodeVV op, Variation variation, const VecOverlay<kW>& arg0, const VecOverlay<kW>& observed, const VecOverlay<kW>& expected, const char* assembly) noexcept {
  VecOpInfo opInfo = vec_op_info_vv(op);
  BLString arg0_str = vec_stringify(arg0, opInfo.arg(0));
  BLString observed_str = vec_stringify(observed, opInfo.ret());
  BLString expected_str = vec_stringify(expected, opInfo.ret());

  EXPECT(false)
    .message("Operation '%s' (variation %u) failed:\n"
             "      Input #0: %s\n"
             "      Expected: %s\n"
             "      Observed: %s\n"
             "Assembly:\n%s",
             vec_op_name_vv(op),
             variation.value,
             arg0_str.data(),
             expected_str.data(),
             observed_str.data(),
             assembly);
}

template<uint32_t kW>
static BL_NOINLINE void test_vecop_vvi_failed(OpcodeVVI op, Variation variation, const VecOverlay<kW>& arg0, const VecOverlay<kW>& observed, const VecOverlay<kW>& expected, uint32_t imm, const char* assembly) noexcept {
  VecOpInfo opInfo = vec_op_info_vvi(op);
  BLString arg0_str = vec_stringify(arg0, opInfo.arg(0));
  BLString observed_str = vec_stringify(observed, opInfo.ret());
  BLString expected_str = vec_stringify(expected, opInfo.ret());

  EXPECT(false)
    .message("Operation '%s' (variation %u) failed:\n"
             "      Input #0: %s\n"
             "      ImmValue: %u (0x%08X)\n"
             "      Expected: %s\n"
             "      Observed: %s\n"
             "Assembly:\n%s",
             vec_op_name_vvi(op),
             variation.value,
             arg0_str.data(),
             imm, imm,
             expected_str.data(),
             observed_str.data(),
             assembly);
}

template<uint32_t kW>
static BL_NOINLINE void test_vecop_vvv_failed(OpcodeVVV op, Variation variation, const VecOverlay<kW>& arg0, const VecOverlay<kW>& arg1, const VecOverlay<kW>& observed, const VecOverlay<kW>& expected, const char* assembly) noexcept {
  VecOpInfo opInfo = vec_op_info_vvv(op);
  BLString arg0_str = vec_stringify(arg0, opInfo.arg(0));
  BLString arg1_str = vec_stringify(arg1, opInfo.arg(1));
  BLString observed_str = vec_stringify(observed, opInfo.ret());
  BLString expected_str = vec_stringify(expected, opInfo.ret());

  EXPECT(false)
    .message("Operation '%s' (variation %u) failed:\n"
             "      Input #0: %s\n"
             "      Input #1: %s\n"
             "      Expected: %s\n"
             "      Observed: %s\n"
             "Assembly:\n%s",
             vec_op_name_vvv(op),
             variation.value,
             arg0_str.data(),
             arg1_str.data(),
             expected_str.data(),
             observed_str.data(),
             assembly);
}

template<uint32_t kW>
static BL_NOINLINE void test_vecop_vvvi_failed(OpcodeVVVI op, Variation variation, const VecOverlay<kW>& arg0, const VecOverlay<kW>& arg1, const VecOverlay<kW>& observed, const VecOverlay<kW>& expected, uint32_t imm, const char* assembly) noexcept {
  VecOpInfo opInfo = vec_op_info_vvvi(op);
  BLString arg0_str = vec_stringify(arg0, opInfo.arg(0));
  BLString arg1_str = vec_stringify(arg1, opInfo.arg(1));
  BLString observed_str = vec_stringify(observed, opInfo.ret());
  BLString expected_str = vec_stringify(expected, opInfo.ret());

  EXPECT(false)
    .message("Operation '%s' (variation %u) failed:\n"
             "      Input #1: %s\n"
             "      Input #2: %s\n"
             "      ImmValue: %u (0x%08X)\n"
             "      Expected: %s\n"
             "      Observed: %s\n"
             "Assembly:\n%s",
             vec_op_name_vvvi(op),
             variation.value,
             arg0_str.data(),
             arg1_str.data(),
             imm, imm,
             expected_str.data(),
             observed_str.data(),
             assembly);
}

template<uint32_t kW>
static BL_NOINLINE void test_vecop_vvvv_failed(OpcodeVVVV op, Variation variation, const VecOverlay<kW>& arg0, const VecOverlay<kW>& arg1, const VecOverlay<kW>& arg2, const VecOverlay<kW>& observed, const VecOverlay<kW>& expected, const char* assembly) noexcept {
  VecOpInfo opInfo = vec_op_info_vvvv(op);
  BLString arg0_str = vec_stringify(arg0, opInfo.arg(0));
  BLString arg1_str = vec_stringify(arg1, opInfo.arg(1));
  BLString arg2_str = vec_stringify(arg2, opInfo.arg(2));
  BLString observed_str = vec_stringify(observed, opInfo.ret());
  BLString expected_str = vec_stringify(expected, opInfo.ret());

  EXPECT(false)
    .message("Operation '%s' (variation %u) failed\n"
             "      Input #1: %s\n"
             "      Input #2: %s\n"
             "      Input #3: %s\n"
             "      Expected: %s\n"
             "      Observed: %s\n"
             "Assembly:\n%s",
             vec_op_name_vvvv(op),
             variation.value,
             arg0_str.data(),
             arg1_str.data(),
             arg2_str.data(),
             expected_str.data(),
             observed_str.data(),
             assembly);
}

// bl::Pipeline::JIT - Tests - Integer Operations - VV
// ===================================================

template<VecWidth kVecWidth, OpcodeVV kOp, typename GenericOp, typename Constraint>
static BL_NOINLINE void test_vecop_vv_constraint(JitContext& ctx, Variation variation = Variation{0}) noexcept {
  constexpr uint32_t kW = byteWidthFromVecWidth(kVecWidth);

  TestVVFunc compiledApply = create_func_vv(ctx, kVecWidth, kOp, variation);
  DataGenInt dg(kRandomSeed);

  VecOpInfo opInfo = vec_op_info_vv(kOp);

  for (uint32_t iter = 0; iter < kTestIterCount; iter++) {
    VecOverlay<kW> a {};
    VecOverlay<kW> observed {};
    VecOverlay<kW> expected {};

    fill_random_data(dg, a, opInfo.arg(0));
    Constraint::apply(a);

    compiledApply(&observed, &a);
    expected = GenericOp::apply(a);

    if (!vec_eq(observed, expected, opInfo.ret()))
      test_vecop_vv_failed(kOp, variation, a, observed, expected, ctx.logger.data());
  }

  ctx.rt.release(compiledApply);
}

template<VecWidth kVecWidth, OpcodeVV kOp, typename GenericOp>
static void test_vecop_vv(JitContext& ctx, Variation variation = Variation{0}) noexcept {
  return test_vecop_vv_constraint<kVecWidth, kOp, GenericOp, ConstraintNone>(ctx, variation);
}

// bl::Pipeline::JIT - Tests - SIMD - Integer Operations - VVI
// ===========================================================

template<VecWidth kVecWidth, OpcodeVVI kOp, typename GenericOp, typename Constraint>
static BL_NOINLINE void test_vecop_vvi_constraint(JitContext& ctx, uint32_t imm, Variation variation = Variation{0}) noexcept {
  constexpr uint32_t kW = byteWidthFromVecWidth(kVecWidth);

  TestVVFunc compiledApply = create_func_vvi(ctx, kVecWidth, kOp, imm, variation);
  DataGenInt dg(kRandomSeed);

  VecOpInfo opInfo = vec_op_info_vvi(kOp);

  for (uint32_t iter = 0; iter < kTestIterCount; iter++) {
    VecOverlay<kW> a {};
    VecOverlay<kW> observed {};
    VecOverlay<kW> expected {};

    fill_random_data(dg, a, opInfo.arg(0));
    Constraint::apply(a);

    compiledApply(&observed, &a);
    expected = GenericOp::apply(a, imm);

    if (!vec_eq(observed, expected, opInfo.ret()))
      test_vecop_vvi_failed(kOp, variation, a, observed, expected, imm, ctx.logger.data());
  }

  ctx.rt.release(compiledApply);
}

template<VecWidth kVecWidth, OpcodeVVI kOp, typename GenericOp>
static void test_vecop_vvi(JitContext& ctx, uint32_t imm, Variation variation = Variation{0}) noexcept {
  return test_vecop_vvi_constraint<kVecWidth, kOp, GenericOp, ConstraintNone>(ctx, imm, variation);
}

// bl::Pipeline::JIT - Tests - SIMD - Integer Operations - VVV
// ===========================================================

template<VecWidth kVecWidth, OpcodeVVV kOp, typename GenericOp, typename Constraint>
static BL_NOINLINE void test_vecop_vvv_constraint(JitContext& ctx, Variation variation = Variation{0}) noexcept {
  constexpr uint32_t kW = byteWidthFromVecWidth(kVecWidth);

  TestVVVFunc compiledApply = create_func_vvv(ctx, kVecWidth, kOp, variation);
  DataGenInt dg(kRandomSeed);

  VecOpInfo opInfo = vec_op_info_vvv(kOp);

  for (uint32_t iter = 0; iter < kTestIterCount; iter++) {
    VecOverlay<kW> a;
    VecOverlay<kW> b;
    VecOverlay<kW> observed;
    VecOverlay<kW> expected;

    fill_random_data(dg, a, opInfo.arg(0));
    fill_random_data(dg, b, opInfo.arg(1));
    Constraint::apply(a);
    Constraint::apply(b);

    compiledApply(&observed, &a, &b);
    expected = GenericOp::apply(a, b);

    if (!vec_eq(observed, expected, opInfo.ret()))
      test_vecop_vvv_failed(kOp, variation, a, b, observed, expected, ctx.logger.data());
  }

  ctx.rt.release(compiledApply);
}

template<VecWidth kVecWidth, OpcodeVVV kOp, typename GenericOp>
static void test_vecop_vvv(JitContext& ctx, Variation variation = Variation{0}) noexcept {
  return test_vecop_vvv_constraint<kVecWidth, kOp, GenericOp, ConstraintNone>(ctx, variation);
}

// bl::Pipeline::JIT - Tests - SIMD - Integer Operations - VVVI
// ============================================================

template<VecWidth kVecWidth, OpcodeVVVI kOp, typename GenericOp, typename Constraint>
static BL_NOINLINE void test_vecop_vvvi_constraint(JitContext& ctx, uint32_t imm, Variation variation = Variation{0}) noexcept {
  constexpr uint32_t kW = byteWidthFromVecWidth(kVecWidth);

  TestVVVFunc compiledApply = create_func_vvvi(ctx, kVecWidth, kOp, imm, variation);
  DataGenInt dg(kRandomSeed);

  VecOpInfo opInfo = vec_op_info_vvvi(kOp);

  for (uint32_t iter = 0; iter < kTestIterCount; iter++) {
    VecOverlay<kW> a;
    VecOverlay<kW> b;
    VecOverlay<kW> observed;
    VecOverlay<kW> expected;

    fill_random_data(dg, a, opInfo.arg(0));
    fill_random_data(dg, b, opInfo.arg(1));
    Constraint::apply(a);
    Constraint::apply(b);

    compiledApply(&observed, &a, &b);
    expected = GenericOp::apply(a, b, imm);

    if (!vec_eq(observed, expected, opInfo.ret()))
      test_vecop_vvvi_failed(kOp, variation, a, b, observed, expected, imm, ctx.logger.data());
  }

  ctx.rt.release(compiledApply);
}

template<VecWidth kVecWidth, OpcodeVVVI kOp, typename GenericOp>
static void test_vecop_vvvi(JitContext& ctx, uint32_t imm, Variation variation = Variation{0}) noexcept {
  return test_vecop_vvvi_constraint<kVecWidth, kOp, GenericOp, ConstraintNone>(ctx, imm, variation);
}

// bl::Pipeline::JIT - Tests - SIMD - Integer Operations - VVVV
// ============================================================

template<VecWidth kVecWidth, OpcodeVVVV kOp, typename GenericOp, typename Constraint>
static BL_NOINLINE void test_vecop_vvvv_constraint(JitContext& ctx, Variation variation = Variation{0}) noexcept {
  constexpr uint32_t kW = byteWidthFromVecWidth(kVecWidth);

  TestVVVVFunc compiledApply = create_func_vvvv(ctx, kVecWidth, kOp, variation);
  DataGenInt dg(kRandomSeed);

  VecOpInfo opInfo = vec_op_info_vvvv(kOp);

  for (uint32_t iter = 0; iter < kTestIterCount; iter++) {
    VecOverlay<kW> a;
    VecOverlay<kW> b;
    VecOverlay<kW> c;
    VecOverlay<kW> observed;
    VecOverlay<kW> expected;

    fill_random_data(dg, a, opInfo.arg(0));
    fill_random_data(dg, b, opInfo.arg(1));
    fill_random_data(dg, c, opInfo.arg(2));
    Constraint::apply(a);
    Constraint::apply(b);
    Constraint::apply(c);

    compiledApply(&observed, &a, &b, &c);
    expected = GenericOp::apply(a, b, c);

    if (!vec_eq(observed, expected, opInfo.ret()))
      test_vecop_vvvv_failed(kOp, variation, a, b, c, observed, expected, ctx.logger.data());
  }
}

template<VecWidth kVecWidth, OpcodeVVVV kOp, typename GenericOp>
static void test_vecop_vvvv(JitContext& ctx, Variation variation = Variation{0}) noexcept {
  return test_vecop_vvvv_constraint<kVecWidth, kOp, GenericOp, ConstraintNone>(ctx, variation);
}

// bl::Pipeline::JIT - Tests - SIMD - Runner
// =========================================

template<VecWidth kVecWidth>
static BL_NOINLINE void test_simd_ops(JitContext& ctx) noexcept {
  // We need to know some behaviors in advance so we can select the right test function,
  // so create a dummy compiler and extract the necessary information from it.
  ScalarOpBehavior scalarOpBehavior;
  FMulAddOpBehavior fMulAddOpBehavior;

  {
    ctx.prepare();
    PipeCompiler pc(&ctx.cc, ctx.features, ctx.optFlags);

    scalarOpBehavior = pc.scalarOpBehavior();
    fMulAddOpBehavior = pc.fMulAddOpBehavior();
  }

  bool valgrindFmaBug = false;

#if defined(BL_JIT_ARCH_X86)
  // When running under valgrind there is a bug in it's instrumentation of FMA SS/SD instructions.
  // Instead of keeping the unaffected elements in the destination register they are cleared instead,
  // which would cause test failures. So, detect whether we are running under Valgind that has this
  // bug and avoid scalar FMA tests in that case.
  if (fMulAddOpBehavior != FMulAddOpBehavior::kNoFMA) {
    float a[4] = { 1, 2, 3, 4 };
    float b[4] = { 2, 4, 8, 1 };
    float c[4] = { 4, 7, 3, 9 };

    float d[4] {};
    madd_fma_check_valgrind_bug(a, b, c, d);

    valgrindFmaBug = d[1] == 0.0;
  }
#endif // BL_JIT_ARCH_X86

  INFO("  Testing mov");
  {
    test_vecop_vv<kVecWidth, OpcodeVV::kMov, vec_op_mov>(ctx);
    test_vecop_vv<kVecWidth, OpcodeVV::kMovU64, vec_op_mov_u64>(ctx);
  }

  INFO("  Testing broadcast");
  {
    // Test all broadcasts - vector based, GP to vector, and memory to vector.
    for (uint32_t v = 0; v < kNumVariationsVV_Broadcast; v++) {
      test_vecop_vv<kVecWidth, OpcodeVV::kBroadcastU8Z, vec_op_broadcast_u8>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kBroadcastU16Z, vec_op_broadcast_u16>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kBroadcastU8, vec_op_broadcast_u8>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kBroadcastU16, vec_op_broadcast_u16>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kBroadcastU32, vec_op_broadcast_u32>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kBroadcastU64, vec_op_broadcast_u64>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kBroadcastF32, vec_op_broadcast_u32>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kBroadcastF64, vec_op_broadcast_u64>(ctx, Variation{v});

      test_vecop_vv<kVecWidth, OpcodeVV::kBroadcastV128_U32, vec_op_broadcast_u128>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kBroadcastV128_U64, vec_op_broadcast_u128>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kBroadcastV128_F32, vec_op_broadcast_u128>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kBroadcastV128_F64, vec_op_broadcast_u128>(ctx, Variation{v});

      if (kVecWidth > VecWidth::k256) {
        test_vecop_vv<kVecWidth, OpcodeVV::kBroadcastV256_U32, vec_op_broadcast_u256>(ctx, Variation{v});
        test_vecop_vv<kVecWidth, OpcodeVV::kBroadcastV256_U64, vec_op_broadcast_u256>(ctx, Variation{v});
        test_vecop_vv<kVecWidth, OpcodeVV::kBroadcastV256_F32, vec_op_broadcast_u256>(ctx, Variation{v});
        test_vecop_vv<kVecWidth, OpcodeVV::kBroadcastV256_F64, vec_op_broadcast_u256>(ctx, Variation{v});
      }
    }
  }

  INFO("  Testing abs (int)");
  {
    for (uint32_t v = 0; v < kNumVariationsVV; v++) {
      test_vecop_vv<kVecWidth, OpcodeVV::kAbsI8, vec_op_abs<int8_t>>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kAbsI16, vec_op_abs<int16_t>>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kAbsI32, vec_op_abs<int32_t>>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kAbsI64, vec_op_abs<int64_t>>(ctx, Variation{v});
    }
  }

  INFO("  Testing not (int)");
  {
    for (uint32_t v = 0; v < kNumVariationsVV; v++) {
      test_vecop_vv<kVecWidth, OpcodeVV::kNotU32, vec_op_not<uint32_t>>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kNotU64, vec_op_not<uint64_t>>(ctx, Variation{v});
    }
  }

  INFO("  Testing cvt (int)");
  {
    for (uint32_t v = 0; v < kNumVariationsVV; v++) {
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtI8LoToI16, vec_op_cvt_i8_lo_to_i16>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtI8HiToI16, vec_op_cvt_i8_hi_to_i16>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtU8LoToU16, vec_op_cvt_u8_lo_to_u16>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtU8HiToU16, vec_op_cvt_u8_hi_to_u16>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtI8ToI32, vec_op_cvt_i8_to_i32>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtU8ToU32, vec_op_cvt_u8_to_u32>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtI16LoToI32, vec_op_cvt_i16_lo_to_i32>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtI16HiToI32, vec_op_cvt_i16_hi_to_i32>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtU16LoToU32, vec_op_cvt_u16_lo_to_u32>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtU16HiToU32, vec_op_cvt_u16_hi_to_u32>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtI32LoToI64, vec_op_cvt_i32_lo_to_i64>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtI32HiToI64, vec_op_cvt_i32_hi_to_i64>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtU32LoToU64, vec_op_cvt_u32_lo_to_u64>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtU32HiToU64, vec_op_cvt_u32_hi_to_u64>(ctx, Variation{v});
    }
  }

  INFO("  Testing abs (float)");
  {
    for (uint32_t v = 0; v < kNumVariationsVV; v++) {
      test_vecop_vv<kVecWidth, OpcodeVV::kAbsF32, vec_op_fabs<float>>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kAbsF64, vec_op_fabs<double>>(ctx, Variation{v});
    }
  }

  INFO("  Testing not (float)");
  {
    for (uint32_t v = 0; v < kNumVariationsVV; v++) {
      test_vecop_vv<kVecWidth, OpcodeVV::kNotF32, vec_op_not<uint32_t>>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kNotF64, vec_op_not<uint64_t>>(ctx, Variation{v});
    }
  }

  INFO("  Testing rounding (float)");
  {
    for (uint32_t v = 0; v < kNumVariationsVV; v++) {
      // Variation 2 means that the source operand is memory, which would ALWAYS zero the rest of the register.
      if (scalarOpBehavior == ScalarOpBehavior::kZeroing || v == 2u) {
        test_vecop_vv<kVecWidth, OpcodeVV::kTruncF32S, scalar_op_trunc<ScalarOpBehavior::kZeroing, float>>(ctx, Variation{v});
        test_vecop_vv<kVecWidth, OpcodeVV::kTruncF64S, scalar_op_trunc<ScalarOpBehavior::kZeroing, double>>(ctx, Variation{v});
        test_vecop_vv<kVecWidth, OpcodeVV::kFloorF32S, scalar_op_floor<ScalarOpBehavior::kZeroing, float>>(ctx, Variation{v});
        test_vecop_vv<kVecWidth, OpcodeVV::kFloorF64S, scalar_op_floor<ScalarOpBehavior::kZeroing, double>>(ctx, Variation{v});
        test_vecop_vv<kVecWidth, OpcodeVV::kCeilF32S, scalar_op_ceil<ScalarOpBehavior::kZeroing, float>>(ctx, Variation{v});
        test_vecop_vv<kVecWidth, OpcodeVV::kCeilF64S, scalar_op_ceil<ScalarOpBehavior::kZeroing, double>>(ctx, Variation{v});
        test_vecop_vv<kVecWidth, OpcodeVV::kRoundF32S, scalar_op_round<ScalarOpBehavior::kZeroing, float>>(ctx, Variation{v});
        test_vecop_vv<kVecWidth, OpcodeVV::kRoundF64S, scalar_op_round<ScalarOpBehavior::kZeroing, double>>(ctx, Variation{v});
      }
      else {
        test_vecop_vv<kVecWidth, OpcodeVV::kTruncF32S, scalar_op_trunc<ScalarOpBehavior::kPreservingVec128, float>>(ctx, Variation{v});
        test_vecop_vv<kVecWidth, OpcodeVV::kTruncF64S, scalar_op_trunc<ScalarOpBehavior::kPreservingVec128, double>>(ctx, Variation{v});
        test_vecop_vv<kVecWidth, OpcodeVV::kFloorF32S, scalar_op_floor<ScalarOpBehavior::kPreservingVec128, float>>(ctx, Variation{v});
        test_vecop_vv<kVecWidth, OpcodeVV::kFloorF64S, scalar_op_floor<ScalarOpBehavior::kPreservingVec128, double>>(ctx, Variation{v});
        test_vecop_vv<kVecWidth, OpcodeVV::kCeilF32S, scalar_op_ceil<ScalarOpBehavior::kPreservingVec128, float>>(ctx, Variation{v});
        test_vecop_vv<kVecWidth, OpcodeVV::kCeilF64S, scalar_op_ceil<ScalarOpBehavior::kPreservingVec128, double>>(ctx, Variation{v});
        test_vecop_vv<kVecWidth, OpcodeVV::kRoundF32S, scalar_op_round<ScalarOpBehavior::kPreservingVec128, float>>(ctx, Variation{v});
        test_vecop_vv<kVecWidth, OpcodeVV::kRoundF64S, scalar_op_round<ScalarOpBehavior::kPreservingVec128, double>>(ctx, Variation{v});
      }

      test_vecop_vv<kVecWidth, OpcodeVV::kTruncF32, vec_op_trunc<float>>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kTruncF64, vec_op_trunc<double>>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kFloorF32, vec_op_floor<float>>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kFloorF64, vec_op_floor<double>>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCeilF32, vec_op_ceil<float>>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCeilF64, vec_op_ceil<double>>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kRoundF32, vec_op_round<float>>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kRoundF64, vec_op_round<double>>(ctx, Variation{v});
    }
  }

  INFO("  Testing rcp (float)");
  {
    for (uint32_t v = 0; v < kNumVariationsVV; v++) {
      test_vecop_vv<kVecWidth, OpcodeVV::kRcpF32, vec_op_rcp<float>>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kRcpF64, vec_op_rcp<double>>(ctx, Variation{v});
    }
  }

  INFO("  Testing sqrt (float)");
  {
    if (scalarOpBehavior == ScalarOpBehavior::kZeroing) {
      test_vecop_vv<kVecWidth, OpcodeVV::kSqrtF32S, scalar_op_sqrt<ScalarOpBehavior::kZeroing, float>>(ctx);
      test_vecop_vv<kVecWidth, OpcodeVV::kSqrtF64S, scalar_op_sqrt<ScalarOpBehavior::kZeroing, double>>(ctx);
    }
    else {
      test_vecop_vv<kVecWidth, OpcodeVV::kSqrtF32S, scalar_op_sqrt<ScalarOpBehavior::kPreservingVec128, float>>(ctx);
      test_vecop_vv<kVecWidth, OpcodeVV::kSqrtF64S, scalar_op_sqrt<ScalarOpBehavior::kPreservingVec128, double>>(ctx);
    }

    for (uint32_t v = 0; v < kNumVariationsVV; v++) {
      test_vecop_vv<kVecWidth, OpcodeVV::kSqrtF32, vec_op_sqrt<float>>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kSqrtF64, vec_op_sqrt<double>>(ctx, Variation{v});
    }
  }

  INFO("  Testing cvt (float)");
  {
    for (uint32_t v = 0; v < kNumVariationsVV; v++) {
      // TODO: [JIT] Re-enable when the content of the remaining part of the register is formalized.
      // test_vecop_vv<kVecWidth, OpcodeVV::kCvtF32ToF64S, scalar_op_cvt_f32_to_f64>(ctx);
      // test_vecop_vv<kVecWidth, OpcodeVV::kCvtF64ToF32S, scalar_op_cvt_f64_to_f32>(ctx);

      test_vecop_vv<kVecWidth, OpcodeVV::kCvtI32ToF32, vec_op_cvt_i32_to_f32>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtF32LoToF64, vec_op_cvt_f32_lo_to_f64>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtF32HiToF64, vec_op_cvt_f32_hi_to_f64>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtF64ToF32Lo, vec_op_cvt_f64_to_f32_lo>(ctx, Variation{0});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtF64ToF32Hi, vec_op_cvt_f64_to_f32_hi>(ctx, Variation{0});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtI32LoToF64, vec_op_cvt_i32_lo_to_f64>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtI32HiToF64, vec_op_cvt_i32_hi_to_f64>(ctx, Variation{v});

      test_vecop_vv<kVecWidth, OpcodeVV::kCvtTruncF32ToI32, vec_op_cvt_trunc_f32_to_i32>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtTruncF64ToI32Lo, vec_op_cvt_trunc_f64_to_i32_lo>(ctx, Variation{0});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtTruncF64ToI32Hi, vec_op_cvt_trunc_f64_to_i32_hi>(ctx, Variation{0});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtRoundF32ToI32, vec_op_cvt_round_f32_to_i32>(ctx, Variation{v});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtRoundF64ToI32Lo, vec_op_cvt_round_f64_to_i32_lo>(ctx, Variation{0});
      test_vecop_vv<kVecWidth, OpcodeVV::kCvtRoundF64ToI32Hi, vec_op_cvt_round_f64_to_i32_hi>(ctx, Variation{0});
    }
  }

  INFO("  Testing bit shift");
  {
    for (uint32_t v = 0; v < kNumVariationsVVI; v++) {
/*
      for (uint32_t i = 1; i < 8; i++) {
        test_vecop_vvi<kVecWidth, OpcodeVVI::kSllU8 , vec_op_slli<uint8_t>>(ctx, i, Variation{v});
        test_vecop_vvi<kVecWidth, OpcodeVVI::kSrlU8 , vec_op_srli<uint8_t>>(ctx, i, Variation{v});
        test_vecop_vvi<kVecWidth, OpcodeVVI::kSraI8 , vec_op_srai<int8_t>>(ctx, i, Variation{v});
      }
*/
      for (uint32_t i = 1; i < 16; i++) {
        test_vecop_vvi<kVecWidth, OpcodeVVI::kSllU16, vec_op_slli<uint16_t>>(ctx, i, Variation{v});
        test_vecop_vvi<kVecWidth, OpcodeVVI::kSrlU16, vec_op_srli<uint16_t>>(ctx, i, Variation{v});
        test_vecop_vvi<kVecWidth, OpcodeVVI::kSraI16, vec_op_srai<int16_t>>(ctx, i, Variation{v});
      }

      for (uint32_t i = 1; i < 32; i++) {
        test_vecop_vvi<kVecWidth, OpcodeVVI::kSllU32, vec_op_slli<uint32_t>>(ctx, i, Variation{v});
        test_vecop_vvi<kVecWidth, OpcodeVVI::kSrlU32, vec_op_srli<uint32_t>>(ctx, i, Variation{v});
        test_vecop_vvi<kVecWidth, OpcodeVVI::kSraI32, vec_op_srai<int32_t>>(ctx, i, Variation{v});
      }

      for (uint32_t i = 1; i < 64; i++) {
        test_vecop_vvi<kVecWidth, OpcodeVVI::kSllU64, vec_op_slli<uint64_t>>(ctx, i, Variation{v});
        test_vecop_vvi<kVecWidth, OpcodeVVI::kSrlU64, vec_op_srli<uint64_t>>(ctx, i, Variation{v});
        test_vecop_vvi<kVecWidth, OpcodeVVI::kSraI64, vec_op_srai<int64_t>>(ctx, i, Variation{v});
      }
    }
  }

  INFO("  Testing sllb_u128 & srlb_u128");
  {
    for (uint32_t v = 0; v < kNumVariationsVVI; v++) {
      for (uint32_t i = 1; i < 16; i++) {
        test_vecop_vvi<kVecWidth, OpcodeVVI::kSllbU128, vec_op_sllb_u128>(ctx, i, Variation{v});
        test_vecop_vvi<kVecWidth, OpcodeVVI::kSrlbU128, vec_op_srlb_u128>(ctx, i, Variation{v});
      }
    }
  }

  INFO("  Testing swizzle_[lo|hi]_u16x4");
  {
    for (uint32_t v = 0; v < kNumVariationsVVI; v++) {
      for (uint32_t i = 0; i < 256; i++) {
        uint32_t imm = swizzle((i >> 6) & 3, (i >> 4) & 3, (i >> 2) & 3, i & 3).value;

        test_vecop_vvi<kVecWidth, OpcodeVVI::kSwizzleLoU16x4, vec_op_swizzle_lo_u16x4>(ctx, imm, Variation{v});
        test_vecop_vvi<kVecWidth, OpcodeVVI::kSwizzleHiU16x4, vec_op_swizzle_hi_u16x4>(ctx, imm, Variation{v});
        test_vecop_vvi<kVecWidth, OpcodeVVI::kSwizzleU16x4, vec_op_swizzle_u16>(ctx, imm, Variation{v});
      }
    }
  }

  INFO("  Testing swizzle_u32x4");
  {
    for (uint32_t v = 0; v < kNumVariationsVVI; v++) {
      for (uint32_t i = 0; i < 256; i++) {
        uint32_t imm = swizzle((i >> 6) & 3, (i >> 4) & 3, (i >> 2) & 3, i & 3).value;

        test_vecop_vvi<kVecWidth, OpcodeVVI::kSwizzleU32x4, vec_op_swizzle_u32x4>(ctx, imm, Variation{v});
        test_vecop_vvi<kVecWidth, OpcodeVVI::kSwizzleF32x4, vec_op_swizzle_u32x4>(ctx, imm, Variation{v});
      }
    }
  }

  INFO("  Testing swizzle_u64x2");
  {
    for (uint32_t v = 0; v < kNumVariationsVVI; v++) {
      for (uint32_t i = 0; i < 4; i++) {
        uint32_t imm = swizzle((i >> 1) & 1, i & 1).value;
        test_vecop_vvi<kVecWidth, OpcodeVVI::kSwizzleU64x2, vec_op_swizzle_u64x2>(ctx, imm, Variation{v});
        test_vecop_vvi<kVecWidth, OpcodeVVI::kSwizzleF64x2, vec_op_swizzle_u64x2>(ctx, imm, Variation{v});
      }
    }
  }

  INFO("  Testing logical (int)");
  {
    for (uint32_t v = 0; v < kNumVariationsVVV; v++) {
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAndU32, vec_op_and<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAndU64, vec_op_and<uint64_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kOrU32, vec_op_or<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kOrU64, vec_op_or<uint64_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kXorU32, vec_op_xor<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kXorU64, vec_op_xor<uint64_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAndnU32, vec_op_andn<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAndnU64, vec_op_andn<uint64_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kBicU32, vec_op_bic<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kBicU64, vec_op_bic<uint64_t>>(ctx, Variation{v});
    }
  }

  INFO("  Testing add / adds (int)");
  {
    for (uint32_t v = 0; v < kNumVariationsVVV; v++) {
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAddU8, vec_op_add<uint8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAddU16, vec_op_add<uint16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAddU32, vec_op_add<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAddU64, vec_op_add<uint64_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAddsI8, vec_op_adds<int8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAddsI16, vec_op_adds<int16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAddsU8, vec_op_adds<uint8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAddsU16, vec_op_adds<uint16_t>>(ctx, Variation{v});
    }
  }

  INFO("  Testing sub / subs (int)");
  {
    for (uint32_t v = 0; v < kNumVariationsVVV; v++) {
      test_vecop_vvv<kVecWidth, OpcodeVVV::kSubU8, vec_op_sub<uint8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kSubU16, vec_op_sub<uint16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kSubU32, vec_op_sub<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kSubU64, vec_op_sub<uint64_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kSubsI8, vec_op_subs<int8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kSubsI16, vec_op_subs<int16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kSubsU8, vec_op_subs<uint8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kSubsU16, vec_op_subs<uint16_t>>(ctx, Variation{v});
    }
  }

  INFO("  Testing mul (int)");
  {
    for (uint32_t v = 0; v < kNumVariationsVVV; v++) {
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMulU16, vec_op_mul<uint16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMulU32, vec_op_mul<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMulU64, vec_op_mul<uint64_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMulhI16, vec_op_mulhi<int16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMulhU16, vec_op_mulhu<uint16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMulU64_LoU32, vec_op_mul_u64_lo_u32>(ctx, Variation{v});
    }
  }

  INFO("  Testing mhadd (int)");
  {
    for (uint32_t v = 0; v < kNumVariationsVVV; v++) {
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMHAddI16_I32, vec_op_mhadd_i16_i32>(ctx, Variation{v});
    }
  }

  INFO("  Testing madd (int)");
  {
    for (uint32_t v = 0; v < kNumVariationsVVVV; v++) {
      test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMAddU16, vec_op_madd<uint16_t>>(ctx, Variation{v});
      test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMAddU32, vec_op_madd<uint32_t>>(ctx, Variation{v});
    }
  }

  INFO("  Testing min / max (int)");
  {
    for (uint32_t v = 0; v < kNumVariationsVVV; v++) {
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMinI8, vec_op_min<int8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMinI16, vec_op_min<int16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMinI32, vec_op_min<int32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMinI64, vec_op_min<int64_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMinU8, vec_op_min<uint8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMinU16, vec_op_min<uint16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMinU32, vec_op_min<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMinU64, vec_op_min<uint64_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMaxI8, vec_op_max<int8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMaxI16, vec_op_max<int16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMaxI32, vec_op_max<int32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMaxI64, vec_op_max<int64_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMaxU8, vec_op_max<uint8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMaxU16, vec_op_max<uint16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMaxU32, vec_op_max<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMaxU64, vec_op_max<uint64_t>>(ctx, Variation{v});
    }
  }

  INFO("  Testing cmp (int)");
  {
    for (uint32_t v = 0; v < kNumVariationsVVV; v++) {
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpEqU8, vec_op_cmp_eq<uint8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpEqU16, vec_op_cmp_eq<uint16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpEqU32, vec_op_cmp_eq<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpEqU64, vec_op_cmp_eq<uint64_t>>(ctx, Variation{v});
/*
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpNeU8, vec_op_cmp_ne<uint8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpNeU16, vec_op_cmp_ne<uint16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpNeU32, vec_op_cmp_ne<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpNeU64, vec_op_cmp_ne<uint64_t>>(ctx, Variation{v});
*/
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGtI8, vec_op_cmp_gt<int8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGtI16, vec_op_cmp_gt<int16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGtI32, vec_op_cmp_gt<int32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGtI64, vec_op_cmp_gt<int64_t>>(ctx, Variation{v});

      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGtU8, vec_op_cmp_gt<uint8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGtU16, vec_op_cmp_gt<uint16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGtU32, vec_op_cmp_gt<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGtU64, vec_op_cmp_gt<uint64_t>>(ctx, Variation{v});

      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGeI8, vec_op_cmp_ge<int8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGeI16, vec_op_cmp_ge<int16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGeI32, vec_op_cmp_ge<int32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGeI64, vec_op_cmp_ge<int64_t>>(ctx, Variation{v});

      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGeU8, vec_op_cmp_ge<uint8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGeU16, vec_op_cmp_ge<uint16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGeU32, vec_op_cmp_ge<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGeU64, vec_op_cmp_ge<uint64_t>>(ctx, Variation{v});

      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLtI8, vec_op_cmp_lt<int8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLtI16, vec_op_cmp_lt<int16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLtI32, vec_op_cmp_lt<int32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLtI64, vec_op_cmp_lt<int64_t>>(ctx, Variation{v});

      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLtU8, vec_op_cmp_lt<uint8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLtU16, vec_op_cmp_lt<uint16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLtU32, vec_op_cmp_lt<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLtU64, vec_op_cmp_lt<uint64_t>>(ctx, Variation{v});

      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLeI8, vec_op_cmp_le<int8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLeI16, vec_op_cmp_le<int16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLeI32, vec_op_cmp_le<int32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLeI64, vec_op_cmp_le<int64_t>>(ctx, Variation{v});

      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLeU8, vec_op_cmp_le<uint8_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLeU16, vec_op_cmp_le<uint16_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLeU32, vec_op_cmp_le<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLeU64, vec_op_cmp_le<uint64_t>>(ctx, Variation{v});
    }
  }

  INFO("  Testing logical (float)");
  {
    for (uint32_t v = 0; v < kNumVariationsVVV; v++) {
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAndF32, vec_op_and<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAndF64, vec_op_and<uint64_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kOrF32, vec_op_or<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kOrF64, vec_op_or<uint64_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kXorF32, vec_op_xor<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kXorF64, vec_op_xor<uint64_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAndnF32, vec_op_andn<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAndnF64, vec_op_andn<uint64_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kBicF32, vec_op_bic<uint32_t>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kBicF64, vec_op_bic<uint64_t>>(ctx, Variation{v});
    }
  }

  INFO("  Testing arithmetic (float)");
  {
    if (scalarOpBehavior == ScalarOpBehavior::kZeroing) {
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAddF32S, scalar_op_fadd<ScalarOpBehavior::kZeroing, float>>(ctx);
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAddF64S, scalar_op_fadd<ScalarOpBehavior::kZeroing, double>>(ctx);
      test_vecop_vvv<kVecWidth, OpcodeVVV::kSubF32S, scalar_op_fsub<ScalarOpBehavior::kZeroing, float>>(ctx);
      test_vecop_vvv<kVecWidth, OpcodeVVV::kSubF64S, scalar_op_fsub<ScalarOpBehavior::kZeroing, double>>(ctx);
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMulF32S, scalar_op_fmul<ScalarOpBehavior::kZeroing, float>>(ctx);
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMulF64S, scalar_op_fmul<ScalarOpBehavior::kZeroing, double>>(ctx);
      test_vecop_vvv<kVecWidth, OpcodeVVV::kDivF32S, scalar_op_fdiv<ScalarOpBehavior::kZeroing, float>>(ctx);
      test_vecop_vvv<kVecWidth, OpcodeVVV::kDivF64S, scalar_op_fdiv<ScalarOpBehavior::kZeroing, double>>(ctx);
    }
    else {
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAddF32S, scalar_op_fadd<ScalarOpBehavior::kPreservingVec128, float>>(ctx);
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAddF64S, scalar_op_fadd<ScalarOpBehavior::kPreservingVec128, double>>(ctx);
      test_vecop_vvv<kVecWidth, OpcodeVVV::kSubF32S, scalar_op_fsub<ScalarOpBehavior::kPreservingVec128, float>>(ctx);
      test_vecop_vvv<kVecWidth, OpcodeVVV::kSubF64S, scalar_op_fsub<ScalarOpBehavior::kPreservingVec128, double>>(ctx);
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMulF32S, scalar_op_fmul<ScalarOpBehavior::kPreservingVec128, float>>(ctx);
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMulF64S, scalar_op_fmul<ScalarOpBehavior::kPreservingVec128, double>>(ctx);
      test_vecop_vvv<kVecWidth, OpcodeVVV::kDivF32S, scalar_op_fdiv<ScalarOpBehavior::kPreservingVec128, float>>(ctx);
      test_vecop_vvv<kVecWidth, OpcodeVVV::kDivF64S, scalar_op_fdiv<ScalarOpBehavior::kPreservingVec128, double>>(ctx);
    }

    for (uint32_t v = 0; v < kNumVariationsVVV; v++) {
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAddF32, vec_op_fadd<float>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kAddF64, vec_op_fadd<double>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kSubF32, vec_op_fsub<float>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kSubF64, vec_op_fsub<double>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMulF32, vec_op_fmul<float>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMulF64, vec_op_fmul<double>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kDivF32, vec_op_fdiv<float>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kDivF64, vec_op_fdiv<double>>(ctx, Variation{v});
    }
  }

  if (fMulAddOpBehavior == FMulAddOpBehavior::kNoFMA) {
    INFO("  Testing madd (no-fma) (float)");
    {
      if (scalarOpBehavior == ScalarOpBehavior::kZeroing) {
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMAddF32S, scalar_op_fmadd_nofma<ScalarOpBehavior::kZeroing, float>>(ctx, Variation{0});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMAddF64S, scalar_op_fmadd_nofma<ScalarOpBehavior::kZeroing, double>>(ctx, Variation{0});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMSubF32S, scalar_op_fmsub_nofma<ScalarOpBehavior::kZeroing, float>>(ctx, Variation{0});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMSubF64S, scalar_op_fmsub_nofma<ScalarOpBehavior::kZeroing, double>>(ctx, Variation{0});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMAddF32S, scalar_op_fnmadd_nofma<ScalarOpBehavior::kZeroing, float>>(ctx, Variation{0});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMAddF64S, scalar_op_fnmadd_nofma<ScalarOpBehavior::kZeroing, double>>(ctx, Variation{0});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMSubF32S, scalar_op_fnmsub_nofma<ScalarOpBehavior::kZeroing, float>>(ctx, Variation{0});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMSubF64S, scalar_op_fnmsub_nofma<ScalarOpBehavior::kZeroing, double>>(ctx, Variation{0});
      }
      else {
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMAddF32S, scalar_op_fmadd_nofma<ScalarOpBehavior::kPreservingVec128, float>>(ctx, Variation{0});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMAddF64S, scalar_op_fmadd_nofma<ScalarOpBehavior::kPreservingVec128, double>>(ctx, Variation{0});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMSubF32S, scalar_op_fmsub_nofma<ScalarOpBehavior::kPreservingVec128, float>>(ctx, Variation{0});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMSubF64S, scalar_op_fmsub_nofma<ScalarOpBehavior::kPreservingVec128, double>>(ctx, Variation{0});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMAddF32S, scalar_op_fnmadd_nofma<ScalarOpBehavior::kPreservingVec128, float>>(ctx, Variation{0});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMAddF64S, scalar_op_fnmadd_nofma<ScalarOpBehavior::kPreservingVec128, double>>(ctx, Variation{0});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMSubF32S, scalar_op_fnmsub_nofma<ScalarOpBehavior::kPreservingVec128, float>>(ctx, Variation{0});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMSubF64S, scalar_op_fnmsub_nofma<ScalarOpBehavior::kPreservingVec128, double>>(ctx, Variation{0});
      }

      for (uint32_t v = 0; v < kNumVariationsVVVV; v++) {
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMAddF32, vec_op_fmadd_nofma<float>>(ctx, Variation{v});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMAddF64, vec_op_fmadd_nofma<double>>(ctx, Variation{v});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMSubF32, vec_op_fmsub_nofma<float>>(ctx, Variation{v});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMSubF64, vec_op_fmsub_nofma<double>>(ctx, Variation{v});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMAddF32, vec_op_fnmadd_nofma<float>>(ctx, Variation{v});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMAddF64, vec_op_fnmadd_nofma<double>>(ctx, Variation{v});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMSubF32, vec_op_fnmsub_nofma<float>>(ctx, Variation{v});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMSubF64, vec_op_fnmsub_nofma<double>>(ctx, Variation{v});
      }
    }
  }
  else {
    INFO("  Testing madd (fma) (float)");
    {
      if (valgrindFmaBug) {
        INFO("    (scalar FMA tests ignored due to a Valgrind bug!)");
      }
      else {
        if (scalarOpBehavior == ScalarOpBehavior::kZeroing) {
          test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMAddF32S, scalar_op_fmadd_fma<ScalarOpBehavior::kZeroing, float>>(ctx, Variation{0});
          test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMAddF64S, scalar_op_fmadd_fma<ScalarOpBehavior::kZeroing, double>>(ctx, Variation{0});
          test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMSubF32S, scalar_op_fmsub_fma<ScalarOpBehavior::kZeroing, float>>(ctx, Variation{0});
          test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMSubF64S, scalar_op_fmsub_fma<ScalarOpBehavior::kZeroing, double>>(ctx, Variation{0});
          test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMAddF32S, scalar_op_fnmadd_fma<ScalarOpBehavior::kZeroing, float>>(ctx, Variation{0});
          test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMAddF64S, scalar_op_fnmadd_fma<ScalarOpBehavior::kZeroing, double>>(ctx, Variation{0});
          test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMSubF32S, scalar_op_fnmsub_fma<ScalarOpBehavior::kZeroing, float>>(ctx, Variation{0});
          test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMSubF64S, scalar_op_fnmsub_fma<ScalarOpBehavior::kZeroing, double>>(ctx, Variation{0});
        }
        else {
          test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMAddF32S, scalar_op_fmadd_fma<ScalarOpBehavior::kPreservingVec128, float>>(ctx, Variation{0});
          test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMAddF64S, scalar_op_fmadd_fma<ScalarOpBehavior::kPreservingVec128, double>>(ctx, Variation{0});
          test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMSubF32S, scalar_op_fmsub_fma<ScalarOpBehavior::kPreservingVec128, float>>(ctx, Variation{0});
          test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMSubF64S, scalar_op_fmsub_fma<ScalarOpBehavior::kPreservingVec128, double>>(ctx, Variation{0});
          test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMAddF32S, scalar_op_fnmadd_fma<ScalarOpBehavior::kPreservingVec128, float>>(ctx, Variation{0});
          test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMAddF64S, scalar_op_fnmadd_fma<ScalarOpBehavior::kPreservingVec128, double>>(ctx, Variation{0});
          test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMSubF32S, scalar_op_fnmsub_fma<ScalarOpBehavior::kPreservingVec128, float>>(ctx, Variation{0});
          test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMSubF64S, scalar_op_fnmsub_fma<ScalarOpBehavior::kPreservingVec128, double>>(ctx, Variation{0});
        }
      }

      for (uint32_t v = 0; v < kNumVariationsVVVV; v++) {
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMAddF32, vec_op_fmadd_fma<float>>(ctx, Variation{v});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMAddF64, vec_op_fmadd_fma<double>>(ctx, Variation{v});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMSubF32, vec_op_fmsub_fma<float>>(ctx, Variation{v});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kMSubF64, vec_op_fmsub_fma<double>>(ctx, Variation{v});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMAddF32, vec_op_fnmadd_fma<float>>(ctx, Variation{v});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMAddF64, vec_op_fnmadd_fma<double>>(ctx, Variation{v});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMSubF32, vec_op_fnmsub_fma<float>>(ctx, Variation{v});
        test_vecop_vvvv<kVecWidth, OpcodeVVVV::kNMSubF64, vec_op_fnmsub_fma<double>>(ctx, Variation{v});
      }
    }
  }

  INFO("  Testing min / max (float)");
  {
#if defined(BL_JIT_ARCH_X86)
    test_vecop_vvv<kVecWidth, OpcodeVVV::kMinF32S, scalar_op_fmin_ternary<ScalarOpBehavior::kPreservingVec128, float>>(ctx);
    test_vecop_vvv<kVecWidth, OpcodeVVV::kMinF64S, scalar_op_fmin_ternary<ScalarOpBehavior::kPreservingVec128, double>>(ctx);
    test_vecop_vvv<kVecWidth, OpcodeVVV::kMaxF32S, scalar_op_fmax_ternary<ScalarOpBehavior::kPreservingVec128, float>>(ctx);
    test_vecop_vvv<kVecWidth, OpcodeVVV::kMaxF64S, scalar_op_fmax_ternary<ScalarOpBehavior::kPreservingVec128, double>>(ctx);

    for (uint32_t v = 0; v < kNumVariationsVVV; v++) {
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMinF32, vec_op_fmin_ternary<float>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMinF64, vec_op_fmin_ternary<double>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMaxF32, vec_op_fmax_ternary<float>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMaxF64, vec_op_fmax_ternary<double>>(ctx, Variation{v});
    }
#endif

#if defined(BL_JIT_ARCH_A64)
    test_vecop_vvv<kVecWidth, OpcodeVVV::kMinF32S, scalar_op_fmin_finite<ScalarOpBehavior::kZeroing, float>>(ctx);
    test_vecop_vvv<kVecWidth, OpcodeVVV::kMinF64S, scalar_op_fmin_finite<ScalarOpBehavior::kZeroing, double>>(ctx);
    test_vecop_vvv<kVecWidth, OpcodeVVV::kMaxF32S, scalar_op_fmax_finite<ScalarOpBehavior::kZeroing, float>>(ctx);
    test_vecop_vvv<kVecWidth, OpcodeVVV::kMaxF64S, scalar_op_fmax_finite<ScalarOpBehavior::kZeroing, double>>(ctx);

    for (uint32_t v = 0; v < kNumVariationsVVV; v++) {
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMinF32, vec_op_fmin_finite<float>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMinF64, vec_op_fmin_finite<double>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMaxF32, vec_op_fmax_finite<float>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kMaxF64, vec_op_fmax_finite<double>>(ctx, Variation{v});
    }
#endif
  }

  INFO("  Testing cmp (float)");
  {
    for (uint32_t v = 0; v < kNumVariationsVVV; v++) {
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpEqF32, vec_op_fcmpo_eq<float>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpEqF64, vec_op_fcmpo_eq<double>>(ctx, Variation{v});

      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpNeF32, vec_op_fcmpu_ne<float>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpNeF64, vec_op_fcmpu_ne<double>>(ctx, Variation{v});

      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGtF32, vec_op_fcmpo_gt<float>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGtF64, vec_op_fcmpo_gt<double>>(ctx, Variation{v});

      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGeF32, vec_op_fcmpo_ge<float>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpGeF64, vec_op_fcmpo_ge<double>>(ctx, Variation{v});

      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLtF32, vec_op_fcmpo_lt<float>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLtF64, vec_op_fcmpo_lt<double>>(ctx, Variation{v});

      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLeF32, vec_op_fcmpo_le<float>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpLeF64, vec_op_fcmpo_le<double>>(ctx, Variation{v});

      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpOrdF32, vec_op_fcmp_ord<float>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpOrdF64, vec_op_fcmp_ord<double>>(ctx, Variation{v});

      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpUnordF32, vec_op_fcmp_unord<float>>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCmpUnordF64, vec_op_fcmp_unord<double>>(ctx, Variation{v});
    }
  }

  INFO("  Testing hadd (float)");
  {
    for (uint32_t v = 0; v < kNumVariationsVVV; v++) {
      test_vecop_vvv<kVecWidth, OpcodeVVV::kHAddF64, vec_op_hadd_f64>(ctx, Variation{v});
    }
  }

  INFO("  Testing combine");
  {
    for (uint32_t v = 0; v < kNumVariationsVVV; v++) {
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCombineLoHiU64, vec_op_combine_lo_hi_u64>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCombineLoHiF64, vec_op_combine_lo_hi_u64>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCombineHiLoU64, vec_op_combine_hi_lo_u64>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kCombineHiLoF64, vec_op_combine_hi_lo_u64>(ctx, Variation{v});
    }
  }

  INFO("  Testing interleave");
  {
    for (uint32_t v = 0; v < kNumVariationsVVV; v++) {
      test_vecop_vvv<kVecWidth, OpcodeVVV::kInterleaveLoU8, vec_op_interleave_lo_u8>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kInterleaveHiU8, vec_op_interleave_hi_u8>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kInterleaveLoU16, vec_op_interleave_lo_u16>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kInterleaveHiU16, vec_op_interleave_hi_u16>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kInterleaveLoU32, vec_op_interleave_lo_u32>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kInterleaveHiU32, vec_op_interleave_hi_u32>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kInterleaveLoU64, vec_op_interleave_lo_u64>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kInterleaveHiU64, vec_op_interleave_hi_u64>(ctx, Variation{v});
    }
  }

  INFO("  Testing packs");
  {
    for (uint32_t v = 0; v < kNumVariationsVVV; v++) {
      test_vecop_vvv<kVecWidth, OpcodeVVV::kPacksI16_I8, vec_op_packs_i16_i8>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kPacksI16_U8, vec_op_packs_i16_u8>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kPacksI32_I16, vec_op_packs_i32_i16>(ctx, Variation{v});
      test_vecop_vvv<kVecWidth, OpcodeVVV::kPacksI32_U16, vec_op_packs_i32_u16>(ctx, Variation{v});
    }
  }

  INFO("  Testing alignr_u128");
  {
    for (uint32_t v = 0; v < kNumVariationsVVVI; v++) {
      for (uint32_t i = 1; i < 16; i++) {
        test_vecop_vvvi<kVecWidth, OpcodeVVVI::kAlignr_U128, vec_op_alignr_u128>(ctx, i, Variation{v});
      }
    }
  }

  INFO("  Testing interleave_shuffle");
  {
    for (uint32_t v = 0; v < kNumVariationsVVVI; v++) {
      for (uint32_t i = 0; i < 256; i++) {
        uint32_t imm = swizzle((i >> 6) & 3, (i >> 4) & 3, (i >> 2) & 3, i & 3).value;

        test_vecop_vvvi<kVecWidth, OpcodeVVVI::kInterleaveShuffleU32x4, vec_op_interleave_shuffle_u32x4>(ctx, imm, Variation{v});
        test_vecop_vvvi<kVecWidth, OpcodeVVVI::kInterleaveShuffleF32x4, vec_op_interleave_shuffle_u32x4>(ctx, imm, Variation{v});
      }

      for (uint32_t i = 0; i < 4; i++) {
        uint32_t imm = swizzle((i >> 1) & 1, i & 1).value;

        test_vecop_vvvi<kVecWidth, OpcodeVVVI::kInterleaveShuffleU64x2, vec_op_interleave_shuffle_u64x2>(ctx, imm, Variation{v});
        test_vecop_vvvi<kVecWidth, OpcodeVVVI::kInterleaveShuffleF64x2, vec_op_interleave_shuffle_u64x2>(ctx, imm, Variation{v});
      }
    }
  }
}

static void test_gp_ops(JitContext& ctx) noexcept {
  test_cond_ops(ctx);
  test_m_ops(ctx);
  test_rm_ops(ctx);
  test_rr_ops(ctx);
  test_rrr_ops(ctx);
}

#if defined(BL_JIT_ARCH_X86)
static void dumpFeatureList(asmjit::String& out, const asmjit::CpuFeatures& features) noexcept {
  asmjit::CpuFeatures::Iterator it = features.iterator();

  bool first = true;
  while (it.hasNext()) {
    size_t featureId = it.next();
    if (!first)
      out.append(' ');
    asmjit::Formatter::formatFeature(out, asmjit::Arch::kHost, uint32_t(featureId));
    first = false;
  }
}

static void test_x86_ops(JitContext& ctx, const asmjit::CpuFeatures& hostFeatures) noexcept {
  using Ext = asmjit::CpuFeatures::X86;
  using CpuFeatures = asmjit::CpuFeatures;

  {
    asmjit::String s;
    dumpFeatureList(s, hostFeatures);
    INFO("Available CPU features: %s", s.data());
  }

  // Features that must always be available;
  CpuFeatures base;
  base.add(Ext::kI486, Ext::kCMOV, Ext::kCMPXCHG8B, Ext::kFPU, Ext::kSSE, Ext::kSSE2);

  // To verify that JIT implements ALL features with ALL possible CPU flags, we use profiles to select features
  // that the JIT compiler will be allowed to use. The features are gradually increased similarly to how new CPU
  // generations introduced them. We cannot cover ALL possible CPUs, but that's not even necessary as we test
  // individual operations where instructions can be selected on the features available.

  // GP variations.
  {
    CpuFeatures profiles[4] {};
    profiles[0] = base;

    profiles[1] = profiles[0];
    profiles[1].add(Ext::kADX, Ext::kBMI);

    profiles[2] = profiles[1];
    profiles[2].add(Ext::kBMI2, Ext::kLZCNT, Ext::kMOVBE, Ext::kPOPCNT);

    profiles[3] = hostFeatures;

    bool first = true;
    CpuFeatures lastFiltered;

    for (const CpuFeatures& profile : profiles) {
      CpuFeatures filtered = profile;

      for (uint32_t i = 0; i < CpuFeatures::kNumBitWords; i++)
        filtered.data()._bits[i] &= hostFeatures.data()._bits[i];

      if (!first && filtered == lastFiltered)
        continue;

      asmjit::String s;
      if (filtered == hostFeatures)
        s.assign("[ALL]");
      else
        dumpFeatureList(s, filtered);

      ctx.features = filtered;

      INFO("Testing JIT compiler GP ops with [%s]", s.data());
      test_gp_ops(ctx);

      first = false;
      lastFiltered = filtered;
    }
  }

  // SIMD variations covering SSE2+, AVX+, and AVX512+ cases.
  {
    CpuFeatures profiles[15] {};
    profiles[0] = base;

    profiles[1] = profiles[0];
    profiles[1].add(Ext::kSSE3);

    profiles[2] = profiles[1];
    profiles[2].add(Ext::kSSSE3);

    profiles[3] = profiles[2];
    profiles[3].add(Ext::kSSE4_1);

    profiles[4] = profiles[3];
    profiles[4].add(Ext::kSSE4_2, Ext::kADX, Ext::kBMI, Ext::kBMI2, Ext::kLZCNT, Ext::kMOVBE, Ext::kPOPCNT);

    profiles[5] = profiles[4];
    profiles[5].add(Ext::kPCLMULQDQ);

    profiles[6] = profiles[5];
    profiles[6].add(Ext::kAVX);

    profiles[7] = profiles[6];
    profiles[7].add(Ext::kAVX2);

    profiles[8] = profiles[7];
    profiles[8].add(Ext::kF16C, Ext::kFMA, Ext::kVAES, Ext::kVPCLMULQDQ);

    profiles[9] = profiles[8];
    profiles[9].add(Ext::kAVX_IFMA, Ext::kAVX_NE_CONVERT, Ext::kAVX_VNNI, Ext::kAVX_VNNI_INT8, Ext::kAVX_VNNI_INT16);

    // We start deliberately from a profile that doesn't contains AVX_xxx
    // extensions as these didn't exist when the first AVX512 CPUs were shipped.
    profiles[10] = profiles[8];
    profiles[10].add(Ext::kAVX512_F, Ext::kAVX512_BW, Ext::kAVX512_DQ, Ext::kAVX512_CD, Ext::kAVX512_VL);

    profiles[11] = profiles[10];
    profiles[11].add(Ext::kAVX512_IFMA, Ext::kAVX512_VBMI);

    profiles[12] = profiles[11];
    profiles[12].add(Ext::kAVX512_BITALG, Ext::kAVX512_VBMI2, Ext::kAVX512_VNNI, Ext::kAVX512_VPOPCNTDQ);

    profiles[13] = profiles[12];
    profiles[13].add(Ext::kAVX512_BF16, Ext::kAVX512_FP16);

    profiles[14] = hostFeatures;

    bool first = true;
    CpuFeatures lastFiltered;

    for (const CpuFeatures& profile : profiles) {
      CpuFeatures filtered = profile;

      for (uint32_t i = 0; i < CpuFeatures::kNumBitWords; i++)
        filtered.data()._bits[i] &= hostFeatures.data()._bits[i];

      if (!first && filtered == lastFiltered)
        continue;

      asmjit::String s;
      if (filtered == hostFeatures)
        s.assign("[ALL]");
      else
        dumpFeatureList(s, filtered);

      ctx.features = filtered;

      INFO("Testing JIT compiler 128-bit SIMD ops with [%s]", s.data());
      test_simd_ops<VecWidth::k128>(ctx);

      if (filtered.x86().hasAVX2()) {
        INFO("Testing JIT compiler 256-bit SIMD ops with [%s]", s.data());
        test_simd_ops<VecWidth::k256>(ctx);
      }

      if (filtered.x86().hasAVX512_F()) {
        INFO("Testing JIT compiler 512-bit SIMD ops with [%s]", s.data());
        test_simd_ops<VecWidth::k512>(ctx);
      }

      first = false;
      lastFiltered = filtered;
    }
  }
}
#endif // BL_JIT_ARCH_X86

#if defined(BL_JIT_ARCH_A64)
static void test_a64_ops(JitContext& ctx, const asmjit::CpuFeatures& hostFeatures) noexcept {
  ctx.features = hostFeatures;

  test_gp_ops(ctx);
  test_simd_ops<VecWidth::k128>(ctx);
}
#endif // BL_JIT_ARCH_A64

UNIT(pipecompiler, BL_TEST_GROUP_PIPELINE_JIT_COMPILER) {
  JitContext ctx;
  asmjit::CpuFeatures hostFeatures = asmjit::CpuInfo::host().features();

#if defined(BL_JIT_ARCH_X86)
  test_x86_ops(ctx, hostFeatures);
#elif defined(BL_JIT_ARCH_A64)
  test_a64_ops(ctx, hostFeatures);
#endif
}

} // {Tests}
} // {JIT}
} // {Pipeline}
} // {bl}

#endif // BL_TEST && !BL_BUILD_NO_JIT
