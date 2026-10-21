#include "matrix_io.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int read_matrix(size_t *rows, size_t *cols, int64_t **matrix)
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
    if (!failed && (*rows > SIZE_MAX / *cols || *rows * *cols > SIZE_MAX / sizeof(**matrix))) {
        fprintf(stderr, "Matrix is too large\n");
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
        if (fscanf(file, "%" SCNd64, &(*matrix)[i]) != 1) {
            fprintf(stderr, "Invalid matrix value at position %zu\n", i);
            failed = 1;
        }
    }

    if (fclose(file) != 0) {
        perror("Closing input.txt");
        failed = 1;
    }
    if (failed) {
        free(*matrix);
        *matrix = NULL;
    }
    return failed ? -1 : 0;
}
