#include "matrix.h"
#include "debug.h"
#include "stdlib.h"

matrix* create_matrix(int rows, int columns, double minValue, double maxValue)
{
    if(rows < 0 || columns < 0 || minValue > maxValue)
    {
        DEBUG("Failed to create matrix. (%d >= 0, %d >= 0, %f < %f)", rows, columns, minValue, maxValue);
        return NULL;
    }

    matrix* newMatrix = calloc(1, sizeof(matrix));
    if(newMatrix == NULL) return NULL;

    newMatrix->rows = rows;
    newMatrix->columns = columns;
    newMatrix->data = calloc((rows*columns), sizeof(int));

    if(newMatrix->data == NULL)
    {
        free(newMatrix);
        return NULL;
    }

    for(int i = 0; i < rows; i++)
    {
        for(int j = 0; j < columns; j++)
        {
            double newValue;
            (minValue == maxValue)?(newValue = minValue):(newValue = (minValue + ((double)rand() / (double)RAND_MAX)*(maxValue-minValue)));
            set_element(newMatrix, i, j, newValue);
        }
    }

    return newMatrix;
}

void destroy_matrix(matrix* m)
{
    if(m)
    {
        if(m->data) free(m->data);
        free(m);
    }
}

void print_matrix(matrix* m)
{
    if(m == NULL)
    {
        DEBUG("Cannot print NULL matrix")
        return NULL;
    }

    printf("% ");
    for(int i = 0; i < m->columns; i++) printf("- ");
    printf("%\n");

    for(int i = 0; i < m->rows; i++)
    {
        printf("| ");
        for(int j = 0; j < m->columns; j++)
        {
            printf("%d ", get_element(m, i, j));
        }
        printf("|\n");
    }

    printf("% ");
    for(int i = 0; i < m->columns; i++) printf("- ");
    printf("%\n");
}

double get_element(matrix* m, int row, int column)
{
    if(m == NULL)
    {
        DEBUG("Cannot get element of NULL matrix")
        return NULL;
    }
    else if(row < 0 || column < 0 || row >= m->rows || column >= m->columns)
    {
        DEBUG("Cannot get element of invalid matrix indices. ([%d, %d] from matrix of dimensions [%d, %d])", row, column, m->rows, m->columns)
        return NULL;
    }
    else if(m->data == NULL)
    {
        DEBUG("Cannot get element of matrix with uninitialized data.")
        return NULL;
    }

    return *(m->data + (row*m->columns) + column);
}

void set_element(matrix* m, int row, int column, double value)
{
    if(m == NULL)
    {
        DEBUG("Cannot set element of NULL matrix")
        return NULL;
    }
    else if(row < 0 || column < 0 || row >= m->rows || column >= m->columns)
    {
        DEBUG("Cannot set element of invalid matrix indices. ([%d, %d] from matrix of dimensions [%d, %d])", row, column, m->rows, m->columns)
        return NULL;
    }
    else if(m->data == NULL)
    {
        DEBUG("Cannot set element of matrix with uninitialized data.")
        return NULL;
    }

    *(m->data + (row*m->columns) + column) = value;
}

void transpose(matrix** m)
{
    matrix* newMatrix = create_matrix((*m)->columns, (*m)->rows, 0, 0);
    for(int i = 0; i < (*m)->rows; i++)
    {
        for(int j = 0; j < (*m)->columns; j++)
        {
            set_element(newMatrix, j, i, get_element((*m), i, j));
        }
    }
    free((*m));
    *m = newMatrix;
}

int can_multiply(matrix* m1, matrix* m2)
{
    return (m1 != NULL && m2 != NULL && m1->columns == m2->rows);
}

int equal_matrix_dimensions(matrix* m1, matrix* m2)
{
    return (m1 != NULL && m2 != NULL && m1->columns == m2->columns && m1->rows == m2->rows);
}

matrix* matrix_multiply(matrix* m1, matrix* m2)
{
    if(!can_multiply(m1, m2))
    {
        DEBUG("Matrix dimensions do not allow multiplication")
        return NULL;
    }

    matrix* product = create_matrix(m1->rows, m2->columns, 0, 0);

    for(int i = 0; i < product->rows; i++)
    {
        for(int j = 0; j < product->columns; j++)
        {
            double newValue = 0;

            for(int k = 0; k < m1->columns; k++)
            {
                newValue+= get_element(m1, i, k)* get_element(m2, k, j);
            }

            set_element(product, i, j, newValue);
        }
    }

    return product;
}

matrix* elementwise_multiply(matrix* m1, matrix* m2)
{
    if(!equal_matrix_dimensions(m1, m2))
    {
        DEBUG("Matrix dimensions do not allow elementwise multiplication")
        return NULL;
    }

    matrix* product = create_matrix(m1->rows, m2->columns, 0, 0);

    for(int i = 0; i < product->rows; i++)
    {
        for(int j = 0; j < product->columns; j++)
        {
            set_element(product, i, j, (get_element(m1, i, j) * get_element(m2, i, j)));
        }
    }

    return product;
}

matrix* matrix_add(matrix* m1, matrix* m2)
{
    if(!equal_matrix_dimensions(m1, m2))
    {
        DEBUG("Matrix dimensions do not allow addition")
        return NULL;
    }

    matrix* sum = create_matrix(m1->rows, m2->columns, 0, 0);

    for(int i = 0; i < sum->rows; i++)
    {
        for(int j = 0; j < sum->columns; j++)
        {
            set_element(sum, i, j, (get_element(m1, i, j) + get_element(m2, i, j)));
        }
    }

    return sum;
}

matrix* matrix_subtract(matrix* m1, matrix* m2)
{
    if(!equal_matrix_dimensions(m1, m2))
    {
        DEBUG("Matrix dimensions do not allow addition")
        return NULL;
    }

    matrix* sum = create_matrix(m1->rows, m2->columns, 0, 0);

    for(int i = 0; i < sum->rows; i++)
    {
        for(int j = 0; j < sum->columns; j++)
        {
            set_element(sum, i, j, (get_element(m1, i, j) - get_element(m2, i, j)));
        }
    }

    return sum;
}

matrix* scalar_add(matrix* m1, double scalar)
{
    matrix* sum = create_matrix(m1->rows, m1->columns, 0, 0);

    for(int i = 0; i < sum->rows; i++)
    {
        for(int j = 0; j < sum->columns; j++)
        {
            set_element(sum, i, j, (get_element(m1, i, j) + scalar));
        }
    }

    return sum;
}

matrix* scalar_multiply(matrix* m1, double scalar)
{
    matrix* product = create_matrix(m1->rows, m1->columns, 0, 0);

    for(int i = 0; i < product->rows; i++)
    {
        for(int j = 0; j < product->columns; j++)
        {
            set_element(product, i, j, (get_element(m1, i, j) * scalar));
        }
    }

    return product;
}

void matrix_map(matrix* m, double (*func)(double))
{
    if(m == NULL)
    {
        DEBUG("Cannot map over elements of NULL matrix")
        return NULL;
    }

    for(int i = 0; i < m->rows; i++)
    {
        for(int j = 0; j < m->columns; j++)
        {
            set_element(m, i, j, (*func)(get_element(m, i, j)));
        }
    }
}