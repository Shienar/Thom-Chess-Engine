#ifndef GPU_FUNCS
#define GPU_FUNCS

#define CL_TARGET_OPENCL_VERSION 200
#include <CL/cl.h>
#include "debug.h"
#include "analyze/neuralnet.h"

//Profiling
#ifdef PERFT_KERNELS
    #define ENQUEUE_EVENT(x) &x
#else
    #define ENQUEUE_EVENT(x) NULL
#endif

//CPU fills one group while gpu uses other group.
#define INPUT_GROUP(block) (block%2)
#define INPUT_GROUP_A 0
#define INPUT_GROUP_B 1

/**
 * cosine annealing is done using timestamp in enqueueKernels()
 */
#define MAX_LR 8e-4f
#define MIN_LR 2.5e-6f
#define INTERVAL_SCALE 2
#define FIRST_INTERVAL MINIBATCHES_PER_EPOCH
#define MAX_INTERVALS 9
#define LOOKAHEAD_RANGE 10

#define KERNEL_COUNT 10
#define MEM_COUNT 56
typedef struct {
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;

    //One kernel per function to run.
    //A union is used for iteration.
    union 
    {
        struct {
            cl_kernel calculateAccumulator;
            cl_kernel forwardPropagate;
            cl_kernel backpropagate;
            cl_kernel calculateGradient4;
            cl_kernel calculateGradient3;
            cl_kernel calculateGradient2;
            cl_kernel calculateGradient1;
            cl_kernel adamw;
            cl_kernel lazyadam;
            cl_kernel lookahead;
        };
        cl_kernel arr[KERNEL_COUNT];
    } kernels;
} openCLContext;

typedef struct {
    union
    {
        struct
        {
            cl_mem activeInputs_A;
            cl_mem expectedOutput_A;
            cl_mem outputBucket_A;

            cl_mem activeInputs_B;
            cl_mem expectedOutput_B;
            cl_mem outputBucket_B;

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
            
            cl_mem loss;
            
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
        };
        cl_mem arr[MEM_COUNT];
    } mem;

} openCLKernelMemory;

extern openCLContext opencl_context;
extern openCLKernelMemory opencl_mem;
extern cl_event readEvent;

int initOpenCL(network_weights* nnue_weights, short* h_active_A, float* h_expected_A, char* h_output_A,
                                                        short* h_active_B, float* h_expected_B, char* h_output_B,
                                                        double* h_lossbuffer);
void freeOpenCL();

#define ENQUEUE_LAZY_ADAM(weights, t, gradient, firstMoment, secondMoment, size, learningRate, rho_inf, event) \
    clSetKernelArg(opencl_context.kernels.lazyadam, 0, sizeof(cl_mem), &weights); \
    clSetKernelArg(opencl_context.kernels.lazyadam, 1, sizeof(cl_mem), &t); \
    clSetKernelArg(opencl_context.kernels.lazyadam, 2, sizeof(cl_mem), &gradient); \
    clSetKernelArg(opencl_context.kernels.lazyadam, 3, sizeof(cl_mem), &firstMoment); \
    clSetKernelArg(opencl_context.kernels.lazyadam, 4, sizeof(cl_mem), &secondMoment); \
    clSetKernelArg(opencl_context.kernels.lazyadam, 5, sizeof(cl_float), &learningRate); \
    clSetKernelArg(opencl_context.kernels.lazyadam, 6, sizeof(cl_float), &rho_inf); \
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.kernels.lazyadam, 1, NULL, &size, NULL, 0, NULL, ENQUEUE_EVENT(event));

#define LOOKAHEAD_UPDATE(fastWeights, slowWeights, size) \
    clSetKernelArg(opencl_context.kernels.lookahead, 0, sizeof(cl_mem), &fastWeights); \
    clSetKernelArg(opencl_context.kernels.lookahead, 1, sizeof(cl_mem), &slowWeights); \
    clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.kernels.lookahead, 1, NULL, &size, NULL, 0, NULL, NULL);

void enqueueKernels(int bufferSide, int doBackprop);
void getWeights(network_weights* weights);

#endif