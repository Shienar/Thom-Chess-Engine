#define MINIBATCH_SIZE 16384
#define KING_BUCKETS 10
#define OUTPUT_BUCKETS 8
#define BITS_PER_KING_BUCKET 768
#define INPUT_BITS_PER_SIDE (BITS_PER_KING_BUCKET * KING_BUCKETS)
#define ACCUMULATOR_NODES 512
#define ACCUMULATOR_NODES_PER_SIDE (ACCUMULATOR_NODES / 2)
#define VECTOR_ACCUMULATOR_NODES_PER_SIDE (ACCUMULATOR_NODES_PER_SIDE / 4)

#define ADAM_BETA1 0.9f
#define ADAM_BETA2 0.999f
#define ADAM_EPSILON 1e-8
#define ADAM_WEIGHT_DECAY 1e-2f

#define EVAL_SCALE 400.0f

#define LOOKAHEAD_ALPHA 0.5f

#define QA 255.0f
#define QB 64.0f
#define QA_QB 16320.0f

//Bound requirement for lizard screlu
#define MIN_WEIGHT -1.98f
#define MAX_WEIGHT 1.98f

/******* UTIL *******/

//From https://streamhpc.com/blog/2016-02-09/atomic-operations-for-floats-in-opencl-improved/
inline void atomicAdd(volatile __global float *addr, float val)
{
    union {
        unsigned int u32;
        float f32;
    } next, expected, current;
    current.f32 = *addr;
    do {
        expected.f32 = current.f32;
        next.f32 = expected.f32 + val;
        current.u32 = atomic_cmpxchg( (volatile __global unsigned int *)addr,
        expected.u32, next.u32);
    } while( current.u32 != expected.u32 );
}

inline float screlu(float x) { float y = fmin(fmax(x, 0.0f), 1.0f); return y * y; }
inline float screlu_derivative(float x) { return (((x > 0.0f) && (x < 1.0f)) ? (2.0f*x) : 0.0 ); }

inline float sigmoid(float x) { return 1.0f / (1.0f + native_exp(-x)); }

#define QAT
#ifdef QAT
inline float fake_quantize(float val, float scale) { return rint(val * scale) / scale; }
inline void fake_quantize4(float4* val, float scale) {
    val->x = fake_quantize(val->x, scale);
    val->y = fake_quantize(val->y, scale);
    val->z = fake_quantize(val->z, scale);
    val->w = fake_quantize(val->w, scale);
    return;
}
#else
inline float fake_quantize(float val, float scale) { return val; }
inline void fake_quantize4(float4* val, float scale) { return; }
#endif

/******* Forward Propagation *******/

#define MULT_ADD_WEIGHT_SUM_VECTOR(sum, weight, vectorIndex) \
        float4 weight; \
        float4 weight##_acc = accumulator_weights[baseIndex + vectorIndex]; \
        float4 weight##_fact = factorizer_weights[baseIndex_factorizer + vectorIndex]; \
        weight.w = weight##_acc.w + weight##_fact.w; \
        weight.x = weight##_acc.x + weight##_fact.x; \
        weight.y = weight##_acc.y + weight##_fact.y; \
        weight.z = weight##_acc.z + weight##_fact.z; \
        fake_quantize4(&weight, QA); \
        sum.w += weight.w; \
        sum.x += weight.x; \
        sum.y += weight.y; \
        sum.z += weight.z

//Grid Dimensions:  MINIBATCH_SIZE * 2,         1, 1
//Block Dimensions: 32, 1, 1
//float 4 vectors processed per thread = (ACCUMULATOR_NODES_PER_SIDE / 4) / 32 = 2
__kernel void calculateAccumulator(__global const short* activeInputs, // [MINIBATCH_SIZE][2][32]
                                                __global const float4* accumulator_weights,
                                                __global const float4* factorizer_weights,
                                                __global const float4* accumulator_bias,
                                                __global float4* accumulator_output)
{
    int groupID = get_group_id(0); // 2 groups per batch item
    int localID = get_local_id(0);

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
        //This is executed within a single wavefront so a shared local group buffer 
        //with broadcast is slower than just loading it directly.
        short currentFeature = activeInputs[offset + f];
        if(currentFeature == -1) break;

        baseIndex = currentFeature * VECTOR_ACCUMULATOR_NODES_PER_SIDE;
        baseIndex_factorizer = (currentFeature % BITS_PER_KING_BUCKET) * VECTOR_ACCUMULATOR_NODES_PER_SIDE;

        MULT_ADD_WEIGHT_SUM_VECTOR(sum1, w1, vector1Index);
        MULT_ADD_WEIGHT_SUM_VECTOR(sum2, w2, vector2Index);
    }

    baseIndex = groupID * VECTOR_ACCUMULATOR_NODES_PER_SIDE;
    accumulator_output[baseIndex + vector1Index] = sum1;
    accumulator_output[baseIndex + vector2Index] = sum2;
}

//Grid Dimensions:  MINIBATCH_SIZE, 1, 1
//Block Dimensions: 32,             1, 1
__kernel void calculateOutput(__global const float* accumulatorOutput,
                                            __global float* output_weights, __global float* output_bias,
                                            __global float* outputs, __global char* outputBucket)
{
    int batchIndex = get_group_id(0);
    int localID = get_local_id(0); //0-31

    int baseIndex = batchIndex * ACCUMULATOR_NODES;

    __local float outputContributions[32];
    outputContributions[localID] = 0.0f;
    int bucket = outputBucket[batchIndex];
    int bucketOffset = bucket * ACCUMULATOR_NODES;

    #pragma unroll
    for(int i = localID; i < ACCUMULATOR_NODES; i+=get_local_size(0))
        outputContributions[localID] += screlu(accumulatorOutput[baseIndex + i]) * fake_quantize(output_weights[bucketOffset + i], QB);

    barrier(CLK_LOCAL_MEM_FENCE);

    #pragma unroll
    for(int offset = 16; offset > 0; offset /= 2)
    {
        if(localID < offset)
            outputContributions[localID] += outputContributions[localID + offset];
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    if(localID == 0)
    {
        float finalOutput = outputContributions[localID] + fake_quantize(output_bias[bucket], QA_QB);
        outputs[batchIndex] = sigmoid(finalOutput);
    }
}

/******* Calculate Deltas *******/

//Grid Dimensions:  MINIBATCH_SIZE,             1, 1
//Block Dimensions: ACCUMULATOR_NODES_PER_SIDE, 1, 1
__kernel void calculateDeltas(__global const float* outputNodes, __global const float* expectedOutputs,
                            __global const float* accumulator_outputs,
                            __global const float* output_weights,
                            __global float* output_delta,
                            __global float* accumulator_delta,
                            __global float* loss, 
                            __global char* outputBucket)
{
    int batchIndex = get_group_id(0);
    int localID = get_local_id(0);

    int bucket = outputBucket[batchIndex];

    __local float shared_output_delta;

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
    barrier(CLK_LOCAL_MEM_FENCE);

    //accumulator_delta
    for(int nodeIndex = localID; nodeIndex < ACCUMULATOR_NODES; nodeIndex+=get_local_size(0)) 
    {
        float sum = shared_output_delta * fake_quantize(output_weights[bucket * ACCUMULATOR_NODES + nodeIndex], QB);
        accumulator_delta[batchIndex * ACCUMULATOR_NODES + nodeIndex] = sum * screlu_derivative(accumulator_outputs[batchIndex * ACCUMULATOR_NODES + nodeIndex]);
    }
}

/******* Calculate & Accumulate Edge Weight Gradients *******/

//Grid Dimensions:  OUTPUT_BUCKETS * ACCUMULATOR_NODES, 1, 1
//Block Dimensions: 32,                                 1, 1
__kernel void calculate_output_gradient(__global const float* output_delta,
                                    __global const float* accumulator_outputs,
                                    __global float* output_weights_gradient_sum,
                                    __global float* output_bias_gradient_sum,
                                    __global char* outputBucket)
{
    int weightIndex = get_group_id(0);
    int inputNodeIndex = weightIndex % ACCUMULATOR_NODES;
    int inputNodeBucket = weightIndex / ACCUMULATOR_NODES;

    int localID = get_local_id(0);

    __local float partialSum[32];
    __local float partialBiasSum[32];
    partialSum[localID] = 0.0f;
    partialBiasSum[localID] = 0.0f;

    for(int batchIndex = localID; batchIndex < MINIBATCH_SIZE; batchIndex += get_local_size(0)) 
    {
        float delta = output_delta[batchIndex];
        int bucket = outputBucket[batchIndex];

        if(bucket == inputNodeBucket)
        {
            partialSum[localID] += delta * screlu(accumulator_outputs[ACCUMULATOR_NODES * batchIndex + inputNodeIndex]);
            if(inputNodeIndex == 0)
                partialBiasSum[localID] += delta;
        }
    }

    #pragma unroll
    for(int offset = 16; offset > 0; offset /= 2)
    {
        if(localID < offset)
        {
            partialSum[localID] += partialSum[localID + offset];
            if(inputNodeIndex == 0)
                partialBiasSum[localID] += partialBiasSum[localID + offset];
        }
        barrier(CLK_LOCAL_MEM_FENCE);
    }

    if(localID == 0)
    {
        output_weights_gradient_sum[weightIndex] = partialSum[localID];
        if(inputNodeIndex == 0) output_bias_gradient_sum[inputNodeBucket] = partialBiasSum[localID];
    }
}

//Grid Dimensions:  ACCUMULATOR_NODES_PER_SIDE, 1, 1
//Block Dimensions: 32,                         1, 1
__kernel void calculator_accumulator_bias_gradient(__global const float* accumulator_delta, __global float* accumulator_bias_gradient_sum)
{
    int nodeIndex = get_group_id(0);
    int localID = get_local_id(0);

    float sum = 0;
    for(int batchIndex = localID; batchIndex < MINIBATCH_SIZE; batchIndex+=get_local_size(0))
    {
        sum += accumulator_delta[batchIndex * ACCUMULATOR_NODES + nodeIndex];
        sum += accumulator_delta[batchIndex * ACCUMULATOR_NODES + ACCUMULATOR_NODES_PER_SIDE + nodeIndex];
    }
    accumulator_bias_gradient_sum[nodeIndex] = sum;
}

//Grid Dimensions:  MINIBATCH_SIZE, 1, 1
//Block Dimensions: 64,             1, 1
__kernel void calculate_accumulator_gradient(__global const short* activeInputs, //[MINIBATCH_SIZE][2][32]
                                    __global const float* accumulator_delta, 
                                    __global float* factorizer_gradient_sum,
                                    __global float* accumulator_gradient_sum)
{
    // This is heavily bottlenecked by atomics, but it is still faster than all the other variants that I've tried.
    // A minibatch can get processed in 11ms. This is 7ms.
    // With a local size of ACCUMULATOR_NODES_PER_SIDE & less deltas per thread, it executes in 9ms.
    int batchIndex = get_group_id(0);
    int localID = get_local_id(0);
    int sideID = localID & 31;
    int side = localID >= 32;

    __local short sharedFeatures[64];
    sharedFeatures[localID] = activeInputs[batchIndex * 64 + localID];
    barrier(CLK_LOCAL_MEM_FENCE);

    int offset = batchIndex * ACCUMULATOR_NODES + ACCUMULATOR_NODES_PER_SIDE * side;

    //32 threads per side handling the deltas for 256 nodes per side.
    float nodeDeltas[8] = {0};
    for(int i = 0; i < 8; i++)
        nodeDeltas[i] = accumulator_delta[offset + sideID + 32 * i];

    int f = 0;
    while(f < 32 && sharedFeatures[f] != -1)
    {
        short featureIndex = sharedFeatures[f + 32 * side];
        int featureOffset = featureIndex * ACCUMULATOR_NODES_PER_SIDE;
        int factorizerFeatureOffset = (featureIndex % BITS_PER_KING_BUCKET) * ACCUMULATOR_NODES_PER_SIDE;

        for(int i = 0; i < 8; i++)
        {
            atomicAdd(&accumulator_gradient_sum[featureOffset + sideID + 32 * i], nodeDeltas[i]);
            atomicAdd(&factorizer_gradient_sum[factorizerFeatureOffset + sideID + 32 * i], nodeDeltas[i]);
        }

        f++;
    }
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
            float denominator = sqrt(secondMoment[i] / (1.0f - biasCorrection2)) + ADAM_EPSILON; \
            weight[i] -= lr * rectificationTerm * (correctedFirstMoment / denominator); \
        } else weight[i] -= lr * correctedFirstMoment; \
        weight[i] -= lr * ADAM_WEIGHT_DECAY * weight[i]; \
        weight[i] = fmin(max, fmax(min, weight[i])); \
        gradientSum[i] = 0.0f; \
    }

//Grid Dimensions:  output_weights_count / 32, 1, 1
//Block Dimensions: 32,                1, 1
__kernel void adamW(__global float* output_weights, __global float* output_gradients, __global float* m_output, __global float* v_output,
                                    __global float* acc_bias_weights, __global float* acc_bias_gradients, __global float* m_acc_bias, __global float* v_acc_bias,
                                    __global float* output_bias_weights, __global float* output_bias_gradients, __global float* m_output_bias, __global float* v_output_bias,
                                    float lr, float biasCorrection1, float biasCorrection2, float rectificationTerm) 
{
    int i = get_group_id(0) * get_local_size(0) + get_local_id(0);
    
    UPDATE_ADAMW(output_weights, output_gradients, m_output, v_output, (OUTPUT_BUCKETS * ACCUMULATOR_NODES), MIN_WEIGHT, MAX_WEIGHT);
    UPDATE_ADAMW(acc_bias_weights, acc_bias_gradients, m_acc_bias, v_acc_bias, ACCUMULATOR_NODES_PER_SIDE, MIN_WEIGHT, MAX_WEIGHT);
    UPDATE_ADAMW(output_bias_weights, output_bias_gradients, m_output_bias, v_output_bias, OUTPUT_BUCKETS, MIN_WEIGHT, MAX_WEIGHT);
}

//Grid Dimensions:  accumulator_weights_count / 32, 1, 1
//Block Dimensions: 32,                1, 1
__kernel void inputAdamW(__global float* acc_weights, __global float* acc_gradients, __global float* m_acc, __global float* v_acc,
                                        __global float* factorizer_weights, __global float* factorizer_gradients, __global float* m_factorizer, __global float* v_factorizer,
                                        float lr, float biasCorrection1, float biasCorrection2, float rectificationTerm)
{
    int i = get_group_id(0) * get_local_size(0) + get_local_id(0);
    int size = get_num_groups(0) * get_local_size(0);

    UPDATE_ADAMW(acc_weights, acc_gradients, m_acc, v_acc, size, MIN_WEIGHT / 2, MAX_WEIGHT / 2);
    UPDATE_ADAMW(factorizer_weights, factorizer_gradients, m_factorizer, v_factorizer, (BITS_PER_KING_BUCKET * ACCUMULATOR_NODES_PER_SIDE), MIN_WEIGHT / 2, MAX_WEIGHT / 2);
}

__kernel void lookahead_update(__global float* fast_weights, __global float* slow_weights)
{
    int i = get_group_id(0) * get_local_size(0) + get_local_id(0);;
    
    slow_weights[i] += LOOKAHEAD_ALPHA * (fast_weights[i] - slow_weights[i]);
    fast_weights[i] = slow_weights[i];
}