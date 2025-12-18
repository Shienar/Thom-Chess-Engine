#ifndef MATRIX
#define MATRIX

typedef struct {
    int rows;
    int columns;
    double* data;
} matrix;

/**
 * Creates and allocates a new matrix with the specified dimensions.
 * Matrix values are initialized randomly between minValue and maxValue.
 * If minValue == maxValue, they are initialized exactly to minValue/maxValue.
 * Rows >= 0
 * Columns >= 0
 * minValue <= maxValue
 */
matrix* create_matrix(int rows, int columns, double minValue, double maxValue);

void destroy_matrix(matrix* m);

/**
 * Prints the elements of a matrix.
 * matrix != NULL
 */
void print_matrix(matrix* m);

/**
 * Returns a matrix element
 * 
 * A matrix with 3 columns and 3 rows
 * has indexes ranging from (0,0) to (2,2)
 * 
 * m != NULL
 * 0 <= row < matrix->rows
 * 0 <= column < matrix->columns
 */
double get_element(matrix* m, int row, int column);

/**
 * Sets a matrix element
 * 
 * A matrix with 3 columns and 3 rows
 * has indexes ranging from (0,0) to (2,2)
 * 
 * m != NULL
 * 0 <= row < matrix->rows
 * 0 <= column < matrix->columns
 */
void set_element(matrix* m, int row, int column, double value);

/**
 * Given a matrix with x rows and y columns,
 * returns a matrix with y rows and x columns 
 * with transposed elements.
 * 
 * m != NULL
 */
void transpose(matrix* m);

/**
 * Returns 1 if the matrices can be multiplied together.
 * Else, returns 0
 */
int can_multiply(matrix* m1, matrix* m2);

/**
 * Returns 1 if the matrices have equal dimensions.
 * Else, returns 0
 */
int equal_matrix_dimensions(matrix* m1, matrix* m2);

/**
 * Creates and allocates a new matrix as a product of
 * the two matrixes provided.
 */
matrix* matrix_multiply(matrix* m1, matrix* m2);

/**
 * Element-by-element multiplication of two matrixes
 * with equal dimensions.
 */
matrix* elementwise_multiply(matrix* m1, matrix* m2);

/**
 * Element-by-element addition of two matrixes
 * with equal dimensions.
 */
matrix* matrix_add(matrix* m1, matrix* m2);

/**
 * Element-by-element subtraction of two matrixes
 * with equal dimensions.
 * m1 - m2
 */
matrix* matrix_subtract(matrix* m1, matrix* m2);

/**
 * Adds a scalar to each matrix element
 * returns a new matrix
 */
matrix* scalar_add(matrix* m1, double scalar);

/**
 * Multiplies a scalar to each matrix element
 * returns a new matrix
 */
matrix* scalar_multiply(matrix* m1, double scalar);

/**
 * For every element of the matrix, pass it in as input
 * to the given function and replace it with the output.
 */
matrix* matrix_map(matrix* m, double (*f)(double));

#endif