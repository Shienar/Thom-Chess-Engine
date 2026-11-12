#include "train/opencl/gpu_cl.h"
#include "compatibility.h"
#include "debug.h"
#include "analyze/nnue/neuralnet.h"
#include "train/train.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

openCLContext openCL_context = {
    .platform = 0,
    .device = 0,
    .context = NULL,
    .queue = NULL,
    .program = NULL,
    .kernels = { { NULL } }
};
clKernelArgs cl_args = { 0 };
clEvents cl_events = { 0};

cl_float lr = 3e-3f;
uint64_t cosineTimestamp;
uint64_t timestamp;
float rho_inf = (2.0 / (1.0 - ADAM_BETA2)) - 1.0;
float rho_timestamp = 0.0;
cl_float rectificationTerm = 0.0;
cl_float biasCorrection1 = 1.0;
cl_float biasCorrection2 = 1.0;

const size_t factorizer_weights_size    = sizeof(((training_weights*)0)->factorizer_weights);
const size_t factorizer_weights_count   = sizeof(((training_weights*)0)->factorizer_weights) / sizeof(float);

const size_t accumulator_weights_size   = sizeof(((training_weights*)0)->accumulator_weights);
const size_t accumulator_weights_count  = sizeof(((training_weights*)0)->accumulator_weights) / sizeof(float);

const size_t accumulator_bias_size      = sizeof(((training_weights*)0)->accumulator_bias);
const size_t accumulator_bias_count     = sizeof(((training_weights*)0)->accumulator_bias) / sizeof(float);

const size_t output_weights_size        = sizeof(((training_weights*)0)->output_weights);
const size_t output_weights_count       = sizeof(((training_weights*)0)->output_weights) / sizeof(float);

const size_t output_bias_size           = sizeof(((training_weights*)0)->output_bias);
const size_t output_bias_count          = sizeof(((training_weights*)0)->output_bias) / sizeof(float);

const size_t activeInputs_size      = MINIBATCH_SIZE * 64 * sizeof(short);
const size_t expectedOutput_size    = MINIBATCH_SIZE * sizeof(float);
const size_t outputBucket_size      = MINIBATCH_SIZE * sizeof(char);
const size_t loss_size              = MINIBATCH_SIZE * sizeof(float);
const size_t accumulatorOutput_size = MINIBATCH_SIZE * ACCUMULATOR_NODES * sizeof(float);
const size_t finalOutput_size       = MINIBATCH_SIZE * sizeof(float);
const size_t output_delta_size      = MINIBATCH_SIZE * sizeof(float);
const size_t accum_delta_size       = MINIBATCH_SIZE * ACCUMULATOR_NODES * sizeof(float);

short* host_activeInputs_A      = NULL; 
short* host_activeInputs_B      = NULL;
float* host_expectedOutputs_A   = NULL;
float* host_expectedOutputs_B   = NULL;
char*  host_outputBuckets_A     = NULL;
char*  host_outputBuckets_B     = NULL;
float* host_lossbuffer          = NULL;

nnue_weights*     host_quantized_weights = NULL;
training_weights* host_training_weights  = NULL;
training_weights* host_first_moments     = NULL;
training_weights* host_second_moments    = NULL;

char* getKernelFunctions(const char* fileName)
{
    FILE* input = fopen(fileName, "rb"); 
    if(!input)
    {
        printf("Cannot locate gpu kernels at %s\n", fileName);
        return NULL;
    }
    
    fseek(input, 0, SEEK_END);
    size_t size = ftell(input);
    rewind(input);

    char* source = calloc(size + 1, sizeof(char));
    if(!source) 
    { 
        DEBUG_ERROR("Failed malloc.");
        fclose(input);
        return NULL; 
    }

    size_t bytesRead = fread(source, 1, size, input);
    source[bytesRead] = '\0';
    
    fclose(input);

    return source;
}

int initKernels(const char* kernelPath,
                short** h_active_A, float** h_expected_A, char** h_bucket_A,
                short** h_active_B, float** h_expected_B, char** h_bucket_B,
                float** h_lossbuffer)
{
    cosineTimestamp = 0;
    timestamp = 0;

    int err = 0;

    err = clGetPlatformIDs(1, &openCL_context.platform, NULL);
    err = clGetDeviceIDs(openCL_context.platform, CL_DEVICE_TYPE_GPU, 1, &openCL_context.device, NULL);
    openCL_context.context = clCreateContext(NULL, 1, &openCL_context.device, NULL, NULL, &err);
    
    #ifdef PERFT_KERNELS
        cl_queue_properties queueProperties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
        openCL_context.queue = clCreateCommandQueueWithProperties(openCL_context.context, openCL_context.device, queueProperties, &err);
    #else
        openCL_context.queue = clCreateCommandQueueWithProperties(openCL_context.context, openCL_context.device, NULL, &err);
    #endif
    
    char* kernelSource = getKernelFunctions(kernelPath);
    openCL_context.program = clCreateProgramWithSource(openCL_context.context, 1, (const char**)&kernelSource, NULL, &err);
    free(kernelSource);
    kernelSource = NULL;

    const char* options = "-cl-std=CL2.0";
    err = clBuildProgram(openCL_context.program, 1, &openCL_context.device, options, NULL, NULL);
    
    if(err)
    {
        char log[4096];
        clGetProgramBuildInfo(openCL_context.program, openCL_context.device, CL_PROGRAM_BUILD_LOG, 4096, log, NULL);
        printf("Build Error:\n%s\n", log);
        return err;
    }

    openCL_context.kernels.calculateAccumulator                 = clCreateKernel(openCL_context.program, "calculateAccumulator", &err);
    openCL_context.kernels.calculateOutput                      = clCreateKernel(openCL_context.program, "calculateOutput", &err);
    openCL_context.kernels.calculateDeltas                      = clCreateKernel(openCL_context.program, "calculateDeltas", &err); 
    openCL_context.kernels.calculate_output_gradient            = clCreateKernel(openCL_context.program, "calculate_output_gradient", &err);
    openCL_context.kernels.calculate_accumulator_bias_gradient  = clCreateKernel(openCL_context.program, "calculate_accumulator_bias_gradient", &err);
    openCL_context.kernels.calculate_accumulator_gradient       = clCreateKernel(openCL_context.program, "calculate_accumulator_gradient", &err);
    openCL_context.kernels.adamw                                = clCreateKernel(openCL_context.program, "adamW", &err);
    openCL_context.kernels.inputadamw                           = clCreateKernel(openCL_context.program, "inputAdamW", &err);
    openCL_context.kernels.lookahead                            = clCreateKernel(openCL_context.program, "lookahead_update", &err);
        
    /** ALLOCATING **/
    //Existing weights/moments from file.
    //Their copying is infrequent so I'm just doing calloc.
    alignedAlloc(host_quantized_weights, 4096, sizeof(nnue_weights));
    alignedAlloc(host_training_weights, 4096, sizeof(training_weights));
    alignedAlloc(host_first_moments, 4096, sizeof(training_weights));
    alignedAlloc(host_second_moments, 4096, sizeof(training_weights));
    //Load weights/moments from file or initialize them.
    //Calloc leaves moments at zero if not in file.
    FILE* rawInput = NULL;
    if((rawInput = fopen("./import/raw.bin", "rb")) != NULL)
    {
        fread(host_training_weights, sizeof(training_weights), 1, rawInput);
        fclose(rawInput);
        rawInput = NULL;

        FILE* mInput = NULL;
        FILE* vInput = NULL;
        //Require both or none of first/second moments.
        if((mInput = fopen("./import/firstMoments.bin", "rb")) != NULL)
        {
            if((vInput = fopen("./import/secondMoments.bin", "rb")) != NULL)
            {   
                fread(host_first_moments,  sizeof(training_weights), 1, mInput);
                fread(host_second_moments, sizeof(training_weights), 1, vInput);
                fclose(vInput);
            }
            fclose(mInput);
        }
    }
    else
        initializeTrainingWeights(host_training_weights);
    //Host pinning (Staging buffers)
    cl_args.mem.host_activeInputs_A     = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, activeInputs_size,   NULL, NULL);
    cl_args.mem.host_activeInputs_B     = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, activeInputs_size,   NULL, NULL);
    cl_args.mem.host_expectedOutput_A   = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, expectedOutput_size, NULL, NULL);
    cl_args.mem.host_expectedOutput_B   = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, expectedOutput_size, NULL, NULL);
    cl_args.mem.host_outputBucket_A     = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, outputBucket_size,   NULL, NULL);
    cl_args.mem.host_outputBucket_B     = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, outputBucket_size,   NULL, NULL);
    //Save & export pointers
    *h_active_A     = host_activeInputs_A    = (short*) clEnqueueMapBuffer(openCL_context.queue, cl_args.mem.host_activeInputs_A,   CL_TRUE, CL_MAP_WRITE | CL_MAP_READ, 0, activeInputs_size,   0, NULL, NULL, &err);
    *h_active_B     = host_activeInputs_B    = (short*) clEnqueueMapBuffer(openCL_context.queue, cl_args.mem.host_activeInputs_B,   CL_TRUE, CL_MAP_WRITE | CL_MAP_READ, 0, activeInputs_size,   0, NULL, NULL, &err);
    *h_expected_A   = host_expectedOutputs_A = (float*) clEnqueueMapBuffer(openCL_context.queue, cl_args.mem.host_expectedOutput_A, CL_TRUE, CL_MAP_WRITE | CL_MAP_READ, 0, expectedOutput_size, 0, NULL, NULL, &err);
    *h_expected_B   = host_expectedOutputs_B = (float*) clEnqueueMapBuffer(openCL_context.queue, cl_args.mem.host_expectedOutput_B, CL_TRUE, CL_MAP_WRITE | CL_MAP_READ, 0, expectedOutput_size, 0, NULL, NULL, &err);
    *h_bucket_A     = host_outputBuckets_A   = (char*)  clEnqueueMapBuffer(openCL_context.queue, cl_args.mem.host_outputBucket_A,   CL_TRUE, CL_MAP_WRITE | CL_MAP_READ, 0, outputBucket_size,   0, NULL, NULL, &err);
    *h_bucket_B     = host_outputBuckets_B   = (char*)  clEnqueueMapBuffer(openCL_context.queue, cl_args.mem.host_outputBucket_B,   CL_TRUE, CL_MAP_WRITE | CL_MAP_READ, 0, outputBucket_size,   0, NULL, NULL, &err);
    alignedAlloc(host_lossbuffer, 4096, loss_size);
    *h_lossbuffer   = host_lossbuffer;
    //gpu buffers (copied into from host-pinned memory)
    cl_args.mem.activeInputs_A      = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, activeInputs_size,     NULL, NULL);
    cl_args.mem.activeInputs_B      = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, activeInputs_size,     NULL, NULL);
    cl_args.mem.expectedOutput_A    = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, expectedOutput_size,   NULL, NULL);
    cl_args.mem.expectedOutput_B    = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, expectedOutput_size,   NULL, NULL);
    cl_args.mem.outputBucket_A      = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, outputBucket_size,     NULL, NULL);
    cl_args.mem.outputBucket_B      = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, outputBucket_size,     NULL, NULL);
    cl_args.mem.loss                = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, loss_size,             NULL, NULL);
    //Fast weights
    cl_args.mem.factorizer_weights_fast     = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, factorizer_weights_size,   (void*)host_training_weights->factorizer_weights, NULL);
    cl_args.mem.accumulator_weights_fast    = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, accumulator_weights_size,  (void*)host_training_weights->accumulator_weights, NULL);
    cl_args.mem.accumulator_bias_fast       = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, accumulator_bias_size,     (void*)host_training_weights->accumulator_bias, NULL);
    cl_args.mem.output_weights_fast         = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, output_weights_size,       (void*)host_training_weights->output_weights, NULL);
    cl_args.mem.output_bias_fast            = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, output_bias_size,          (void*)host_training_weights->output_bias, NULL);
    //Slow Weights
    cl_args.mem.factorizer_weights_slow     = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, factorizer_weights_size,   (void*)host_training_weights->factorizer_weights, NULL);
    cl_args.mem.accumulator_weights_slow    = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, accumulator_weights_size,  (void*)host_training_weights->accumulator_weights, NULL);
    cl_args.mem.accumulator_bias_slow       = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, accumulator_bias_size,     (void*)host_training_weights->accumulator_bias, NULL);
    cl_args.mem.output_weights_slow         = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, output_weights_size,       (void*)host_training_weights->output_weights, NULL);
    cl_args.mem.output_bias_slow            = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, output_bias_size,          (void*)host_training_weights->output_bias, NULL);
    //Intermediate Outputs
    cl_args.mem.accumulatorOutput   = clCreateBuffer(openCL_context.context, CL_MEM_READ_WRITE, accumulatorOutput_size, NULL, NULL);
    cl_args.mem.finalOutput         = clCreateBuffer(openCL_context.context, CL_MEM_READ_WRITE, finalOutput_size, NULL, NULL);
    //Deltas
    cl_args.mem.output_delta           = clCreateBuffer(openCL_context.context, CL_MEM_READ_WRITE, output_delta_size, NULL, NULL);
    cl_args.mem.accumulator_delta      = clCreateBuffer(openCL_context.context, CL_MEM_READ_WRITE, accum_delta_size, NULL, NULL);
    //Gradient Sums
    cl_args.mem.factorizer_weights_gradient_sum     = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, factorizer_weights_size,   NULL, NULL);
    cl_args.mem.accumulator_weights_gradient_sum    = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, accumulator_weights_size,  NULL, NULL);
    cl_args.mem.accumulator_bias_gradient_sum       = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, accumulator_bias_size,     NULL, NULL);
    cl_args.mem.output_weights_gradient_sum         = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, output_weights_size,       NULL, NULL);
    cl_args.mem.output_bias_gradient_sum            = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, output_bias_size,          NULL, NULL);
    //Adam First moments
    cl_args.mem.m_factorizer_weights    = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, factorizer_weights_size,   NULL, NULL);
    cl_args.mem.m_accumulator_weights   = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, accumulator_weights_size,  NULL, NULL);
    cl_args.mem.m_accumulator_bias      = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, accumulator_bias_size,     NULL, NULL);
    cl_args.mem.m_output_weights        = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, output_weights_size,       NULL, NULL);
    cl_args.mem.m_output_bias           = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, output_bias_size,          NULL, NULL);
    //Adam Second moments
    cl_args.mem.v_factorizer_weights    = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, factorizer_weights_size,   NULL, NULL);
    cl_args.mem.v_accumulator_weights   = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, accumulator_weights_size,  NULL, NULL);
    cl_args.mem.v_accumulator_bias      = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, accumulator_bias_size,     NULL, NULL);
    cl_args.mem.v_output_weights        = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, output_weights_size,       NULL, NULL);
    cl_args.mem.v_output_bias           = clCreateBuffer(openCL_context.context,  CL_MEM_READ_WRITE, output_bias_size,          NULL, NULL);

    /** ZEROING **/
    float zero = 0.0f;
    //Gradients
    clEnqueueFillBuffer(openCL_context.queue, cl_args.mem.factorizer_weights_gradient_sum,  &zero, sizeof(float), 0, factorizer_weights_size,   0, NULL, NULL);
    clEnqueueFillBuffer(openCL_context.queue, cl_args.mem.accumulator_weights_gradient_sum, &zero, sizeof(float), 0, accumulator_weights_size,  0, NULL, NULL);
    clEnqueueFillBuffer(openCL_context.queue, cl_args.mem.accumulator_bias_gradient_sum,    &zero, sizeof(float), 0, accumulator_bias_size,     0, NULL, NULL);
    clEnqueueFillBuffer(openCL_context.queue, cl_args.mem.output_weights_gradient_sum,      &zero, sizeof(float), 0, output_weights_size,       0, NULL, NULL);
    clEnqueueFillBuffer(openCL_context.queue, cl_args.mem.output_bias_gradient_sum,         &zero, sizeof(float), 0, output_bias_size,          0, NULL, NULL);
    //First moments 
    clEnqueueFillBuffer(openCL_context.queue, cl_args.mem.m_factorizer_weights,  &zero, sizeof(float), 0, factorizer_weights_size,   0, NULL, NULL);
    clEnqueueFillBuffer(openCL_context.queue, cl_args.mem.m_accumulator_weights, &zero, sizeof(float), 0, accumulator_weights_size,  0, NULL, NULL);
    clEnqueueFillBuffer(openCL_context.queue, cl_args.mem.m_accumulator_bias,    &zero, sizeof(float), 0, accumulator_bias_size,     0, NULL, NULL);
    clEnqueueFillBuffer(openCL_context.queue, cl_args.mem.m_output_weights,      &zero, sizeof(float), 0, output_weights_size,       0, NULL, NULL);
    clEnqueueFillBuffer(openCL_context.queue, cl_args.mem.m_output_bias,         &zero, sizeof(float), 0, output_bias_size,          0, NULL, NULL);
    //Second moments
    clEnqueueFillBuffer(openCL_context.queue, cl_args.mem.v_factorizer_weights,  &zero, sizeof(float), 0, factorizer_weights_size,   0, NULL, NULL);
    clEnqueueFillBuffer(openCL_context.queue, cl_args.mem.v_accumulator_weights, &zero, sizeof(float), 0, accumulator_weights_size,  0, NULL, NULL);
    clEnqueueFillBuffer(openCL_context.queue, cl_args.mem.v_accumulator_bias,    &zero, sizeof(float), 0, accumulator_bias_size,     0, NULL, NULL);
    clEnqueueFillBuffer(openCL_context.queue, cl_args.mem.v_output_weights,      &zero, sizeof(float), 0, output_weights_size,       0, NULL, NULL);
    clEnqueueFillBuffer(openCL_context.queue, cl_args.mem.v_output_bias,         &zero, sizeof(float), 0, output_bias_size,          0, NULL, NULL);

    clFinish(openCL_context.queue);

    /** Set static arguments */

    //clSetKernelArg(openCL_context.kernels.calculateAccumulator, 0, sizeof(cl_mem), active input); 
    clSetKernelArg(openCL_context.kernels.calculateAccumulator, 1, sizeof(cl_mem), &cl_args.mem.accumulator_weights_fast);
    clSetKernelArg(openCL_context.kernels.calculateAccumulator, 2, sizeof(cl_mem), &cl_args.mem.factorizer_weights_fast);
    clSetKernelArg(openCL_context.kernels.calculateAccumulator, 3, sizeof(cl_mem), &cl_args.mem.accumulator_bias_fast);
    clSetKernelArg(openCL_context.kernels.calculateAccumulator, 4, sizeof(cl_mem), &cl_args.mem.accumulatorOutput);

    clSetKernelArg(openCL_context.kernels.calculateOutput, 0, sizeof(cl_mem), &cl_args.mem.accumulatorOutput);
    clSetKernelArg(openCL_context.kernels.calculateOutput, 1, sizeof(cl_mem), &cl_args.mem.output_weights_fast);
    clSetKernelArg(openCL_context.kernels.calculateOutput, 2, sizeof(cl_mem), &cl_args.mem.output_bias_fast);
    clSetKernelArg(openCL_context.kernels.calculateOutput, 3, sizeof(cl_mem), &cl_args.mem.finalOutput);
    // clSetKernelArg(openCL_context.kernels.calculateOutput, 4, sizeof(cl_mem), output bucket); 

    clSetKernelArg(openCL_context.kernels.calculateDeltas, 0, sizeof(cl_mem), &cl_args.mem.finalOutput);
    //clSetKernelArg(openCL_context.kernels.calculateDeltas, 1, sizeof(cl_mem), expected output); 
    clSetKernelArg(openCL_context.kernels.calculateDeltas, 2, sizeof(cl_mem), &cl_args.mem.accumulatorOutput);
    clSetKernelArg(openCL_context.kernels.calculateDeltas, 3, sizeof(cl_mem), &cl_args.mem.output_weights_fast);
    clSetKernelArg(openCL_context.kernels.calculateDeltas, 4, sizeof(cl_mem), &cl_args.mem.output_delta);
    clSetKernelArg(openCL_context.kernels.calculateDeltas, 5, sizeof(cl_mem), &cl_args.mem.accumulator_delta);
    clSetKernelArg(openCL_context.kernels.calculateDeltas, 6, sizeof(cl_mem), &cl_args.mem.loss);
    //clSetKernelArg(openCL_context.kernels.calculateDeltas, 7, sizeof(cl_mem), output buckets); 

    clSetKernelArg(openCL_context.kernels.calculate_output_gradient, 0, sizeof(cl_mem), &cl_args.mem.output_delta);
    clSetKernelArg(openCL_context.kernels.calculate_output_gradient, 1, sizeof(cl_mem), &cl_args.mem.accumulatorOutput);
    clSetKernelArg(openCL_context.kernels.calculate_output_gradient, 2, sizeof(cl_mem), &cl_args.mem.output_weights_gradient_sum);
    clSetKernelArg(openCL_context.kernels.calculate_output_gradient, 3, sizeof(cl_mem), &cl_args.mem.output_bias_gradient_sum);
    //clSetKernelArg(openCL_context.kernels.calculate_output_gradient, 4, sizeof(cl_mem), output bucket); 

    //clSetKernelArg(openCL_context.kernels.calculate_accumulator_gradient, 0, sizeof(cl_mem), active inputs); 
    clSetKernelArg(openCL_context.kernels.calculate_accumulator_gradient, 1, sizeof(cl_mem), &cl_args.mem.accumulator_delta);
    clSetKernelArg(openCL_context.kernels.calculate_accumulator_gradient, 2, sizeof(cl_mem), &cl_args.mem.factorizer_weights_gradient_sum);
    clSetKernelArg(openCL_context.kernels.calculate_accumulator_gradient, 3, sizeof(cl_mem), &cl_args.mem.accumulator_weights_gradient_sum);

    clSetKernelArg(openCL_context.kernels.calculate_accumulator_bias_gradient, 0, sizeof(cl_mem), &cl_args.mem.accumulator_delta);
    clSetKernelArg(openCL_context.kernels.calculate_accumulator_bias_gradient, 1, sizeof(cl_mem), &cl_args.mem.accumulator_bias_gradient_sum);

    clSetKernelArg(openCL_context.kernels.inputadamw, 0, sizeof(cl_mem), &cl_args.mem.accumulator_weights_fast);
    clSetKernelArg(openCL_context.kernels.inputadamw, 1, sizeof(cl_mem), &cl_args.mem.accumulator_weights_gradient_sum);
    clSetKernelArg(openCL_context.kernels.inputadamw, 2, sizeof(cl_mem), &cl_args.mem.m_accumulator_weights);
    clSetKernelArg(openCL_context.kernels.inputadamw, 3, sizeof(cl_mem), &cl_args.mem.v_accumulator_weights);
    clSetKernelArg(openCL_context.kernels.inputadamw, 4, sizeof(cl_mem), &cl_args.mem.factorizer_weights_fast);
    clSetKernelArg(openCL_context.kernels.inputadamw, 5, sizeof(cl_mem), &cl_args.mem.factorizer_weights_gradient_sum);
    clSetKernelArg(openCL_context.kernels.inputadamw, 6, sizeof(cl_mem), &cl_args.mem.m_factorizer_weights);
    clSetKernelArg(openCL_context.kernels.inputadamw, 7, sizeof(cl_mem), &cl_args.mem.v_factorizer_weights);
    clSetKernelArg(openCL_context.kernels.inputadamw, 8, sizeof(float), &lr);
    clSetKernelArg(openCL_context.kernels.inputadamw, 9, sizeof(float), &biasCorrection1);
    clSetKernelArg(openCL_context.kernels.inputadamw, 10, sizeof(float), &biasCorrection2);
    clSetKernelArg(openCL_context.kernels.inputadamw, 11, sizeof(float), &rectificationTerm);

    clSetKernelArg(openCL_context.kernels.adamw, 0, sizeof(cl_mem), &cl_args.mem.output_weights_fast);                
    clSetKernelArg(openCL_context.kernels.adamw, 1, sizeof(cl_mem), &cl_args.mem.output_weights_gradient_sum);        
    clSetKernelArg(openCL_context.kernels.adamw, 2, sizeof(cl_mem), &cl_args.mem.m_output_weights);                   
    clSetKernelArg(openCL_context.kernels.adamw, 3, sizeof(cl_mem), &cl_args.mem.v_output_weights);                   
    clSetKernelArg(openCL_context.kernels.adamw, 4, sizeof(cl_mem), &cl_args.mem.accumulator_bias_fast);              
    clSetKernelArg(openCL_context.kernels.adamw, 5, sizeof(cl_mem), &cl_args.mem.accumulator_bias_gradient_sum);      
    clSetKernelArg(openCL_context.kernels.adamw, 6, sizeof(cl_mem), &cl_args.mem.m_accumulator_bias);                 
    clSetKernelArg(openCL_context.kernels.adamw, 7, sizeof(cl_mem), &cl_args.mem.v_accumulator_bias);                 
    clSetKernelArg(openCL_context.kernels.adamw, 8, sizeof(cl_mem), &cl_args.mem.output_bias_fast);
    clSetKernelArg(openCL_context.kernels.adamw, 9, sizeof(cl_mem), &cl_args.mem.output_bias_gradient_sum);
    clSetKernelArg(openCL_context.kernels.adamw, 10, sizeof(cl_mem), &cl_args.mem.m_output_bias);
    clSetKernelArg(openCL_context.kernels.adamw, 11, sizeof(cl_mem), &cl_args.mem.v_output_bias);
    clSetKernelArg(openCL_context.kernels.adamw, 12, sizeof(float), &lr);
    clSetKernelArg(openCL_context.kernels.adamw, 13, sizeof(float), &biasCorrection1);
    clSetKernelArg(openCL_context.kernels.adamw, 14, sizeof(float), &biasCorrection2);
    clSetKernelArg(openCL_context.kernels.adamw, 15, sizeof(float), &rectificationTerm);

    return 0;
}

void freeKernels()
{

    clEnqueueUnmapMemObject(openCL_context.queue, cl_args.mem.host_activeInputs_A, host_activeInputs_A, 0, NULL, NULL);
    clEnqueueUnmapMemObject(openCL_context.queue, cl_args.mem.host_activeInputs_B, host_activeInputs_B, 0, NULL, NULL);
    clEnqueueUnmapMemObject(openCL_context.queue, cl_args.mem.host_expectedOutput_A, host_expectedOutputs_A, 0, NULL, NULL);
    clEnqueueUnmapMemObject(openCL_context.queue, cl_args.mem.host_expectedOutput_B, host_expectedOutputs_B, 0, NULL, NULL);
    clEnqueueUnmapMemObject(openCL_context.queue, cl_args.mem.host_outputBucket_A, host_outputBuckets_A, 0, NULL, NULL);
    clEnqueueUnmapMemObject(openCL_context.queue, cl_args.mem.host_outputBucket_B, host_outputBuckets_B, 0, NULL, NULL);
    clFinish(openCL_context.queue);

    for(int i = 0; i < KERNEL_COUNT; i++) 
        clReleaseKernel(openCL_context.kernels.arr[i]);
    
    clReleaseProgram(openCL_context.program);
    clReleaseCommandQueue(openCL_context.queue);

    if(host_quantized_weights)  alignedFree(host_quantized_weights);
    if(host_training_weights)   alignedFree(host_training_weights);
    if(host_first_moments)      alignedFree(host_first_moments);
    if(host_second_moments)     alignedFree(host_second_moments);
    if(host_lossbuffer)         alignedFree(host_lossbuffer);

    for(int i = 0; i < MEM_COUNT; i++) 
    {
        if(cl_args.mem.arr[i]) 
        {
            clReleaseMemObject(cl_args.mem.arr[i]);
            cl_args.mem.arr[i] = NULL;
        }
    }
    
    clReleaseContext(openCL_context.context);
}

double print_prof(const char* name, cl_event e)
{
    cl_ulong start, end;
    clGetEventProfilingInfo(e, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
    clGetEventProfilingInfo(e, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);

    double execute_ms = (end - start) / 1000000.0;
    printf("%-30s | %0.4f ms\n", name, execute_ms);
    clReleaseEvent(e);
    return execute_ms;
}

size_t calcAccumulator_GlobalSize[3]  = { 32 * MINIBATCH_SIZE * 2, 1, 1};
size_t calcAccumulator_LocalSize[3]   = { 32,                      1, 1};

size_t calcOutput_GlobalSize[3]  = { 32 * MINIBATCH_SIZE, 1, 1};
size_t calcOutput_LocalSize[3]   = { 32,                  1, 1};

size_t calcDeltas_GlobalSize[3]  = { ACCUMULATOR_NODES_PER_SIDE * MINIBATCH_SIZE,   1, 1};
size_t calcDeltas_LocalSize[3]   = { ACCUMULATOR_NODES_PER_SIDE,                    1, 1};

size_t calcOutputGradient_GlobalSize[3]  = { 32 * OUTPUT_BUCKETS * ACCUMULATOR_NODES, 1, 1};
size_t calcOutputGradient_LocalSize[3]   = { 32,                                      1, 1};

size_t calcAccumulatorBiasGradient_GlobalSize[3]  = { 32 * ACCUMULATOR_NODES_PER_SIDE, 1, 1};
size_t calcAccumulatorBiasGradient_LocalSize[3]   = { 32,                              1, 1};

size_t calcAccumulatorGradient_GlobalSize[3]  = { 64 * MINIBATCH_SIZE, 1, 1};
size_t calcAccumulatorGradient_LocalSize[3]   = { 64,                  1, 1};

//weights are guaranteed divisible by 32
size_t adamw_GlobalSize[3]  = { output_weights_count, 1, 1};
size_t adamw_LocalSize[3]   = { 32,                        1, 1};

size_t inputAdamw_GlobalSize[3]  = { accumulator_weights_count, 1, 1};
size_t inputAdamw_LocalSize[3] = { 32,                             1, 1};

void waitForLossRead() { clWaitForEvents(1, &cl_events.readLoss); clReleaseEvent(cl_events.readLoss); }

void enqueueKernels(int bufferSide)
{
    //cosine annealing
    cosineTimestamp = _min(cosineTimestamp + 1, max_cosine_anneal_timestamp);
    lr = min_lr + 0.5 * (max_lr - min_lr) * (1.0 + cos(PI * (cosineTimestamp) / max_cosine_anneal_timestamp));
    lr = _min(max_lr, _max(min_lr, lr));

    clEnqueueWriteBuffer(openCL_context.queue, (bufferSide == INPUT_GROUP_A) ? cl_args.mem.activeInputs_A : cl_args.mem.activeInputs_B, CL_FALSE, 0, activeInputs_size, 
                            (bufferSide == INPUT_GROUP_A) ? host_activeInputs_A : host_activeInputs_B, 0, NULL, NULL);
    clEnqueueWriteBuffer(openCL_context.queue, (bufferSide == INPUT_GROUP_A) ? cl_args.mem.expectedOutput_A : cl_args.mem.expectedOutput_B, CL_FALSE, 0, expectedOutput_size, 
                            (bufferSide == INPUT_GROUP_A) ? host_expectedOutputs_A : host_expectedOutputs_B, 0, NULL, NULL);
    clEnqueueWriteBuffer(openCL_context.queue, (bufferSide == INPUT_GROUP_A) ? cl_args.mem.outputBucket_A : cl_args.mem.outputBucket_B, CL_FALSE, 0, outputBucket_size, 
                            (bufferSide == INPUT_GROUP_A) ? host_outputBuckets_A : host_outputBuckets_B, 0, NULL, NULL);
                
    float zero = 0.0f;
    clEnqueueFillBuffer(openCL_context.queue, cl_args.mem.loss, &zero, sizeof(float), 0, loss_size, 0, NULL, NULL);

    clSetKernelArg(openCL_context.kernels.calculateAccumulator, 0, sizeof(cl_mem), (bufferSide == INPUT_GROUP_A) ? &cl_args.mem.activeInputs_A : &cl_args.mem.activeInputs_B);
    clEnqueueNDRangeKernel(openCL_context.queue, openCL_context.kernels.calculateAccumulator, 3, NULL, calcAccumulator_GlobalSize, calcAccumulator_LocalSize, 0, NULL, ENQUEUE_EVENT(cl_events.calcAccum));

    clSetKernelArg(openCL_context.kernels.calculateOutput, 4, sizeof(cl_mem), (bufferSide == INPUT_GROUP_A) ? &cl_args.mem.outputBucket_A : &cl_args.mem.outputBucket_B);
    clEnqueueNDRangeKernel(openCL_context.queue, openCL_context.kernels.calculateOutput, 3, NULL, calcOutput_GlobalSize, calcOutput_LocalSize, 0, NULL, ENQUEUE_EVENT(cl_events.calculateOutput));

    clSetKernelArg(openCL_context.kernels.calculateDeltas, 1, sizeof(cl_mem), (bufferSide == INPUT_GROUP_A) ? &cl_args.mem.expectedOutput_A : &cl_args.mem.expectedOutput_B);
    clSetKernelArg(openCL_context.kernels.calculateDeltas, 7, sizeof(cl_mem), (bufferSide == INPUT_GROUP_A) ? &cl_args.mem.outputBucket_A : &cl_args.mem.outputBucket_B);
    clEnqueueNDRangeKernel(openCL_context.queue, openCL_context.kernels.calculateDeltas, 3, NULL, calcDeltas_GlobalSize, calcDeltas_LocalSize, 0, NULL, ENQUEUE_EVENT(cl_events.calculateDelta));

    clEnqueueReadBuffer(openCL_context.queue, cl_args.mem.loss, CL_FALSE, 0, loss_size, (void*) host_lossbuffer, 0, NULL, &cl_events.readLoss);

    clSetKernelArg(openCL_context.kernels.calculate_output_gradient, 4, sizeof(cl_mem), (bufferSide == INPUT_GROUP_A) ? &cl_args.mem.outputBucket_A : &cl_args.mem.outputBucket_B);
    clEnqueueNDRangeKernel(openCL_context.queue, openCL_context.kernels.calculate_output_gradient, 3, NULL, calcOutputGradient_GlobalSize, calcOutputGradient_LocalSize, 0, NULL, ENQUEUE_EVENT(cl_events.calculate_output_gradient));
    
    clEnqueueNDRangeKernel(openCL_context.queue, openCL_context.kernels.calculate_accumulator_bias_gradient, 3, NULL, calcAccumulatorBiasGradient_GlobalSize, calcAccumulatorBiasGradient_LocalSize, 0, NULL, ENQUEUE_EVENT(cl_events.calculate_accumulator_bias_gradient));

    clSetKernelArg(openCL_context.kernels.calculate_accumulator_gradient, 0, sizeof(cl_mem), (bufferSide == INPUT_GROUP_A) ? &cl_args.mem.activeInputs_A : &cl_args.mem.activeInputs_B);
    clEnqueueNDRangeKernel(openCL_context.queue, openCL_context.kernels.calculate_accumulator_gradient, 3, NULL, calcAccumulatorGradient_GlobalSize, calcAccumulatorGradient_LocalSize, 0, NULL, ENQUEUE_EVENT(cl_events.calculate_accumulator_gradient));

    timestamp++;
    biasCorrection1 *= ADAM_BETA1;
    biasCorrection2 *= ADAM_BETA2;
    rho_timestamp = rho_inf - (2.0f * timestamp * biasCorrection2) / (1.0f - biasCorrection2);
    if(rho_timestamp > 4.0f) 
        rectificationTerm = sqrtf(((rho_timestamp - 4.0f) * (rho_timestamp - 2.0f) * rho_inf) / ((rho_inf - 4.0f) * (rho_inf - 2.0f) * rho_timestamp));
    else 
        rectificationTerm = 0.0f;

    clSetKernelArg(openCL_context.kernels.adamw, 12, sizeof(cl_float), &lr);
    clSetKernelArg(openCL_context.kernels.adamw, 13, sizeof(cl_float), &biasCorrection1);
    clSetKernelArg(openCL_context.kernels.adamw, 14, sizeof(cl_float), &biasCorrection2);
    clSetKernelArg(openCL_context.kernels.adamw, 15, sizeof(cl_float), &rectificationTerm);
    clEnqueueNDRangeKernel(openCL_context.queue, openCL_context.kernels.adamw,      3, NULL, adamw_GlobalSize,      adamw_LocalSize,       0, NULL, ENQUEUE_EVENT(cl_events.denseUpdate));
    
    clSetKernelArg(openCL_context.kernels.inputadamw, 8,  sizeof(cl_float), &lr);
    clSetKernelArg(openCL_context.kernels.inputadamw, 9,  sizeof(cl_float), &biasCorrection1);
    clSetKernelArg(openCL_context.kernels.inputadamw, 10, sizeof(cl_float), &biasCorrection2);
    clSetKernelArg(openCL_context.kernels.inputadamw, 11, sizeof(cl_float), &rectificationTerm);
    clEnqueueNDRangeKernel(openCL_context.queue, openCL_context.kernels.inputadamw, 3, NULL, inputAdamw_GlobalSize, inputAdamw_LocalSize,  0, NULL, ENQUEUE_EVENT(cl_events.inputUpdate));

    if(timestamp % LOOKAHEAD_RANGE == 0)
    {
        LOOKAHEAD_UPDATE(cl_args.mem.factorizer_weights_fast, cl_args.mem.factorizer_weights_slow, factorizer_weights_count);

        LOOKAHEAD_UPDATE(cl_args.mem.accumulator_weights_fast, cl_args.mem.accumulator_weights_slow, accumulator_weights_count);
        LOOKAHEAD_UPDATE(cl_args.mem.accumulator_bias_fast, cl_args.mem.accumulator_bias_slow, accumulator_bias_count);

        LOOKAHEAD_UPDATE(cl_args.mem.output_weights_fast, cl_args.mem.output_weights_slow, output_weights_count);
        LOOKAHEAD_UPDATE(cl_args.mem.output_bias_fast, cl_args.mem.output_bias_slow, output_bias_count);
    }

    #ifdef PERFT_KERNELS
        //Profiling disallows the efficient usage of ping-pong buffers, which slows down execution. 
        //It doesn't really matter since profiling only gets run for a short period.
        clFinish(openCL_context.queue);

        //Overlap is prevented with explicit synchronization.
        //This increases overhead while perfting, so the true time is somewhere between sum and total.
        cl_ulong start, end;
        clGetEventProfilingInfo(cl_events.calcAccum, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
        clGetEventProfilingInfo(cl_events.inputUpdate, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
        float sum_time_ms = 0.0f;
        float total_time_ms = (end - start) / 1000000.0;

        printf("\n--- Profiling ---\n");
        sum_time_ms += print_prof("Calculate Accumulator", cl_events.calcAccum);
        sum_time_ms += print_prof("Calculate Output", cl_events.calculateOutput);
        sum_time_ms += print_prof("Calculate Deltas", cl_events.calculateDelta);
        sum_time_ms += print_prof("Output Gradient", cl_events.calculate_output_gradient);
        sum_time_ms += print_prof("Accumulator Bias Gradient", cl_events.calculate_accumulator_bias_gradient);
        sum_time_ms += print_prof("Accumulator Gradient", cl_events.calculate_accumulator_gradient);
        sum_time_ms += print_prof("Output Layer & Biases", cl_events.denseUpdate);
        sum_time_ms += print_prof("Input Layer", cl_events.inputUpdate);
        printf("%-30s | %0.4f ms\n", "Sum", sum_time_ms);
        printf("%-30s | %0.4f ms\n", "Total", total_time_ms);
        printf("-----------------\n");    
    #endif
}

void saveGPUWeights()
{ 
    clFinish(openCL_context.queue);

    //Training Weights
    clEnqueueReadBuffer(openCL_context.queue, cl_args.mem.factorizer_weights_slow,  CL_FALSE, 0, factorizer_weights_size,   host_training_weights->factorizer_weights,   0, NULL, NULL);
    clEnqueueReadBuffer(openCL_context.queue, cl_args.mem.accumulator_weights_slow, CL_FALSE, 0, accumulator_weights_size,  host_training_weights->accumulator_weights,  0, NULL, NULL);
    clEnqueueReadBuffer(openCL_context.queue, cl_args.mem.accumulator_bias_slow,    CL_FALSE, 0, accumulator_bias_size,     host_training_weights->accumulator_bias,     0, NULL, NULL);
    clEnqueueReadBuffer(openCL_context.queue, cl_args.mem.output_weights_slow,      CL_FALSE, 0, output_weights_size,       host_training_weights->output_weights,       0, NULL, NULL);
    clEnqueueReadBuffer(openCL_context.queue, cl_args.mem.output_bias_slow,         CL_FALSE, 0, output_bias_size,          host_training_weights->output_bias,          0, NULL, NULL);

    //First Moments
    clEnqueueReadBuffer(openCL_context.queue, cl_args.mem.m_factorizer_weights,   CL_FALSE, 0, factorizer_weights_size,     host_first_moments->factorizer_weights,   0, NULL, NULL);
    clEnqueueReadBuffer(openCL_context.queue, cl_args.mem.m_accumulator_weights,  CL_FALSE, 0, accumulator_weights_size,    host_first_moments->accumulator_weights,  0, NULL, NULL);
    clEnqueueReadBuffer(openCL_context.queue, cl_args.mem.m_accumulator_bias,     CL_FALSE, 0, accumulator_bias_size,       host_first_moments->accumulator_bias,     0, NULL, NULL);
    clEnqueueReadBuffer(openCL_context.queue, cl_args.mem.m_output_weights,       CL_FALSE, 0, output_weights_size,         host_first_moments->output_weights,       0, NULL, NULL);
    clEnqueueReadBuffer(openCL_context.queue, cl_args.mem.m_output_bias,          CL_FALSE, 0, output_bias_size,            host_first_moments->output_bias,          0, NULL, NULL);

    //Second Moments
    clEnqueueReadBuffer(openCL_context.queue, cl_args.mem.v_factorizer_weights,   CL_FALSE, 0, factorizer_weights_size,     host_second_moments->factorizer_weights,   0, NULL, NULL);
    clEnqueueReadBuffer(openCL_context.queue, cl_args.mem.v_accumulator_weights,  CL_FALSE, 0, accumulator_weights_size,    host_second_moments->accumulator_weights,  0, NULL, NULL);
    clEnqueueReadBuffer(openCL_context.queue, cl_args.mem.v_accumulator_bias,     CL_FALSE, 0, accumulator_bias_size,       host_second_moments->accumulator_bias,     0, NULL, NULL);
    clEnqueueReadBuffer(openCL_context.queue, cl_args.mem.v_output_weights,       CL_FALSE, 0, output_weights_size,         host_second_moments->output_weights,       0, NULL, NULL);
    clEnqueueReadBuffer(openCL_context.queue, cl_args.mem.v_output_bias,          CL_TRUE,  0, output_bias_size,            host_second_moments->output_bias,          0, NULL, NULL);

    clFinish(openCL_context.queue);

    quantizeWeights(host_training_weights, host_quantized_weights);
    saveQuantizedWeights(host_quantized_weights, "./import/quantized.bin");
    saveRawWeights(host_training_weights,   "./import/raw.bin");
    saveRawWeights(host_first_moments,      "./import/firstMoments.bin");
    saveRawWeights(host_second_moments,     "./import/secondMoments.bin");
}