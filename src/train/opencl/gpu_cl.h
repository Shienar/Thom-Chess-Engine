#pragma once

#define CL_TARGET_OPENCL_VERSION 200
#include <CL/cl.h>
#include "debug.h"
#include "analyze/nnue/neuralnet.h"
#include "train/train.h"

//Profiling
#ifdef PERFT_KERNELS
    #define ENQUEUE_EVENT(x) &x
#else
    #define ENQUEUE_EVENT(x) NULL
#endif

#define KERNEL_COUNT 9
#define MEM_COUNT 42
typedef struct openCLContext {
    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
    cl_command_queue queue;
    cl_program program;

    union 
    {
        struct {
            cl_kernel calculateAccumulator;
            cl_kernel calculateOutput;
            cl_kernel calculateDeltas;
            cl_kernel calculate_output_gradient;
            cl_kernel calculate_accumulator_gradient;
            cl_kernel calculate_accumulator_bias_gradient;
            cl_kernel adamw;
            cl_kernel inputadamw;
            cl_kernel lookahead;
        };
        cl_kernel arr[KERNEL_COUNT];
    } kernels;
} openCLContext;

typedef struct clKernelArgs {
    union
    {
        struct
        {
            cl_mem host_activeInputs_A;
            cl_mem host_expectedOutput_A;
            cl_mem host_outputBucket_A;
            
            cl_mem host_activeInputs_B;
            cl_mem host_expectedOutput_B;
            cl_mem host_outputBucket_B;

            cl_mem activeInputs_A;
            cl_mem expectedOutput_A;
            cl_mem outputBucket_A;

            cl_mem activeInputs_B;
            cl_mem expectedOutput_B;
            cl_mem outputBucket_B;

            cl_mem factorizer_weights_fast;
            cl_mem factorizer_weights_slow;

            cl_mem accumulator_weights_fast;
            cl_mem accumulator_weights_slow;

            cl_mem accumulator_bias_fast;
            cl_mem accumulator_bias_slow;
            
            cl_mem output_weights_fast;
            cl_mem output_weights_slow;

            cl_mem output_bias_fast;
            cl_mem output_bias_slow;
            
            cl_mem accumulatorOutput;
            cl_mem finalOutput;
            
            cl_mem loss;
            
            cl_mem output_delta;
            cl_mem accumulator_delta;
            
            cl_mem factorizer_weights_gradient_sum;

            cl_mem accumulator_weights_gradient_sum;
            cl_mem accumulator_bias_gradient_sum;

            cl_mem output_weights_gradient_sum;
            cl_mem output_bias_gradient_sum;

            //Adam first moments
            cl_mem m_factorizer_weights;

            cl_mem m_accumulator_weights;
            cl_mem m_accumulator_bias;

            cl_mem m_output_weights;
            cl_mem m_output_bias;

            //Adam second moments
            cl_mem v_factorizer_weights;

            cl_mem v_accumulator_weights;
            cl_mem v_accumulator_bias;

            cl_mem v_output_weights;
            cl_mem v_output_bias;
        };
        cl_mem arr[MEM_COUNT];
    } mem;

} clKernelArgs;

typedef struct clEvents {
    cl_event readLoss;

    cl_event calcAccum;
    cl_event calculateOutput;
    cl_event calculateDelta;
    cl_event calculate_accumulator_gradient;
    cl_event calculate_accumulator_bias_gradient;
    cl_event calculate_output_gradient;
    cl_event denseUpdate;
    cl_event inputUpdate;

} clEvents;

extern openCLContext openCL_context;
extern clKernelArgs cl_args;
extern clEvents cl_events;

int initKernels(const char* kernelPath,
                    short** h_active_A, float** h_expected_A, char** h_bucket_A,
                    short** h_active_B, float** h_expected_B, char** h_bucket_B,
                    float** h_lossbuffer);
void freeKernels();

#define LOOKAHEAD_UPDATE(fastWeights, slowWeights, size) \
    do { \
        size_t localSize = 32; \
        size_t globalSize = 32 * ((size + 32 - 1) / 32); \
        clSetKernelArg(openCL_context.kernels.lookahead, 0, sizeof(cl_mem), &(fastWeights)); \
        clSetKernelArg(openCL_context.kernels.lookahead, 1, sizeof(cl_mem), &(slowWeights)); \
        clEnqueueNDRangeKernel(openCL_context.queue, openCL_context.kernels.lookahead, 1, NULL, &globalSize, &localSize, 0, NULL, NULL); \
    }while(0)

void enqueueKernels(int bufferSide);
void waitForLossRead();
void saveGPUWeights();