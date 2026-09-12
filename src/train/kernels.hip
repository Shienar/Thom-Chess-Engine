#include <hip/hip_runtime.h>
#include <cstdint>
#include <cassert>

#define MINIBATCH_SIZE 16384
#define KING_BUCKETS 10
#define OUTPUT_BUCKETS 8
#define BITS_PER_KING_BUCKET 768
#define INPUT_BITS (2 * KING_BUCKETS * BITS_PER_KING_BUCKET)
#define INPUT_BITS_PER_SIDE (INPUT_BITS / 2)
#define ACCUMULATOR_NODES 512
#define ACCUMULATOR_NODES_PER_SIDE (ACCUMULATOR_NODES / 2)
#define VECTOR_ACCUMULATOR_NODES (ACCUMULATOR_NODES / 4)
#define VECTOR_ACCUMULATOR_NODES_PER_SIDE (ACCUMULATOR_NODES_PER_SIDE / 4)

#define ADAM_BETA1 0.9f
#define ADAM_BETA2 0.999f
#define ADAM_EPSILON 1e-8
#define ADAM_WEIGHT_DECAY 1e-3f

#define EVAL_SCALE 400.0f

#define LOOKAHEAD_ALPHA 0.5f

#define QA 255.0f
#define QB 64.0f
#define QA_QB 16320.0f

//Bound requirement for lizard screlu
#define MIN_WEIGHT -1.98f
#define MAX_WEIGHT 1.98f

/******* UTIL *******/

__device__ inline float screlu(float x) { float y = fminf(fmaxf(x, 0.0f), 1.0f); return y * y; }
__device__ inline float screlu_derivative(float x) { return (((x > 0.0f) && (x < 1.0f)) ? (2.0f*x) : 0.0 ); }

__device__ inline float sigmoid(float x) { return 1.0f / (1.0f + __expf(-x)); }

#define QAT
#ifdef QAT
__device__ inline float fake_quantize(float val, float scale) { return rintf(val * scale) / scale; }
__device__ inline void fake_quantize4(float4* val, float scale) {
    val->x = fake_quantize(val->x, scale);
    val->y = fake_quantize(val->y, scale);
    val->z = fake_quantize(val->z, scale);
    val->w = fake_quantize(val->w, scale);
    return;
}
#else
__device__ inline float fake_quantize(float val, float scale) { return val; }
__device__ inline void fake_quantize4(float4* val, float scale) { return; }
#endif

/******* Forward Propagation *******/
//Grid Dimensions:  MINIBATCH_SIZE * 2,         1, 1
//Block Dimensions: 32, 1, 1
// float 4 vectors processed per thread = (ACCUMULATOR_NODES_PER_SIDE / 4) / 32 = 2
extern "C" __global__ void calculateAccumulator(const short* activeInputs, // [MINIBATCH_SIZE][2][32]
                                                const float4* accumulator_weights,
                                                const float4* factorizer_weights,
                                                const float4* accumulator_bias,
                                                float4* accumulator_output)
{
    int groupID = blockIdx.x; // 2 groups per batch item
    int localID = threadIdx.x;

    int batchIndex = groupID >> 1; // groupId / 2
    int side = groupID & 1;  // groupId % 2

    int offset = batchIndex * 64 + 32 * side;

    int vector1Index = localID;
    int vector2Index = localID + 32; 

    float4 sum1 = accumulator_bias[vector1Index];
    float4 sum2 = accumulator_bias[vector2Index];

    fake_quantize4(&sum1, QA);
    fake_quantize4(&sum2, QA);

    int baseIndex;
    int baseIndex_factorizer;

    #pragma unroll
    for(int f = 0; f < 32; f++) 
    {
        //This is executed within a single wavefront so shared local 
        //group buffer / broadcast is slower than just loading it directly.
        short currentFeature = activeInputs[offset + f];
        if(currentFeature == -1) break;

        baseIndex = currentFeature * VECTOR_ACCUMULATOR_NODES_PER_SIDE;
        baseIndex_factorizer = (currentFeature % BITS_PER_KING_BUCKET) * VECTOR_ACCUMULATOR_NODES_PER_SIDE;

        float4 w1;
        float4 w1_acc = accumulator_weights[baseIndex + vector1Index];
        float4 w1_fact = factorizer_weights[baseIndex_factorizer + vector1Index];
        
        w1.w = w1_acc.w + w1_fact.w;
        w1.x = w1_acc.x + w1_fact.x;
        w1.y = w1_acc.y + w1_fact.y;
        w1.z = w1_acc.z + w1_fact.z;
        fake_quantize4(&w1, QA);
        
        float4 w2;
        float4 w2_acc  = accumulator_weights[baseIndex + vector2Index];
        float4 w2_fact = factorizer_weights[baseIndex_factorizer + vector2Index];

        w2.w = w2_acc.w + w2_fact.w;
        w2.x = w2_acc.x + w2_fact.x;
        w2.y = w2_acc.y + w2_fact.y;
        w2.z = w2_acc.z + w2_fact.z;
        fake_quantize4(&w2, QA);

        sum1.w += w1.w;
        sum1.x += w1.x;
        sum1.y += w1.y;
        sum1.z += w1.z;

        sum2.w += w2.w;
        sum2.x += w2.x;
        sum2.y += w2.y;
        sum2.z += w2.z;
    }

    baseIndex = groupID * VECTOR_ACCUMULATOR_NODES_PER_SIDE;
    accumulator_output[baseIndex + vector1Index] = sum1;
    accumulator_output[baseIndex + vector2Index] = sum2;
}

//Grid Dimensions:  MINIBATCH_SIZE, 1, 1
//Block Dimensions: 32,             1, 1
extern "C" __global__ void calculateOutput(const float* accumulatorOutput,
                                            float* output_weights, float* output_bias,
                                            float* outputs, char* outputBucket)
{
    int batchIndex = blockIdx.x;
    int localID = threadIdx.x; //0-31

    int baseIndex = batchIndex * ACCUMULATOR_NODES;

    float myOutputContribution = 0.0f;
    int bucket = outputBucket[batchIndex];
    int bucketOffset = bucket * ACCUMULATOR_NODES;

    #pragma unroll
    for(int i = localID; i < ACCUMULATOR_NODES; i+= 32)
        myOutputContribution += screlu(accumulatorOutput[baseIndex + i]) * fake_quantize(output_weights[bucketOffset + i], QB);

    __syncthreads();

    #pragma unroll
    for(int offset = 16; offset > 0; offset /= 2) 
        myOutputContribution += __shfl_down(myOutputContribution, offset, 32);

    if(localID == 0)
    {
        float finalOutput = myOutputContribution + fake_quantize(output_bias[bucket], QA_QB);
        
        outputs[batchIndex] = sigmoid(finalOutput);
    }
}

/******* Calculate Deltas *******/

//Grid Dimensions:  MINIBATCH_SIZE,             1, 1
//Block Dimensions: ACCUMULATOR_NODES_PER_SIDE, 1, 1
extern "C" __global__ void calculateDeltas(const float* outputNodes, const float* expectedOutputs,
                            const float* accumulator_outputs,
                            const float* output_weights,
                            float* output_delta,
                            float* accumulator_delta,
                            float* loss, 
                            char* outputBucket)
{
    int batchIndex = blockIdx.x; //0-16383
    int localID = threadIdx.x; //0-255

    int bucket = outputBucket[batchIndex];

    __shared__ float shared_output_delta;

    //output_delta
    if(localID == 0) 
    {
        float output = outputNodes[batchIndex];
        float expected = expectedOutputs[batchIndex];

        shared_output_delta = output - expected;
        
        loss[batchIndex] = (shared_output_delta * shared_output_delta);
        shared_output_delta *= (output * (1.0f - output));

        output_delta[batchIndex] = shared_output_delta;
    } 
    __syncthreads();

    //accumulator_delta
    //512 delta / 256 threads = 2 deltas per
    for(int nodeIndex = localID; nodeIndex < ACCUMULATOR_NODES; nodeIndex+=ACCUMULATOR_NODES_PER_SIDE) 
    {
        float sum = shared_output_delta * fake_quantize(output_weights[bucket * ACCUMULATOR_NODES + nodeIndex], QB);
        accumulator_delta[batchIndex * ACCUMULATOR_NODES + nodeIndex] = sum * screlu_derivative(accumulator_outputs[batchIndex * ACCUMULATOR_NODES + nodeIndex]);
    }
}

/******* Calculate & Accumulate Edge Weight Gradients *******/

//Grid Dimensions:  OUTPUT_BUCKETS * ACCUMULATOR_NODES, 1, 1
//Block Dimensions: 32,                                 1, 1
extern "C" __global__ void calculate_output_gradient(const float* output_delta,
                                    const float* accumulator_outputs,
                                    float* output_weights_gradient_sum,
                                    float* output_bias_gradient_sum,
                                    char* outputBucket)
{
    int weightIndex = blockIdx.x;
    int inputNodeIndex = weightIndex % ACCUMULATOR_NODES;
    int inputNodeBucket = weightIndex / ACCUMULATOR_NODES;

    int localID = threadIdx.x;

    float partialSum = 0.0f;
    float partialBiasSum = 0.0f;

    for(int batchIndex = localID; batchIndex < MINIBATCH_SIZE; batchIndex += 32) 
    {
        float delta = output_delta[batchIndex];
        int bucket = outputBucket[batchIndex];

        if(bucket == inputNodeBucket)
        {
            partialSum += delta * screlu(accumulator_outputs[ACCUMULATOR_NODES * batchIndex + inputNodeIndex]);
            if(inputNodeIndex == 0)
                partialBiasSum += delta;
        }
    }

    #pragma unroll
    for(int offset = 16; offset > 0; offset /= 2)
    {
        partialSum += __shfl_down(partialSum, offset, 32);
        if(inputNodeIndex == 0)
            partialBiasSum += __shfl_down(partialBiasSum, offset, 32);
    }

    if(localID == 0)
    {
        output_weights_gradient_sum[weightIndex] = partialSum;
        if(inputNodeIndex == 0) output_bias_gradient_sum[inputNodeBucket] = partialBiasSum;
    }
}

//Grid Dimensions:  MINIBATCH_SIZE,             1, 1
//Block Dimensions: ACCUMULATOR_NODES_PER_SIDE, 1, 1
extern "C" __global__ void calculate_accumulator_gradient(const short* activeInputs, //[MINIBATCH_SIZE][2][32]
                                    const float* accumulator_delta, 
                                    float* factorizer_gradient_sum,
                                    float* accumulator_gradient_sum, 
                                    float* accumulator_bias_gradient_sum)
{
    int batchIndex = blockIdx.x;
    int localID = threadIdx.x;

    // Shared storage for the 64 active features of this board position
    __shared__ short sharedFeatures[64];
    __shared__ float batch_accumulator_deltas[ACCUMULATOR_NODES];

    if(localID < 64) 
        sharedFeatures[localID] = activeInputs[batchIndex * 64 + localID];
    
    __syncthreads();
    int offset = batchIndex * ACCUMULATOR_NODES;

    float delta_A = accumulator_delta[offset + localID];
    float delta_B = accumulator_delta[offset + ACCUMULATOR_NODES_PER_SIDE + localID];

    int f = 0;
    while(f < 32 && sharedFeatures[f] != -1)
    {
        short featureA = sharedFeatures[f];
        short featureB = sharedFeatures[32 + f];

        atomicAdd(&accumulator_gradient_sum[featureA * ACCUMULATOR_NODES_PER_SIDE + localID], delta_A);
        atomicAdd(&accumulator_gradient_sum[featureB * ACCUMULATOR_NODES_PER_SIDE + localID], delta_B);
        
        featureA = featureA % BITS_PER_KING_BUCKET;
        featureB = featureB % BITS_PER_KING_BUCKET;

        atomicAdd(&factorizer_gradient_sum[featureA * ACCUMULATOR_NODES_PER_SIDE + localID], delta_A);
        atomicAdd(&factorizer_gradient_sum[featureB * ACCUMULATOR_NODES_PER_SIDE + localID], delta_B);

        f++;
    }

    //Bias
    atomicAdd(&accumulator_bias_gradient_sum[localID], delta_A + delta_B);
}

/******* Adam Update *******/
#define UPDATE_ADAMW(weight, gradientSum, firstMoment, secondMoment, size, min, max) \
    if(i < size) \
    { \
        float grad = gradientSum[i] / MINIBATCH_SIZE; \
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
    }

//Grid Dimensions:  output_weights_count / 32, 1, 1
//Block Dimensions: 32,                1, 1
extern "C" __global__ void adamW(float* output_weights, float* output_gradients, float* m_output, float* v_output,
                                    float* acc_bias_weights, float* acc_bias_gradients, float* m_acc_bias, float* v_acc_bias,
                                    float* output_bias_weights, float* output_bias_gradients, float* m_output_bias, float* v_output_bias,
                                    float lr, float biasCorrection1, float biasCorrection2, float rectificationTerm) 
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    
    UPDATE_ADAMW(output_weights, output_gradients, m_output, v_output, (OUTPUT_BUCKETS * ACCUMULATOR_NODES), MIN_WEIGHT, MAX_WEIGHT);
    UPDATE_ADAMW(acc_bias_weights, acc_bias_gradients, m_acc_bias, v_acc_bias, ACCUMULATOR_NODES_PER_SIDE, MIN_WEIGHT, MAX_WEIGHT);
    UPDATE_ADAMW(output_bias_weights, output_bias_gradients, m_output_bias, v_output_bias, OUTPUT_BUCKETS, MIN_WEIGHT, MAX_WEIGHT);
}

//Grid Dimensions:  accumulator_weights_count / 32, 1, 1
//Block Dimensions: 32,                1, 1
extern "C" __global__ void inputAdamW(float* acc_weights, float* acc_gradients, float* m_acc, float* v_acc,
                                        float* factorizer_weights, float* factorizer_gradients, float* m_factorizer, float* v_factorizer,
                                        float lr, float biasCorrection1, float biasCorrection2, float rectificationTerm)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int size = gridDim.x * blockDim.x;

    UPDATE_ADAMW(acc_weights, acc_gradients, m_acc, v_acc, size, MIN_WEIGHT / 2, MAX_WEIGHT / 2);
    UPDATE_ADAMW(factorizer_weights, factorizer_gradients, m_factorizer, v_factorizer, (BITS_PER_KING_BUCKET * ACCUMULATOR_NODES_PER_SIDE), MIN_WEIGHT / 2, MAX_WEIGHT / 2);
}

extern "C" __global__ void lookahead_update(float* fast_weights, float* slow_weights)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;;
    
    slow_weights[i] += LOOKAHEAD_ALPHA * (fast_weights[i] - slow_weights[i]);
    fast_weights[i] = slow_weights[i];
}