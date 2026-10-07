#include "filter.h"
#include "benchmark.h"
#include "matrix_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
