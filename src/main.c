#include "filter.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int read_matrix(size_t *rows, size_t *cols, double **matrix)
{
    FILE *file = fopen("input.txt", "r");
    if (file == NULL) {
        perror("input.txt");
        return -1;
    }

    int failed = 0;
    if (fscanf(file, "%zu %zu", rows, cols) != 2 || *rows == 0 || *cols == 0) {
        fprintf(stderr, "Invalid matrix size in input.txt\n");
        failed = 1;
    }
    if (!failed) {
        *matrix = malloc(*rows * *cols * sizeof(**matrix));
        if (*matrix == NULL) {
            perror("malloc matrix");
            failed = 1;
        }
    }
    for (size_t i = 0; !failed && i < *rows * *cols; ++i) {
        if (fscanf(file, "%lf", &(*matrix)[i]) != 1) {
            fprintf(stderr, "Invalid matrix value at position %zu\n", i);
            failed = 1;
        }
    }

    if (fclose(file) != 0) {
        perror("closing input.txt");
        failed = 1;
    }
    if (failed) {
        free(*matrix);
        *matrix = NULL;
    }
    return failed ? -1 : 0;
}

static double seconds_between(struct timespec start, struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
}

static int run_timed_pass(Worker *workers, size_t worker_count, double *total_seconds)
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

static int measure_one_worker(const double *input, double *output, size_t rows, size_t cols, size_t window, size_t iterations, double *seconds)
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
        Worker worker = {.input = source, .output = target, .rows = rows, .cols = cols, .window = window, .first_row = 0, .last_row = rows
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

int main(int argc, char **argv)
{
    size_t limit;

    if (argc != 3 || strcmp(argv[1], "-t") != 0 || argv[2][0] == '-' ||
        sscanf(argv[2], "%zu", &limit) != 1 || limit == 0) {
        fprintf(stderr, "Invalid input\n");
        return EXIT_FAILURE;
    }

    size_t rows = 0;
    size_t cols = 0;

    double *input = NULL;
    if (read_matrix(&rows, &cols, &input) != 0)
        return EXIT_FAILURE;

    size_t window, iterations;

    printf("Enter window size:\n");
    if (scanf("%zu", &window) != 1 || window <= 0 || window > rows ||
        window > cols || window % 2 == 0) {
        fprintf(stderr, "Window must be odd and not larger than the matrix\n");
        free(input);
        return EXIT_FAILURE;
    }

    printf("Enter number of filter applications:\n");
    if (scanf("%zu", &iterations) != 1 || iterations <= 0) {
        fprintf(stderr, "K must be a positive number\n");
        free(input);
        return EXIT_FAILURE;
    }

    size_t count = rows * cols;
    // because we can have more workers than rows, we limit the number of workers to the number of rows
    size_t worker_count = limit < rows ? limit : rows;

    double *output = malloc(count * sizeof(*output));
    Worker *workers = malloc(worker_count * sizeof(*workers));

    if (output == NULL || workers == NULL) {
        perror("malloc");
        free(input); free(output); free(workers);
        return EXIT_FAILURE;
    }

    double one_worker_seconds = 0.0;
    if (worker_count > 1 && measure_one_worker(input, output, rows, cols, window, iterations, &one_worker_seconds) != 0) {
        free(input); free(output); free(workers);
        return EXIT_FAILURE;
    }

    FILE *output_file = fopen("output.txt", "a");
    if (output_file == NULL) {
        perror("output.txt");
        free(input); free(output); free(workers);
        return EXIT_FAILURE;
    }

    fprintf(output_file, "\nMatrix %zu x %zu, window = %zu, K = %zu, workers = %zu\n", rows, cols, window, iterations, worker_count);

    double total_seconds = 0.0;
    int failed = 0;

    for (size_t pass = 1; pass <= iterations; ++pass) {
        for (size_t i = 0; i < worker_count; ++i) {
            workers[i] = (Worker){
                .input = input, .output = output, .rows = rows, .cols = cols, .window = window,
                .first_row = i * rows / worker_count, .last_row = (i + 1) * rows / worker_count
            };
        }

        if (run_timed_pass(workers, worker_count, &total_seconds) != 0) {
            failed = 1;
            break;
        }

        fprintf(output_file, "Pass %zu:\n", pass);
        for (size_t row = 0; row < rows; ++row) {
            for (size_t col = 0; col < cols; ++col)
                fprintf(output_file, "%.6f%c", output[row * cols + col], col + 1 == cols ? '\n' : ' ');
        }

        if (ferror(output_file)) {
            perror("writing output.txt");
            failed = 1;
            break;
        }

        double *temporary = input;
        input = output;
        output = temporary;
    }

    if (!failed) {
        if (worker_count == 1)
            one_worker_seconds = total_seconds;
        if (total_seconds <= 0.0) {
            fprintf(stderr, "Measured time is too short\n");
            failed = 1;
        } else {
            double speedup = one_worker_seconds / total_seconds;
            double efficiency = speedup / (double)worker_count;
            fprintf(output_file, "T1: %.9f s; T%zu: %.9f s\nSpeedup: %.6f; Efficiency: %.6f\n", one_worker_seconds, worker_count, total_seconds,                  speedup, efficiency);

            for (size_t i = 0; i < worker_count; ++i)
                printf("Worker %zu: PID=%ld TID=%ld\n",
                       i + 1, (long)getpid(), (long)workers[i].tid);
        }
        if (ferror(output_file)) {
            perror("writing output.txt");
            failed = 1;
        }
    }
    if (fclose(output_file) != 0) {
        perror("closing output.txt");
        failed = 1;
    }

    free(input); 
    free(output); 
    free(workers);
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
