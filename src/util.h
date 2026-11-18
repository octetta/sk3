#ifndef SKRED_UTIL_H
#define SKRED_UTIL_H

#define NS_TO_MS (1000000)
#define S_TO_MS (1000)

#include <time.h>
#include <stdint.h>
#ifdef _WIN32
#include "portable_win.h"
#endif

/* High-resolution timestamp benchmarking state */
typedef struct {
  struct timespec a;       /* Interval start timestamp */
  struct timespec b;       /* Interval stop timestamp */
  int64_t diff;            /* Elapsed interval in nanoseconds */
  int state;               /* Benchmark state (BEN_0, BEN_A, BEN_B, BEN_D) */
  int order;               /* Sequence execution counter */
  int frames;              /* Processed frames in interval */
} skred_bench_t;
typedef skred_bench_t sben_t;

#define BENLEN (4)

enum { BEN_0, BEN_A, BEN_B, BEN_D };

#define BEN_MARK_A(ben,benp,nf,bo) \
  { clock_gettime(BENCH_CLOCK, &ben[benp].a); \
  ben[benp].frames = nf; \
  ben[benp].order = bo; \
  ben[benp].state = BEN_A; }

#define BEN_MARK_B(bench, benchp, bencho) \
  { clock_gettime(BENCH_CLOCK, &bench[benchp].b); \
  bench[benchp].state = BEN_B; \
  bencho++; \
  benchp = ((bencho) % BENLEN); }

int64_t ts_diff_ns(const struct timespec *a, const struct timespec *b);

#define BENCH_CLOCK CLOCK_MONOTONIC


/* Platform-specific headers and definitions */
#if defined(_WIN32)
    #include <windows.h>
    #define PATH_SEP '\\'
    #define MAX_PATH_LEN MAX_PATH
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
    #include <limits.h>
    #define PATH_SEP '/'
    #define MAX_PATH_LEN PATH_MAX
#elif defined(__linux__)
    #include <unistd.h>
    #include <limits.h>
    #define PATH_SEP '/'
    #define MAX_PATH_LEN PATH_MAX
#elif defined(__EMSCRIPTEN__)
    #define PATH_SEP '/'
    #define MAX_PATH_LEN PATH_MAX
#else
    #error "Platform not supported"
#endif

int get_executable_path(char* buffer, size_t size);
void get_directory(const char* full_path, char* dir_out);
int join_path(char* buffer, size_t size, const char* base, const char* subpath);

#endif /* SKRED_UTIL_H */
