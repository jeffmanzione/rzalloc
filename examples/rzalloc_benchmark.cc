#include <benchmark/benchmark.h>

#include <cstdlib>
#include <vector>

extern "C" {
#include "rzalloc/rzalloc.h"
}

// -----------------------------------------------------------------------------
// Single alloc/free benchmarks
// -----------------------------------------------------------------------------

template <size_t N>
static void BM_Malloc(benchmark::State& state) {
  for (auto _ : state) {
    void* p = std::malloc(N);
    benchmark::DoNotOptimize(p);
    std::free(p);
  }
}

template <size_t N>
static void BM_Rzalloc(benchmark::State& state) {
  RzallocArena arena;
  arena_init(&arena, N);

  for (auto _ : state) {
    void* p = arena_malloc(&arena);
    benchmark::DoNotOptimize(p);
    arena_free(&arena, p);
  }

  arena_clear(&arena);
}

// -----------------------------------------------------------------------------
// Long-lived benchmarks (no free during iterations)
// -----------------------------------------------------------------------------

template <size_t N>
static void BM_Malloc_LongLived(benchmark::State& state) {
  std::vector<void*> objects;
  objects.reserve(state.max_iterations);

  for (auto _ : state) {
    void* p = std::malloc(N);
    benchmark::DoNotOptimize(p);
    objects.push_back(p);
  }

  // Cleanup after benchmark ends
  for (void* p : objects) {
    benchmark::DoNotOptimize(p);
    std::free(p);
  }
}

template <size_t N>
static void BM_Rzalloc_LongLived(benchmark::State& state) {
  RzallocArena arena;
  arena_init(&arena, N);

  std::vector<void*> objects;
  objects.reserve(state.max_iterations);

  for (auto _ : state) {
    void* p = arena_malloc(&arena);
    benchmark::DoNotOptimize(p);
    objects.push_back(p);
  }

  arena_clear(&arena);  // Frees all allocations at once
}

// -----------------------------------------------------------------------------
// Batch Allocate + Free
// -----------------------------------------------------------------------------

static void BatchMalloc(benchmark::State& state) {
  const size_t size = state.range(0);
  const int batch = 1000;

  for (auto _ : state) {
    for (int i = 0; i < batch; ++i) {
      void* p = std::malloc(size);
      benchmark::DoNotOptimize(p);
      std::free(p);
    }
  }
}

static void BatchRzalloc(benchmark::State& state) {
  const size_t size = state.range(0);
  const int batch = 1000;

  RzallocArena arena;
  arena_init(&arena, size);

  for (auto _ : state) {
    for (int i = 0; i < batch; ++i) {
      void* p = arena_malloc(&arena);
      benchmark::DoNotOptimize(p);
      arena_free(&arena, p);
    }
  }

  arena_clear(&arena);
}

// -----------------------------------------------------------------------------
// Batch Long-Lived (no frees)
// -----------------------------------------------------------------------------

static void BatchMalloc_LongLived(benchmark::State& state) {
  const size_t size = state.range(0);
  const int batch = 1000;

  std::vector<void*> objects;

  for (auto _ : state) {
    for (int i = 0; i < batch; ++i) {
      void* p = std::malloc(size);
      benchmark::DoNotOptimize(p);
      objects.push_back(p);
    }
  }

  for (void* p : objects) {
    benchmark::DoNotOptimize(p);
    std::free(p);
  }
}

static void BatchRzalloc_LongLived(benchmark::State& state) {
  const size_t size = state.range(0);
  const int batch = 1000;

  RzallocArena arena;
  arena_init(&arena, size);

  std::vector<void*> objects;

  for (auto _ : state) {
    for (int i = 0; i < batch; ++i) {
      void* p = arena_malloc(&arena);
      benchmark::DoNotOptimize(p);
      objects.push_back(p);
    }
  }

  arena_clear(&arena);
}

// -----------------------------------------------------------------------------
// Benchmark Registrations
// -----------------------------------------------------------------------------

#define REGISTER_SIZES()                \
  BENCHMARK(BM_Malloc<16>);             \
  BENCHMARK(BM_Rzalloc<16>);            \
  BENCHMARK(BM_Malloc<32>);             \
  BENCHMARK(BM_Rzalloc<32>);            \
  BENCHMARK(BM_Malloc<64>);             \
  BENCHMARK(BM_Rzalloc<64>);            \
  BENCHMARK(BM_Malloc<128>);            \
  BENCHMARK(BM_Rzalloc<128>);           \
  BENCHMARK(BM_Malloc<256>);            \
  BENCHMARK(BM_Rzalloc<256>);           \
  BENCHMARK(BM_Malloc<512>);            \
  BENCHMARK(BM_Rzalloc<512>);           \
  BENCHMARK(BM_Malloc<1024>);           \
  BENCHMARK(BM_Rzalloc<1024>);          \
  BENCHMARK(BM_Malloc_LongLived<16>);   \
  BENCHMARK(BM_Rzalloc_LongLived<16>);  \
  BENCHMARK(BM_Malloc_LongLived<32>);   \
  BENCHMARK(BM_Rzalloc_LongLived<32>);  \
  BENCHMARK(BM_Malloc_LongLived<64>);   \
  BENCHMARK(BM_Rzalloc_LongLived<64>);  \
  BENCHMARK(BM_Malloc_LongLived<128>);  \
  BENCHMARK(BM_Rzalloc_LongLived<128>); \
  BENCHMARK(BM_Malloc_LongLived<256>);  \
  BENCHMARK(BM_Rzalloc_LongLived<256>); \
  BENCHMARK(BM_Malloc_LongLived<512>);  \
  BENCHMARK(BM_Rzalloc_LongLived<512>); \
  BENCHMARK(BM_Malloc_LongLived<1024>); \
  BENCHMARK(BM_Rzalloc_LongLived<1024>);

REGISTER_SIZES();

BENCHMARK(BatchMalloc)->Arg(16)->Arg(64)->Arg(256)->Arg(1024);
BENCHMARK(BatchRzalloc)->Arg(16)->Arg(64)->Arg(256)->Arg(1024);

BENCHMARK(BatchMalloc_LongLived)->Arg(16)->Arg(64)->Arg(256)->Arg(1024);
BENCHMARK(BatchRzalloc_LongLived)->Arg(16)->Arg(64)->Arg(256)->Arg(1024);

BENCHMARK_MAIN();
