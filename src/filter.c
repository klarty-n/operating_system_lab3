#define _GNU_SOURCE
#include "filter.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void *worker_main(void *argument)
// function that applies a matrix filter to a part of the input matrix (in arguments we set rows that will be processed by this worker) and puts the result in the output matrix. The function was made to be run in a separate thread for each worker
{
    Worker *work = argument;
    size_t radius = work->window / 2;
    double weight = 1.0 / (double)(work->window * work->window);
    work->tid = gettid();

    for (size_t row = work->first_row; row < work->last_row; ++row) {
        for (size_t col = 0; col < work->cols; ++col) {
            double sum = 0.0;
            for (size_t kr = 0; kr < work->window; ++kr) {
                if (row + kr < radius || row + kr - radius >= work->rows)
                    continue; 
                size_t source_row = row + kr - radius;
                for (size_t kc = 0; kc < work->window; ++kc) {
                    if (col + kc < radius || col + kc - radius >= work->cols)
                        continue;
                    size_t source_col = col + kc - radius;
                    sum += work->input[source_row * work->cols + source_col] * weight;
                }
            }
            work->output[row * work->cols + col] = sum;
        }
    }
    return NULL;
}

int filter_pass(Worker *workers, size_t count)
{
    pthread_t *threads = malloc(count * sizeof(*threads));
    if (threads == NULL) {
        perror("malloc threads");
        return -1;
    }

    size_t created = 0;
    int failed = 0;
    for (; created < count; ++created) {
        int error = pthread_create(&threads[created], NULL, worker_main, &workers[created]);
        if (error != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(error));
            failed = 1;
            break;
        }
    }
    for (size_t i = 0; i < created; ++i) {
        int error = pthread_join(threads[i], NULL);
        if (error != 0) {
            fprintf(stderr, "pthread_join: %s\n", strerror(error));
            failed = 1;
        }
    }
    free(threads);
    return failed ? -1 : 0;
}
