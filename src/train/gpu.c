#include "gpu_funcs.h"
#include <string.h>

hipContext hip_context = {
    .deviceID = 0,
    .queue = NULL,
    .module = NULL,
    .kernels = { { NULL } }
};

hipKernelArgs hip_mem = { { { NULL } } };
hipEvents hip_events = {
    .readLoss = NULL,
    .startEvents = { { NULL } },
    .endEvents = { { NULL } } 
};

float lr = MAX_LR;
uint64_t cosineTimestamp;
uint64_t timestamp;
float rho_inf = (2.0 / (1.0 - ADAM_BETA2)) - 1.0;
float rho_timestamp = 0.0;
float rectificationTerm = 0.0;
float biasCorrection1 = 1.0;
float biasCorrection2 = 1.0;

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
char* host_outputBuckets_A      = NULL;
char* host_outputBuckets_B      = NULL;
float* host_lossbuffer          = NULL;

void* calculateAccumulatorArgs[5];
void* calculateOutputArgs[5];
void* calculateDeltaArgs[8];
void* outputGradientArgs[5];
void* accumulatorGradientArgs[5];
void* inputAdamArgs[12];
void* denseAdamArgs[16];

unsigned char* loadCompiledKernels(size_t* size, const char* fileName)
{
    FILE* input = fopen(fileName, "rb"); 
    if(!input)
    {
        printf("Cannot locate compiled gpu kernels at %s\n", fileName);
        return NULL;
    }
    
    fseek(input, 0, SEEK_END);
    *size = ftell(input);
    rewind(input);

    unsigned char* binary = malloc(*size);
    if(!binary) 
    { 
        DEBUG_ERROR("Failed malloc.");
        fclose(input);
        return NULL; 
    }

    if(fread(binary, 1, *size, input) < 1)
    {
        DEBUG_ERROR("Mismatched read size.");
        fclose(input);
        return NULL;
    }
    
    fclose(input);

    return binary;
}

hipError_t initHIP(training_weights* raw_weights, const char* compiledKernelPath,
                    short** h_active_A, float** h_expected_A, char** h_bucket_A,
                    short** h_active_B, float** h_expected_B, char** h_bucket_B,
                    float** h_lossbuffer)
{

    cosineTimestamp = 0;
    timestamp = 0;

    hipError_t err;
    
    if((err = hipInit(0)) != hipSuccess) { DEBUG_ERROR("Failed to initialize HIP."); return err; }
    
    hip_context.deviceID = 0;
    if((err = hipSetDevice(hip_context.deviceID)) != hipSuccess) { DEBUG_ERROR("Failed to acquire GPU device.");  return err; }

    #ifdef PERFT_KERNELS
        err = hipStreamCreate(&hip_context.queue);
    #else
        err = hipStreamCreateWithFlags(&hip_context.queue, hipStreamNonBlocking);
    #endif
    if(err != hipSuccess) { DEBUG_ERROR("Failed to initialize queue."); return err; }
    
    size_t binarySize = 0;
    unsigned char* binaryKernels = loadCompiledKernels(&binarySize, compiledKernelPath);
    if(!binaryKernels || (err = hipModuleLoadData(&hip_context.module, binaryKernels)) != hipSuccess) { DEBUG_ERROR("Failed to load compiled binaries."); return err;}
    free(binaryKernels);
    binaryKernels = NULL;

    err = hipModuleGetFunction(&hip_context.kernels.calculateAccumulator, hip_context.module, "calculateAccumulator");
    err = hipModuleGetFunction(&hip_context.kernels.calculateOutput, hip_context.module, "calculateOutput");
    err = hipModuleGetFunction(&hip_context.kernels.calculateDeltas, hip_context.module, "calculateDeltas"); 
    err = hipModuleGetFunction(&hip_context.kernels.calculate_output_gradient, hip_context.module, "calculate_output_gradient");
    err = hipModuleGetFunction(&hip_context.kernels.calculate_accumulator_gradient, hip_context.module, "calculate_accumulator_gradient");
    err = hipModuleGetFunction(&hip_context.kernels.adamw, hip_context.module, "adamW");
    err = hipModuleGetFunction(&hip_context.kernels.inputadamw, hip_context.module, "inputAdamW");
    err = hipModuleGetFunction(&hip_context.kernels.lookahead, hip_context.module, "lookahead_update");

    if(err != hipSuccess) { DEBUG_ERROR("Error getting kernels."); return 1; }

    err = hipEventCreate(&hip_events.readLoss);
    for(int i = 0; i < EVENT_TRACKED_KERNELS; i++) 
    {
        err = hipEventCreate(&hip_events.startEvents.arr[i]);
        err = hipEventCreate(&hip_events.endEvents.arr[i]);
    }
    if(err != hipSuccess) { DEBUG_ERROR("Error creating events."); return 1; }

    /** ALLOCATING **/
    //Host pinning (Staging buffers)
    hipHostMalloc((void**)&host_activeInputs_A,     activeInputs_size,      hipHostMallocDefault);
    hipHostMalloc((void**)&host_activeInputs_B,     activeInputs_size,      hipHostMallocDefault);
    hipHostMalloc((void**)&host_expectedOutputs_A,  expectedOutput_size,    hipHostMallocDefault);
    hipHostMalloc((void**)&host_expectedOutputs_B,  expectedOutput_size,    hipHostMallocDefault);
    hipHostMalloc((void**)&host_outputBuckets_A,    outputBucket_size,      hipHostMallocDefault);
    hipHostMalloc((void**)&host_outputBuckets_B,    outputBucket_size,      hipHostMallocDefault);
    hipHostMalloc((void**)&host_lossbuffer,         loss_size,              hipHostMallocDefault);
    //Export pointers
    *h_active_A = host_activeInputs_A;
    *h_active_B = host_activeInputs_B;
    *h_expected_A = host_expectedOutputs_A;
    *h_expected_B = host_expectedOutputs_B;
    *h_bucket_A = host_outputBuckets_A;
    *h_bucket_B = host_outputBuckets_B;
    *h_lossbuffer = host_lossbuffer;
    //gpu buffers (copied from host-pinning)
    hipMalloc(&hip_mem.mem.activeInputs_A,      activeInputs_size);
    hipMalloc(&hip_mem.mem.activeInputs_B,      activeInputs_size);
    hipMalloc(&hip_mem.mem.expectedOutput_A,    expectedOutput_size);
    hipMalloc(&hip_mem.mem.expectedOutput_B,    expectedOutput_size);
    hipMalloc(&hip_mem.mem.outputBucket_A,      outputBucket_size);
    hipMalloc(&hip_mem.mem.outputBucket_B,      outputBucket_size);
    hipMalloc(&hip_mem.mem.loss,                loss_size);
    //Fast weights
    hipMalloc(&hip_mem.mem.factorizer_weights_fast,     factorizer_weights_size);
    hipMalloc(&hip_mem.mem.accumulator_weights_fast,    accumulator_weights_size);
    hipMalloc(&hip_mem.mem.accumulator_bias_fast,       accumulator_bias_size);
    hipMalloc(&hip_mem.mem.output_weights_fast,         output_weights_size);
    hipMalloc(&hip_mem.mem.output_bias_fast,            output_bias_size);
    //Slow Weights
    hipMalloc(&hip_mem.mem.factorizer_weights_slow,     factorizer_weights_size);
    hipMalloc(&hip_mem.mem.accumulator_weights_slow,    accumulator_weights_size);
    hipMalloc(&hip_mem.mem.accumulator_bias_slow,       accumulator_bias_size);
    hipMalloc(&hip_mem.mem.output_weights_slow,         output_weights_size);
    hipMalloc(&hip_mem.mem.output_bias_slow,            output_bias_size);
    //Intermediate Outputs
    hipMalloc(&hip_mem.mem.accumulatorOutput,   accumulatorOutput_size);
    hipMalloc(&hip_mem.mem.finalOutput,         finalOutput_size);
    //Deltas
    hipMalloc(&hip_mem.mem.output_delta,        output_delta_size);
    hipMalloc(&hip_mem.mem.accumulator_delta,   accum_delta_size);
    //Gradient Sums
    hipMalloc(&hip_mem.mem.factorizer_weights_gradient_sum,     factorizer_weights_size);
    hipMalloc(&hip_mem.mem.accumulator_weights_gradient_sum,    accumulator_weights_size);
    hipMalloc(&hip_mem.mem.accumulator_bias_gradient_sum,       accumulator_bias_size);
    hipMalloc(&hip_mem.mem.output_weights_gradient_sum,         output_weights_size);
    hipMalloc(&hip_mem.mem.output_bias_gradient_sum,            output_bias_size);
    //Adam First moments
    hipMalloc(&hip_mem.mem.m_factorizer_weights,    factorizer_weights_size);
    hipMalloc(&hip_mem.mem.m_accumulator_weights,   accumulator_weights_size);
    hipMalloc(&hip_mem.mem.m_accumulator_bias,      accumulator_bias_size);
    hipMalloc(&hip_mem.mem.m_output_weights,        output_weights_size);
    hipMalloc(&hip_mem.mem.m_output_bias,           output_bias_size);
    //Adam Second moments
    hipMalloc(&hip_mem.mem.v_factorizer_weights,    factorizer_weights_size);
    hipMalloc(&hip_mem.mem.v_accumulator_weights,   accumulator_weights_size);
    hipMalloc(&hip_mem.mem.v_accumulator_bias,      accumulator_bias_size);
    hipMalloc(&hip_mem.mem.v_output_weights,        output_weights_size);
    hipMalloc(&hip_mem.mem.v_output_bias,           output_bias_size);

    /** COPYING **/
    //factorizer_weights
    hipMemcpyAsync(hip_mem.mem.factorizer_weights_slow, (void*)raw_weights->factorizer_weights,     factorizer_weights_size, hipMemcpyHostToDevice, hip_context.queue);
    hipMemcpyAsync(hip_mem.mem.factorizer_weights_fast, hip_mem.mem.factorizer_weights_slow,        factorizer_weights_size, hipMemcpyDeviceToDevice, hip_context.queue);
    //accumulator_weights
    hipMemcpyAsync(hip_mem.mem.accumulator_weights_slow, (void*)raw_weights->accumulator_weights,   accumulator_weights_size, hipMemcpyHostToDevice, hip_context.queue);
    hipMemcpyAsync(hip_mem.mem.accumulator_weights_fast, hip_mem.mem.accumulator_weights_slow,      accumulator_weights_size, hipMemcpyDeviceToDevice, hip_context.queue);
    //output_weights
    hipMemcpyAsync(hip_mem.mem.output_weights_slow, (void*)raw_weights->output_weights, output_weights_size, hipMemcpyHostToDevice, hip_context.queue);
    hipMemcpyAsync(hip_mem.mem.output_weights_fast, hip_mem.mem.output_weights_slow,    output_weights_size, hipMemcpyDeviceToDevice, hip_context.queue);
    //accumulator_bias
    hipMemcpyAsync(hip_mem.mem.accumulator_bias_slow, (void*)raw_weights->accumulator_bias,     accumulator_bias_size, hipMemcpyHostToDevice, hip_context.queue);
    hipMemcpyAsync(hip_mem.mem.accumulator_bias_fast, hip_mem.mem.accumulator_bias_slow,        accumulator_bias_size, hipMemcpyDeviceToDevice, hip_context.queue);
    //output_bias
    hipMemcpyAsync(hip_mem.mem.output_bias_slow, (void*)raw_weights->output_bias,   output_bias_size, hipMemcpyHostToDevice, hip_context.queue);
    hipMemcpyAsync(hip_mem.mem.output_bias_fast, hip_mem.mem.output_bias_slow,      output_bias_size, hipMemcpyDeviceToDevice, hip_context.queue);

    /** ZEROING **/
    //First moments 
    hipMemsetAsync(hip_mem.mem.m_factorizer_weights,    0, factorizer_weights_size,     hip_context.queue);
    hipMemsetAsync(hip_mem.mem.m_accumulator_weights,   0, accumulator_weights_size,    hip_context.queue);
    hipMemsetAsync(hip_mem.mem.m_accumulator_bias,      0, accumulator_bias_size,       hip_context.queue);
    hipMemsetAsync(hip_mem.mem.m_output_weights,        0, output_weights_size,         hip_context.queue);
    hipMemsetAsync(hip_mem.mem.m_output_bias,           0, output_bias_size,            hip_context.queue);
    //Second moments
    hipMemsetAsync(hip_mem.mem.v_factorizer_weights,    0, factorizer_weights_size,     hip_context.queue);
    hipMemsetAsync(hip_mem.mem.v_accumulator_weights,   0, accumulator_weights_size,    hip_context.queue);
    hipMemsetAsync(hip_mem.mem.v_accumulator_bias,      0, accumulator_bias_size,       hip_context.queue);
    hipMemsetAsync(hip_mem.mem.v_output_weights,        0, output_weights_size,         hip_context.queue);
    hipMemsetAsync(hip_mem.mem.v_output_bias,           0, output_bias_size,            hip_context.queue);
    //Gradients
    hipMemsetAsync(hip_mem.mem.factorizer_weights_gradient_sum,     0, factorizer_weights_size,     hip_context.queue);
    hipMemsetAsync(hip_mem.mem.accumulator_weights_gradient_sum,    0, accumulator_weights_size,    hip_context.queue);
    hipMemsetAsync(hip_mem.mem.accumulator_bias_gradient_sum,       0, accumulator_bias_size,       hip_context.queue);
    hipMemsetAsync(hip_mem.mem.output_weights_gradient_sum,         0, output_weights_size,         hip_context.queue);
    hipMemsetAsync(hip_mem.mem.output_bias_gradient_sum,            0, output_bias_size,            hip_context.queue);

    hipStreamSynchronize(hip_context.queue);

    /** Set static arguments */
    //calculateAccumulatorArgs[0] = active inputs
    calculateAccumulatorArgs[1] = &hip_mem.mem.accumulator_weights_fast;
    calculateAccumulatorArgs[2] = &hip_mem.mem.factorizer_weights_fast;
    calculateAccumulatorArgs[3] = &hip_mem.mem.accumulator_bias_fast;
    calculateAccumulatorArgs[4] = &hip_mem.mem.accumulatorOutput;

    calculateOutputArgs[0] = &hip_mem.mem.accumulatorOutput;
    calculateOutputArgs[1] = &hip_mem.mem.output_weights_fast;
    calculateOutputArgs[2] = &hip_mem.mem.output_bias_fast;
    calculateOutputArgs[3] = &hip_mem.mem.finalOutput;
    //calculateOutputArgs[4] = output bucket

    calculateDeltaArgs[0] = &hip_mem.mem.finalOutput;
    //calculateDeltaArgs[1] = expectedOutput;
    calculateDeltaArgs[2] = &hip_mem.mem.accumulatorOutput;
    calculateDeltaArgs[3] = &hip_mem.mem.output_weights_fast;
    calculateDeltaArgs[4] = &hip_mem.mem.output_delta;
    calculateDeltaArgs[5] = &hip_mem.mem.accumulator_delta;
    calculateDeltaArgs[6] = &hip_mem.mem.loss;
    //calculateDeltaArgs[7] = output bucket

    outputGradientArgs[0] = &hip_mem.mem.output_delta;
    outputGradientArgs[1] = &hip_mem.mem.accumulatorOutput;
    outputGradientArgs[2] = &hip_mem.mem.output_weights_gradient_sum;
    outputGradientArgs[3] = &hip_mem.mem.output_bias_gradient_sum;
    //outputGradientArgs[4] = output bucket

    //accumulatorGradientArgs[0] = active inputs;
    accumulatorGradientArgs[1] = &hip_mem.mem.accumulator_delta;
    accumulatorGradientArgs[2] = &hip_mem.mem.factorizer_weights_gradient_sum;
    accumulatorGradientArgs[3] = &hip_mem.mem.accumulator_weights_gradient_sum;
    accumulatorGradientArgs[4] = &hip_mem.mem.accumulator_bias_gradient_sum;

    inputAdamArgs[0]  = &hip_mem.mem.accumulator_weights_fast;
    inputAdamArgs[1]  = &hip_mem.mem.accumulator_weights_gradient_sum;
    inputAdamArgs[2]  = &hip_mem.mem.m_accumulator_weights;
    inputAdamArgs[3]  = &hip_mem.mem.v_accumulator_weights;
    inputAdamArgs[4]  = &hip_mem.mem.factorizer_weights_fast;
    inputAdamArgs[5]  = &hip_mem.mem.factorizer_weights_gradient_sum;
    inputAdamArgs[6]  = &hip_mem.mem.m_factorizer_weights;
    inputAdamArgs[7]  = &hip_mem.mem.v_factorizer_weights;
    inputAdamArgs[8]  = &lr;
    inputAdamArgs[9]  = &biasCorrection1;
    inputAdamArgs[10] = &biasCorrection2;
    inputAdamArgs[11] = &rectificationTerm;

    denseAdamArgs[0]  = &hip_mem.mem.output_weights_fast;                
    denseAdamArgs[1]  = &hip_mem.mem.output_weights_gradient_sum;        
    denseAdamArgs[2]  = &hip_mem.mem.m_output_weights;                   
    denseAdamArgs[3]  = &hip_mem.mem.v_output_weights;                   
    denseAdamArgs[4]  = &hip_mem.mem.accumulator_bias_fast;              
    denseAdamArgs[5]  = &hip_mem.mem.accumulator_bias_gradient_sum;      
    denseAdamArgs[6]  = &hip_mem.mem.m_accumulator_bias;                 
    denseAdamArgs[7]  = &hip_mem.mem.v_accumulator_bias;                 
    denseAdamArgs[8]  = &hip_mem.mem.output_bias_fast;
    denseAdamArgs[9]  = &hip_mem.mem.output_bias_gradient_sum;
    denseAdamArgs[10] = &hip_mem.mem.m_output_bias;
    denseAdamArgs[11] = &hip_mem.mem.v_output_bias;
    denseAdamArgs[12] = &lr;
    denseAdamArgs[13] = &biasCorrection1;
    denseAdamArgs[14] = &biasCorrection2;
    denseAdamArgs[15] = &rectificationTerm;

    return hipSuccess;
}

void freeHIP()
{
    hipEventDestroy(hip_events.readLoss); 
    hip_events.readLoss = NULL;
    for(int i = 0; i < EVENT_TRACKED_KERNELS; i++) 
    {
        if(hip_events.startEvents.arr[i]) 
        {
            hipEventDestroy(hip_events.startEvents.arr[i]);
            hip_events.startEvents.arr[i] = NULL;
        }
        if(hip_events.endEvents.arr[i]) 
        {
            hipEventDestroy(hip_events.endEvents.arr[i]);
            hip_events.endEvents.arr[i] = NULL;
        }
    }

    if(hip_context.module) { hipModuleUnload(hip_context.module); hip_context.module = NULL; }
    for(int i = 0; i < KERNEL_COUNT; i++) { hip_context.kernels.arr[i] = NULL; }

    if(host_activeInputs_A)     hipHostFree(host_activeInputs_A);
    if(host_activeInputs_B)     hipHostFree(host_activeInputs_B);
    if(host_expectedOutputs_A)  hipHostFree(host_expectedOutputs_A);
    if(host_expectedOutputs_B)  hipHostFree(host_expectedOutputs_B);
    if(host_outputBuckets_A)    hipHostFree(host_outputBuckets_A);
    if(host_outputBuckets_B)    hipHostFree(host_outputBuckets_B);
    if(host_lossbuffer)         hipHostFree(host_lossbuffer);

    host_activeInputs_A = NULL;
    host_activeInputs_B = NULL;
    host_expectedOutputs_A = NULL;
    host_expectedOutputs_B = NULL;
    host_outputBuckets_A = NULL;
    host_outputBuckets_B = NULL;
    host_lossbuffer = NULL;

    for(int i = 0; i < MEM_COUNT; i++) 
    {
        if(hip_mem.mem.arr[i]) 
        {
            hipFree(hip_mem.mem.arr[i]);
            hip_mem.mem.arr[i] = NULL;
        }
    }

    if(hip_context.queue) { hipStreamDestroy(hip_context.queue); hip_context.queue = NULL; }
}

float print_prof(const char* name, hipEvent_t start, hipEvent_t stop)
{
    float execute_ms = 0.0f;
    hipError_t err = hipEventElapsedTime(&execute_ms, start, stop);
    if(err == hipSuccess) 
    {
        printf("%-24s | %0.4f ms\n", name, execute_ms);
        return execute_ms;
    }
    else
    {
        printf("%-24s | Profiling Error\n", name);
        return 0.0f;
    }
}

void enqueueKernels(int bufferSide)
{
    //cosine annealing
    lr = MIN_LR + 0.5 * (MAX_LR - MIN_LR) * (1.0 + cos(PI * (cosineTimestamp++) / MAX_COSINE_ANNEAL_TIMESTAMP));
    lr = clamp(lr, MIN_LR, MAX_LR);

    hipMemcpyAsync( (bufferSide == INPUT_GROUP_A) ? hip_mem.mem.activeInputs_A  : hip_mem.mem.activeInputs_B, 
                    (bufferSide == INPUT_GROUP_A) ? host_activeInputs_A         : host_activeInputs_B, 
                    activeInputs_size, hipMemcpyHostToDevice, hip_context.queue);
    hipMemcpyAsync( (bufferSide == INPUT_GROUP_A) ? hip_mem.mem.expectedOutput_A : hip_mem.mem.expectedOutput_B, 
                    (bufferSide == INPUT_GROUP_A) ? host_expectedOutputs_A       : host_expectedOutputs_B, 
                    expectedOutput_size, hipMemcpyHostToDevice, hip_context.queue);
    hipMemcpyAsync( (bufferSide == INPUT_GROUP_A) ? hip_mem.mem.outputBucket_A  : hip_mem.mem.outputBucket_B, 
                    (bufferSide == INPUT_GROUP_A) ? host_outputBuckets_A        : host_outputBuckets_B, 
                    outputBucket_size, hipMemcpyHostToDevice, hip_context.queue);
    
    hipMemsetAsync(hip_mem.mem.loss, 0, loss_size, hip_context.queue);

    calculateAccumulatorArgs[0] = (bufferSide == INPUT_GROUP_A) ? &hip_mem.mem.activeInputs_A : &hip_mem.mem.activeInputs_B;
    ENQUEUE_EVENT(hip_events.startEvents.calcAccum, hip_context.queue);
    hipModuleLaunchKernel(hip_context.kernels.calculateAccumulator,
                            MINIBATCH_SIZE * 2, 1, 1,
                            32, 1, 1,
                            0, hip_context.queue, calculateAccumulatorArgs, NULL);

    ENQUEUE_EVENT(hip_events.endEvents.calcAccum, hip_context.queue);

    calculateOutputArgs[4] = (bufferSide == INPUT_GROUP_A) ? &hip_mem.mem.outputBucket_A : &hip_mem.mem.outputBucket_B;
    ENQUEUE_EVENT(hip_events.startEvents.calculateOutput, hip_context.queue);
    hipModuleLaunchKernel(hip_context.kernels.calculateOutput,
                            MINIBATCH_SIZE, 1, 1,
                            32, 1, 1,
                            0, hip_context.queue, calculateOutputArgs, NULL);
    ENQUEUE_EVENT(hip_events.endEvents.calculateOutput, hip_context.queue);


    calculateDeltaArgs[1] = (bufferSide == INPUT_GROUP_A) ? &hip_mem.mem.expectedOutput_A : &hip_mem.mem.expectedOutput_B;
    calculateDeltaArgs[7] = (bufferSide == INPUT_GROUP_A) ? &hip_mem.mem.outputBucket_A   : &hip_mem.mem.outputBucket_B;
    ENQUEUE_EVENT(hip_events.startEvents.calculateDelta, hip_context.queue);
    hipModuleLaunchKernel(hip_context.kernels.calculateDeltas,
                            MINIBATCH_SIZE, 1, 1,
                            ACCUMULATOR_NODES_PER_SIDE, 1, 1,
                            0, hip_context.queue, calculateDeltaArgs, NULL);
    ENQUEUE_EVENT(hip_events.endEvents.calculateDelta, hip_context.queue);


    hipMemcpyAsync(host_lossbuffer, hip_mem.mem.loss, loss_size, hipMemcpyDeviceToHost, hip_context.queue);
    hipEventRecord(hip_events.readLoss, hip_context.queue);

    outputGradientArgs[4] = (bufferSide == INPUT_GROUP_A) ? &hip_mem.mem.outputBucket_A : &hip_mem.mem.outputBucket_B;
    ENQUEUE_EVENT(hip_events.startEvents.calculate_output_gradient, hip_context.queue);
    hipModuleLaunchKernel(hip_context.kernels.calculate_output_gradient,
                            OUTPUT_BUCKETS * ACCUMULATOR_NODES, 1, 1,
                            32,  1, 1,
                            0, hip_context.queue, outputGradientArgs, NULL);
    ENQUEUE_EVENT(hip_events.endEvents.calculate_output_gradient, hip_context.queue);

    
    accumulatorGradientArgs[0] = (bufferSide == INPUT_GROUP_A) ? &hip_mem.mem.activeInputs_A: &hip_mem.mem.activeInputs_B;
    ENQUEUE_EVENT(hip_events.startEvents.calculate_accumulator_gradient, hip_context.queue);
    hipModuleLaunchKernel(hip_context.kernels.calculate_accumulator_gradient,
                            MINIBATCH_SIZE,             1, 1,
                            ACCUMULATOR_NODES_PER_SIDE, 1, 1,
                            0, hip_context.queue, accumulatorGradientArgs, NULL);
    ENQUEUE_EVENT(hip_events.endEvents.calculate_accumulator_gradient, hip_context.queue);


    timestamp++;
    biasCorrection1 *= ADAM_BETA1;
    biasCorrection2 *= ADAM_BETA2;
    rho_timestamp = rho_inf - (2.0f * timestamp * biasCorrection2) / (1.0f - biasCorrection2);
    if(rho_timestamp > 4.0f) 
        rectificationTerm = sqrtf(((rho_timestamp - 4.0f) * (rho_timestamp - 2.0f) * rho_inf) / ((rho_inf - 4.0f) * (rho_inf - 2.0f) * rho_timestamp));
    else 
        rectificationTerm = 0.0f;

    ENQUEUE_EVENT(hip_events.startEvents.denseUpdate, hip_context.queue);
    hipModuleLaunchKernel(hip_context.kernels.adamw,
                            output_weights_count / 32, 1, 1,
                            32, 1, 1,
                            0, hip_context.queue, denseAdamArgs, NULL);
    ENQUEUE_EVENT(hip_events.endEvents.denseUpdate, hip_context.queue);

 
    ENQUEUE_EVENT(hip_events.startEvents.inputUpdate, hip_context.queue);
    hipModuleLaunchKernel(hip_context.kernels.inputadamw,
                            accumulator_weights_count / 32, 1, 1,
                            32, 1, 1,
                            0, hip_context.queue, inputAdamArgs, NULL);
    ENQUEUE_EVENT(hip_events.endEvents.inputUpdate, hip_context.queue);


    if(timestamp % LOOKAHEAD_RANGE == 0)
    {
        LOOKAHEAD_UPDATE(hip_mem.mem.factorizer_weights_fast, hip_mem.mem.factorizer_weights_slow, factorizer_weights_count);

        LOOKAHEAD_UPDATE(hip_mem.mem.accumulator_weights_fast, hip_mem.mem.accumulator_weights_slow, accumulator_weights_count);
        LOOKAHEAD_UPDATE(hip_mem.mem.accumulator_bias_fast, hip_mem.mem.accumulator_bias_slow, accumulator_bias_count);

        LOOKAHEAD_UPDATE(hip_mem.mem.output_weights_fast, hip_mem.mem.output_weights_slow, output_weights_count);
        LOOKAHEAD_UPDATE(hip_mem.mem.output_bias_fast, hip_mem.mem.output_bias_slow, output_bias_count);
    }

    #ifdef PERFT_KERNELS
        //Profiling disallows the efficient usage of ping-pong buffers, which slows down execution. 
        //It doesn't really matter since profiling only gets run for a short period.
        hipEventSynchronize(hip_events.endEvents.inputUpdate); 

        //Overlap is prevented with explicit synchronization.
        //This increases overhead while perfting, so the true time is somewhere between sum and total.
        float total_time_ms = 0.0f;
        float sum_time_ms = 0.0f;
        hipEventElapsedTime(&total_time_ms, hip_events.startEvents.calcAccum, hip_events.endEvents.inputUpdate);

        printf("\n--- Profiling ---\n");
        sum_time_ms += print_prof("Calculate Accumulator", hip_events.startEvents.calcAccum, hip_events.endEvents.calcAccum);
        sum_time_ms += print_prof("Calculate Output", hip_events.startEvents.calculateOutput, hip_events.endEvents.calculateOutput);
        sum_time_ms += print_prof("calculateDeltaagation", hip_events.startEvents.calculateDelta, hip_events.endEvents.calculateDelta);
        sum_time_ms += print_prof("Output Gradient", hip_events.startEvents.calculate_output_gradient, hip_events.endEvents.calculate_output_gradient);
        sum_time_ms += print_prof("Accumulator Gradient", hip_events.startEvents.calculate_accumulator_gradient, hip_events.endEvents.calculate_accumulator_gradient);
        sum_time_ms += print_prof("Output Layer & Biases", hip_events.startEvents.denseUpdate, hip_events.endEvents.denseUpdate);
        sum_time_ms += print_prof("Input Layer", hip_events.startEvents.inputUpdate, hip_events.endEvents.inputUpdate);
        printf("%-24s | %0.4f ms\n", "Sum", sum_time_ms);
        printf("%-24s | %0.4f ms\n", "Total", total_time_ms);

        printf("-----------------\n");    
    #endif
}

void getWeights(training_weights* weights)
{
    hipMemcpyDtoHAsync(weights->factorizer_weights, hip_mem.mem.factorizer_weights_slow, factorizer_weights_size, hip_context.queue);

    hipMemcpyDtoHAsync(weights->accumulator_weights, hip_mem.mem.accumulator_weights_slow, accumulator_weights_size, hip_context.queue);
    hipMemcpyDtoHAsync(weights->output_weights, hip_mem.mem.output_weights_slow, output_weights_size, hip_context.queue);
    
    hipMemcpyDtoHAsync(weights->accumulator_bias, hip_mem.mem.accumulator_bias_slow, accumulator_bias_size, hip_context.queue);
    hipMemcpyDtoHAsync(weights->output_bias, hip_mem.mem.output_bias_slow, output_bias_size, hip_context.queue);

    hipStreamSynchronize(hip_context.queue);
}