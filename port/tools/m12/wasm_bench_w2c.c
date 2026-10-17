#include <stdio.h>
#include <time.h>
#include "bench_wasm.h"  /* wasm2c output of wasm_bench.cpp */
int main(void) {
  wasm_rt_init();
  w2c_bench inst;
  wasm2c_bench_instantiate(&inst);
  struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a);
  unsigned r = w2c_bench_bench_run(&inst, 3000);
  clock_gettime(CLOCK_MONOTONIC, &b);
  printf("wasm2c: result %u, %.3f s\n", r, (b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) / 1e9);
  wasm2c_bench_free(&inst);
  wasm_rt_free();
  return 0;
}
