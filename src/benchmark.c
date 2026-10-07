#include "benchmark.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double seconds_between(struct timespec start, struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
}

int run_timed_pass(Worker *workers, size_t worker_count, double *total_seconds)
{
    struct timespec start, end;
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
        perror("clock_gettime start");
        return -1;
    }
    if (filter_pass(workers, worker_count) != 0)
        return -1;
    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        perror("clock_gettime end");
        return -1;
    }
    *total_seconds += seconds_between(start, end);
    return 0;
}

int measure_one_worker(const double *input, double *output, size_t rows,
                       size_t cols, size_t window, size_t iterations,
                       double *seconds)
{
    size_t count = rows * cols;
    double *copy = malloc(count * sizeof(*copy));
    if (copy == NULL) {
        perror("malloc baseline copy");
        return -1;
    }
    memcpy(copy, input, count * sizeof(*copy));

    double *source = copy;
    double *target = output;
    *seconds = 0.0;
    int failed = 0;

    for (size_t pass = 0; pass < iterations; ++pass) {
        Worker worker = {
            .input = source, .output = target, .rows = rows, .cols = cols,
            .window = window, .first_row = 0, .last_row = rows
        };
        if (run_timed_pass(&worker, 1, seconds) != 0) {
            failed = 1;
            break;
        }
        double *temporary = source;
        source = target;
        target = temporary;
    }
    free(copy);
    return failed ? -1 : 0;
}
