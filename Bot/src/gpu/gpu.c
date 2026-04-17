#include "gpu_funcs.h"

//The link below contains a lot of good info on this topic.
//https://ics-websites.science.uu.nl/docs/vakken/mov/2019/files/OptmzdSummary%20-%20lecture4%20-%20OpenCL.pdf 

openCLContext opencl_context = {
    .platform = 0,
    .device = 0,
    .context = NULL,
    .queue = NULL,
    .program = NULL,
    .calculateAccumulator = NULL,
    .calculateH2 = NULL,
    .calculateH3 = NULL,
    .calculateOutput = NULL,
    .calculateDelta4 = NULL,
    .calculateDelta3 = NULL,
    .calculateDelta2 = NULL,
    .calculateDelta1 = NULL,
    .calculateGradient4 = NULL,
    .calculateGradient3 = NULL,
    .calculateGradient2 = NULL,
    .calculateGradient1 = NULL
};

openCLKernelMemory opencl_mem = {NULL};

char* getKernelFunctions()
{
    FILE* input = fopen("./src/gpu/kernels.cl", "rb"); 
    if(!input)
    {
        DEBUG("Cannot locate GPU kernel functions.");
        exit(EXIT_FAILURE);
    }
    
    fseek(input, 0, SEEK_END);
    size_t size = ftell(input);
    rewind(input);

    char* source = CALLOC(size + 1, sizeof(char));

    size_t bytesRead = fread(source, 1, size, input);
    source[bytesRead] = '\0';
    
    fclose(input);

    return source;
}

int initOpenCL(network_weights_training* trainingNNUE)
{
    int err = 0;

    err = clGetPlatformIDs(1, &opencl_context.platform, NULL);
    err = clGetDeviceIDs(opencl_context.platform, CL_DEVICE_TYPE_GPU, 1, &opencl_context.device, NULL);
    opencl_context.context = clCreateContext(NULL, 1, &opencl_context.device, NULL, NULL, &err);
    
    opencl_context.queue = clCreateCommandQueueWithProperties(opencl_context.context, opencl_context.device, NULL, &err);
    
    char* kernelSource = getKernelFunctions();
    opencl_context.program = clCreateProgramWithSource(opencl_context.context, 1, (const char**)&kernelSource, NULL, &err);
    FREE(kernelSource);
    kernelSource = NULL;

    err = clBuildProgram(opencl_context.program, 1, &opencl_context.device, NULL, NULL, NULL);
    
    if (err) {
        char log[4096];
        clGetProgramBuildInfo(opencl_context.program, opencl_context.device, CL_PROGRAM_BUILD_LOG, 4096, log, NULL);
        printf("Build Error:\n%s\n", log);
        return err;
    }

    opencl_context.calculateAccumulator = clCreateKernel(opencl_context.program, "calculateAccumulator", &err);
    opencl_context.calculateH2 = clCreateKernel(opencl_context.program, "calculateH2", &err);
    opencl_context.calculateH3 = clCreateKernel(opencl_context.program, "calculateH3", &err);
    opencl_context.calculateOutput = clCreateKernel(opencl_context.program, "calculateOutput", &err);
    opencl_context.calculateDelta4 = clCreateKernel(opencl_context.program, "calculateDelta4", &err);
    opencl_context.calculateDelta3 = clCreateKernel(opencl_context.program, "calculateDelta3", &err);
    opencl_context.calculateDelta2 = clCreateKernel(opencl_context.program, "calculateDelta2", &err);
    opencl_context.calculateDelta1 = clCreateKernel(opencl_context.program, "calculateDelta1", &err);
    opencl_context.calculateGradient4 = clCreateKernel(opencl_context.program, "calculateGradient4", &err);
    opencl_context.calculateGradient3 = clCreateKernel(opencl_context.program, "calculateGradient3", &err);
    opencl_context.calculateGradient2 = clCreateKernel(opencl_context.program, "calculateGradient2", &err);
    opencl_context.calculateGradient1 = clCreateKernel(opencl_context.program, "calculateGradient1", &err);
    opencl_context.adam = clCreateKernel(opencl_context.program, "adam", &err);

    
    opencl_mem.activeInputs = clCreateBuffer(opencl_context.context, CL_MEM_READ_ONLY, POSITIONS_PER_FILE * 64 * sizeof(short), NULL, NULL);
    opencl_mem.activeCount = clCreateBuffer(opencl_context.context, CL_MEM_READ_ONLY, POSITIONS_PER_FILE * sizeof(char), NULL, NULL);

    opencl_mem.weights1 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS, NULL, NULL);
    opencl_mem.weights2 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * SECOND_HIDDEN_LAYER_NODES * ACCUMULATOR_NODES, NULL, NULL);
    opencl_mem.weights3 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * THIRD_HIDDEN_LAYER_NODES * SECOND_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.weights4 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, NULL, NULL);

    opencl_mem.bias1 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE, NULL, NULL);
    opencl_mem.bias2 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * SECOND_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.bias3 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.bias4 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float), NULL, NULL);

    opencl_mem.accumulatorOutput = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, POSITIONS_PER_FILE * sizeof(float) * ACCUMULATOR_NODES, NULL, NULL);
    opencl_mem.h2Output = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, POSITIONS_PER_FILE * sizeof(float) * SECOND_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.h3Output = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, POSITIONS_PER_FILE * sizeof(float) * THIRD_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.finalOutput = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, POSITIONS_PER_FILE * sizeof(float), NULL, NULL);
    opencl_mem.expectedOutput = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, POSITIONS_PER_FILE * sizeof(float), NULL, NULL);
    
    opencl_mem.sumsquarederror = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(double), NULL, NULL);

    opencl_mem.delta4 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * POSITIONS_PER_FILE, NULL, NULL);
    opencl_mem.delta3 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * POSITIONS_PER_FILE * THIRD_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.delta2 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * POSITIONS_PER_FILE * SECOND_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.delta1 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * POSITIONS_PER_FILE * ACCUMULATOR_NODES, NULL, NULL);

    
    opencl_mem.gradient1Sum = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS, NULL, NULL);
    opencl_mem.gradientBias1Sum = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE, NULL, NULL);

    opencl_mem.gradient2Sum = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * SECOND_HIDDEN_LAYER_NODES * ACCUMULATOR_NODES, NULL, NULL);
    opencl_mem.gradientBias2Sum = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * SECOND_HIDDEN_LAYER_NODES, NULL, NULL);

    opencl_mem.gradient3Sum = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * THIRD_HIDDEN_LAYER_NODES * SECOND_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.gradientBias3Sum = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, NULL, NULL);

    opencl_mem.gradient4Sum = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.gradientBias4Sum = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float), NULL, NULL);

    
    opencl_mem.m_weights1 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS, NULL, NULL);
    opencl_mem.m_weights2 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * SECOND_HIDDEN_LAYER_NODES * ACCUMULATOR_NODES, NULL, NULL);
    opencl_mem.m_weights3 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * THIRD_HIDDEN_LAYER_NODES * SECOND_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.m_weights4 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, NULL, NULL);
    
    opencl_mem.m_bias1 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE, NULL, NULL);
    opencl_mem.m_bias2 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * SECOND_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.m_bias3 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.m_bias4 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float), NULL, NULL);
    
    opencl_mem.v_weights1 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS, NULL, NULL);
    opencl_mem.v_weights2 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * SECOND_HIDDEN_LAYER_NODES * ACCUMULATOR_NODES, NULL, NULL);
    opencl_mem.v_weights3 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * THIRD_HIDDEN_LAYER_NODES * SECOND_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.v_weights4 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, NULL, NULL);
    
    opencl_mem.v_bias1 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE, NULL, NULL);
    opencl_mem.v_bias2 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * SECOND_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.v_bias3 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.v_bias4 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float), NULL, NULL);

    float* transposedWeight = NULL;
    transposedWeight = CALLOC(HALF_INPUT_BITS * ACCUMULATOR_NODES_PER_SIDE, sizeof(float));
    for (int i = 0; i < HALF_INPUT_BITS; i++) {
        for (int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++) {
            // CPU [output][input] -> GPU [input][output]
            transposedWeight[i * ACCUMULATOR_NODES_PER_SIDE + j] = trainingNNUE->weights1[j][i];
        }
    }
    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.weights1, CL_TRUE, 0, HALF_INPUT_BITS * ACCUMULATOR_NODES_PER_SIDE * sizeof(float), (void*)transposedWeight, 0, NULL, NULL);
    FREE(transposedWeight);
    
    transposedWeight = CALLOC(ACCUMULATOR_NODES * SECOND_HIDDEN_LAYER_NODES, sizeof(float));
    for (int i = 0; i < ACCUMULATOR_NODES; i++) {
        for (int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++) {
            // CPU [output][input] -> GPU [input][output]
            transposedWeight[i * SECOND_HIDDEN_LAYER_NODES + j] = trainingNNUE->weights2[j][i];
        }
    }
    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.weights2, CL_TRUE, 0, ACCUMULATOR_NODES * SECOND_HIDDEN_LAYER_NODES * sizeof(float), (void*)transposedWeight, 0, NULL, NULL);
    FREE(transposedWeight);
    
    transposedWeight = CALLOC(SECOND_HIDDEN_LAYER_NODES * THIRD_HIDDEN_LAYER_NODES, sizeof(float));
    for (int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++) {
        for (int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++) {
            // CPU [output][input] -> GPU [input][output]
            transposedWeight[i * THIRD_HIDDEN_LAYER_NODES + j] = trainingNNUE->weights3[j][i];
        }
    }
    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.weights3, CL_TRUE, 0, SECOND_HIDDEN_LAYER_NODES * THIRD_HIDDEN_LAYER_NODES * sizeof(float), (void*)transposedWeight, 0, NULL, NULL);
    FREE(transposedWeight);
    
    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.weights4, CL_TRUE, 0, THIRD_HIDDEN_LAYER_NODES * sizeof(float), (void*)&trainingNNUE->weights4, 0, NULL, NULL);

    
    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.bias1, CL_TRUE, 0, ACCUMULATOR_NODES_PER_SIDE * sizeof(float), (void*)&trainingNNUE->weights1_bias, 0, NULL, NULL);
    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.bias2, CL_TRUE, 0, SECOND_HIDDEN_LAYER_NODES * sizeof(float), (void*)&trainingNNUE->weights2_bias, 0, NULL, NULL);
    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.bias3, CL_TRUE, 0, THIRD_HIDDEN_LAYER_NODES * sizeof(float), (void*)&trainingNNUE->weights3_bias, 0, NULL, NULL);
    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.bias4, CL_TRUE, 0, OUTPUT_LAYER_NODES * sizeof(float), (void*)&trainingNNUE->weights4_bias, 0, NULL, NULL);
    
    float zero = 0.0f;
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.m_weights1, &zero, sizeof(float), 0, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.m_weights2, &zero, sizeof(float), 0, sizeof(float) * SECOND_HIDDEN_LAYER_NODES * ACCUMULATOR_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.m_weights3, &zero, sizeof(float), 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES * SECOND_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.m_weights4, &zero, sizeof(float), 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, 0, NULL, NULL);

    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.m_bias1, &zero, sizeof(float), 0, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.m_bias2, &zero, sizeof(float), 0, sizeof(float) * SECOND_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.m_bias3, &zero, sizeof(float), 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.m_bias4, &zero, sizeof(float), 0, sizeof(float), 0, NULL, NULL);

    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.v_weights1, &zero, sizeof(float), 0, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.v_weights2, &zero, sizeof(float), 0, sizeof(float) * SECOND_HIDDEN_LAYER_NODES * ACCUMULATOR_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.v_weights3, &zero, sizeof(float), 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES * SECOND_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.v_weights4, &zero, sizeof(float), 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, 0, NULL, NULL);

    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.v_bias1, &zero, sizeof(float), 0, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.v_bias2, &zero, sizeof(float), 0, sizeof(float) * SECOND_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.v_bias3, &zero, sizeof(float), 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.v_bias4, &zero, sizeof(float), 0, sizeof(float), 0, NULL, NULL);

    clSetKernelArg(opencl_context.calculateAccumulator, 0, sizeof(cl_mem), &opencl_mem.activeInputs);
    clSetKernelArg(opencl_context.calculateAccumulator, 1, sizeof(cl_mem), &opencl_mem.activeCount);
    clSetKernelArg(opencl_context.calculateAccumulator, 2, sizeof(cl_mem), &opencl_mem.weights1);
    clSetKernelArg(opencl_context.calculateAccumulator, 3, sizeof(cl_mem), &opencl_mem.bias1);
    clSetKernelArg(opencl_context.calculateAccumulator, 4, sizeof(cl_mem), &opencl_mem.accumulatorOutput);

    clSetKernelArg(opencl_context.calculateH2, 0, sizeof(cl_mem), &opencl_mem.accumulatorOutput);
    clSetKernelArg(opencl_context.calculateH2, 1, sizeof(cl_mem), &opencl_mem.weights2);
    clSetKernelArg(opencl_context.calculateH2, 2, sizeof(cl_mem), &opencl_mem.bias2);
    clSetKernelArg(opencl_context.calculateH2, 3, sizeof(cl_mem), &opencl_mem.h2Output);

    clSetKernelArg(opencl_context.calculateH3, 0, sizeof(cl_mem), &opencl_mem.h2Output);
    clSetKernelArg(opencl_context.calculateH3, 1, sizeof(cl_mem), &opencl_mem.weights3);
    clSetKernelArg(opencl_context.calculateH3, 2, sizeof(cl_mem), &opencl_mem.bias3);
    clSetKernelArg(opencl_context.calculateH3, 3, sizeof(cl_mem), &opencl_mem.h3Output);

    clSetKernelArg(opencl_context.calculateOutput, 0, sizeof(cl_mem), &opencl_mem.h3Output);
    clSetKernelArg(opencl_context.calculateOutput, 1, sizeof(cl_mem), &opencl_mem.weights4);
    clSetKernelArg(opencl_context.calculateOutput, 2, sizeof(cl_mem), &opencl_mem.bias4);
    clSetKernelArg(opencl_context.calculateOutput, 3, sizeof(cl_mem), &opencl_mem.finalOutput);

    clSetKernelArg(opencl_context.calculateDelta4, 0, sizeof(cl_mem), &opencl_mem.finalOutput);
    clSetKernelArg(opencl_context.calculateDelta4, 1, sizeof(cl_mem), &opencl_mem.expectedOutput);
    clSetKernelArg(opencl_context.calculateDelta4, 2, sizeof(cl_mem), &opencl_mem.delta4);
    clSetKernelArg(opencl_context.calculateDelta4, 3, sizeof(cl_mem), &opencl_mem.sumsquarederror);

    clSetKernelArg(opencl_context.calculateDelta3, 0, sizeof(cl_mem), &opencl_mem.delta4);
    clSetKernelArg(opencl_context.calculateDelta3, 1, sizeof(cl_mem), &opencl_mem.weights4);
    clSetKernelArg(opencl_context.calculateDelta3, 2, sizeof(cl_mem), &opencl_mem.h3Output);
    clSetKernelArg(opencl_context.calculateDelta3, 3, sizeof(cl_mem), &opencl_mem.delta3);

    clSetKernelArg(opencl_context.calculateDelta2, 0, sizeof(cl_mem), &opencl_mem.delta3);
    clSetKernelArg(opencl_context.calculateDelta2, 1, sizeof(cl_mem), &opencl_mem.weights3);
    clSetKernelArg(opencl_context.calculateDelta2, 2, sizeof(cl_mem), &opencl_mem.h2Output);
    clSetKernelArg(opencl_context.calculateDelta2, 3, sizeof(cl_mem), &opencl_mem.delta2);

    clSetKernelArg(opencl_context.calculateDelta1, 0, sizeof(cl_mem), &opencl_mem.delta2);
    clSetKernelArg(opencl_context.calculateDelta1, 1, sizeof(cl_mem), &opencl_mem.weights2);
    clSetKernelArg(opencl_context.calculateDelta1, 2, sizeof(cl_mem), &opencl_mem.accumulatorOutput);
    clSetKernelArg(opencl_context.calculateDelta1, 3, sizeof(cl_mem), &opencl_mem.delta1);

    clSetKernelArg(opencl_context.calculateGradient4, 0, sizeof(cl_mem), &opencl_mem.delta4);
    clSetKernelArg(opencl_context.calculateGradient4, 1, sizeof(cl_mem), &opencl_mem.h3Output);
    clSetKernelArg(opencl_context.calculateGradient4, 2, sizeof(cl_mem), &opencl_mem.gradient4Sum);
    clSetKernelArg(opencl_context.calculateGradient4, 3, sizeof(cl_mem), &opencl_mem.gradientBias4Sum);

    clSetKernelArg(opencl_context.calculateGradient3, 0, sizeof(cl_mem), &opencl_mem.delta3);
    clSetKernelArg(opencl_context.calculateGradient3, 1, sizeof(cl_mem), &opencl_mem.h2Output);
    clSetKernelArg(opencl_context.calculateGradient3, 2, sizeof(cl_mem), &opencl_mem.gradient3Sum);
    clSetKernelArg(opencl_context.calculateGradient3, 3, sizeof(cl_mem), &opencl_mem.gradientBias3Sum);

    clSetKernelArg(opencl_context.calculateGradient2, 0, sizeof(cl_mem), &opencl_mem.delta2);
    clSetKernelArg(opencl_context.calculateGradient2, 1, sizeof(cl_mem), &opencl_mem.accumulatorOutput);
    clSetKernelArg(opencl_context.calculateGradient2, 2, sizeof(cl_mem), &opencl_mem.gradient2Sum);
    clSetKernelArg(opencl_context.calculateGradient2, 3, sizeof(cl_mem), &opencl_mem.gradientBias2Sum);

    clSetKernelArg(opencl_context.calculateGradient1, 0, sizeof(cl_mem), &opencl_mem.activeInputs);
    clSetKernelArg(opencl_context.calculateGradient1, 1, sizeof(cl_mem), &opencl_mem.activeCount);
    clSetKernelArg(opencl_context.calculateGradient1, 2, sizeof(cl_mem), &opencl_mem.delta1);
    clSetKernelArg(opencl_context.calculateGradient1, 3, sizeof(cl_mem), &opencl_mem.gradient1Sum);
    clSetKernelArg(opencl_context.calculateGradient1, 4, sizeof(cl_mem), &opencl_mem.gradientBias1Sum);

    return (err == CL_SUCCESS);
}

void freeOpenCL()
{
    clReleaseMemObject(opencl_mem.activeInputs);
    clReleaseMemObject(opencl_mem.activeCount);
    clReleaseMemObject(opencl_mem.weights1);
    clReleaseMemObject(opencl_mem.weights2);
    clReleaseMemObject(opencl_mem.weights3);
    clReleaseMemObject(opencl_mem.weights4);
    clReleaseMemObject(opencl_mem.bias1);
    clReleaseMemObject(opencl_mem.bias2);
    clReleaseMemObject(opencl_mem.bias3);
    clReleaseMemObject(opencl_mem.bias4);
    clReleaseMemObject(opencl_mem.accumulatorOutput);
    clReleaseMemObject(opencl_mem.h2Output);
    clReleaseMemObject(opencl_mem.h3Output);
    clReleaseMemObject(opencl_mem.finalOutput);
    clReleaseMemObject(opencl_mem.expectedOutput);
    clReleaseMemObject(opencl_mem.sumsquarederror);
    clReleaseMemObject(opencl_mem.delta4);
    clReleaseMemObject(opencl_mem.delta3);
    clReleaseMemObject(opencl_mem.delta2);
    clReleaseMemObject(opencl_mem.delta1);
    clReleaseMemObject(opencl_mem.gradient1Sum);
    clReleaseMemObject(opencl_mem.gradient2Sum);
    clReleaseMemObject(opencl_mem.gradient3Sum);
    clReleaseMemObject(opencl_mem.gradient4Sum);
    clReleaseMemObject(opencl_mem.gradientBias1Sum);
    clReleaseMemObject(opencl_mem.gradientBias2Sum);
    clReleaseMemObject(opencl_mem.gradientBias3Sum);
    clReleaseMemObject(opencl_mem.gradientBias4Sum);
    clReleaseMemObject(opencl_mem.m_weights1);
    clReleaseMemObject(opencl_mem.m_weights2);
    clReleaseMemObject(opencl_mem.m_weights3);
    clReleaseMemObject(opencl_mem.m_weights4);
    clReleaseMemObject(opencl_mem.m_bias1);
    clReleaseMemObject(opencl_mem.m_bias2);
    clReleaseMemObject(opencl_mem.m_bias3);
    clReleaseMemObject(opencl_mem.m_bias4);
    clReleaseMemObject(opencl_mem.v_weights1);
    clReleaseMemObject(opencl_mem.v_weights2);
    clReleaseMemObject(opencl_mem.v_weights3);
    clReleaseMemObject(opencl_mem.v_weights4);
    clReleaseMemObject(opencl_mem.v_bias1);
    clReleaseMemObject(opencl_mem.v_bias2);
    clReleaseMemObject(opencl_mem.v_bias3);
    clReleaseMemObject(opencl_mem.v_bias4);

    if (opencl_context.calculateAccumulator)  
    {
        clReleaseKernel(opencl_context.calculateAccumulator);
        opencl_context.calculateAccumulator = NULL;
    }
    if (opencl_context.calculateH2)  
    {
        clReleaseKernel(opencl_context.calculateH2);
        opencl_context.calculateH2 = NULL;
    }
    if (opencl_context.calculateH3)  
    {
        clReleaseKernel(opencl_context.calculateH3);
        opencl_context.calculateH3 = NULL;
    }
    if (opencl_context.calculateOutput)  
    {
        clReleaseKernel(opencl_context.calculateOutput);
        opencl_context.calculateOutput = NULL;
    }
    if (opencl_context.calculateDelta4)  
    {
        clReleaseKernel(opencl_context.calculateDelta4);
        opencl_context.calculateDelta4 = NULL;
    }
    if (opencl_context.calculateDelta3)  
    {
        clReleaseKernel(opencl_context.calculateDelta3);
        opencl_context.calculateDelta3 = NULL;
    }
    if (opencl_context.calculateDelta2)  
    {
        clReleaseKernel(opencl_context.calculateDelta2);
        opencl_context.calculateDelta2 = NULL;
    }
    if (opencl_context.calculateDelta1)  
    {
        clReleaseKernel(opencl_context.calculateDelta1);
        opencl_context.calculateDelta1 = NULL;
    }
    if (opencl_context.calculateGradient4)  
    {
        clReleaseKernel(opencl_context.calculateGradient4);
        opencl_context.calculateGradient4 = NULL;
    }
    if (opencl_context.calculateGradient3)  
    {
        clReleaseKernel(opencl_context.calculateGradient3);
        opencl_context.calculateGradient3 = NULL;
    }
    if (opencl_context.calculateGradient2)  
    {
        clReleaseKernel(opencl_context.calculateGradient2);
        opencl_context.calculateGradient2 = NULL;
    }
    if (opencl_context.calculateGradient1)  
    {
        clReleaseKernel(opencl_context.calculateGradient1);
        opencl_context.calculateGradient1 = NULL;
    }
    if (opencl_context.adam)  
    {
        clReleaseKernel(opencl_context.adam);
        opencl_context.adam = NULL;
    }
    if (opencl_context.program) 
    {
        clReleaseProgram(opencl_context.program);
        opencl_context.program = NULL;
    }
    if (opencl_context.queue)   
    {
        clReleaseCommandQueue(opencl_context.queue);
        opencl_context.queue = NULL;
    }
    if (opencl_context.context) 
    {
        clReleaseContext(opencl_context.context);
        opencl_context.context = NULL;
    }
}

int timestamp = 0;
void enqueueKernels(short* activeInputs, char* activeCount, float* expectedOutputs, float learningRate)
{
    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.activeInputs, CL_FALSE, 0, POSITIONS_PER_FILE * 64 * sizeof(short), (void*)activeInputs, 0, NULL, NULL);
    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.activeCount, CL_FALSE, 0, POSITIONS_PER_FILE * sizeof(char), (void*)activeCount, 0, NULL, NULL);
    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.expectedOutput, CL_FALSE, 0, POSITIONS_PER_FILE * sizeof(float), (void*)expectedOutputs, 0, NULL, NULL);

    float zero = 0.0f;
    double zero_d = 0.0;
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.sumsquarederror, &zero_d, sizeof(double), 0, sizeof(double), 0, NULL, NULL);

    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.gradient1Sum, &zero, sizeof(float), 0, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.gradient2Sum, &zero, sizeof(float), 0, sizeof(float) * SECOND_HIDDEN_LAYER_NODES * ACCUMULATOR_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.gradient3Sum, &zero, sizeof(float), 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES * SECOND_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.gradient4Sum, &zero, sizeof(float), 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.gradientBias1Sum, &zero, sizeof(float), 0, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.gradientBias2Sum, &zero, sizeof(float), 0, sizeof(float) * SECOND_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.gradientBias3Sum, &zero, sizeof(float), 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.gradientBias4Sum, &zero, sizeof(float), 0, sizeof(float), 0, NULL, NULL);

    size_t calcAccumSize[2] = {POSITIONS_PER_FILE, ACCUMULATOR_NODES};
    size_t calcAccumSize_Local[2] = {1, 32};
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.calculateAccumulator, 2, NULL, calcAccumSize, calcAccumSize_Local, 0, NULL, NULL);
    
    size_t calcH2Size[2] = {POSITIONS_PER_FILE, SECOND_HIDDEN_LAYER_NODES};
    size_t calcH2Size_Local[2] = {1, 32};
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.calculateH2, 2, NULL, calcH2Size, calcH2Size_Local, 0, NULL, NULL);
    
    size_t calcH3Size[2] = {POSITIONS_PER_FILE, THIRD_HIDDEN_LAYER_NODES};
    size_t calcH3Size_Local[2] = {1, SECOND_HIDDEN_LAYER_NODES};
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.calculateH3, 2, NULL, calcH3Size, calcH3Size_Local, 0, NULL, NULL);

    size_t calcOutputSize[2] = {POSITIONS_PER_FILE, THIRD_HIDDEN_LAYER_NODES};
    size_t calcOutputSize_Local[2] = {1, 32};
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.calculateOutput, 2, NULL, calcOutputSize, calcOutputSize_Local, 0, NULL, NULL);
    
    size_t calcDelta4Size = POSITIONS_PER_FILE;
    size_t calcDelta4Size_Local = 64;
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.calculateDelta4, 1, NULL, &calcDelta4Size, &calcDelta4Size_Local, 0, NULL, NULL);

    size_t calcDelta3Size[2] = { POSITIONS_PER_FILE, THIRD_HIDDEN_LAYER_NODES };
    size_t calcDelta3Size_Local[2] = { 1, 32 };
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.calculateDelta3, 2, NULL, calcDelta3Size, calcDelta3Size_Local, 0, NULL, NULL);

    size_t calcDelta2Size[2] = { POSITIONS_PER_FILE, SECOND_HIDDEN_LAYER_NODES };
    size_t calcDelta2Size_Local[2] = { 1, 32 };
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.calculateDelta2, 2, NULL, calcDelta2Size, calcDelta2Size_Local, 0, NULL, NULL);

    size_t calcDelta1Size[2] = { POSITIONS_PER_FILE, ACCUMULATOR_NODES };
    size_t calcDelta1Size_Local[2] = { 1, 32 };
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.calculateDelta1, 2, NULL, calcDelta1Size, calcDelta1Size_Local, 0, NULL, NULL);
    
    size_t calcGrad4Size[2] = { 32, 64 };
    size_t calcGrad4Size_Local[2]  = { 1, 64 };
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.calculateGradient4, 2, NULL, calcGrad4Size, calcGrad4Size_Local, 0, NULL, NULL);

    size_t calcGrad3Size[2] = { 1024, 64 };
    size_t calcGrad3Size_Local[2]  = { 1, 64 }; 
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.calculateGradient3, 2, NULL, calcGrad3Size, calcGrad3Size_Local, 0, NULL, NULL);

    size_t calcGrad2Size[2] = { POSITIONS_PER_FILE, 64 };
    size_t calcGrad2Size_Local[2]  = { 1, 64 }; 
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.calculateGradient2, 2, NULL, calcGrad2Size, calcGrad2Size_Local, 0, NULL, NULL);
    
    size_t calcGrad1Size[2] = { POSITIONS_PER_FILE, 256 };
    size_t calcGrad1Size_Local[2]  = { 1, 64 }; 
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.calculateGradient1, 2, NULL, calcGrad1Size, calcGrad1Size_Local, 0, NULL, NULL);

    timestamp++;
    cl_float biasCorrection1 = pow(ADAM_BETA1, timestamp);
    cl_float biasCorrection2 = pow(ADAM_BETA2, timestamp);
    cl_float lr = (cl_float)learningRate;

    size_t weight1Size = HALF_INPUT_BITS * ACCUMULATOR_NODES_PER_SIDE;
    size_t weight2Size = ACCUMULATOR_NODES * SECOND_HIDDEN_LAYER_NODES;
    size_t weight3Size = SECOND_HIDDEN_LAYER_NODES * THIRD_HIDDEN_LAYER_NODES;
    size_t weight4Size = THIRD_HIDDEN_LAYER_NODES;
    
    size_t bias1Size = ACCUMULATOR_NODES_PER_SIDE;
    size_t bias2Size = SECOND_HIDDEN_LAYER_NODES;
    size_t bias3Size = THIRD_HIDDEN_LAYER_NODES;
    size_t bias4Size = OUTPUT_LAYER_NODES;

    ENQUEUE_ADAM(opencl_mem.weights1, opencl_mem.gradient1Sum, opencl_mem.m_weights1, opencl_mem.v_weights1, weight1Size, lr, biasCorrection1, biasCorrection2);
    ENQUEUE_ADAM(opencl_mem.weights2, opencl_mem.gradient2Sum, opencl_mem.m_weights2, opencl_mem.v_weights2, weight2Size, lr, biasCorrection1, biasCorrection2);
    ENQUEUE_ADAM(opencl_mem.weights3, opencl_mem.gradient3Sum, opencl_mem.m_weights3, opencl_mem.v_weights3, weight3Size, lr, biasCorrection1, biasCorrection2);
    ENQUEUE_ADAM(opencl_mem.weights4, opencl_mem.gradient4Sum, opencl_mem.m_weights4, opencl_mem.v_weights4, weight4Size, lr, biasCorrection1, biasCorrection2);

    ENQUEUE_ADAM(opencl_mem.bias1, opencl_mem.gradientBias1Sum, opencl_mem.m_bias1, opencl_mem.v_bias1, bias1Size, lr, biasCorrection1, biasCorrection2);
    ENQUEUE_ADAM(opencl_mem.bias2, opencl_mem.gradientBias2Sum, opencl_mem.m_bias2, opencl_mem.v_bias2, bias2Size, lr, biasCorrection1, biasCorrection2);
    ENQUEUE_ADAM(opencl_mem.bias3, opencl_mem.gradientBias3Sum, opencl_mem.m_bias3, opencl_mem.v_bias3, bias3Size, lr, biasCorrection1, biasCorrection2);
    ENQUEUE_ADAM(opencl_mem.bias4, opencl_mem.gradientBias4Sum, opencl_mem.m_bias4, opencl_mem.v_bias4, bias4Size, lr, biasCorrection1, biasCorrection2);

    clFlush(opencl_context.queue);
}

double getSumSquaredError()
{
    double returnedValue = 0.0;
    cl_int err = clEnqueueReadBuffer(opencl_context.queue, opencl_mem.sumsquarederror, CL_TRUE, 0, sizeof(double), &returnedValue, 0, NULL, NULL);
    if(!isfinite(returnedValue) || isnan(returnedValue))
    {
        DEBUG("Fetched SSE of %f", returnedValue);
        returnedValue = 1.0;
    }
    if(err != CL_SUCCESS)
    {
        DEBUG("\nError reading sum squared error.");
    }
    return returnedValue;
}

void getWeights(network_weights_training* weights)
{
    float* transposedWeights1 = CALLOC(HALF_INPUT_BITS * ACCUMULATOR_NODES_PER_SIDE, sizeof(float));
    float* transposedWeights2 = CALLOC(ACCUMULATOR_NODES * SECOND_HIDDEN_LAYER_NODES, sizeof(float));
    float* transposedWeights3 = CALLOC(SECOND_HIDDEN_LAYER_NODES * THIRD_HIDDEN_LAYER_NODES, sizeof(float));
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.weights1, CL_FALSE, 0, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS, transposedWeights1, 0, NULL, NULL);
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.weights2, CL_FALSE, 0, sizeof(float) * SECOND_HIDDEN_LAYER_NODES * ACCUMULATOR_NODES, transposedWeights2, 0, NULL, NULL);
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.weights3, CL_TRUE, 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES * SECOND_HIDDEN_LAYER_NODES, transposedWeights3, 0, NULL, NULL);
    
    for (int i = 0; i < HALF_INPUT_BITS; i++) {
        for (int j = 0; j < ACCUMULATOR_NODES_PER_SIDE; j++) {
            //GPU [input][output] -> CPU [output][input]
            weights->weights1[j][i] = transposedWeights1[i * ACCUMULATOR_NODES_PER_SIDE + j];
        }
    }
    
    for (int i = 0; i < ACCUMULATOR_NODES; i++) {
        for (int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++) {
            //GPU [input][output] -> CPU [output][input]
            weights->weights2[j][i] = transposedWeights2[i * SECOND_HIDDEN_LAYER_NODES + j];
        }
    }
    
    for (int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++) {
        for (int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++) {
            //GPU [input][output] -> CPU [output][input]
            weights->weights3[j][i] = transposedWeights3[i * THIRD_HIDDEN_LAYER_NODES + j];
        }
    }
    
    FREE(transposedWeights1);
    FREE(transposedWeights2);
    FREE(transposedWeights3);
    
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.weights4, CL_FALSE, 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, weights->weights4, 0, NULL, NULL);
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.bias1, CL_FALSE, 0, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE, weights->weights1_bias, 0, NULL, NULL);
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.bias2, CL_FALSE, 0, sizeof(float) * SECOND_HIDDEN_LAYER_NODES, weights->weights2_bias, 0, NULL, NULL);
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.bias3, CL_FALSE, 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, weights->weights3_bias, 0, NULL, NULL);
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.bias4, CL_TRUE, 0, sizeof(float), &weights->weights4_bias, 0, NULL, NULL);
}