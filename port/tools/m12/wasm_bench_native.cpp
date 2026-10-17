#include <stdio.h>
#include <time.h>
extern "C" unsigned bench_run(unsigned frames);
int main() {
  timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a);
  unsigned r = bench_run(3000);
  clock_gettime(CLOCK_MONOTONIC, &b);
  printf("native: result %u, %.3f s\n", r, (b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) / 1e9);
}
