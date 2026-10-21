#define _GNU_SOURCE
#include "filter.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int compare_int64(const void *left, const void *right)
{
    int64_t a = *(const int64_t *)left;
    int64_t b = *(const int64_t *)right;
    return (a > b) - (a < b);
}

// matrix edge processing
static size_t clamped_index(size_t center, size_t offset, size_t radius, size_t limit)
{
    if (offset < radius) {
        size_t distance = radius - offset;
        return center < distance ? 0 : center - distance;
    }
    size_t distance = offset - radius;
    return distance >= limit - 1 - center ? limit - 1 : center + distance;
}

static void *worker_main(void *argument)
{
    Worker *work = argument;
    size_t radius = work->window / 2;
    size_t sample_count = work->window * work->window;
    int64_t *samples = malloc(sample_count * sizeof(*samples));
    work->tid = gettid();

    if (samples == NULL)
        pthread_exit(work);

    for (size_t row = work->first_row; row < work->last_row; ++row) {
        for (size_t col = 0; col < work->cols; ++col) {
            size_t used = 0;
            for (size_t kr = 0; kr < work->window; ++kr) {
                size_t source_row = clamped_index(row, kr, radius, work->rows);
                for (size_t kc = 0; kc < work->window; ++kc) {
                    size_t source_col = clamped_index(col, kc, radius, work->cols);
                    samples[used++] = work->input[source_row * work->cols + source_col];
                }
            }
            qsort(samples, sample_count, sizeof(*samples), compare_int64);
            work->output[row * work->cols + col] = samples[sample_count / 2];
        }
    }
    free(samples);
    pthread_exit(NULL);
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
        void *thread_result = NULL;
        int error = pthread_join(threads[i], &thread_result);
        if (error != 0) {
            fprintf(stderr, "pthread_join: %s\n", strerror(error));
            failed = 1;
        } else if (thread_result != NULL) {
            fprintf(stderr, "Cannot allocate median window for worker %zu\n", i + 1);
            failed = 1;
        }
    }
    free(threads);
    return failed ? -1 : 0;
}
