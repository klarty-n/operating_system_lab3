#ifndef MATRIX_IO_H
#define MATRIX_IO_H

#include <stddef.h>
#include <stdint.h>

int read_matrix(size_t *rows, size_t *cols, int64_t **matrix);

#endif
