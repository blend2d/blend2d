#include <blend2d/blend2d.h>
#include <blend2d-testing/commons/zero_bench.h>

namespace bl::Benchmarks {

static constexpr uint32_t kCount = 1000000;

static void single_create_destroy([[maybe_unused]] ZeroBench::Context& ctx) {
  uint32_t x = 0;

  for (uint32_t n = 0; n < kCount; n++) {
    BLCoverageSet cov;
    ZeroBench::consume(cov._impl());
  }

  ZeroBench::consume(x);
  ctx.set_value(0, kCount * 2);
}

static void single_create_add_test_destroy([[maybe_unused]] ZeroBench::Context& ctx) {
  uint32_t x = 0;

  for (uint32_t n = 0; n < kCount; n++) {
    BLCoverageSet cov;
    cov.add(n);
    x += cov.contains(n);
  }

  ZeroBench::consume(x);
  ctx.set_value(0, kCount * 4);
}

static void single_add_test([[maybe_unused]] ZeroBench::Context& ctx) {
  uint32_t x = 0;
  BLCoverageSet cov;

  for (uint32_t n = 0; n < kCount; n++) {
    cov.add(n);
    x += cov.contains(n);
  }

  ZeroBench::consume(x);
  ctx.set_value(0, kCount * 2);
}

static void single_add_test_remove([[maybe_unused]] ZeroBench::Context& ctx) {
  uint32_t x = 0;
  BLCoverageSet cov;

  for (uint32_t n = 0; n < kCount; n++) {
    cov.add(n);
    x += cov.contains(n);
    cov.remove(n);
  }

  ZeroBench::consume(x);
  ctx.set_value(0, kCount * 3);
}

static void single_add_get_cardinality([[maybe_unused]] ZeroBench::Context& ctx) {
  uint32_t x = 0;
  BLCoverageSet cov;

  for (uint32_t n = 0; n < kCount; n++) {
    cov.add(n);
    x += cov.cardinality();
  }

  ZeroBench::consume(x);
  ctx.set_value(0, kCount * 2);
}

static void single_add_forward([[maybe_unused]] ZeroBench::Context& ctx) {
  BLCoverageSet cov;

  for (uint32_t n = 0; n < kCount; n++) {
    cov.add(n);
  }

  ctx.set_value(0, kCount);
}

static void single_add_reverse([[maybe_unused]] ZeroBench::Context& ctx) {
  BLCoverageSet cov;

  for (uint32_t n = 0; n < kCount; n++) {
    cov.add(kCount - n - 1);
  }

  ctx.set_value(0, kCount);
}

static void single_add_alternating([[maybe_unused]] ZeroBench::Context& ctx) {
  BLCoverageSet cov;

  for (uint32_t n = 0; n < kCount; n++) {
    if (n & 1)
      cov.add(n);
    else
      cov.add(kCount - n - 1);
  }

  ctx.set_value(0, kCount);
}

static void single_add_remove_even([[maybe_unused]] ZeroBench::Context& ctx) {
  BLCoverageSet cov;
  for (uint32_t i = 0; i < kCount; i += 2) {
    cov.add(i);
  }

  for (uint32_t i = 0; i < kCount; i += 2) {
    cov.remove(i);
  }

  ctx.set_value(0, kCount);
}

template<uint32_t N>
static void range_add_forward([[maybe_unused]] ZeroBench::Context& ctx) {
  BLCoverageSet cov;

  for (uint32_t i = 0; i < kCount; i += N) {
    cov.add_range(i, i + N);
  }

  ctx.set_value(0, kCount / N);
}

template<uint32_t N, uint32_t RangeCount>
static void range_add_forward_batch([[maybe_unused]] ZeroBench::Context& ctx) {
  BLCoverageSet cov;

  BLCoverageSetRange ranges[RangeCount];

  uint32_t i = 0;
  size_t range_count = 0;

  do {
    for (; i < kCount && range_count < RangeCount; i += N) {
      ranges[range_count++] = BLCoverageSetRange{.start = i, .end = i + N};
    }
    cov.add_ranges(ranges, range_count);
    range_count = 0;
  } while (i < kCount);

  ctx.set_value(0, (kCount / N + RangeCount - 1) / RangeCount);
}

template<uint32_t N>
static void range_add_alternating([[maybe_unused]] ZeroBench::Context& ctx) {
  BLCoverageSet cov;
  uint32_t alt = 0;

  for (uint32_t i = 0; i < kCount; i += N, alt ^= 1) {
    if (alt == 0)
      cov.add_range(i, i + N);
    else
      cov.add_range(kCount - i, kCount - i + N);
  }

  ctx.set_value(0, kCount / N);
}

template<uint32_t N>
static void range_add_remove([[maybe_unused]] ZeroBench::Context& ctx) {
  BLCoverageSet cov;

  for (uint32_t i = 0; i < kCount; i += N) cov.add_range(i, i + N);
  for (uint32_t i = 0; i < kCount; i += N) cov.remove_range(i, i + N - 1);

  ctx.set_value(0, (kCount / N) * 2);
}

template<uint32_t N>
static void range_add_equals([[maybe_unused]] ZeroBench::Context& ctx) {
  uint32_t sum = 0;
  BLCoverageSet cov1;
  BLCoverageSet cov2;

  for (uint32_t i = 0; i < kCount; i += N) {
    cov1.add_range(i, i + N);
    cov2.add_range(i, i + N);
    sum += cov1.equals(cov2);
  }

  ZeroBench::consume(sum);
  ctx.set_value(0, (kCount / N) * 3);
}

static void combine_and([[maybe_unused]] ZeroBench::Context& ctx) {
  uint32_t sum = 0;
  BLCoverageSet cov1;
  BLCoverageSet cov2;

  for (uint32_t i = 0; i < kCount; i += 1) {
    cov1.add(i);
    cov2.add(i);
    cov1.combine(cov2, BL_BOOLEAN_OP_AND);
    cov2.clear();
  }

  ZeroBench::consume(sum);
  ctx.set_value(0, kCount * 4);
}

static void combine_or([[maybe_unused]] ZeroBench::Context& ctx) {
  uint32_t sum = 0;
  BLCoverageSet cov1;
  BLCoverageSet cov2;

  for (uint32_t i = 0; i < kCount; i += 1) {
    cov2.add(i);
    cov1.combine(cov2, BL_BOOLEAN_OP_OR);
    cov2.clear();
  }

  ZeroBench::consume(sum);
  ctx.set_value(0, kCount * 3);
}

static void combine_xor([[maybe_unused]] ZeroBench::Context& ctx) {
  uint32_t sum = 0;
  BLCoverageSet cov1;
  BLCoverageSet cov2;

  for (uint32_t i = 0; i < kCount; i += 1) {
    cov2.add(i);
    cov1.combine(cov2, BL_BOOLEAN_OP_XOR);
    cov2.clear();
  }

  ZeroBench::consume(sum);
  ctx.set_value(0, kCount * 3);
}

} // {bl::Benchmarks}

int main(int argc, char* argv[]) {
  using namespace bl::Benchmarks;

  ZeroBench::BenchmarkRef benchmarks[] = {
    ZERO_BENCH_RECORD(single_create_destroy),
    ZERO_BENCH_RECORD(single_create_add_test_destroy),
    ZERO_BENCH_RECORD(single_add_test),
    ZERO_BENCH_RECORD(single_add_test_remove),
    ZERO_BENCH_RECORD(single_add_get_cardinality),
    ZERO_BENCH_RECORD(single_add_forward),
    ZERO_BENCH_RECORD(single_add_reverse),
    ZERO_BENCH_RECORD(single_add_alternating),
    ZERO_BENCH_RECORD(single_add_remove_even),
    ZERO_BENCH_RECORD(range_add_forward<10>),
    ZERO_BENCH_RECORD(range_add_forward_batch<10, 2>),
    ZERO_BENCH_RECORD(range_add_forward_batch<10, 4>),
    ZERO_BENCH_RECORD(range_add_forward_batch<10, 8>),
    ZERO_BENCH_RECORD(range_add_forward_batch<10, 16>),
    ZERO_BENCH_RECORD(range_add_forward_batch<10, 32>),
    ZERO_BENCH_RECORD(range_add_forward_batch<10, 64>),
    ZERO_BENCH_RECORD(range_add_forward_batch<10, 128>),
    ZERO_BENCH_RECORD(range_add_alternating<10>),
    ZERO_BENCH_RECORD(range_add_forward<100>),
    ZERO_BENCH_RECORD(range_add_alternating<100>),
    ZERO_BENCH_RECORD(range_add_forward<5000>),
    ZERO_BENCH_RECORD(range_add_alternating<5000>),
    ZERO_BENCH_RECORD(range_add_remove<10>),
    ZERO_BENCH_RECORD(range_add_remove<100>),
    ZERO_BENCH_RECORD(range_add_remove<256>),
    ZERO_BENCH_RECORD(range_add_remove<1024>),
    ZERO_BENCH_RECORD(range_add_equals<10>),
    ZERO_BENCH_RECORD(range_add_equals<100>),
    ZERO_BENCH_RECORD(range_add_equals<1000>),
    ZERO_BENCH_RECORD(range_add_equals<5000>),
    ZERO_BENCH_RECORD(combine_and),
    ZERO_BENCH_RECORD(combine_or),
    ZERO_BENCH_RECORD(combine_xor)
  };

  ZeroBench::Runner runner(argc, argv);
  runner.set_label(0, "Coverage Set Benchmark");
  runner.add_metric("API Call Count", {
    .color = ZeroBench::kDefaultMetricColor,
    .value_kind = ZeroBench::ValueKind::kCount,
    .value_index = 0,
    .aggregate = ZeroBench::Aggregate::kStatic,
    .scale = ZeroBench::Scale::kDefault
  });
  runner.add_time_metrics();
  return runner.run(benchmarks);
}
