#ifndef FILTER_H
#define FILTER_H

#include <stddef.h>
#include <sys/types.h>

typedef struct {
    const double *input;
    double *output;
    size_t rows;
    size_t cols;
    size_t window;
    size_t first_row;
    size_t last_row;
    pid_t tid;
} Worker;

int filter_pass(Worker *workers, size_t count);

#endif
