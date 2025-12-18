#include "neuralnet.h"
#include <math.h>
#include "debug.h"

void init_neuralnet()
{
    weights1 = create_matrix(INPUT_NODES, HIDDEN_LAYER1_NODES, -0.1, 0.1);
    weights2 = create_matrix(HIDDEN_LAYER1_NODES, HIDDEN_LAYER2_NODES, -0.1, 0.1);
    weights3 = create_matrix(HIDDEN_LAYER2_NODES, HIDDEN_LAYER3_NODES, -0.1, 0.1);
    weights4 = create_matrix(HIDDEN_LAYER3_NODES, OUTPUT_NODES, -0.1, 0.1);
}

double net_error(matrix* output, matrix* expected_output)
{
    if(!equal_matrix_dimensions(output, expected_output) || output->columns > 0 || expected_output->columns > 0)
    {
        DEBUG("Cannot find net error.\n\toutput: %dx%d\n\texpected: %dx%d\n", output->rows, output->columns, expected_output->rows, expected_output->columns)
        return NULL;
    }

    double error = 0;
    for(int i = 0; i < output->rows; i++)
    {
        error+= pow((get_element(expected_output, i, 0) - get_element(output, i, 0)), 2);
    }
    return 0.5 * error;
}

double relu(double x)
{
    return fmin(fmax(0, x), 1);
}

matrix* forward_propagate(matrix* input)
{
    if(input->columns != 1 || input->rows != INPUT_NODES)
    {
        DEBUG("Cannot forward propagate.\n\tinput: %dx%d\n\texpected: %dx%d\n", input->rows, input->columns, 1, INPUT_NODES)
        return NULL;
    }

    matrix* hidden1 = matrix_multiply(input, weights1);
    matrix_map(hidden1, relu);

    matrix* hidden2 = matrix_multiply(hidden1, weights2);
    matrix_map(hidden2, relu);
    hidden_destroy(hidden1);

    matrix* hidden3 = matrix_multiply(hidden2, weights3);
    matrix_map(hidden3, relu);
    matrix_destroy(hidden2);

    matrix* output = matrix_multiply(hidden3, weights4);
    matrix_map(output, relu);
    matrix_destroy(hidden3);

    return output;
}

void back_propagate()
{

}