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

//cosine annealing is done using timestamp in enqueueKernels()
#define MIN_LR 1e-4f
#define MAX_LR 1e-3f
#define INTERVAL_SCALE 1.5f
#define FIRST_INTERVAL 20

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
    cl_kernel adam;
} openCLContext;

typedef struct {
    cl_mem activeInputs_A;
    cl_mem expectedOutput_A;

    cl_mem activeInputs_B;
    cl_mem expectedOutput_B;

    cl_mem weights1;
    cl_mem weights2;
    cl_mem weights3;
    cl_mem weights4;

    cl_mem bias1;
    cl_mem bias2;
    cl_mem bias3;
    cl_mem bias4;
    
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

} openCLKernelMemory;


extern openCLContext opencl_context;
extern openCLKernelMemory opencl_mem;
extern cl_event readEvent;

int initOpenCL(network_weights_training* trainingNNUE, short* host_activeInputs_A, float* host_expectedOutputs_A,
                                                        short* host_activeInputs_B, float* host_expectedOutputs_B);
void freeOpenCL();

#define ENQUEUE_ADAM(weights, gradient, firstMoment, secondMoment, size, learningRate, biasCorrection1, biasCorrection2) \
    clSetKernelArg(opencl_context.adam, 0, sizeof(cl_mem), &weights); \
    clSetKernelArg(opencl_context.adam, 1, sizeof(cl_mem), &gradient); \
    clSetKernelArg(opencl_context.adam, 2, sizeof(cl_mem), &firstMoment); \
    clSetKernelArg(opencl_context.adam, 3, sizeof(cl_mem), &secondMoment); \
    clSetKernelArg(opencl_context.adam, 4, sizeof(cl_float), &learningRate); \
    clSetKernelArg(opencl_context.adam, 5, sizeof(cl_float), &biasCorrection1); \
    clSetKernelArg(opencl_context.adam, 6, sizeof(cl_float), &biasCorrection2); \
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.adam, 1, NULL, &size, NULL, 0, NULL, NULL);

void enqueueKernels(int bufferSide, double* outputSSE);
void getWeights(network_weights_training* weights);

#endif