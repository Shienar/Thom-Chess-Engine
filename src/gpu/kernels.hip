#include <hip/hip_runtime.h>

#define MINIBATCH_SIZE 16384
#define INPUT_BITS 12800
#define HALF_INPUT_BITS (INPUT_BITS / 2)
#define ACCUMULATOR_NODES 512
#define ACCUMULATOR_NODES_PER_SIDE (ACCUMULATOR_NODES / 2)
#define VECTOR_ACCUMULATOR_NODES (ACCUMULATOR_NODES / 4)
#define VECTOR_ACCUMULATOR_NODES_PER_SIDE (ACCUMULATOR_NODES_PER_SIDE / 4)
#define SECOND_HIDDEN_LAYER_NODES 32
#define THIRD_HIDDEN_LAYER_NODES 32
#define OUTPUT_BUCKETS 1

#define ADAM_BETA1 0.9f
#define ADAM_BETA2 0.999f
#define ADAM_EPSILON 1e-8f
#define ADAM_WEIGHT_DECAY 1e-3f

#define EVAL_SCALE 400.0f
#define OUTPUT_SCALE 16.0f

#define LOOKAHEAD_ALPHA 0.5f

#define QA 255.0f
#define QB 64.0f

#define QA_16_MIN (-32768.0f / QA)
#define QA_16_MAX (32767.0f / QA)

#define QB_8_MIN (-128.0f / QB)
#define QB_8_MAX (127.0f / QB)

#define B_HIDDEN_MIN (-2147483648.0f / (QA * QB))
#define B_HIDDEN_MAX (2147483647.0f / (QA * QB))

/******* UTIL *******/

__device__ inline float screlu(float x) { float y = fminf(fmaxf(x, 0.0f), 1.0f); return y * y; }
__device__ inline float crelu(float x) { return fminf(fmaxf(x, 0.0f), 1.0f); }
__device__ inline float screlu_derivative(float x) { return (((x > 0.0f) && (x < 1.0f)) ? (2.0f*x) : 0.0f ); }
__device__ inline float crelu_derivative(float x) { return (float) ((x > 0.0f) && (x < 1.0f)); }

__device__ inline float sigmoid(float x) { return 1.0f / (1.0f + __expf(-x)); }

__device__ inline float fake_quantize(float val, float scale, float min_int, float max_int) { return fminf(max_int, fmaxf(min_int, rintf(val * scale))) / scale; }
__device__ inline float4 fake_quantize4(float4 val, float scale, float min_int, float max_int) {
    float4 out;
    out.x = fake_quantize(val.x, scale, min_int, max_int);
    out.y = fake_quantize(val.y, scale, min_int, max_int);
    out.z = fake_quantize(val.z, scale, min_int, max_int);
    out.w = fake_quantize(val.w, scale, min_int, max_int);
    return out;
}

/******* Forward Propagation *******/
//~ 0.81 ms
//Grid Dimensions:  MINIBATCH_SIZE * 2, 1, 1
//Block Dimensions: 32,                 1, 1
extern "C" __global__ void calculateAccumulator(const short* activeInputs, // [MINIBATCH_SIZE][2][32] - max 32 active features
                                                const float4* weights, //shared
                                                const float4* bias,    //shared
                                                float4* output)  //contains every raw output in entire batch
{
    int groupID = blockIdx.x;  // 2 groups per batch item
    int localID = threadIdx.x;  // 0 to 31

    int batchIndex = groupID >> 1;      // groupId / 2
    int side       = groupID & 1;       // groupId % 2

    int offset = batchIndex * 64 + 32 * side;

    int vector1Index = localID * 2;
    int vector2Index = localID * 2 + 1; 

    float4 sum1 = fake_quantize4(bias[vector1Index], QA, -32768.0f, 32767.0f);
    float4 sum2 = fake_quantize4(bias[vector2Index], QA, -32768.0f, 32767.0f);

    int baseIndex;

    unsigned long long mask_32 = 0x00000000FFFFFFFF;
    #pragma unroll
    for (int f = 0; f < 32; f++) 
    {
        //This is executed within a single wavefront so shared local 
        //group buffer / broadcast is slower than just loading it directly.
        short currentFeature = activeInputs[offset + f];

        if(currentFeature == -1) break;

        baseIndex = currentFeature * VECTOR_ACCUMULATOR_NODES_PER_SIDE;

        float4 w1 = fake_quantize4(weights[baseIndex + vector1Index], QA, -32768.0f, 32767.0f);
        float4 w2 = fake_quantize4(weights[baseIndex + vector2Index], QA, -32768.0f, 32767.0f);

        sum1.x += w1.x;  sum1.y += w1.y;  sum1.z += w1.z;  sum1.w += w1.w;
        sum2.x += w2.x;  sum2.y += w2.y;  sum2.z += w2.z;  sum2.w += w2.w;
    }

    baseIndex = groupID * VECTOR_ACCUMULATOR_NODES_PER_SIDE;
    output[baseIndex + vector1Index] = sum1;
    output[baseIndex + vector2Index] = sum2;
}

//~1ms
//Grid Dimensions:  MINIBATCH_SIZE, 1, 1
//Block Dimensions: 32,             1, 1
extern "C" __global__ void forwardPropagate(const float* accumulatorOutput,
                                            float* h2_weights, float* h2_bias, float* h2_output,
                                            float* h3_weights, float* h3_bias, float* h3_output,
                                            float* output_weights, float* output_bias,
                                            float* outputs)
{
    int batchIndex = blockIdx.x;
    int localID = threadIdx.x; //0-31

    int baseIndex = batchIndex * ACCUMULATOR_NODES;

    //each local thread contributes to loading part of the input.
    //this same array will get reused later on, even if its too big to store outputs for h2/h3
    __shared__ float shared[ACCUMULATOR_NODES];
    #pragma unroll
    for(int i = localID; i < ACCUMULATOR_NODES; i+= 32)
    {
        shared[i] = screlu(accumulatorOutput[baseIndex + i]);
    }
    __syncthreads();

    //calculate h2
    float sum = fake_quantize(h2_bias[localID], QA * QB, -2147483648.0f, 2147483647.0f);;

    #pragma unroll 4
    for (int inputIndex = 0; inputIndex < ACCUMULATOR_NODES; inputIndex++) 
    {
        sum += shared[inputIndex] * fake_quantize(h2_weights[inputIndex * SECOND_HIDDEN_LAYER_NODES + localID], QB, -128.0f, 127.0f);
    }
    sum = crelu(sum);

    __syncthreads(); 

    shared[localID] = sum; //faster retrieval for use in this function.
    h2_output[batchIndex * SECOND_HIDDEN_LAYER_NODES + localID] = sum; //for use in backprop

    __syncthreads();

    //calculate h3
    sum = fake_quantize(h3_bias[localID], QA * QB,  -2147483648.0f, 2147483647.0f);

    #pragma unroll
    for (int inputIndex = 0; inputIndex < SECOND_HIDDEN_LAYER_NODES; inputIndex++) 
    {
        sum += shared[inputIndex] * fake_quantize(h3_weights[inputIndex * THIRD_HIDDEN_LAYER_NODES + localID], QB, -128.0f, 127.0f);
    }
    sum = crelu(sum);
    
    __syncthreads(); 

    shared[localID] = sum;
    h3_output[batchIndex * THIRD_HIDDEN_LAYER_NODES + localID] = sum;

    __syncthreads();

    //calculate output
    float myOutputContribution = shared[localID] * fake_quantize(output_weights[localID], QB, -128.0f, 127.0f);

    unsigned long long mask_32 = 0x00000000FFFFFFFF;
    #pragma unroll
    for (int offset = 16; offset > 0; offset /= 2) 
    {
        myOutputContribution += __shfl_down_sync(mask_32, myOutputContribution, offset, 32);
    }

    if(localID == 0)  
    {
        float finalOutput = myOutputContribution + fake_quantize(output_bias[0], QA * QB, -2147483648.0f, 2147483647.0f);
        
        //32 float weights in range of [-2, 1.98] cannot reach expected outputs in range of +-10,000 so we must scale the output.
        //Scale by the relative scale of the quantized output node during fprop.
        finalOutput *= (QA / OUTPUT_SCALE);

        outputs[batchIndex] = sigmoid(finalOutput / EVAL_SCALE);
    }
}

/******* Backpropagation (Delta calculations) *******/

//~2.5 ms
//Grid Dimensions:  MINIBATCH_SIZE,             1, 1
//Block Dimensions: ACCUMULATOR_NODES_PER_SIDE, 1, 1
extern "C" __global__ void backpropagate( const float* outputNodes, const float* expectedOutputs,
                            const float* h3, 
                            const float* h2,
                            const float* h1,
                            const float* weights4,
                            const float* weights3,
                            const float* weights2,
                            float* delta4,
                            float* delta3,
                            float* delta2,
                            float* delta1,
                            float* loss)
{
    int batchIndex = blockIdx.x; //0-16383
    int localID = threadIdx.x; //0-255

    __shared__ float shared_d4;
    __shared__ float shared_d3[32];
    __shared__ float shared_d2[32]; 

    //delta4
    if(localID == 0) 
    {
        float output = outputNodes[batchIndex];
        float expected = expectedOutputs[batchIndex];

        shared_d4 = output - expected;
        
        loss[batchIndex] = (shared_d4 * shared_d4);
        shared_d4 *= (output * (1.0f - output)) * (QA / OUTPUT_SCALE); //sigmoid derivative, Chain Rule

        delta4[batchIndex] = shared_d4;
    } 
    __syncthreads();
    
    //delta3
    if(localID < THIRD_HIDDEN_LAYER_NODES) 
    {
        float h3_val = h3[batchIndex * THIRD_HIDDEN_LAYER_NODES + localID];
        float d3 = shared_d4 * fake_quantize(weights4[localID], QB, -128.0f, 127.0f) * crelu_derivative(h3_val);
        delta3[batchIndex * THIRD_HIDDEN_LAYER_NODES + localID] = d3;
        shared_d3[localID] = d3;
    }
    __syncthreads();

    //delta2
    if(localID < SECOND_HIDDEN_LAYER_NODES) 
    {
        float sum = 0.0f;
        #pragma unroll
        for (int outputIndex = 0; outputIndex < THIRD_HIDDEN_LAYER_NODES; outputIndex++) 
        {
            sum += shared_d3[outputIndex] * fake_quantize(weights3[localID * THIRD_HIDDEN_LAYER_NODES + outputIndex], QB, -128.0f, 127.0f);
        }
        float h2_val = h2[batchIndex * SECOND_HIDDEN_LAYER_NODES + localID];
        float d2 = sum * crelu_derivative(h2_val);
        delta2[batchIndex * SECOND_HIDDEN_LAYER_NODES + localID] = d2;
        shared_d2[localID] = d2;
    }
    __syncthreads();

    //delta 1
    //512 delta / 256 threads = 2 deltas per
    for (int nodeIndex = localID; nodeIndex < ACCUMULATOR_NODES; nodeIndex+=256) 
    {
        float sum = 0.0f;
        #pragma unroll
        for (int outputIndex = 0; outputIndex < SECOND_HIDDEN_LAYER_NODES; outputIndex++) 
        {
            sum += shared_d2[outputIndex] * fake_quantize(weights2[nodeIndex * SECOND_HIDDEN_LAYER_NODES + outputIndex], QB, -128.0f, 127.0f);
        }
        delta1[batchIndex * ACCUMULATOR_NODES + nodeIndex] = sum * screlu_derivative(h1[batchIndex * ACCUMULATOR_NODES + nodeIndex]);
    }
}

/******* Calculate & Accumulate Edge Weight Gradients *******/

//~0.22 ms
//Grid Dimensions:  32, 1, 1
//Block Dimensions: 32, 1, 1
extern "C" __global__ void calculateGradient4(const float* delta4, //1 per batch sample
                                    const float* h3, //THIRD_HIDDEN_LAYER_NODES per batch sample
                                    float* gradient4_Sum, //THIRD_HIDDEN_LAYER_NODES * OUTPUT_BUCKETS shared
                                    float* bias4_Sum)
{
    int weightIndex = blockIdx.x; //0-31
    int localID = threadIdx.x; //0-31

    float partialSum = 0.0f;
    float partialBiasSum = 0.0f; //Only managed by the threads with weightIndex == 0.

    for(int batchIndex = localID; batchIndex < MINIBATCH_SIZE; batchIndex+=32) 
    {
        float d4 = delta4[batchIndex];

        partialSum += d4 * h3[THIRD_HIDDEN_LAYER_NODES * batchIndex + weightIndex];
        partialBiasSum += d4;
    }

    __syncthreads();
    //Parallel reduction
    unsigned long long mask_32 = 0x00000000FFFFFFFF;
    #pragma unroll
    for (int offset = 16; offset > 0; offset /= 2) {
        partialSum += __shfl_down_sync(mask_32, partialSum, offset, 32);
        partialBiasSum += __shfl_down_sync(mask_32, partialBiasSum, offset, 32);
    }

    if(localID == 0)
    {
        gradient4_Sum[weightIndex] = partialSum;
        if(weightIndex == 0) bias4_Sum[0] = partialBiasSum;
    }
}

//~0.5 ms
//Grid Dimensions:  MINIBATCH_SIZE,             1, 1
//Block Dimensions: ACCUMULATOR_NODES_PER_SIDE, 1, 1
extern "C" __global__ void calculateGradient3(const float* delta3, //THIRD_HIDDEN_LAYER_NODES per batch sample
                                const float* h2, //SECOND_HIDDEN_LAYER_NODES per batch sample
                                float* gradient3_Sum, //SECOND_HIDDEN_LAYER_NODES * THIRD_HIDDEN_LAYER_NODES shared
                                float* bias3_Sum) //THIRD_HIDDEN_LAYER_NODES shared
{
    int inIndex  = blockIdx.x; 
    int outIndex = threadIdx.x; 
    int flatWeightIndex = inIndex * THIRD_HIDDEN_LAYER_NODES + outIndex;

    float sum = 0.0f;
    float biasSum = 0.0f; //Only managed by the threads with inIndex == 0.
    
    for(int batchIndex = 0; batchIndex < MINIBATCH_SIZE; batchIndex++)  
    {
        float d3 = delta3[batchIndex * THIRD_HIDDEN_LAYER_NODES + outIndex];
        sum += d3 * h2[batchIndex * SECOND_HIDDEN_LAYER_NODES + inIndex];
        if(inIndex == 0) biasSum += d3;
    }

    gradient3_Sum[flatWeightIndex] = sum;

    if(inIndex == 0) bias3_Sum[outIndex] = biasSum;
}

//~1ms
//Grid Dimensions:  SECOND_HIDDEN_LAYER_NODES / 16, ACCUMULATOR_NODES / 16, 1
//Block Dimensions: 16,                             16,                     1
extern "C" __global__ void calculateGradient2(const float* delta2, //SECOND_HIDDEN_LAYER_NODES per batch sample
                                const float* h1, //ACCUMULATOR_NODES per batch sample
                                float* gradient2_Sum, //ACCUMULATOR_NODES * SECOND_HIDDEN_LAYER_NODES shared
                                float* bias2_Sum) //SECOND_HIDDEN_LAYER_NODES shared
{

    //16,384 total threads in global group. One per nonbias weight.
    //256 (16 x 16) threads in one group, sharing local data and loading together.
    //row == 0 thread calculates bias for their output column.
    int tileX = threadIdx.x; //0 to 15 (local col)
    int tileY = threadIdx.y; //0 to 15 (local row)
    int col = blockIdx.x * blockDim.x + tileX; // 0 to 31. Two column groups (16 per)
    int row = blockIdx.y * blockDim.y + tileY; // 0 to 511. 32 row groups (16 per)

    __shared__ float tile_h1[16][16];
    __shared__ float tile_delta2[16][16];

    float accumulator = 0.0f;
    float biasAccumulator = 0.0f; 

    for (int batchIndex = 0; batchIndex < MINIBATCH_SIZE; batchIndex+=16) 
    {
        /**
          * Load into local memory.
          *
          * The extra activation here is faster than storing it activated and
          * using sqrt() in the derivative function.
          *
          * Tile_h1[batchIndex][row/inputs]
          *     - TileY is local row
          *     - TileX is used as a placeholder for batchIndex
          *
          * Tile_delta2[batchIndex][column/outputs]
          *     - TileX is local column
          *     - TileY is used as a placeholder for batchIndex
          *
          */

        tile_h1[tileX][tileY] = screlu(h1[(batchIndex + tileX) * ACCUMULATOR_NODES + row]); 
        tile_delta2[tileY][tileX] = delta2[(batchIndex + tileY) * 32 + col];

        __syncthreads();

        #pragma unroll
        for (int batchOffset = 0; batchOffset < 16; batchOffset++)  
        {
            accumulator += tile_h1[batchOffset][tileY] * tile_delta2[batchOffset][tileX];
            biasAccumulator += tile_delta2[batchOffset][tileX];
        }
        
        __syncthreads();
    }

    // Write the result out to global memory
    gradient2_Sum[row * 32 + col] = accumulator;
    if(tileY == 0) atomicAdd(&bias2_Sum[col], biasAccumulator);
}

//~4.2ms. Drops to 3.1ms if atomic adds treated as nonatomic. There's just a lot of weights to handle.
//Grid Dimensions:  MINIBATCH_SIZE,             1, 1
//Block Dimensions: ACCUMULATOR_NODES_PER_SIDE, 1, 1
extern "C" __global__ void calculateGradient1(const short* activeInputs,       //contains indexes of weights of every active input in entire batch - [MINIBATCH_SIZE][2][32] - max 32 active features
                                    const float* delta1, // ACCUMULATOR_NODES per batch sample
                                    float* gradient1_Sum, // ACCUMULATOR_NODES_PER_SIDE * HALF_INPUT_BITS shared
                                    float* bias1_Sum)    // ACCUMULATOR_NODES_PER_SIDE shared
{
    int batchIndex = blockIdx.x;
    int localID = threadIdx.x;

    // Shared storage for the 64 active features of this board position
    __shared__ short sharedFeatures[64];
    __shared__ float d1[512];

    if(localID < 64) sharedFeatures[localID] = activeInputs[batchIndex * 64 + localID];
    
    int offset = batchIndex * ACCUMULATOR_NODES;
    
    #pragma unroll
    for(int i= localID; i < ACCUMULATOR_NODES; i+=256)
    {
        d1[i] = delta1[offset + i];
    }
    __syncthreads();

    __shared__ float local_grad_sum_A[256];
    __shared__ float local_grad_sum_B[256];

    int f = 0;
    while(sharedFeatures[f] != -1 && f < 32)
    {
        //Local accumulation and featureA/B save 0.2ms.
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
        firstMoment[i] = ADAM_BETA1 * firstMoment[i] + (1.0f - ADAM_BETA1) * grad; \
        secondMoment[i] = ADAM_BETA2 * secondMoment[i] + (1.0f - ADAM_BETA2) * grad * grad; \
        float correctedFirstMoment = firstMoment[i] / (1.0f - biasCorrection1); \
        if(rectificationTerm > 0.0f) { \
            float denominator = sqrtf(secondMoment[i] / (1.0f - biasCorrection2)) + ADAM_EPSILON; \
            weight[i] -= lr * rectificationTerm * (correctedFirstMoment / denominator); \
        } else { \
            weight[i] -= lr * correctedFirstMoment; \
        } \
        weight[i] -= lr * ADAM_WEIGHT_DECAY * weight[i]; \
        weight[i] = fminf(max, fmaxf(min, weight[i])); \
        gradientSum[i] = 0.0f; \
    }

//~0.1 ms
//Grid Dimensions:  weight2Count / 32, 1, 1
//Block Dimensions: 32,                1, 1
extern "C" __global__ void adamW(float* w2, float* g2, float* m2, float* v2,
                    float* w3, float* g3, float* m3, float* v3,
                    float* w4, float* g4, float* m4, float* v4,
                    float* b1, float* gb1, float* mb1, float* vb1,
                    float* b2, float* gb2, float* mb2, float* vb2,
                    float* b3, float* gb3, float* mb3, float* vb3,
                    float* b4, float* gb4, float* mb4, float* vb4,
                    float lr, float biasCorrection1, float biasCorrection2, float rectificationTerm) 
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;

    UPDATE_ADAMW(w2, g2, m2, v2, 16384, QB_8_MIN, QB_8_MAX); // 512 * 32
    UPDATE_ADAMW(w3, g3, m3, v3, 1024, QB_8_MIN, QB_8_MAX);  // 32 * 32
    UPDATE_ADAMW(b1, gb1, mb1, vb1, 256, QA_16_MIN, QA_16_MAX);
    UPDATE_ADAMW(b2, gb2, mb2, vb2, 32, B_HIDDEN_MIN, B_HIDDEN_MAX);
    UPDATE_ADAMW(b3, gb3, mb3, vb3, 32, B_HIDDEN_MIN, B_HIDDEN_MAX);
    UPDATE_ADAMW(w4, g4, m4, v4, 32, QB_8_MIN, QB_8_MAX);
    UPDATE_ADAMW(b4, gb4, mb4, vb4, 8, B_HIDDEN_MIN, B_HIDDEN_MAX);
    
}

//0.42ms
//Roughly half (e.g. ~778k / 1,638,400) will be active.
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