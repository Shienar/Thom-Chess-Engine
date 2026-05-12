#ifndef GPU_FUNCS
#define GPU_FUNCS

#define CL_TARGET_OPENCL_VERSION 200
#include <CL/cl.h>
#include "../debug.h"
#include "../analyze/neuralnet.h"

//CPU fills one group while gpu uses other group.
#define INPUT_GROUP(block) (block%2)
#define INPUT_GROUP_A 0
#define INPUT_GROUP_B 1

/**
 * cosine annealing is done using timestamp in enqueueKernels()
 */
#define MAX_LR 2.5e-4f
#define MIN_LR 8e-6f
#define INTERVAL_SCALE 1.5f
#define FIRST_INTERVAL 500
#define MAX_INTERVALS 20
#define LOOKAHEAD_RANGE 10
typedef struct {
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;

    //One kernel per function to run.
    cl_kernel calculateAccumulator_A;
    cl_kernel calculateAccumulator_B;
    cl_kernel forwardPropagate;
    cl_kernel backpropagate_A;
    cl_kernel backpropagate_B;
    cl_kernel calculateGradient4;
    cl_kernel calculateGradient3;
    cl_kernel calculateGradient2;
    cl_kernel calculateGradient1_A;
    cl_kernel calculateGradient1_B;
    cl_kernel adamw;
    cl_kernel lazyadam;
    cl_kernel lookahead;
} openCLContext;

typedef struct {
    cl_mem activeInputs_A;
    cl_mem expectedOutput_A;

    cl_mem activeInputs_B;
    cl_mem expectedOutput_B;

    cl_mem weights1_fast;
    cl_mem weights2_fast;
    cl_mem weights3_fast;
    cl_mem weights4_fast;

    cl_mem bias1_fast;
    cl_mem bias2_fast;
    cl_mem bias3_fast;
    cl_mem bias4_fast;
    
    cl_mem weights1_slow;
    cl_mem weights2_slow;
    cl_mem weights3_slow;
    cl_mem weights4_slow;

    cl_mem bias1_slow;
    cl_mem bias2_slow;
    cl_mem bias3_slow;
    cl_mem bias4_slow;
    
    cl_mem accumulatorOutput;
    cl_mem h2Output;
    cl_mem h3Output;
    cl_mem finalOutput;
    
    cl_mem sumsquarederror;
    
    cl_mem delta4;
    cl_mem delta3;
    cl_mem delta2;
    cl_mem delta1;
    
    cl_mem gradient1Sum;
    cl_mem gradient2Sum;
    cl_mem gradient3Sum;
    cl_mem gradient4Sum;

    cl_mem gradientBias1Sum;
    cl_mem gradientBias2Sum;
    cl_mem gradientBias3Sum;
    cl_mem gradientBias4Sum;

    //Adam first moments
    cl_mem m_weights1;
    cl_mem m_weights2;
    cl_mem m_weights3;
    cl_mem m_weights4;

    cl_mem m_bias1;
    cl_mem m_bias2;
    cl_mem m_bias3;
    cl_mem m_bias4;

    //Adam second moments
    cl_mem v_weights1;
    cl_mem v_weights2;
    cl_mem v_weights3;
    cl_mem v_weights4;

    cl_mem v_bias1;
    cl_mem v_bias2;
    cl_mem v_bias3;
    cl_mem v_bias4;

    cl_mem sparseTimestamps;

} openCLKernelMemory;


extern openCLContext opencl_context;
extern openCLKernelMemory opencl_mem;
extern cl_event readEvent;

int initOpenCL(network_weights_training* trainingNNUE, short* host_activeInputs_A, float* host_expectedOutputs_A,
                                                        short* host_activeInputs_B, float* host_expectedOutputs_B);
void freeOpenCL();

#define ENQUEUE_ADAMW(weights, gradient, firstMoment, secondMoment, size, learningRate, biasCorrection1, biasCorrection2, rectificationTerm) \
    clSetKernelArg(opencl_context.adamw, 0, sizeof(cl_mem), &weights); \
    clSetKernelArg(opencl_context.adamw, 1, sizeof(cl_mem), &gradient); \
    clSetKernelArg(opencl_context.adamw, 2, sizeof(cl_mem), &firstMoment); \
    clSetKernelArg(opencl_context.adamw, 3, sizeof(cl_mem), &secondMoment); \
    clSetKernelArg(opencl_context.adamw, 4, sizeof(cl_float), &learningRate); \
    clSetKernelArg(opencl_context.adamw, 5, sizeof(cl_float), &biasCorrection1); \
    clSetKernelArg(opencl_context.adamw, 6, sizeof(cl_float), &biasCorrection2); \
    clSetKernelArg(opencl_context.adamw, 7, sizeof(cl_float), &rectificationTerm); \
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.adamw, 1, NULL, &size, NULL, 0, NULL, NULL);

#define ENQUEUE_LAZY_ADAM(weights, t, gradient, firstMoment, secondMoment, size, learningRate, rho_inf) \
    clSetKernelArg(opencl_context.lazyadam, 0, sizeof(cl_mem), &weights); \
    clSetKernelArg(opencl_context.lazyadam, 1, sizeof(cl_mem), &t); \
    clSetKernelArg(opencl_context.lazyadam, 2, sizeof(cl_mem), &gradient); \
    clSetKernelArg(opencl_context.lazyadam, 3, sizeof(cl_mem), &firstMoment); \
    clSetKernelArg(opencl_context.lazyadam, 4, sizeof(cl_mem), &secondMoment); \
    clSetKernelArg(opencl_context.lazyadam, 5, sizeof(cl_float), &learningRate); \
    clSetKernelArg(opencl_context.lazyadam, 6, sizeof(cl_float), &rho_inf); \
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.lazyadam, 1, NULL, &size, NULL, 0, NULL, NULL);

#define LOOKAHEAD_UPDATE(fastWeights, slowWeights, size) \
    clSetKernelArg(opencl_context.lookahead, 0, sizeof(cl_mem), &fastWeights); \
    clSetKernelArg(opencl_context.lookahead, 1, sizeof(cl_mem), &slowWeights); \
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.lookahead, 1, NULL, &size, NULL, 0, NULL, NULL);

void enqueueKernels(int bufferSide, double* outputSSE, int doBackprop);
void getWeights(network_weights_training* weights);

#endif