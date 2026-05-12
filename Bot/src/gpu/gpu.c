#include "gpu_funcs.h"

//The link below contains a lot of good info on this topic.
//https://ics-websites.science.uu.nl/docs/vakken/mov/2019/files/OptmzdSummary%20-%20lecture4%20-%20OpenCL.pdf 

openCLContext opencl_context = {
    .platform = 0,
    .device = 0,
    .context = NULL,
    .queue = NULL,
    .program = NULL,
    .calculateAccumulator_A = NULL,
    .calculateAccumulator_B = NULL,
    .forwardPropagate = NULL,
    .backpropagate_A = NULL,
    .backpropagate_B = NULL,
    .calculateGradient4 = NULL,
    .calculateGradient3 = NULL,
    .calculateGradient2 = NULL,
    .calculateGradient1_A = NULL,
    .calculateGradient1_B = NULL,
    .lookahead = NULL
};

openCLKernelMemory opencl_mem = {NULL};

cl_event readEvent;
cl_float lr = MAX_LR;
int64_t cosineIntervalLength;
uint64_t cosineTimestamp;
uint64_t timestamp;
int intervalCount;
float rho_inf = (2 / (1.0 - ADAM_BETA2)) - 1;
float rho_timestamp = 0.0;
cl_float rectificationTerm = 0.0;
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

    char* source = calloc(size + 1, sizeof(char));

    size_t bytesRead = fread(source, 1, size, input);
    source[bytesRead] = '\0';
    
    fclose(input);

    return source;
}

int initOpenCL(network_weights_training* trainingNNUE, short* host_activeInputs_A, float* host_expectedOutputs_A,
                                                        short* host_activeInputs_B, float* host_expectedOutputs_B)
{
    cosineIntervalLength = FIRST_INTERVAL;
    cosineTimestamp = 0;
    timestamp = 0;
    intervalCount = 0;

    int err = 0;

    err = clGetPlatformIDs(1, &opencl_context.platform, NULL);
    err = clGetDeviceIDs(opencl_context.platform, CL_DEVICE_TYPE_GPU, 1, &opencl_context.device, NULL);
    opencl_context.context = clCreateContext(NULL, 1, &opencl_context.device, NULL, NULL, &err);
    
    //cl_queue_properties queueProperties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    //opencl_context.queue = clCreateCommandQueueWithProperties(opencl_context.context, opencl_context.device, queueProperties, &err);
    opencl_context.queue = clCreateCommandQueueWithProperties(opencl_context.context, opencl_context.device, NULL, &err);
    
    char* kernelSource = getKernelFunctions();
    opencl_context.program = clCreateProgramWithSource(opencl_context.context, 1, (const char**)&kernelSource, NULL, &err);
    free(kernelSource);
    kernelSource = NULL;

    err = clBuildProgram(opencl_context.program, 1, &opencl_context.device, NULL, NULL, NULL);
    
    if (err)
    {
        char log[4096];
        clGetProgramBuildInfo(opencl_context.program, opencl_context.device, CL_PROGRAM_BUILD_LOG, 4096, log, NULL);
        printf("Build Error:\n%s\n", log);
        return err;
    }

    opencl_context.calculateAccumulator_A = clCreateKernel(opencl_context.program, "calculateAccumulator", &err);
    opencl_context.calculateAccumulator_B = clCreateKernel(opencl_context.program, "calculateAccumulator", &err);
    opencl_context.forwardPropagate = clCreateKernel(opencl_context.program, "forwardPropagate", &err);
    opencl_context.backpropagate_A = clCreateKernel(opencl_context.program, "backpropagate", &err); 
    opencl_context.backpropagate_B = clCreateKernel(opencl_context.program, "backpropagate", &err);
    opencl_context.calculateGradient4 = clCreateKernel(opencl_context.program, "calculateGradient4", &err);
    opencl_context.calculateGradient3 = clCreateKernel(opencl_context.program, "calculateGradient3", &err);
    opencl_context.calculateGradient2 = clCreateKernel(opencl_context.program, "calculateGradient2", &err);
    opencl_context.calculateGradient1_A = clCreateKernel(opencl_context.program, "calculateGradient1", &err);
    opencl_context.calculateGradient1_B = clCreateKernel(opencl_context.program, "calculateGradient1", &err);
    opencl_context.adamw = clCreateKernel(opencl_context.program, "adamW", &err);
    opencl_context.lazyadam = clCreateKernel(opencl_context.program, "lazyAdam", &err);
    opencl_context.lookahead = clCreateKernel(opencl_context.program, "lookahead_update", &err);

    
    opencl_mem.activeInputs_A = clCreateBuffer(opencl_context.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, MINIBATCH_SIZE * 64 * sizeof(short), host_activeInputs_A, NULL);
    opencl_mem.expectedOutput_A = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, MINIBATCH_SIZE * sizeof(float), host_expectedOutputs_A, NULL);
    
    opencl_mem.activeInputs_B = clCreateBuffer(opencl_context.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, MINIBATCH_SIZE * 64 * sizeof(short), host_activeInputs_B, NULL);
    opencl_mem.expectedOutput_B = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, MINIBATCH_SIZE * sizeof(float), host_expectedOutputs_B, NULL);

    opencl_mem.weights1_fast = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS, NULL, NULL);
    opencl_mem.weights2_fast = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * SECOND_HIDDEN_LAYER_NODES * ACCUMULATOR_NODES, NULL, NULL);
    opencl_mem.weights3_fast = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * THIRD_HIDDEN_LAYER_NODES * SECOND_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.weights4_fast = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, NULL, NULL);

    opencl_mem.bias1_fast = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE, NULL, NULL);
    opencl_mem.bias2_fast = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * SECOND_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.bias3_fast = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.bias4_fast = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float), NULL, NULL);
    
    opencl_mem.weights1_slow = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS, NULL, NULL);
    opencl_mem.weights2_slow = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * SECOND_HIDDEN_LAYER_NODES * ACCUMULATOR_NODES, NULL, NULL);
    opencl_mem.weights3_slow = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * THIRD_HIDDEN_LAYER_NODES * SECOND_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.weights4_slow = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, NULL, NULL);

    opencl_mem.bias1_slow = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE, NULL, NULL);
    opencl_mem.bias2_slow = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * SECOND_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.bias3_slow = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.bias4_slow = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float), NULL, NULL);

    opencl_mem.accumulatorOutput = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, MINIBATCH_SIZE * sizeof(float) * ACCUMULATOR_NODES, NULL, NULL);
    opencl_mem.h2Output = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, MINIBATCH_SIZE * sizeof(float) * SECOND_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.h3Output = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, MINIBATCH_SIZE * sizeof(float) * THIRD_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.finalOutput = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, MINIBATCH_SIZE * sizeof(float), NULL, NULL);
    
    opencl_mem.sumsquarederror = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(double), NULL, NULL);

    opencl_mem.delta4 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * MINIBATCH_SIZE, NULL, NULL);
    opencl_mem.delta3 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * MINIBATCH_SIZE * THIRD_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.delta2 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * MINIBATCH_SIZE * SECOND_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.delta1 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * MINIBATCH_SIZE * ACCUMULATOR_NODES, NULL, NULL);

    
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

    int zero_d = 0;
    opencl_mem.sparseTimestamps = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(int) * ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.sparseTimestamps, &zero_d, sizeof(int), 0, sizeof(int) * ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS, 0, NULL, NULL);

    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.weights1_slow, CL_TRUE, 0, HALF_INPUT_BITS * ACCUMULATOR_NODES_PER_SIDE * sizeof(float), (void*)trainingNNUE->weights1, 0, NULL, NULL);
    clEnqueueCopyBuffer(opencl_context.queue, opencl_mem.weights1_slow, opencl_mem.weights1_fast, 0, 0, HALF_INPUT_BITS * ACCUMULATOR_NODES_PER_SIDE * sizeof(float), 0, NULL, NULL);

    float* transposedWeight = calloc(ACCUMULATOR_NODES * SECOND_HIDDEN_LAYER_NODES, sizeof(float));
    for (int i = 0; i < ACCUMULATOR_NODES; i++) {
        for (int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++) {
            // CPU [output][input] -> GPU [input][output]
            transposedWeight[i * SECOND_HIDDEN_LAYER_NODES + j] = trainingNNUE->weights2[j][i];
        }
    }
    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.weights2_slow, CL_TRUE, 0, ACCUMULATOR_NODES * SECOND_HIDDEN_LAYER_NODES * sizeof(float), (void*)transposedWeight, 0, NULL, NULL);
    clEnqueueCopyBuffer(opencl_context.queue, opencl_mem.weights2_slow, opencl_mem.weights2_fast, 0, 0, ACCUMULATOR_NODES * SECOND_HIDDEN_LAYER_NODES * sizeof(float), 0, NULL, NULL);
    free(transposedWeight);
    
    transposedWeight = calloc(SECOND_HIDDEN_LAYER_NODES * THIRD_HIDDEN_LAYER_NODES, sizeof(float));
    for (int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++) {
        for (int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++) {
            // CPU [output][input] -> GPU [input][output]
            transposedWeight[i * THIRD_HIDDEN_LAYER_NODES + j] = trainingNNUE->weights3[j][i];
        }
    }
    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.weights3_slow, CL_TRUE, 0, SECOND_HIDDEN_LAYER_NODES * THIRD_HIDDEN_LAYER_NODES * sizeof(float), (void*)transposedWeight, 0, NULL, NULL);
    clEnqueueCopyBuffer(opencl_context.queue, opencl_mem.weights3_slow, opencl_mem.weights3_fast, 0, 0, SECOND_HIDDEN_LAYER_NODES * THIRD_HIDDEN_LAYER_NODES * sizeof(float), 0, NULL, NULL);
    free(transposedWeight);
    
    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.weights4_slow, CL_TRUE, 0, THIRD_HIDDEN_LAYER_NODES * sizeof(float), (void*)&trainingNNUE->weights4, 0, NULL, NULL);
    clEnqueueCopyBuffer(opencl_context.queue, opencl_mem.weights4_slow, opencl_mem.weights4_fast, 0, 0, THIRD_HIDDEN_LAYER_NODES * sizeof(float), 0, NULL, NULL);
    
    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.bias1_slow, CL_TRUE, 0, ACCUMULATOR_NODES_PER_SIDE * sizeof(float), (void*)&trainingNNUE->weights1_bias, 0, NULL, NULL);
    clEnqueueCopyBuffer(opencl_context.queue, opencl_mem.bias1_slow, opencl_mem.bias1_fast, 0, 0, ACCUMULATOR_NODES_PER_SIDE * sizeof(float), 0, NULL, NULL);

    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.bias2_slow, CL_TRUE, 0, SECOND_HIDDEN_LAYER_NODES * sizeof(float), (void*)&trainingNNUE->weights2_bias, 0, NULL, NULL);
    clEnqueueCopyBuffer(opencl_context.queue, opencl_mem.bias2_slow, opencl_mem.bias2_fast, 0, 0, SECOND_HIDDEN_LAYER_NODES * sizeof(float), 0, NULL, NULL);

    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.bias3_slow, CL_TRUE, 0, THIRD_HIDDEN_LAYER_NODES * sizeof(float), (void*)&trainingNNUE->weights3_bias, 0, NULL, NULL);
    clEnqueueCopyBuffer(opencl_context.queue, opencl_mem.bias3_slow, opencl_mem.bias3_fast, 0, 0, THIRD_HIDDEN_LAYER_NODES * sizeof(float), 0, NULL, NULL);

    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.bias4_slow, CL_TRUE, 0, OUTPUT_LAYER_NODES * sizeof(float), (void*)&trainingNNUE->weights4_bias, 0, NULL, NULL);
    clEnqueueCopyBuffer(opencl_context.queue, opencl_mem.bias4_slow, opencl_mem.bias4_fast, 0, 0, OUTPUT_LAYER_NODES * sizeof(float), 0, NULL, NULL);
    
    float zero_f = 0.0f;
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.m_weights1, &zero_f, sizeof(float), 0, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.m_weights2, &zero_f, sizeof(float), 0, sizeof(float) * SECOND_HIDDEN_LAYER_NODES * ACCUMULATOR_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.m_weights3, &zero_f, sizeof(float), 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES * SECOND_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.m_weights4, &zero_f, sizeof(float), 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, 0, NULL, NULL);

    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.m_bias1, &zero_f, sizeof(float), 0, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.m_bias2, &zero_f, sizeof(float), 0, sizeof(float) * SECOND_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.m_bias3, &zero_f, sizeof(float), 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.m_bias4, &zero_f, sizeof(float), 0, sizeof(float), 0, NULL, NULL);

    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.v_weights1, &zero_f, sizeof(float), 0, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.v_weights2, &zero_f, sizeof(float), 0, sizeof(float) * SECOND_HIDDEN_LAYER_NODES * ACCUMULATOR_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.v_weights3, &zero_f, sizeof(float), 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES * SECOND_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.v_weights4, &zero_f, sizeof(float), 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, 0, NULL, NULL);

    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.v_bias1, &zero_f, sizeof(float), 0, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.v_bias2, &zero_f, sizeof(float), 0, sizeof(float) * SECOND_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.v_bias3, &zero_f, sizeof(float), 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.v_bias4, &zero_f, sizeof(float), 0, sizeof(float), 0, NULL, NULL);

    clSetKernelArg(opencl_context.calculateAccumulator_A, 0, sizeof(cl_mem), &opencl_mem.activeInputs_A);
    clSetKernelArg(opencl_context.calculateAccumulator_A, 1, sizeof(cl_mem), &opencl_mem.weights1_fast);
    clSetKernelArg(opencl_context.calculateAccumulator_A, 2, sizeof(cl_mem), &opencl_mem.bias1_fast);
    clSetKernelArg(opencl_context.calculateAccumulator_A, 3, sizeof(cl_mem), &opencl_mem.accumulatorOutput);
    
    clSetKernelArg(opencl_context.calculateAccumulator_B, 0, sizeof(cl_mem), &opencl_mem.activeInputs_B);
    clSetKernelArg(opencl_context.calculateAccumulator_B, 1, sizeof(cl_mem), &opencl_mem.weights1_fast);
    clSetKernelArg(opencl_context.calculateAccumulator_B, 2, sizeof(cl_mem), &opencl_mem.bias1_fast);
    clSetKernelArg(opencl_context.calculateAccumulator_B, 3, sizeof(cl_mem), &opencl_mem.accumulatorOutput);

    clSetKernelArg(opencl_context.forwardPropagate, 0, sizeof(cl_mem), &opencl_mem.accumulatorOutput);
    clSetKernelArg(opencl_context.forwardPropagate, 1, sizeof(cl_mem), &opencl_mem.weights2_fast);
    clSetKernelArg(opencl_context.forwardPropagate, 2, sizeof(cl_mem), &opencl_mem.bias2_fast);
    clSetKernelArg(opencl_context.forwardPropagate, 3, sizeof(cl_mem), &opencl_mem.h2Output);
    clSetKernelArg(opencl_context.forwardPropagate, 4, sizeof(cl_mem), &opencl_mem.weights3_fast);
    clSetKernelArg(opencl_context.forwardPropagate, 5, sizeof(cl_mem), &opencl_mem.bias3_fast);
    clSetKernelArg(opencl_context.forwardPropagate, 6, sizeof(cl_mem), &opencl_mem.h3Output);
    clSetKernelArg(opencl_context.forwardPropagate, 7, sizeof(cl_mem), &opencl_mem.weights4_fast);
    clSetKernelArg(opencl_context.forwardPropagate, 8, sizeof(cl_mem), &opencl_mem.bias4_fast);
    clSetKernelArg(opencl_context.forwardPropagate, 9, sizeof(cl_mem), &opencl_mem.finalOutput);
    
    clSetKernelArg(opencl_context.backpropagate_A, 0, sizeof(cl_mem), &opencl_mem.finalOutput);
    clSetKernelArg(opencl_context.backpropagate_A, 1, sizeof(cl_mem), &opencl_mem.expectedOutput_A);
    clSetKernelArg(opencl_context.backpropagate_A, 2, sizeof(cl_mem), &opencl_mem.h3Output);
    clSetKernelArg(opencl_context.backpropagate_A, 3, sizeof(cl_mem), &opencl_mem.h2Output);
    clSetKernelArg(opencl_context.backpropagate_A, 4, sizeof(cl_mem), &opencl_mem.accumulatorOutput);
    clSetKernelArg(opencl_context.backpropagate_A, 5, sizeof(cl_mem), &opencl_mem.weights4_fast);
    clSetKernelArg(opencl_context.backpropagate_A, 6, sizeof(cl_mem), &opencl_mem.weights3_fast);
    clSetKernelArg(opencl_context.backpropagate_A, 7, sizeof(cl_mem), &opencl_mem.weights2_fast);
    clSetKernelArg(opencl_context.backpropagate_A, 8, sizeof(cl_mem), &opencl_mem.delta4);
    clSetKernelArg(opencl_context.backpropagate_A, 9, sizeof(cl_mem), &opencl_mem.delta3);
    clSetKernelArg(opencl_context.backpropagate_A, 10, sizeof(cl_mem), &opencl_mem.delta2);
    clSetKernelArg(opencl_context.backpropagate_A, 11, sizeof(cl_mem), &opencl_mem.delta1);
    clSetKernelArg(opencl_context.backpropagate_A, 12, sizeof(cl_mem), &opencl_mem.sumsquarederror);
    
    clSetKernelArg(opencl_context.backpropagate_B, 0, sizeof(cl_mem), &opencl_mem.finalOutput);
    clSetKernelArg(opencl_context.backpropagate_B, 1, sizeof(cl_mem), &opencl_mem.expectedOutput_B);
    clSetKernelArg(opencl_context.backpropagate_B, 2, sizeof(cl_mem), &opencl_mem.h3Output);
    clSetKernelArg(opencl_context.backpropagate_B, 3, sizeof(cl_mem), &opencl_mem.h2Output);
    clSetKernelArg(opencl_context.backpropagate_B, 4, sizeof(cl_mem), &opencl_mem.accumulatorOutput);
    clSetKernelArg(opencl_context.backpropagate_B, 5, sizeof(cl_mem), &opencl_mem.weights4_fast);
    clSetKernelArg(opencl_context.backpropagate_B, 6, sizeof(cl_mem), &opencl_mem.weights3_fast);
    clSetKernelArg(opencl_context.backpropagate_B, 7, sizeof(cl_mem), &opencl_mem.weights2_fast);
    clSetKernelArg(opencl_context.backpropagate_B, 8, sizeof(cl_mem), &opencl_mem.delta4);
    clSetKernelArg(opencl_context.backpropagate_B, 9, sizeof(cl_mem), &opencl_mem.delta3);
    clSetKernelArg(opencl_context.backpropagate_B, 10, sizeof(cl_mem), &opencl_mem.delta2);
    clSetKernelArg(opencl_context.backpropagate_B, 11, sizeof(cl_mem), &opencl_mem.delta1);
    clSetKernelArg(opencl_context.backpropagate_B, 12, sizeof(cl_mem), &opencl_mem.sumsquarederror);

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

    clSetKernelArg(opencl_context.calculateGradient1_A, 0, sizeof(cl_mem), &opencl_mem.activeInputs_A);
    clSetKernelArg(opencl_context.calculateGradient1_A, 1, sizeof(cl_mem), &opencl_mem.delta1);
    clSetKernelArg(opencl_context.calculateGradient1_A, 2, sizeof(cl_mem), &opencl_mem.gradient1Sum);
    clSetKernelArg(opencl_context.calculateGradient1_A, 3, sizeof(cl_mem), &opencl_mem.gradientBias1Sum);
    
    clSetKernelArg(opencl_context.calculateGradient1_B, 0, sizeof(cl_mem), &opencl_mem.activeInputs_B);
    clSetKernelArg(opencl_context.calculateGradient1_B, 1, sizeof(cl_mem), &opencl_mem.delta1);
    clSetKernelArg(opencl_context.calculateGradient1_B, 2, sizeof(cl_mem), &opencl_mem.gradient1Sum);
    clSetKernelArg(opencl_context.calculateGradient1_B, 3, sizeof(cl_mem), &opencl_mem.gradientBias1Sum);

    return (err == CL_SUCCESS);
}

void freeOpenCL()
{
    clReleaseMemObject(opencl_mem.activeInputs_A);
    clReleaseMemObject(opencl_mem.expectedOutput_A);
    clReleaseMemObject(opencl_mem.activeInputs_B);
    clReleaseMemObject(opencl_mem.expectedOutput_B);
    clReleaseMemObject(opencl_mem.weights1_fast);
    clReleaseMemObject(opencl_mem.weights2_fast);
    clReleaseMemObject(opencl_mem.weights3_fast);
    clReleaseMemObject(opencl_mem.weights4_fast);
    clReleaseMemObject(opencl_mem.bias1_fast);
    clReleaseMemObject(opencl_mem.bias2_fast);
    clReleaseMemObject(opencl_mem.bias3_fast);
    clReleaseMemObject(opencl_mem.bias4_fast);
    clReleaseMemObject(opencl_mem.weights1_slow);
    clReleaseMemObject(opencl_mem.weights2_slow);
    clReleaseMemObject(opencl_mem.weights3_slow);
    clReleaseMemObject(opencl_mem.weights4_slow);
    clReleaseMemObject(opencl_mem.bias1_slow);
    clReleaseMemObject(opencl_mem.bias2_slow);
    clReleaseMemObject(opencl_mem.bias3_slow);
    clReleaseMemObject(opencl_mem.bias4_slow);
    clReleaseMemObject(opencl_mem.accumulatorOutput);
    clReleaseMemObject(opencl_mem.h2Output);
    clReleaseMemObject(opencl_mem.h3Output);
    clReleaseMemObject(opencl_mem.finalOutput);
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
    clReleaseMemObject(opencl_mem.sparseTimestamps);

    if (opencl_context.calculateAccumulator_A)  
    {
        clReleaseKernel(opencl_context.calculateAccumulator_A);
        opencl_context.calculateAccumulator_A = NULL;
    }
    if (opencl_context.calculateAccumulator_B)  
    {
        clReleaseKernel(opencl_context.calculateAccumulator_B);
        opencl_context.calculateAccumulator_B = NULL;
    }
    if (opencl_context.forwardPropagate)  
    {
        clReleaseKernel(opencl_context.forwardPropagate);
        opencl_context.forwardPropagate = NULL;
    }
    if (opencl_context.backpropagate_A)  
    {
        clReleaseKernel(opencl_context.backpropagate_A);
        opencl_context.backpropagate_A = NULL;
    }
    if (opencl_context.backpropagate_B)  
    {
        clReleaseKernel(opencl_context.backpropagate_B);
        opencl_context.backpropagate_B = NULL;
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
    if (opencl_context.calculateGradient1_A)  
    {
        clReleaseKernel(opencl_context.calculateGradient1_A);
        opencl_context.calculateGradient1_A = NULL;
    }
    if (opencl_context.calculateGradient1_B)  
    {
        clReleaseKernel(opencl_context.calculateGradient1_B);
        opencl_context.calculateGradient1_B = NULL;
    }
    if (opencl_context.adamw)  
    {
        clReleaseKernel(opencl_context.adamw);
        opencl_context.adamw = NULL;
    }
    if (opencl_context.lazyadam)  
    {
        clReleaseKernel(opencl_context.lazyadam);
        opencl_context.lazyadam = NULL;
    }
    if (opencl_context.lookahead)  
    {
        clReleaseKernel(opencl_context.lookahead);
        opencl_context.lookahead = NULL;
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

void enqueueKernels(int bufferSide, double* outputSSE, int doBackprop)
{
    //cosine annealing
    lr = MIN_LR + 0.5 * (MAX_LR - MIN_LR) * (1.0 + cos(PI * (cosineTimestamp++) / cosineIntervalLength));
    if(cosineTimestamp >= cosineIntervalLength && intervalCount < MAX_INTERVALS)
    {
        cosineTimestamp = 0;
        cosineIntervalLength*=INTERVAL_SCALE;
        intervalCount++;
    }
    

    if(bufferSide == INPUT_GROUP_A)
    {
        void* ptr1 = clEnqueueMapBuffer(opencl_context.queue, opencl_mem.activeInputs_A, CL_FALSE, CL_MAP_WRITE_INVALIDATE_REGION, 0, MINIBATCH_SIZE * 64 * sizeof(short), 0, NULL, NULL, NULL);
        void* ptr2 = clEnqueueMapBuffer(opencl_context.queue, opencl_mem.expectedOutput_A, CL_FALSE, CL_MAP_WRITE_INVALIDATE_REGION, 0, MINIBATCH_SIZE * sizeof(float), 0, NULL, NULL, NULL);

        clEnqueueUnmapMemObject(opencl_context.queue, opencl_mem.activeInputs_A, ptr1, 0, NULL, NULL);
        clEnqueueUnmapMemObject(opencl_context.queue, opencl_mem.expectedOutput_A, ptr2, 0, NULL, NULL);
    }
    else if(bufferSide == INPUT_GROUP_B)
    {
        void* ptr1 = clEnqueueMapBuffer(opencl_context.queue, opencl_mem.activeInputs_B, CL_FALSE, CL_MAP_WRITE_INVALIDATE_REGION, 0, MINIBATCH_SIZE * 64 * sizeof(short), 0, NULL, NULL, NULL);
        void* ptr2 = clEnqueueMapBuffer(opencl_context.queue, opencl_mem.expectedOutput_B, CL_FALSE, CL_MAP_WRITE_INVALIDATE_REGION, 0, MINIBATCH_SIZE * sizeof(float), 0, NULL, NULL, NULL);

        clEnqueueUnmapMemObject(opencl_context.queue, opencl_mem.activeInputs_B, ptr1, 0, NULL, NULL);
        clEnqueueUnmapMemObject(opencl_context.queue, opencl_mem.expectedOutput_B, ptr2, 0, NULL, NULL);

    }

    float zero_f = 0.0f;
    double zero_lf = 0.0;
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.sumsquarederror, &zero_lf, sizeof(double), 0, sizeof(double), 0, NULL, NULL);

    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.gradient1Sum, &zero_f, sizeof(float), 0, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.gradient2Sum, &zero_f, sizeof(float), 0, sizeof(float) * SECOND_HIDDEN_LAYER_NODES * ACCUMULATOR_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.gradient3Sum, &zero_f, sizeof(float), 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES * SECOND_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.gradient4Sum, &zero_f, sizeof(float), 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.gradientBias1Sum, &zero_f, sizeof(float), 0, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.gradientBias2Sum, &zero_f, sizeof(float), 0, sizeof(float) * SECOND_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.gradientBias3Sum, &zero_f, sizeof(float), 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.gradientBias4Sum, &zero_f, sizeof(float), 0, sizeof(float), 0, NULL, NULL);
    
    //cl_event perf;
    //cl_ulong start, end;

    size_t calcAccumSize[2] = {MINIBATCH_SIZE, ACCUMULATOR_NODES};
    size_t calcAccumSize_Local[2] = {1, 64};
    if(bufferSide == INPUT_GROUP_A) clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.calculateAccumulator_A, 2, NULL, calcAccumSize, calcAccumSize_Local, 0, NULL, NULL);
    else clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.calculateAccumulator_B, 2, NULL, calcAccumSize, calcAccumSize_Local, 0, NULL, NULL);
    
    size_t fpropSize[2] = {MINIBATCH_SIZE, 64};
    size_t fpropSizee_Local[2] = {1, 64};
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.forwardPropagate, 2, NULL, fpropSize, fpropSizee_Local, 0, NULL, NULL);
    
    size_t calcDeltaSize[2] = {MINIBATCH_SIZE, 64};
    size_t calcDeltaSize_Local[2] = {1, 64};
    if(bufferSide == INPUT_GROUP_A) clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.backpropagate_A, 2, NULL, calcDeltaSize, calcDeltaSize_Local, 0, NULL, NULL);
    else clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.backpropagate_B, 2, NULL, calcDeltaSize, calcDeltaSize_Local, 0, NULL, NULL);
    
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.sumsquarederror, CL_FALSE, 0, sizeof(double), outputSSE, 0, NULL, &readEvent);

    if(!doBackprop) return;

    size_t calcGrad4Size[2] = { 32, 64 };
    size_t calcGrad4Size_Local[2]  = { 1, 64 };
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.calculateGradient4, 2, NULL, calcGrad4Size, calcGrad4Size_Local, 0, NULL, NULL);

    size_t calcGrad3Size[2] = { 1024, 1 };
    size_t calcGrad3Size_Local[2]  = { 64, 1 }; 
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.calculateGradient3, 2, NULL, calcGrad3Size, calcGrad3Size_Local, 0, NULL, NULL);

    size_t calcGrad2Size = ACCUMULATOR_NODES * 64;
    size_t calcGrad2Size_Local = 64; 
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.calculateGradient2, 1, NULL, &calcGrad2Size, &calcGrad2Size_Local, 0, NULL, NULL);
    

    size_t calcGrad1Size[2] = {MINIBATCH_SIZE, 64};
    size_t calcGrad1Size_Local[2] = {1, 64}; 
    if(bufferSide == INPUT_GROUP_A) clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.calculateGradient1_A, 2, NULL, calcGrad1Size, calcGrad1Size_Local, 0, NULL, NULL);
    else clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.calculateGradient1_B, 2, NULL, calcGrad1Size, calcGrad1Size_Local, 0, NULL, NULL);

    timestamp++;
    cl_float biasCorrection1 = pow(ADAM_BETA1, timestamp);
    cl_float biasCorrection2 = pow(ADAM_BETA2, timestamp);
    rho_timestamp = rho_inf - (2.0f * timestamp * biasCorrection2) / (1.0f - biasCorrection2);
    if (rho_timestamp > 5.0f) rectificationTerm = sqrt(((rho_timestamp - 4.0f) * (rho_timestamp - 2.0f) * rho_inf) / ((rho_inf - 4.0f) * (rho_inf - 2.0f) * rho_timestamp));
    else rectificationTerm = 0.0f;

    size_t weight1Size = HALF_INPUT_BITS * ACCUMULATOR_NODES_PER_SIDE;
    size_t weight2Size = ACCUMULATOR_NODES * SECOND_HIDDEN_LAYER_NODES;
    size_t weight3Size = SECOND_HIDDEN_LAYER_NODES * THIRD_HIDDEN_LAYER_NODES;
    size_t weight4Size = THIRD_HIDDEN_LAYER_NODES;
    
    size_t bias1Size = ACCUMULATOR_NODES_PER_SIDE;
    size_t bias2Size = SECOND_HIDDEN_LAYER_NODES;
    size_t bias3Size = THIRD_HIDDEN_LAYER_NODES;
    size_t bias4Size = OUTPUT_LAYER_NODES;

    ENQUEUE_LAZY_ADAM(opencl_mem.weights1_fast, opencl_mem.sparseTimestamps, opencl_mem.gradient1Sum, opencl_mem.m_weights1, opencl_mem.v_weights1, weight1Size, lr, rho_inf);
    ENQUEUE_ADAMW(opencl_mem.weights2_fast, opencl_mem.gradient2Sum, opencl_mem.m_weights2, opencl_mem.v_weights2, weight2Size, lr, biasCorrection1, biasCorrection2, rectificationTerm);
    ENQUEUE_ADAMW(opencl_mem.weights3_fast, opencl_mem.gradient3Sum, opencl_mem.m_weights3, opencl_mem.v_weights3, weight3Size, lr, biasCorrection1, biasCorrection2, rectificationTerm);
    ENQUEUE_ADAMW(opencl_mem.weights4_fast, opencl_mem.gradient4Sum, opencl_mem.m_weights4, opencl_mem.v_weights4, weight4Size, lr, biasCorrection1, biasCorrection2, rectificationTerm);

    ENQUEUE_ADAMW(opencl_mem.bias1_fast, opencl_mem.gradientBias1Sum, opencl_mem.m_bias1, opencl_mem.v_bias1, bias1Size, lr, biasCorrection1, biasCorrection2, rectificationTerm);
    ENQUEUE_ADAMW(opencl_mem.bias2_fast, opencl_mem.gradientBias2Sum, opencl_mem.m_bias2, opencl_mem.v_bias2, bias2Size, lr, biasCorrection1, biasCorrection2, rectificationTerm);
    ENQUEUE_ADAMW(opencl_mem.bias3_fast, opencl_mem.gradientBias3Sum, opencl_mem.m_bias3, opencl_mem.v_bias3, bias3Size, lr, biasCorrection1, biasCorrection2, rectificationTerm);
    ENQUEUE_ADAMW(opencl_mem.bias4_fast, opencl_mem.gradientBias4Sum, opencl_mem.m_bias4, opencl_mem.v_bias4, bias4Size, lr, biasCorrection1, biasCorrection2, rectificationTerm);

    if(timestamp % LOOKAHEAD_RANGE == 0)
    {
        LOOKAHEAD_UPDATE(opencl_mem.weights1_fast, opencl_mem.weights1_slow, weight1Size);
        LOOKAHEAD_UPDATE(opencl_mem.weights2_fast, opencl_mem.weights2_slow, weight2Size);
        LOOKAHEAD_UPDATE(opencl_mem.weights3_fast, opencl_mem.weights3_slow, weight3Size);
        LOOKAHEAD_UPDATE(opencl_mem.weights4_fast, opencl_mem.weights4_slow, weight4Size);

        LOOKAHEAD_UPDATE(opencl_mem.bias1_fast, opencl_mem.bias1_slow, bias1Size);
        LOOKAHEAD_UPDATE(opencl_mem.bias2_fast, opencl_mem.bias2_slow, bias2Size);
        LOOKAHEAD_UPDATE(opencl_mem.bias3_fast, opencl_mem.bias3_slow, bias3Size);
        LOOKAHEAD_UPDATE(opencl_mem.bias4_fast, opencl_mem.bias4_slow, bias4Size);
    }

    clFlush(opencl_context.queue);
}

void getWeights(network_weights_training* weights)
{
    float* transposedWeights2 = calloc(ACCUMULATOR_NODES * SECOND_HIDDEN_LAYER_NODES, sizeof(float));
    float* transposedWeights3 = calloc(SECOND_HIDDEN_LAYER_NODES * THIRD_HIDDEN_LAYER_NODES, sizeof(float));
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.weights1_slow, CL_FALSE, 0, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS, weights->weights1, 0, NULL, NULL);
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.weights2_slow, CL_FALSE, 0, sizeof(float) * SECOND_HIDDEN_LAYER_NODES * ACCUMULATOR_NODES, transposedWeights2, 0, NULL, NULL);
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.weights3_slow, CL_TRUE, 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES * SECOND_HIDDEN_LAYER_NODES, transposedWeights3, 0, NULL, NULL);
    
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
    
    free(transposedWeights2);
    free(transposedWeights3);
    
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.weights4_slow, CL_FALSE, 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, weights->weights4, 0, NULL, NULL);
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.bias1_slow, CL_FALSE, 0, sizeof(float) * ACCUMULATOR_NODES_PER_SIDE, weights->weights1_bias, 0, NULL, NULL);
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.bias2_slow, CL_FALSE, 0, sizeof(float) * SECOND_HIDDEN_LAYER_NODES, weights->weights2_bias, 0, NULL, NULL);
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.bias3_slow, CL_FALSE, 0, sizeof(float) * THIRD_HIDDEN_LAYER_NODES, weights->weights3_bias, 0, NULL, NULL);
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.bias4_slow, CL_TRUE, 0, sizeof(float), &weights->weights4_bias, 0, NULL, NULL);
}