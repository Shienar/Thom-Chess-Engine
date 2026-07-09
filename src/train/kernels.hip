#include <hip/hip_runtime.h>

#define MINIBATCH_SIZE 16384
#define INPUT_BITS (2 * 10 * 768)
#define HALF_INPUT_BITS (INPUT_BITS / 2)
#define ACCUMULATOR_NODES 1024
#define ACCUMULATOR_NODES_PER_SIDE (ACCUMULATOR_NODES / 2)
#define VECTOR_ACCUMULATOR_NODES (ACCUMULATOR_NODES / 4)
#define VECTOR_ACCUMULATOR_NODES_PER_SIDE (ACCUMULATOR_NODES_PER_SIDE / 4)
#define OUTPUT_BUCKETS 8

#define ADAM_BETA1 0.9f
#define ADAM_BETA2 0.999f
#define ADAM_EPSILON 1e-8f
#define ADAM_WEIGHT_DECAY 1e-3f
#define OUTPUT_SCALE 16

#define EVAL_SCALE 400.0f

#define LOOKAHEAD_ALPHA 0.5f

#define QA 255.0f
#define QB 64.0f

#define QA_16_MIN (-32768.0f / QA)
#define QA_16_MAX (32767.0f / QA)

#define QB_8_MIN (-128.0f / QB)
#define QB_8_MAX (127.0f / QB)

#define B_32_MIN (-2147483648.0f / (QA * QB))
#define B_32_MAX (2147483647.0f / (QA * QB))

/******* UTIL *******/

__device__ inline float screlu(float x) { float y = fminf(fmaxf(x, 0.0f), 1.0f); return y * y; }
__device__ inline float crelu(float x) { return fminf(fmaxf(x, 0.0f), 1.0f); }
__device__ inline float screlu_derivative(float x) { return (((x > 0.0f) && (x < 1.0f)) ? (2.0f*x) : 0.0f ); }
__device__ inline float crelu_derivative(float x) { return (float) ((x > 0.0f) && (x < 1.0f)); }

__device__ inline float sigmoid(float x) { return 1.0f / (1.0f + __expf(-x)); }


//Only bring in fake quantization towards the end of training
#define QAT
#ifdef QAT
__device__ inline float fake_quantize(float val, float scale, float min_int, float max_int) { return  fminf(max_int, fmaxf(min_int, rintf(val * scale))) / scale; }
__device__ inline float4 fake_quantize4(float4 val, float scale, float min_int, float max_int) {
    float4 out;
    out.x = fake_quantize(val.x, scale, min_int, max_int);
    out.y = fake_quantize(val.y, scale, min_int, max_int);
    out.z = fake_quantize(val.z, scale, min_int, max_int);
    out.w = fake_quantize(val.w, scale, min_int, max_int);
    return out;
}
#else
__device__ inline float fake_quantize(float val, float scale, float min_int, float max_int) { return val; }
__device__ inline float4 fake_quantize4(float4 val, float scale, float min_int, float max_int) { return val; }
#endif

/******* Forward Propagation *******/
//Grid Dimensions:  MINIBATCH_SIZE * 2,         1, 1
//Block Dimensions: ACCUMULATOR_NODES_PER_SIDE, 1, 1
extern "C" __global__ void calculateAccumulator(const short* activeInputs, // [MINIBATCH_SIZE][2][32]
                                                const float4* weights,
                                                const float4* bias,
                                                float4* output)
{
    int groupID = blockIdx.x;  // 2 groups per batch item
    int localID = threadIdx.x;

    int batchIndex = groupID >> 1;      // groupId / 2
    int side       = groupID & 1;       // groupId % 2

    int offset = batchIndex * 64 + 32 * side;

    int vector1Index = localID * 4;
    int vector2Index = localID * 4 + 1; 
    int vector3Index = localID * 4 + 2;
    int vector4Index = localID * 4 + 3; 

    float4 sum1 = fake_quantize4(bias[vector1Index], QA, -32768.0f, 32767.0f);
    float4 sum2 = fake_quantize4(bias[vector2Index], QA, -32768.0f, 32767.0f);
    float4 sum3 = fake_quantize4(bias[vector3Index], QA, -32768.0f, 32767.0f);
    float4 sum4 = fake_quantize4(bias[vector4Index], QA, -32768.0f, 32767.0f);

    int baseIndex;

    unsigned long long mask_32 = 0x00000000FFFFFFFF;
    #pragma unroll
    for(int f = 0; f < 32; f++) 
    {
        //This is executed within a single wavefront so shared local 
        //group buffer / broadcast is slower than just loading it directly.
        short currentFeature = activeInputs[offset + f];

        if(currentFeature == -1) break;

        baseIndex = currentFeature * VECTOR_ACCUMULATOR_NODES_PER_SIDE;

        float4 w1 = fake_quantize4(weights[baseIndex + vector1Index], QA, -32768.0f, 32767.0f);
        float4 w2 = fake_quantize4(weights[baseIndex + vector2Index], QA, -32768.0f, 32767.0f);
        float4 w3 = fake_quantize4(weights[baseIndex + vector3Index], QA, -32768.0f, 32767.0f);
        float4 w4 = fake_quantize4(weights[baseIndex + vector4Index], QA, -32768.0f, 32767.0f);

        sum1.x += w1.x;  sum1.y += w1.y;  sum1.z += w1.z;  sum1.w += w1.w;
        sum2.x += w2.x;  sum2.y += w2.y;  sum2.z += w2.z;  sum2.w += w2.w;
        sum3.x += w3.x;  sum3.y += w3.y;  sum3.z += w3.z;  sum3.w += w3.w;
        sum4.x += w4.x;  sum4.y += w4.y;  sum4.z += w4.z;  sum4.w += w4.w;
    }

    baseIndex = groupID * VECTOR_ACCUMULATOR_NODES_PER_SIDE;
    output[baseIndex + vector1Index] = sum1;
    output[baseIndex + vector2Index] = sum2;
    output[baseIndex + vector3Index] = sum3;
    output[baseIndex + vector4Index] = sum4;
}

//Grid Dimensions:  MINIBATCH_SIZE, 1, 1
//Block Dimensions: 32,             1, 1
extern "C" __global__ void forwardPropagate(const float* accumulatorOutput,
                                            float* output_weights, float* output_bias,
                                            float* outputs, char* outputBucket)
{
    int batchIndex = blockIdx.x;
    int localID = threadIdx.x; //0-32

    int baseIndex = batchIndex * ACCUMULATOR_NODES;

    float myOutputContribution = 0.0f;
    int bucket = outputBucket[batchIndex];

    #pragma unroll
    for(int i = localID; i < ACCUMULATOR_NODES; i+= 32)
    {
        myOutputContribution += fake_quantize(screlu(accumulatorOutput[baseIndex + i]), QA, 0.0f, QA) * output_weights[bucket * ACCUMULATOR_NODES + i];
    }

    __syncthreads();

    unsigned long long mask_32 = 0x00000000FFFFFFFF;
    #pragma unroll
    for(int offset = 16; offset > 0; offset /= 2) 
    {
        myOutputContribution += __shfl_down_sync(mask_32, myOutputContribution, offset, 32);
    }

    if(localID == 0)  
    {
        float finalOutput = myOutputContribution + fake_quantize(output_bias[bucket], QA * QB, -2147483648.0f, 2147483647.0f);
        
        //Scale by the relative scale of the quantized output node during fprop.
        finalOutput *= (QA / OUTPUT_SCALE);

        outputs[batchIndex] = sigmoid(finalOutput / EVAL_SCALE);
    }
}

/******* Backpropagation (Delta calculations) *******/

//Grid Dimensions:  MINIBATCH_SIZE,             1, 1
//Block Dimensions: ACCUMULATOR_NODES_PER_SIDE, 1, 1
extern "C" __global__ void backpropagate( const float* outputNodes, const float* expectedOutputs,
                            const float* h1,
                            const float* weights2,
                            float* delta2,
                            float* delta1,
                            float* loss, 
                            char* outputBucket)
{
    int batchIndex = blockIdx.x; //0-16383
    int localID = threadIdx.x; //0-511

    int bucket = outputBucket[batchIndex];

    __shared__ float shared_d2;

    //delta2
    if(localID == 0) 
    {
        float output = outputNodes[batchIndex];
        float expected = expectedOutputs[batchIndex];

        shared_d2 = output - expected;
        
        loss[batchIndex] = (shared_d2 * shared_d2);
        shared_d2 *= (output * (1.0f - output)) * (QA / OUTPUT_SCALE); //sigmoid derivative, Chain Rule

        delta2[batchIndex] = shared_d2;
    } 
    __syncthreads();

    //delta 1
    //1024 delta / 512 threads = 2 deltas per
    for(int nodeIndex = localID; nodeIndex < ACCUMULATOR_NODES; nodeIndex+=ACCUMULATOR_NODES_PER_SIDE) 
    {
        float sum = shared_d2 * fake_quantize(weights2[bucket * ACCUMULATOR_NODES + nodeIndex], QB, -128.0f, 127.0f);
        delta1[batchIndex * ACCUMULATOR_NODES + nodeIndex] = sum * screlu_derivative(h1[batchIndex * ACCUMULATOR_NODES + nodeIndex]);
    }
}

/******* Calculate & Accumulate Edge Weight Gradients *******/

//Grid Dimensions:  OUTPUT_BUCKETS * ACCUMULATOR_NODES, 1, 1
//Block Dimensions: 32,                                 1, 1
extern "C" __global__ void calculateGradient2(const float* delta2,
                                    const float* h1,
                                    float* gradient2_Sum,
                                    float* bias2_Sum,
                                    char* outputBucket)
{
    int weightIndex = blockIdx.x;
    int bucketWeightIndex = weightIndex % ACCUMULATOR_NODES;
    int weightBucket = weightIndex / ACCUMULATOR_NODES;

    int localID = threadIdx.x;

    float partialSum = 0.0f;
    float partialBiasSum = 0.0f;

    for(int batchIndex = localID; batchIndex < MINIBATCH_SIZE; batchIndex += 32) 
    {
        float d2 = delta2[batchIndex];
        int bucket = outputBucket[batchIndex];

        if(bucket == weightBucket)
        {
            partialSum += d2 * fake_quantize(screlu(h1[ACCUMULATOR_NODES * batchIndex + bucketWeightIndex]), QA, 0.0f, QA);
            if(weightIndex == 0) partialBiasSum += d2;
        }
    }

    unsigned long long mask_32 = 0x00000000FFFFFFFF;
    #pragma unroll
    for(int offset = 16; offset > 0; offset /= 2)
    {
        partialSum += __shfl_down_sync(mask_32, partialSum, offset, 32);
        if(weightIndex == 0) partialBiasSum += __shfl_down_sync(mask_32, partialBiasSum, offset, 32);
    }

    if(localID == 0)
    {
        gradient2_Sum[weightIndex] = partialSum;
        if(weightIndex == 0) bias2_Sum[0] = partialBiasSum;
    }
}

//Grid Dimensions:  MINIBATCH_SIZE,             1, 1
//Block Dimensions: ACCUMULATOR_NODES_PER_SIDE, 1, 1
extern "C" __global__ void calculateGradient1(const short* activeInputs, //[MINIBATCH_SIZE][2][32]
                                    const float* delta1, 
                                    float* gradient1_Sum, 
                                    float* bias1_Sum)
{
    int batchIndex = blockIdx.x;
    int localID = threadIdx.x;

    // Shared storage for the 64 active features of this board position
    __shared__ short sharedFeatures[64];
    __shared__ float d1[ACCUMULATOR_NODES];

    if(localID < 64) sharedFeatures[localID] = activeInputs[batchIndex * 64 + localID];
    
    int offset = batchIndex * ACCUMULATOR_NODES;
    
    #pragma unroll
    for(int i = localID; i < ACCUMULATOR_NODES; i+=ACCUMULATOR_NODES_PER_SIDE)
    {
        d1[i] = delta1[offset + i];
    }
    __syncthreads();

    __shared__ float local_grad_sum_A[ACCUMULATOR_NODES_PER_SIDE];
    __shared__ float local_grad_sum_B[ACCUMULATOR_NODES_PER_SIDE];

    int f = 0;
    while(sharedFeatures[f] != -1 && f < 32)
    {
        //Local accumulation and featureA/B saves a small amount of time.
        local_grad_sum_A[localID] = d1[localID];
        local_grad_sum_B[localID] = d1[ACCUMULATOR_NODES_PER_SIDE + localID];
        __syncthreads();

        short featureA = sharedFeatures[f];
        short featureB = sharedFeatures[32 + f];

        atomicAdd(&gradient1_Sum[featureA * ACCUMULATOR_NODES_PER_SIDE + localID], local_grad_sum_A[localID]);
        atomicAdd(&gradient1_Sum[featureB * ACCUMULATOR_NODES_PER_SIDE + localID], local_grad_sum_B[localID]);

        f++;
    }

    //Bias
    atomicAdd(&bias1_Sum[localID], d1[localID] + d1[localID + ACCUMULATOR_NODES_PER_SIDE]);
}

/******* Adam Update *******/
#define UPDATE_ADAMW(weight, gradientSum, firstMoment, secondMoment, size, min, max) \
    if(i < size) \
    { \
        float grad = gradientSum[i] / MINIBATCH_SIZE; \
        if(grad) { \
            firstMoment[i] = ADAM_BETA1 * firstMoment[i] + (1.0f - ADAM_BETA1) * grad; \
            secondMoment[i] = ADAM_BETA2 * secondMoment[i] + (1.0f - ADAM_BETA2) * grad * grad; \
            float correctedFirstMoment = firstMoment[i] / (1.0f - biasCorrection1); \
            if(rectificationTerm > 0.0f) { \
                float denominator = sqrtf(secondMoment[i] / (1.0f - biasCorrection2)) + ADAM_EPSILON; \
                weight[i] -= lr * rectificationTerm * (correctedFirstMoment / denominator); \
            } else weight[i] -= lr * correctedFirstMoment; \
            weight[i] -= lr * ADAM_WEIGHT_DECAY * weight[i]; \
            weight[i] = fminf(max, fmaxf(min, weight[i])); \
            gradientSum[i] = 0.0f; \
        } \
    }

//Grid Dimensions:  weight2Count / 32, 1, 1
//Block Dimensions: 32,                1, 1
extern "C" __global__ void adamW(float* w2, float* g2, float* m2, float* v2,
                                    float* b1, float* gb1, float* mb1, float* vb1,
                                    float* b2, float* gb2, float* mb2, float* vb2,
                                    float lr, float biasCorrection1, float biasCorrection2, float rectificationTerm) 
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    
    UPDATE_ADAMW(w2, g2, m2, v2, (OUTPUT_BUCKETS * ACCUMULATOR_NODES), QB_8_MIN, QB_8_MAX);
    UPDATE_ADAMW(b1, gb1, mb1, vb1, ACCUMULATOR_NODES_PER_SIDE, QA_16_MIN, QA_16_MAX);
    UPDATE_ADAMW(b2, gb2, mb2, vb2, OUTPUT_BUCKETS, B_32_MIN, B_32_MAX);
    
}

//Grid Dimensions:  weight1Count / 32, 1, 1
//Block Dimensions: 32,                1, 1
extern "C" __global__ void inputAdamW(float* w1, float* g1, float* m1, float* v1,
                                        float lr, float biasCorrection1, float biasCorrection2, float rectificationTerm)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int size = gridDim.x * blockDim.x;

    UPDATE_ADAMW(w1, g1, m1, v1, size, QA_16_MIN, QA_16_MAX);
}

extern "C" __global__ void lookahead_update(float* fast_weights, float* slow_weights)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;;
    
    slow_weights[i] += LOOKAHEAD_ALPHA * (fast_weights[i] - slow_weights[i]);
    fast_weights[i] = slow_weights[i];
}