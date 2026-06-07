#include "gpu_funcs.h"
#include <string.h>

openCLContext opencl_context = {
    .platform = 0,
    .device = 0,
    .context = NULL,
    .queue = NULL,
    .program = NULL,
    .kernels = { { NULL } }
};

openCLKernelMemory opencl_mem = {NULL};

cl_event readEvent;
cl_float lr = MAX_LR;
int64_t cosineIntervalLength;
uint64_t cosineTimestamp;
uint64_t timestamp;
int intervalCount;
float rho_inf = (2.0 / (1.0 - ADAM_BETA2)) - 1.0;
float rho_timestamp = 0.0;
cl_float rectificationTerm = 0.0;
cl_float biasCorrection1 = 1.0;
cl_float biasCorrection2 = 1.0;

size_t weight1Size = sizeof(((network_weights*)0)->weights1);
size_t weight2Size = sizeof(((network_weights*)0)->weights2);
size_t weight3Size = sizeof(((network_weights*)0)->weights3);
size_t weight4Size = sizeof(((network_weights*)0)->weights4);

size_t bias1Size = sizeof(((network_weights*)0)->weights1_bias);
size_t bias2Size = sizeof(((network_weights*)0)->weights2_bias);
size_t bias3Size = sizeof(((network_weights*)0)->weights3_bias);
size_t bias4Size = sizeof(((network_weights*)0)->weights4_bias);

size_t weight1Count = sizeof(((network_weights*)0)->weights1) / sizeof(float);
size_t weight2Count = sizeof(((network_weights*)0)->weights2) / sizeof(float);
size_t weight3Count = sizeof(((network_weights*)0)->weights3) / sizeof(float);
size_t weight4Count = sizeof(((network_weights*)0)->weights4) / sizeof(float);

size_t bias1Count = sizeof(((network_weights*)0)->weights1_bias) / sizeof(float);
size_t bias2Count = sizeof(((network_weights*)0)->weights2_bias) / sizeof(float);
size_t bias3Count = sizeof(((network_weights*)0)->weights3_bias) / sizeof(float);
size_t bias4Count = sizeof(((network_weights*)0)->weights4_bias) / sizeof(float);

short* host_activeInputs_A = NULL; 
float* host_expectedOutputs_A = NULL;
char* host_outputBucket_A = NULL;
short* host_activeInputs_B = NULL;
float* host_expectedOutputs_B = NULL;
char* host_outputBucket_B = NULL;
double* host_lossbuffer = NULL;

char* getKernelFunctions()
{
    FILE* input = fopen("../src/gpu/kernels.cl", "rb"); 
    if(!input)
    {
        DEBUG_ERROR("Cannot locate GPU kernel functions.");
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

int initOpenCL(network_weights* nnue_weights, short* h_active_A, float* h_expected_A, char* h_output_A,
                                                        short* h_active_B, float* h_expected_B, char* h_output_B,
                                                        double* h_lossbuffer)
{
    cosineIntervalLength = FIRST_INTERVAL;
    cosineTimestamp = 0;
    timestamp = 0;
    intervalCount = 0;

    host_activeInputs_A = h_active_A; 
    host_expectedOutputs_A = h_expected_A;
    host_outputBucket_A = h_output_A;
    host_activeInputs_B = h_active_B;
    host_expectedOutputs_B = h_expected_B;
    host_outputBucket_B = h_output_B;
    host_lossbuffer = h_lossbuffer;

    int err = 0;

    err = clGetPlatformIDs(1, &opencl_context.platform, NULL);
    err = clGetDeviceIDs(opencl_context.platform, CL_DEVICE_TYPE_GPU, 1, &opencl_context.device, NULL);
    opencl_context.context = clCreateContext(NULL, 1, &opencl_context.device, NULL, NULL, &err);
    
    #ifdef PERFT_KERNELS
        cl_queue_properties queueProperties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
        opencl_context.queue = clCreateCommandQueueWithProperties(opencl_context.context, opencl_context.device, queueProperties, &err);
    #else
        opencl_context.queue = clCreateCommandQueueWithProperties(opencl_context.context, opencl_context.device, NULL, &err);
    #endif
    
    char* kernelSource = getKernelFunctions();
    opencl_context.program = clCreateProgramWithSource(opencl_context.context, 1, (const char**)&kernelSource, NULL, &err);
    free(kernelSource);
    kernelSource = NULL;

    err = clBuildProgram(opencl_context.program, 1, &opencl_context.device, "-g -cl-fast-relaxed-math", NULL, NULL); 
    
    if(err)
    {
        char log[4096];
        clGetProgramBuildInfo(opencl_context.program, opencl_context.device, CL_PROGRAM_BUILD_LOG, 4096, log, NULL);
        printf("Build Error:\n%s\n", log);
        return err;
    }

    opencl_context.kernels.calculateAccumulator = clCreateKernel(opencl_context.program, "calculateAccumulator", &err);
    opencl_context.kernels.forwardPropagate = clCreateKernel(opencl_context.program, "forwardPropagate", &err);
    opencl_context.kernels.backpropagate = clCreateKernel(opencl_context.program, "backpropagate", &err); 
    opencl_context.kernels.calculateGradient4 = clCreateKernel(opencl_context.program, "calculateGradient4", &err);
    opencl_context.kernels.calculateGradient3 = clCreateKernel(opencl_context.program, "calculateGradient3", &err);
    opencl_context.kernels.calculateGradient2 = clCreateKernel(opencl_context.program, "calculateGradient2", &err);
    opencl_context.kernels.calculateGradient1 = clCreateKernel(opencl_context.program, "calculateGradient1", &err);
    opencl_context.kernels.adamw = clCreateKernel(opencl_context.program, "adamW", &err);
    opencl_context.kernels.lazyadam = clCreateKernel(opencl_context.program, "lazyAdam", &err);
    opencl_context.kernels.lookahead = clCreateKernel(opencl_context.program, "lookahead_update", &err);
    
    opencl_mem.mem.activeInputs_A = clCreateBuffer(opencl_context.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, MINIBATCH_SIZE * 64 * sizeof(short), host_activeInputs_A, NULL);
    opencl_mem.mem.activeInputs_B = clCreateBuffer(opencl_context.context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, MINIBATCH_SIZE * 64 * sizeof(short), host_activeInputs_B, NULL);

    opencl_mem.mem.expectedOutput_A = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, MINIBATCH_SIZE * sizeof(float), host_expectedOutputs_A, NULL);
    opencl_mem.mem.expectedOutput_B = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, MINIBATCH_SIZE * sizeof(float), host_expectedOutputs_B, NULL);
    
    opencl_mem.mem.outputBucket_A = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, MINIBATCH_SIZE * sizeof(char), host_outputBucket_A, NULL);
    opencl_mem.mem.outputBucket_B = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, MINIBATCH_SIZE * sizeof(char), host_outputBucket_B, NULL);

    opencl_mem.mem.weights1_fast = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight1Size, NULL, NULL);
    opencl_mem.mem.weights2_fast = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight2Size, NULL, NULL);
    opencl_mem.mem.weights3_fast = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight3Size, NULL, NULL);
    opencl_mem.mem.weights4_fast = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight4Size, NULL, NULL);
    
    opencl_mem.mem.bias1_fast = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias1Size, NULL, NULL);
    opencl_mem.mem.bias2_fast = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias2Size, NULL, NULL);
    opencl_mem.mem.bias3_fast = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias3Size, NULL, NULL);
    opencl_mem.mem.bias4_fast = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias4Size, NULL, NULL);
    
    opencl_mem.mem.weights1_slow = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight1Size, NULL, NULL);
    opencl_mem.mem.weights2_slow = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight2Size, NULL, NULL);
    opencl_mem.mem.weights3_slow = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight3Size, NULL, NULL);
    opencl_mem.mem.weights4_slow = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight4Size, NULL, NULL);
    
    opencl_mem.mem.bias1_slow = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias1Size, NULL, NULL);
    opencl_mem.mem.bias2_slow = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias2Size, NULL, NULL);
    opencl_mem.mem.bias3_slow = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias3Size, NULL, NULL);
    opencl_mem.mem.bias4_slow = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias4Size, NULL, NULL);

    opencl_mem.mem.accumulatorOutput = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, MINIBATCH_SIZE * sizeof(float) * ACCUMULATOR_NODES, NULL, NULL);
    opencl_mem.mem.h2Output = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, MINIBATCH_SIZE * sizeof(float) * SECOND_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.mem.h3Output = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, MINIBATCH_SIZE * sizeof(float) * THIRD_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.mem.finalOutput = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, MINIBATCH_SIZE * sizeof(float), NULL, NULL);
    
    opencl_mem.mem.loss = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, MINIBATCH_SIZE * sizeof(double), host_lossbuffer, NULL);

    opencl_mem.mem.delta4 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * MINIBATCH_SIZE, NULL, NULL);
    opencl_mem.mem.delta3 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * MINIBATCH_SIZE * THIRD_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.mem.delta2 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * MINIBATCH_SIZE * SECOND_HIDDEN_LAYER_NODES, NULL, NULL);
    opencl_mem.mem.delta1 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(float) * MINIBATCH_SIZE * ACCUMULATOR_NODES, NULL, NULL);

    opencl_mem.mem.gradient1Sum = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight1Size, NULL, NULL);
    opencl_mem.mem.gradientBias1Sum = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias1Size, NULL, NULL);

    opencl_mem.mem.gradient2Sum = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight2Size, NULL, NULL);
    opencl_mem.mem.gradientBias2Sum = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias2Size, NULL, NULL);

    opencl_mem.mem.gradient3Sum = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight3Size, NULL, NULL);
    opencl_mem.mem.gradientBias3Sum = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias3Size, NULL, NULL);

    opencl_mem.mem.gradient4Sum = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight4Size, NULL, NULL);
    opencl_mem.mem.gradientBias4Sum = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias4Size, NULL, NULL);

    opencl_mem.mem.m_weights1 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight1Size, NULL, NULL);
    opencl_mem.mem.m_weights2 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight2Size, NULL, NULL);
    opencl_mem.mem.m_weights3 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight3Size, NULL, NULL);
    opencl_mem.mem.m_weights4 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight4Size, NULL, NULL);
    
    opencl_mem.mem.m_bias1 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias1Size, NULL, NULL);
    opencl_mem.mem.m_bias2 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias2Size, NULL, NULL);
    opencl_mem.mem.m_bias3 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias3Size, NULL, NULL);
    opencl_mem.mem.m_bias4 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias4Size, NULL, NULL);
    
    opencl_mem.mem.v_weights1 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight1Size, NULL, NULL);
    opencl_mem.mem.v_weights2 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight2Size, NULL, NULL);
    opencl_mem.mem.v_weights3 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight3Size, NULL, NULL);
    opencl_mem.mem.v_weights4 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, weight4Size, NULL, NULL);
    
    opencl_mem.mem.v_bias1 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias1Size, NULL, NULL);
    opencl_mem.mem.v_bias2 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias2Size, NULL, NULL);
    opencl_mem.mem.v_bias3 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias3Size, NULL, NULL);
    opencl_mem.mem.v_bias4 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, bias4Size, NULL, NULL);

    int zero_d = 0;
    opencl_mem.mem.sparseTimestamps = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, sizeof(int) * ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.mem.sparseTimestamps, &zero_d, sizeof(int), 0, sizeof(int) * ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS, 0, NULL, NULL);

    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.mem.weights1_slow, CL_TRUE, 0, weight1Size, (void*)nnue_weights->weights1, 0, NULL, NULL);
    clEnqueueCopyBuffer(opencl_context.queue, opencl_mem.mem.weights1_slow, opencl_mem.mem.weights1_fast, 0, 0, weight1Size, 0, NULL, NULL);

    float* transposedWeight = calloc(ACCUMULATOR_NODES * SECOND_HIDDEN_LAYER_NODES, sizeof(float));
    for (int i = 0; i < ACCUMULATOR_NODES; i++) {
        for (int j = 0; j < SECOND_HIDDEN_LAYER_NODES; j++) {
            // CPU [output][input] -> GPU [input][output]
            transposedWeight[i * SECOND_HIDDEN_LAYER_NODES + j] = nnue_weights->weights2[j][i];
        }
    }
    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.mem.weights2_slow, CL_TRUE, 0, weight2Size, (void*)transposedWeight, 0, NULL, NULL);
    clEnqueueCopyBuffer(opencl_context.queue, opencl_mem.mem.weights2_slow, opencl_mem.mem.weights2_fast, 0, 0, weight2Size, 0, NULL, NULL);
    free(transposedWeight);
    
    transposedWeight = calloc(SECOND_HIDDEN_LAYER_NODES * THIRD_HIDDEN_LAYER_NODES, sizeof(float));
    for (int i = 0; i < SECOND_HIDDEN_LAYER_NODES; i++) {
        for (int j = 0; j < THIRD_HIDDEN_LAYER_NODES; j++) {
            // CPU [output][input] -> GPU [input][output]
            transposedWeight[i * THIRD_HIDDEN_LAYER_NODES + j] = nnue_weights->weights3[j][i];
        }
    }
    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.mem.weights3_slow, CL_TRUE, 0, weight3Size, (void*)transposedWeight, 0, NULL, NULL);
    clEnqueueCopyBuffer(opencl_context.queue, opencl_mem.mem.weights3_slow, opencl_mem.mem.weights3_fast, 0, 0, weight3Size, 0, NULL, NULL);
    free(transposedWeight);
    
    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.mem.weights4_slow, CL_TRUE, 0, weight4Size, (void*)&nnue_weights->weights4, 0, NULL, NULL);
    clEnqueueCopyBuffer(opencl_context.queue, opencl_mem.mem.weights4_slow, opencl_mem.mem.weights4_fast, 0, 0, weight4Size, 0, NULL, NULL);
    
    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.mem.bias1_slow, CL_TRUE, 0, bias1Size, (void*)&nnue_weights->weights1_bias, 0, NULL, NULL);
    clEnqueueCopyBuffer(opencl_context.queue, opencl_mem.mem.bias1_slow, opencl_mem.mem.bias1_fast, 0, 0, bias1Size, 0, NULL, NULL);

    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.mem.bias2_slow, CL_TRUE, 0, bias2Size, (void*)&nnue_weights->weights2_bias, 0, NULL, NULL);
    clEnqueueCopyBuffer(opencl_context.queue, opencl_mem.mem.bias2_slow, opencl_mem.mem.bias2_fast, 0, 0, bias2Size, 0, NULL, NULL);

    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.mem.bias3_slow, CL_TRUE, 0, bias3Size, (void*)&nnue_weights->weights3_bias, 0, NULL, NULL);
    clEnqueueCopyBuffer(opencl_context.queue, opencl_mem.mem.bias3_slow, opencl_mem.mem.bias3_fast, 0, 0, bias3Size, 0, NULL, NULL);

    clEnqueueWriteBuffer(opencl_context.queue, opencl_mem.mem.bias4_slow, CL_TRUE, 0, bias4Size, (void*)&nnue_weights->weights4_bias, 0, NULL, NULL);
    clEnqueueCopyBuffer(opencl_context.queue, opencl_mem.mem.bias4_slow, opencl_mem.mem.bias4_fast, 0, 0, bias4Size, 0, NULL, NULL);
    
    float zero_f = 0.0f;
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.mem.m_weights1, &zero_f, sizeof(float), 0, weight1Size, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.mem.m_weights2, &zero_f, sizeof(float), 0, weight2Size, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.mem.m_weights3, &zero_f, sizeof(float), 0, weight3Size, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.mem.m_weights4, &zero_f, sizeof(float), 0, weight4Size, 0, NULL, NULL);
    
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.mem.m_bias1, &zero_f, sizeof(float), 0, bias1Size, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.mem.m_bias2, &zero_f, sizeof(float), 0, bias2Size, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.mem.m_bias3, &zero_f, sizeof(float), 0, bias3Size, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.mem.m_bias4, &zero_f, sizeof(float), 0, bias4Size, 0, NULL, NULL);

    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.mem.v_weights1, &zero_f, sizeof(float), 0, weight1Size, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.mem.v_weights2, &zero_f, sizeof(float), 0, weight2Size, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.mem.v_weights3, &zero_f, sizeof(float), 0, weight3Size, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.mem.v_weights4, &zero_f, sizeof(float), 0, weight4Size, 0, NULL, NULL);
    
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.mem.v_bias1, &zero_f, sizeof(float), 0, bias1Size, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.mem.v_bias2, &zero_f, sizeof(float), 0, bias2Size, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.mem.v_bias3, &zero_f, sizeof(float), 0, bias3Size, 0, NULL, NULL);
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.mem.v_bias4, &zero_f, sizeof(float), 0, bias4Size, 0, NULL, NULL);

    clSetKernelArg(opencl_context.kernels.calculateAccumulator, 0, sizeof(cl_mem), &opencl_mem.mem.activeInputs_A);
    clSetKernelArg(opencl_context.kernels.calculateAccumulator, 1, sizeof(cl_mem), &opencl_mem.mem.weights1_fast);
    clSetKernelArg(opencl_context.kernels.calculateAccumulator, 2, sizeof(cl_mem), &opencl_mem.mem.bias1_fast);
    clSetKernelArg(opencl_context.kernels.calculateAccumulator, 3, sizeof(cl_mem), &opencl_mem.mem.accumulatorOutput);

    clSetKernelArg(opencl_context.kernels.forwardPropagate, 0, sizeof(cl_mem), &opencl_mem.mem.accumulatorOutput);
    clSetKernelArg(opencl_context.kernels.forwardPropagate, 1, sizeof(cl_mem), &opencl_mem.mem.weights2_fast);
    clSetKernelArg(opencl_context.kernels.forwardPropagate, 2, sizeof(cl_mem), &opencl_mem.mem.bias2_fast);
    clSetKernelArg(opencl_context.kernels.forwardPropagate, 3, sizeof(cl_mem), &opencl_mem.mem.h2Output);
    clSetKernelArg(opencl_context.kernels.forwardPropagate, 4, sizeof(cl_mem), &opencl_mem.mem.weights3_fast);
    clSetKernelArg(opencl_context.kernels.forwardPropagate, 5, sizeof(cl_mem), &opencl_mem.mem.bias3_fast);
    clSetKernelArg(opencl_context.kernels.forwardPropagate, 6, sizeof(cl_mem), &opencl_mem.mem.h3Output);
    clSetKernelArg(opencl_context.kernels.forwardPropagate, 7, sizeof(cl_mem), &opencl_mem.mem.weights4_fast);
    clSetKernelArg(opencl_context.kernels.forwardPropagate, 8, sizeof(cl_mem), &opencl_mem.mem.bias4_fast);
    clSetKernelArg(opencl_context.kernels.forwardPropagate, 9, sizeof(cl_mem), &opencl_mem.mem.outputBucket_A);
    clSetKernelArg(opencl_context.kernels.forwardPropagate, 10, sizeof(cl_mem), &opencl_mem.mem.finalOutput);
    
    clSetKernelArg(opencl_context.kernels.backpropagate, 0, sizeof(cl_mem), &opencl_mem.mem.finalOutput);
    clSetKernelArg(opencl_context.kernels.backpropagate, 1, sizeof(cl_mem), &opencl_mem.mem.expectedOutput_A);
    clSetKernelArg(opencl_context.kernels.backpropagate, 2, sizeof(cl_mem), &opencl_mem.mem.h3Output);
    clSetKernelArg(opencl_context.kernels.backpropagate, 3, sizeof(cl_mem), &opencl_mem.mem.h2Output);
    clSetKernelArg(opencl_context.kernels.backpropagate, 4, sizeof(cl_mem), &opencl_mem.mem.accumulatorOutput);
    clSetKernelArg(opencl_context.kernels.backpropagate, 5, sizeof(cl_mem), &opencl_mem.mem.weights4_fast);
    clSetKernelArg(opencl_context.kernels.backpropagate, 6, sizeof(cl_mem), &opencl_mem.mem.weights3_fast);
    clSetKernelArg(opencl_context.kernels.backpropagate, 7, sizeof(cl_mem), &opencl_mem.mem.weights2_fast);
    clSetKernelArg(opencl_context.kernels.backpropagate, 8, sizeof(cl_mem), &opencl_mem.mem.delta4);
    clSetKernelArg(opencl_context.kernels.backpropagate, 9, sizeof(cl_mem), &opencl_mem.mem.delta3);
    clSetKernelArg(opencl_context.kernels.backpropagate, 10, sizeof(cl_mem), &opencl_mem.mem.delta2);
    clSetKernelArg(opencl_context.kernels.backpropagate, 11, sizeof(cl_mem), &opencl_mem.mem.delta1);
    clSetKernelArg(opencl_context.kernels.backpropagate, 12, sizeof(cl_mem), &opencl_mem.mem.outputBucket_A);
    clSetKernelArg(opencl_context.kernels.backpropagate, 13, sizeof(cl_mem), &opencl_mem.mem.loss);

    clSetKernelArg(opencl_context.kernels.calculateGradient4, 0, sizeof(cl_mem), &opencl_mem.mem.delta4);
    clSetKernelArg(opencl_context.kernels.calculateGradient4, 1, sizeof(cl_mem), &opencl_mem.mem.h3Output);
    clSetKernelArg(opencl_context.kernels.calculateGradient4, 2, sizeof(cl_mem), &opencl_mem.mem.gradient4Sum);
    clSetKernelArg(opencl_context.kernels.calculateGradient4, 3, sizeof(cl_mem), &opencl_mem.mem.gradientBias4Sum);
    clSetKernelArg(opencl_context.kernels.calculateGradient4, 4, sizeof(cl_mem), &opencl_mem.mem.outputBucket_A);

    clSetKernelArg(opencl_context.kernels.calculateGradient3, 0, sizeof(cl_mem), &opencl_mem.mem.delta3);
    clSetKernelArg(opencl_context.kernels.calculateGradient3, 1, sizeof(cl_mem), &opencl_mem.mem.h2Output);
    clSetKernelArg(opencl_context.kernels.calculateGradient3, 2, sizeof(cl_mem), &opencl_mem.mem.gradient3Sum);
    clSetKernelArg(opencl_context.kernels.calculateGradient3, 3, sizeof(cl_mem), &opencl_mem.mem.gradientBias3Sum);

    clSetKernelArg(opencl_context.kernels.calculateGradient2, 0, sizeof(cl_mem), &opencl_mem.mem.delta2);
    clSetKernelArg(opencl_context.kernels.calculateGradient2, 1, sizeof(cl_mem), &opencl_mem.mem.accumulatorOutput);
    clSetKernelArg(opencl_context.kernels.calculateGradient2, 2, sizeof(cl_mem), &opencl_mem.mem.gradient2Sum);
    clSetKernelArg(opencl_context.kernels.calculateGradient2, 3, sizeof(cl_mem), &opencl_mem.mem.gradientBias2Sum);

    clSetKernelArg(opencl_context.kernels.calculateGradient1, 0, sizeof(cl_mem), &opencl_mem.mem.activeInputs_A);
    clSetKernelArg(opencl_context.kernels.calculateGradient1, 1, sizeof(cl_mem), &opencl_mem.mem.delta1);
    clSetKernelArg(opencl_context.kernels.calculateGradient1, 2, sizeof(cl_mem), &opencl_mem.mem.gradient1Sum);
    clSetKernelArg(opencl_context.kernels.calculateGradient1, 3, sizeof(cl_mem), &opencl_mem.mem.gradientBias1Sum);

    //Weights2
    clSetKernelArg(opencl_context.kernels.adamw, 0, sizeof(cl_mem), &opencl_mem.mem.weights2_fast);
    clSetKernelArg(opencl_context.kernels.adamw, 1, sizeof(cl_mem), &opencl_mem.mem.gradient2Sum);
    clSetKernelArg(opencl_context.kernels.adamw, 2, sizeof(cl_mem), &opencl_mem.mem.m_weights2);
    clSetKernelArg(opencl_context.kernels.adamw, 3, sizeof(cl_mem), &opencl_mem.mem.v_weights2);
    // Weights 3
    clSetKernelArg(opencl_context.kernels.adamw, 4, sizeof(cl_mem), &opencl_mem.mem.weights3_fast);
    clSetKernelArg(opencl_context.kernels.adamw, 5, sizeof(cl_mem), &opencl_mem.mem.gradient3Sum);
    clSetKernelArg(opencl_context.kernels.adamw, 6, sizeof(cl_mem), &opencl_mem.mem.m_weights3);
    clSetKernelArg(opencl_context.kernels.adamw, 7, sizeof(cl_mem), &opencl_mem.mem.v_weights3);
    // Weights 4
    clSetKernelArg(opencl_context.kernels.adamw, 8, sizeof(cl_mem), &opencl_mem.mem.weights4_fast);
    clSetKernelArg(opencl_context.kernels.adamw, 9, sizeof(cl_mem), &opencl_mem.mem.gradient4Sum);
    clSetKernelArg(opencl_context.kernels.adamw, 10, sizeof(cl_mem), &opencl_mem.mem.m_weights4);
    clSetKernelArg(opencl_context.kernels.adamw, 11, sizeof(cl_mem), &opencl_mem.mem.v_weights4);
    // Bias 1
    clSetKernelArg(opencl_context.kernels.adamw, 12, sizeof(cl_mem), &opencl_mem.mem.bias1_fast);
    clSetKernelArg(opencl_context.kernels.adamw, 13, sizeof(cl_mem), &opencl_mem.mem.gradientBias1Sum);
    clSetKernelArg(opencl_context.kernels.adamw, 14, sizeof(cl_mem), &opencl_mem.mem.m_bias1);
    clSetKernelArg(opencl_context.kernels.adamw, 15, sizeof(cl_mem), &opencl_mem.mem.v_bias1);
    // Bias 2
    clSetKernelArg(opencl_context.kernels.adamw, 16, sizeof(cl_mem), &opencl_mem.mem.bias2_fast);
    clSetKernelArg(opencl_context.kernels.adamw, 17, sizeof(cl_mem), &opencl_mem.mem.gradientBias2Sum);
    clSetKernelArg(opencl_context.kernels.adamw, 18, sizeof(cl_mem), &opencl_mem.mem.m_bias2);
    clSetKernelArg(opencl_context.kernels.adamw, 19, sizeof(cl_mem), &opencl_mem.mem.v_bias2);
    // Bias 3
    clSetKernelArg(opencl_context.kernels.adamw, 20, sizeof(cl_mem), &opencl_mem.mem.bias3_fast);
    clSetKernelArg(opencl_context.kernels.adamw, 21, sizeof(cl_mem), &opencl_mem.mem.gradientBias3Sum);
    clSetKernelArg(opencl_context.kernels.adamw, 22, sizeof(cl_mem), &opencl_mem.mem.m_bias3);
    clSetKernelArg(opencl_context.kernels.adamw, 23, sizeof(cl_mem), &opencl_mem.mem.v_bias3);
    // Bias 4
    clSetKernelArg(opencl_context.kernels.adamw, 24, sizeof(cl_mem), &opencl_mem.mem.bias4_fast);
    clSetKernelArg(opencl_context.kernels.adamw, 25, sizeof(cl_mem), &opencl_mem.mem.gradientBias4Sum);
    clSetKernelArg(opencl_context.kernels.adamw, 26, sizeof(cl_mem), &opencl_mem.mem.m_bias4);
    clSetKernelArg(opencl_context.kernels.adamw, 27, sizeof(cl_mem), &opencl_mem.mem.v_bias4);

    return (err == CL_SUCCESS);
}

void freeOpenCL()
{
    for (int i = 0; i < KERNEL_COUNT; i++) 
    {
        if (opencl_context.kernels.arr[i]) 
        {
            clReleaseKernel(opencl_context.kernels.arr[i]);
            opencl_context.kernels.arr[i] = NULL;
        }
    }

    for (int i = 0; i < MEM_COUNT; i++) 
    {
        if (opencl_mem.mem.arr[i]) clReleaseMemObject(opencl_mem.mem.arr[i]);
    }
}

void print_prof(const char* name, cl_event e)
{
    cl_ulong start, end;
    clGetEventProfilingInfo(e, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
    clGetEventProfilingInfo(e, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);

    double execute_ms = (end - start) / 1000000.0;
    printf("%-24s | %0.4f ms\n", name, execute_ms);
    clReleaseEvent(e);
}

void enqueueKernels(int bufferSide, int doBackprop)
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
        void* ptr1 = clEnqueueMapBuffer(opencl_context.queue, opencl_mem.mem.activeInputs_A, CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, MINIBATCH_SIZE * 64 * sizeof(short), 0, NULL, NULL, NULL);
        void* ptr2 = clEnqueueMapBuffer(opencl_context.queue, opencl_mem.mem.expectedOutput_A, CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, MINIBATCH_SIZE * sizeof(float), 0, NULL, NULL, NULL);
        void* ptr3 = clEnqueueMapBuffer(opencl_context.queue, opencl_mem.mem.outputBucket_A, CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, MINIBATCH_SIZE * sizeof(char), 0, NULL, NULL, NULL);

        clEnqueueUnmapMemObject(opencl_context.queue, opencl_mem.mem.activeInputs_A, ptr1, 0, NULL, NULL);
        clEnqueueUnmapMemObject(opencl_context.queue, opencl_mem.mem.expectedOutput_A, ptr2, 0, NULL, NULL);
        clEnqueueUnmapMemObject(opencl_context.queue, opencl_mem.mem.outputBucket_A, ptr3, 0, NULL, NULL);
    }
    else if(bufferSide == INPUT_GROUP_B)
    {
        void* ptr1 = clEnqueueMapBuffer(opencl_context.queue, opencl_mem.mem.activeInputs_B, CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, MINIBATCH_SIZE * 64 * sizeof(short), 0, NULL, NULL, NULL);
        void* ptr2 = clEnqueueMapBuffer(opencl_context.queue, opencl_mem.mem.expectedOutput_B, CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, MINIBATCH_SIZE * sizeof(float), 0, NULL, NULL, NULL);
        void* ptr3 = clEnqueueMapBuffer(opencl_context.queue, opencl_mem.mem.outputBucket_B, CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, MINIBATCH_SIZE * sizeof(char), 0, NULL, NULL, NULL);

        clEnqueueUnmapMemObject(opencl_context.queue, opencl_mem.mem.activeInputs_B, ptr1, 0, NULL, NULL);
        clEnqueueUnmapMemObject(opencl_context.queue, opencl_mem.mem.expectedOutput_B, ptr2, 0, NULL, NULL);
        clEnqueueUnmapMemObject(opencl_context.queue, opencl_mem.mem.outputBucket_B, ptr3, 0, NULL, NULL);
    }

    double zero_lf = 0.0;
    clEnqueueFillBuffer(opencl_context.queue, opencl_mem.mem.loss, &zero_lf, sizeof(double), 0, MINIBATCH_SIZE * sizeof(double), 0, NULL, NULL);

    #ifdef PERFT_KERNELS
        cl_event event_accum, event_fprop, event_back, event_grad4, event_grad3, event_grad2, event_grad1, event_w1, event_dense;
    #endif

    size_t calcAccumSize[1] = {MINIBATCH_SIZE * ACCUMULATOR_NODES};
    size_t calcAccumSize_Local[1] = {ACCUMULATOR_NODES_PER_SIDE};
    clSetKernelArg(opencl_context.kernels.calculateAccumulator, 0, sizeof(cl_mem), (bufferSide == INPUT_GROUP_A) ? &opencl_mem.mem.activeInputs_A : &opencl_mem.mem.activeInputs_B);
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.kernels.calculateAccumulator, 1, NULL, calcAccumSize, calcAccumSize_Local, 0, NULL, ENQUEUE_EVENT(event_accum));
    
    size_t fpropSize[2] = {MINIBATCH_SIZE, 64};
    size_t fpropSizee_Local[2] = {1, 64};
    clSetKernelArg(opencl_context.kernels.forwardPropagate, 9, sizeof(cl_mem), (bufferSide == INPUT_GROUP_A) ? &opencl_mem.mem.outputBucket_A : &opencl_mem.mem.outputBucket_B);
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.kernels.forwardPropagate, 2, NULL, fpropSize, fpropSizee_Local, 0, NULL, ENQUEUE_EVENT(event_fprop));
    
    size_t calcDeltaSize[1] = { MINIBATCH_SIZE * ACCUMULATOR_NODES_PER_SIDE };
    size_t calcDeltaSize_Local[1] = { ACCUMULATOR_NODES_PER_SIDE };
    clSetKernelArg(opencl_context.kernels.backpropagate, 1, sizeof(cl_mem), (bufferSide == INPUT_GROUP_A) ? &opencl_mem.mem.expectedOutput_A: &opencl_mem.mem.expectedOutput_B);
    clSetKernelArg(opencl_context.kernels.backpropagate, 12, sizeof(cl_mem), (bufferSide == INPUT_GROUP_A) ? &opencl_mem.mem.outputBucket_A: &opencl_mem.mem.outputBucket_B);
    clEnqueueNDRangeKernel(opencl_context.queue,opencl_context.kernels.backpropagate, 1, NULL, calcDeltaSize, calcDeltaSize_Local, 0, NULL, ENQUEUE_EVENT(event_back));
    
    void* lossptr = clEnqueueMapBuffer(opencl_context.queue, opencl_mem.mem.loss, CL_TRUE, CL_MAP_WRITE_INVALIDATE_REGION, 0, MINIBATCH_SIZE * sizeof(double), 0, NULL, NULL, NULL);
    clEnqueueUnmapMemObject(opencl_context.queue, opencl_mem.mem.activeInputs_A, lossptr, 0, NULL, &readEvent);

    if(!doBackprop) return;

    size_t calcGrad4Size[2] = { 32, 64 };
    size_t calcGrad4Size_Local[2]  = { 1, 64 };
    clSetKernelArg(opencl_context.kernels.calculateGradient4, 4, sizeof(cl_mem), (bufferSide == INPUT_GROUP_A) ? &opencl_mem.mem.outputBucket_A: &opencl_mem.mem.outputBucket_B);
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.kernels.calculateGradient4, 2, NULL, calcGrad4Size, calcGrad4Size_Local, 0, NULL, ENQUEUE_EVENT(event_grad4));

    size_t calcGrad3Size[1] = { 1024 };
    size_t calcGrad3Size_Local[1]  = { 64 }; 
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.kernels.calculateGradient3, 1, NULL, calcGrad3Size, calcGrad3Size_Local, 0, NULL, ENQUEUE_EVENT(event_grad3));

    size_t calcGrad2Size[2] = { SECOND_HIDDEN_LAYER_NODES, ACCUMULATOR_NODES };
    size_t calcGrad2Size_Local[2] = { 16, 16 }; 
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.kernels.calculateGradient2, 2, NULL, calcGrad2Size, calcGrad2Size_Local, 0, NULL, ENQUEUE_EVENT(event_grad2));

    size_t calcGrad1Size[2] = { MINIBATCH_SIZE, 64 };
    size_t calcGrad1Size_Local[2] = { 1, 64 }; 
    clSetKernelArg(opencl_context.kernels.calculateGradient1, 0, sizeof(cl_mem),(bufferSide == INPUT_GROUP_A) ? &opencl_mem.mem.activeInputs_A: &opencl_mem.mem.activeInputs_B);
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.kernels.calculateGradient1, 2, NULL, calcGrad1Size, calcGrad1Size_Local, 0, NULL, ENQUEUE_EVENT(event_grad1));

    timestamp++;
    biasCorrection1 = powf(ADAM_BETA1, timestamp);
    biasCorrection2 = powf(ADAM_BETA2, timestamp);
    rho_timestamp = rho_inf - (2.0f * timestamp * biasCorrection2) / (1.0f - biasCorrection2);
    if (rho_timestamp > 5.0f) rectificationTerm = sqrt(((rho_timestamp - 4.0f) * (rho_timestamp - 2.0f) * rho_inf) / ((rho_inf - 4.0f) * (rho_inf - 2.0f) * rho_timestamp));
    else rectificationTerm = 0.0f;

    

    clSetKernelArg(opencl_context.kernels.adamw, 28, sizeof(cl_float), &lr);
    clSetKernelArg(opencl_context.kernels.adamw, 29, sizeof(cl_float), &biasCorrection1);
    clSetKernelArg(opencl_context.kernels.adamw, 30, sizeof(cl_float), &biasCorrection2);
    clSetKernelArg(opencl_context.kernels.adamw, 31, sizeof(cl_float), &rectificationTerm);
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.kernels.adamw, 1, NULL, &weight2Count, NULL, 0, NULL, ENQUEUE_EVENT(event_dense));

    ENQUEUE_LAZY_ADAM(opencl_mem.mem.weights1_fast, opencl_mem.mem.sparseTimestamps, opencl_mem.mem.gradient1Sum, opencl_mem.mem.m_weights1, opencl_mem.mem.v_weights1, weight1Count, lr, rho_inf, event_w1);

    if(timestamp % LOOKAHEAD_RANGE == 0)
    {
        LOOKAHEAD_UPDATE(opencl_mem.mem.weights1_fast, opencl_mem.mem.weights1_slow, weight1Count);
        LOOKAHEAD_UPDATE(opencl_mem.mem.weights2_fast, opencl_mem.mem.weights2_slow, weight2Count);
        LOOKAHEAD_UPDATE(opencl_mem.mem.weights3_fast, opencl_mem.mem.weights3_slow, weight3Count);
        LOOKAHEAD_UPDATE(opencl_mem.mem.weights4_fast, opencl_mem.mem.weights4_slow, weight4Count);

        LOOKAHEAD_UPDATE(opencl_mem.mem.bias1_fast, opencl_mem.mem.bias1_slow, bias1Count);
        LOOKAHEAD_UPDATE(opencl_mem.mem.bias2_fast, opencl_mem.mem.bias2_slow, bias2Count);
        LOOKAHEAD_UPDATE(opencl_mem.mem.bias3_fast, opencl_mem.mem.bias3_slow, bias3Count);
        LOOKAHEAD_UPDATE(opencl_mem.mem.bias4_fast, opencl_mem.mem.bias4_slow, bias4Count);
    }

    #ifdef PERFT_KERNELS
        //Profiling disallows the efficient usage of ping-pong buffers,
        //which slows down execution.
        clWaitForEvents(1, &event_dense); 

        printf("\n--- Profiling ---\n");
        print_prof("Accumulator", event_accum);
        print_prof("Forward Propagation", event_fprop);
        print_prof("Backpropagation", event_back);
        print_prof("Gradient 4", event_grad4);
        print_prof("Gradient 3", event_grad3);
        print_prof("Gradient 2", event_grad2);
        print_prof("Gradient 1", event_grad1);
        print_prof("W1", event_w1);
        print_prof("Dense", event_dense);
        printf("-----------------\n");
    #endif

    clFlush(opencl_context.queue);
}

void getWeights(network_weights* weights)
{
    float* transposedWeights2 = malloc(weight2Size);
    float* transposedWeights3 = malloc(weight3Size);
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.mem.weights1_slow, CL_FALSE, 0, weight1Size, weights->weights1, 0, NULL, NULL);
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.mem.weights2_slow, CL_FALSE, 0, weight2Size, transposedWeights2, 0, NULL, NULL);
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.mem.weights3_slow, CL_TRUE, 0, weight3Size, transposedWeights3, 0, NULL, NULL);
    
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
    
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.mem.weights4_slow, CL_FALSE, 0, weight4Size, weights->weights4, 0, NULL, NULL);
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.mem.bias1_slow, CL_FALSE, 0, bias1Size, weights->weights1_bias, 0, NULL, NULL);
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.mem.bias2_slow, CL_FALSE, 0, bias2Size, weights->weights2_bias, 0, NULL, NULL);
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.mem.bias3_slow, CL_FALSE, 0, bias3Size, weights->weights3_bias, 0, NULL, NULL);
    clEnqueueReadBuffer(opencl_context.queue, opencl_mem.mem.bias4_slow, CL_TRUE, 0, bias4Size, &weights->weights4_bias, 0, NULL, NULL);
}