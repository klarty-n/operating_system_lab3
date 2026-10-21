#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "filter.h"

int run_timed_pass(Worker *workers, size_t worker_count, double *total_seconds);

int measure_one_worker(const int64_t *input, int64_t *output, size_t rows, size_t cols, size_t window, size_t iterations, double *seconds);

#endif
