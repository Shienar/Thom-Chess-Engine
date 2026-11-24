#include "train/hip/gpu_hip.h"
#include "debug.h"
#include "compatibility.h"
#include "train/train.h"
#include "analyze/nnue/neuralnet.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

hip_context_struct hip_context = {
	.device_id = 0,
	.queue = NULL,
	.module = NULL,
	.kernels = { { NULL } }
};

hip_kernel_args hip_mem = { { { NULL } } };
hip_event_struct hip_events = {
	.read_loss = NULL,
	.start_events = { { NULL } },
	.end_events = { { NULL } }
};

float lr = 3e-3f;
uint64_t cosine_timestamp;
uint64_t timestamp;
float rho_inf = (2.0 / (1.0 - ADAM_BETA2)) - 1.0;
float rho_timestamp = 0.0;
float rectification_term = 0.0;
float bias_correction_1 = 1.0;
float bias_correction_2 = 1.0;

const size_t factorizer_weights_size	= sizeof(((training_weights *)0)->factorizer_weights);
const size_t factorizer_weights_count   = sizeof(((training_weights *)0)->factorizer_weights) / sizeof(float);

const size_t accumulator_weights_size   = sizeof(((training_weights *)0)->accumulator_weights);
const size_t accumulator_weights_count  = sizeof(((training_weights *)0)->accumulator_weights) / sizeof(float);

const size_t accumulator_bias_size	  = sizeof(((training_weights *)0)->accumulator_bias);
const size_t accumulator_bias_count	  = sizeof(((training_weights *)0)->accumulator_bias) / sizeof(float);

const size_t output_weights_size		= sizeof(((training_weights *)0)->output_weights);
const size_t output_weights_count	    = sizeof(((training_weights *)0)->output_weights) / sizeof(float);

const size_t output_bias_size		   = sizeof(((training_weights *)0)->output_bias);
const size_t output_bias_count		   = sizeof(((training_weights *)0)->output_bias) / sizeof(float);

const size_t active_inputs_size	     	= MINIBATCH_SIZE * 64 * sizeof(short);
const size_t expected_output_size	 	= MINIBATCH_SIZE * sizeof(float);
const size_t output_bucket_size	    	= MINIBATCH_SIZE * sizeof(char);
const size_t loss_size			  		= MINIBATCH_SIZE * sizeof(float);
const size_t accumulator_output_size 	= MINIBATCH_SIZE * ACCUMULATOR_NODES * sizeof(float);
const size_t final_output_size	   		= MINIBATCH_SIZE * sizeof(float);
const size_t output_delta_size	  		= MINIBATCH_SIZE * sizeof(float);
const size_t accum_delta_size	  	 	= MINIBATCH_SIZE * ACCUMULATOR_NODES * sizeof(float);

short *host_active_inputs_A	  		= NULL;
short *host_active_inputs_B	  		= NULL;
float *host_expected_outputs_A   	= NULL;
float *host_expected_outputs_B   	= NULL;
char *host_output_buckets_A	 		= NULL;
char *host_output_buckets_B	 		= NULL;
float *host_lossbuffer		  		= NULL;

nnue_weights *host_quantized_weights 		= NULL;
training_weights *host_training_weights  	= NULL;
training_weights *host_first_moments	 	= NULL;
training_weights *host_second_moments		= NULL;

void *calculate_accumulator_args[5];
void *calculate_output_args[5];
void *calculate_delta_args[8];
void *output_gradient_args[5];
void *accumulator_gradient_args[4];
void *accumulator_bias_gradient_args[2];
void *input_adam_args[12];
void *dense_adam_args[16];

unsigned char *load_compiled_kernels(size_t *size, const char *file_name)
{
	FILE *input = fopen(file_name, "rb");

	if (!input) {
		printf("Cannot locate compiled gpu kernels at %s\n", file_name);
		return NULL;
	}

	fseek(input, 0, SEEK_END);
	*size = ftell(input);
	rewind(input);

	unsigned char *binary = malloc(*size + 1);

	if (!binary) {
		DEBUG_ERROR("Failed malloc.");
		fclose(input);
		return NULL;
	}

	if (fread(binary, 1, *size, input) < 1) {
		DEBUG_ERROR("Mismatched read size.");
		fclose(input);
		return NULL;
	}

	binary[*size] = '\0';
	fclose(input);
	return binary;
}

hipError_t init_kernels(const char *compiled_kernel_path,
					short **h_active_A, float **h_expected_A, char **h_bucket_A,
					short **h_active_B, float **h_expected_B, char **h_bucket_B,
					float **h_lossbuffer)
{
	cosine_timestamp = 0;
	timestamp = 0;

	hipError_t err;

	if ((err = hipInit(0)) != hipSuccess) {
		DEBUG_ERROR("Failed to initialize HIP.");
		return err;
	}

	hip_context.device_id = 0;
	if ((err = hipSetDevice(hip_context.device_id)) != hipSuccess) {
		DEBUG_ERROR("Failed to acquire GPU device.");
		return err;
	}

	#ifdef PERFT_KERNELS
		err = hipStreamCreate(&hip_context.queue);
	#else
		err = hipStreamCreateWithFlags(&hip_context.queue, hipStreamNonBlocking);
	#endif
	if (err != hipSuccess) {
		DEBUG_ERROR("Failed to initialize queue.");
		return err;
	}

	size_t binary_size = 0;
	unsigned char *binary_kernels = load_compiled_kernels(&binary_size, compiled_kernel_path);

	if (!binary_kernels || (err = hipModuleLoadData(&hip_context.module, binary_kernels)) != hipSuccess) {
		DEBUG_ERROR("Failed to load compiled binaries.");
		return err;
	}
	free(binary_kernels);
	binary_kernels = NULL;

	err = hipModuleGetFunction(&hip_context.kernels.calculate_accumulator, hip_context.module, "calculate_accumulator");
	err = hipModuleGetFunction(&hip_context.kernels.calculate_output, hip_context.module, "calculate_output");
	err = hipModuleGetFunction(&hip_context.kernels.calculate_deltas, hip_context.module, "calculate_deltas");
	err = hipModuleGetFunction(&hip_context.kernels.calculate_output_gradient, hip_context.module, "calculate_output_gradient");
	err = hipModuleGetFunction(&hip_context.kernels.calculate_accumulator_bias_gradient, hip_context.module, "calculate_accumulator_bias_gradient");
	err = hipModuleGetFunction(&hip_context.kernels.calculate_accumulator_gradient, hip_context.module, "calculate_accumulator_gradient");
	err = hipModuleGetFunction(&hip_context.kernels.adamw, hip_context.module, "adam_w");
	err = hipModuleGetFunction(&hip_context.kernels.inputadamw, hip_context.module, "input_adam_w");
	err = hipModuleGetFunction(&hip_context.kernels.lookahead, hip_context.module, "lookahead_update");

	if (err != hipSuccess) {
		DEBUG_ERROR("Error getting kernels.");
		return 1;
	}

	err = hipEventCreate(&hip_events.read_loss);
	for (int i = 0; i < EVENT_TRACKED_KERNELS; i++) {
		err = hipEventCreate(&hip_events.start_events.arr[i]);
		err = hipEventCreate(&hip_events.end_events.arr[i]);
	}
	if (err != hipSuccess) {
		DEBUG_ERROR("Error creating events.");
		return 1;
	}

	/** ALLOCATING **/
	//Existing weights/moments from file.
	//Their copying is infrequent so I'm just doing calloc.
	host_quantized_weights = calloc(1, sizeof(nnue_weights));
	host_training_weights  = calloc(1, sizeof(training_weights));
	host_first_moments	 = calloc(1, sizeof(training_weights));
	host_second_moments	= calloc(1, sizeof(training_weights));
	//Host pinning (Staging buffers)
	hipHostMalloc((void **)&host_active_inputs_A,	 active_inputs_size,	  hipHostMallocDefault);
	hipHostMalloc((void **)&host_active_inputs_B,	 active_inputs_size,	  hipHostMallocDefault);
	hipHostMalloc((void **)&host_expected_outputs_A,  expected_output_size,	hipHostMallocDefault);
	hipHostMalloc((void **)&host_expected_outputs_B,  expected_output_size,	hipHostMallocDefault);
	hipHostMalloc((void **)&host_output_buckets_A,	output_bucket_size,	  hipHostMallocDefault);
	hipHostMalloc((void **)&host_output_buckets_B,	output_bucket_size,	  hipHostMallocDefault);
	hipHostMalloc((void **)&host_lossbuffer,		 loss_size,			  hipHostMallocDefault);
	//Export pointers
	*h_active_A = host_active_inputs_A;
	*h_active_B = host_active_inputs_B;
	*h_expected_A = host_expected_outputs_A;
	*h_expected_B = host_expected_outputs_B;
	*h_bucket_A = host_output_buckets_A;
	*h_bucket_B = host_output_buckets_B;
	*h_lossbuffer = host_lossbuffer;
	//gpu buffers (copied from host-pinning)
	hipMalloc(&hip_mem.mem.active_inputs_A,	  	active_inputs_size);
	hipMalloc(&hip_mem.mem.active_inputs_B,	  	active_inputs_size);
	hipMalloc(&hip_mem.mem.expected_output_A,	expected_output_size);
	hipMalloc(&hip_mem.mem.expected_output_B,	expected_output_size);
	hipMalloc(&hip_mem.mem.output_bucket_A,	  	output_bucket_size);
	hipMalloc(&hip_mem.mem.output_bucket_B,	  	output_bucket_size);
	hipMalloc(&hip_mem.mem.loss,				loss_size);
	//Fast weights
	hipMalloc(&hip_mem.mem.factorizer_weights_fast,	 	factorizer_weights_size);
	hipMalloc(&hip_mem.mem.accumulator_weights_fast,	accumulator_weights_size);
	hipMalloc(&hip_mem.mem.accumulator_bias_fast,	   	accumulator_bias_size);
	hipMalloc(&hip_mem.mem.output_weights_fast,		 	output_weights_size);
	hipMalloc(&hip_mem.mem.output_bias_fast,			output_bias_size);
	//Slow Weights
	hipMalloc(&hip_mem.mem.factorizer_weights_slow,	 	factorizer_weights_size);
	hipMalloc(&hip_mem.mem.accumulator_weights_slow,	accumulator_weights_size);
	hipMalloc(&hip_mem.mem.accumulator_bias_slow,	  	accumulator_bias_size);
	hipMalloc(&hip_mem.mem.output_weights_slow,			output_weights_size);
	hipMalloc(&hip_mem.mem.output_bias_slow,			output_bias_size);
	//Intermediate Outputs
	hipMalloc(&hip_mem.mem.accumulator_output,   accumulator_output_size);
	hipMalloc(&hip_mem.mem.final_output,		 final_output_size);
	//Deltas
	hipMalloc(&hip_mem.mem.output_delta,		output_delta_size);
	hipMalloc(&hip_mem.mem.accumulator_delta,   accum_delta_size);
	//Gradient Sums
	hipMalloc(&hip_mem.mem.factorizer_weights_gradient_sum,	 	factorizer_weights_size);
	hipMalloc(&hip_mem.mem.accumulator_weights_gradient_sum,	accumulator_weights_size);
	hipMalloc(&hip_mem.mem.accumulator_bias_gradient_sum,	   	accumulator_bias_size);
	hipMalloc(&hip_mem.mem.output_weights_gradient_sum,		 	output_weights_size);
	hipMalloc(&hip_mem.mem.output_bias_gradient_sum,			output_bias_size);
	//Adam First moments
	hipMalloc(&hip_mem.mem.m_factorizer_weights,	factorizer_weights_size);
	hipMalloc(&hip_mem.mem.m_accumulator_weights,   accumulator_weights_size);
	hipMalloc(&hip_mem.mem.m_accumulator_bias,	  	accumulator_bias_size);
	hipMalloc(&hip_mem.mem.m_output_weights,		output_weights_size);
	hipMalloc(&hip_mem.mem.m_output_bias,		   	output_bias_size);
	//Adam Second moments
	hipMalloc(&hip_mem.mem.v_factorizer_weights,	factorizer_weights_size);
	hipMalloc(&hip_mem.mem.v_accumulator_weights,   accumulator_weights_size);
	hipMalloc(&hip_mem.mem.v_accumulator_bias,	  	accumulator_bias_size);
	hipMalloc(&hip_mem.mem.v_output_weights,		output_weights_size);
	hipMalloc(&hip_mem.mem.v_output_bias,		   	output_bias_size);

	//Load weights/moments from file or initialize them.
	//Calloc leaves moments at zero if not in file.
	FILE *raw_input = NULL;

	if ((raw_input = fopen("./import/raw.bin", "rb")) != NULL) {
		fread(host_training_weights, sizeof(training_weights), 1, raw_input);
		fclose(raw_input);
		raw_input = NULL;

		FILE *m_input = NULL;
		FILE *v_input = NULL;
		//Require both or none of first/second moments.
		if ((m_input = fopen("./import/first_moments.bin", "rb")) != NULL) {
			if ((v_input = fopen("./import/second_moments.bin", "rb")) != NULL) {
				fread(host_first_moments,  sizeof(training_weights), 1, m_input);
				fread(host_second_moments, sizeof(training_weights), 1, v_input);
				fclose(v_input);
			}
			fclose(m_input);
		}
	} else
		initialize_training_weights(host_training_weights);

	/** COPYING **/
	//factorizer_weights
	hipMemcpyAsync(hip_mem.mem.factorizer_weights_slow, (void *)host_training_weights->factorizer_weights,   	factorizer_weights_size, hipMemcpyHostToDevice, hip_context.queue);
	hipMemcpyAsync(hip_mem.mem.factorizer_weights_fast, hip_mem.mem.factorizer_weights_slow,					factorizer_weights_size, hipMemcpyDeviceToDevice, hip_context.queue);
	//accumulator_weights
	hipMemcpyAsync(hip_mem.mem.accumulator_weights_slow, (void *)host_training_weights->accumulator_weights,	 	accumulator_weights_size, hipMemcpyHostToDevice, hip_context.queue);
	hipMemcpyAsync(hip_mem.mem.accumulator_weights_fast, hip_mem.mem.accumulator_weights_slow,				  		accumulator_weights_size, hipMemcpyDeviceToDevice, hip_context.queue);
	//output_weights
	hipMemcpyAsync(hip_mem.mem.output_weights_slow, (void *)host_training_weights->output_weights,   	output_weights_size, hipMemcpyHostToDevice, hip_context.queue);
	hipMemcpyAsync(hip_mem.mem.output_weights_fast, hip_mem.mem.output_weights_slow,					output_weights_size, hipMemcpyDeviceToDevice, hip_context.queue);
	//accumulator_bias
	hipMemcpyAsync(hip_mem.mem.accumulator_bias_slow, (void *)host_training_weights->accumulator_bias,	accumulator_bias_size, hipMemcpyHostToDevice, hip_context.queue);
	hipMemcpyAsync(hip_mem.mem.accumulator_bias_fast, hip_mem.mem.accumulator_bias_slow,				accumulator_bias_size, hipMemcpyDeviceToDevice, hip_context.queue);
	//output_bias
	hipMemcpyAsync(hip_mem.mem.output_bias_slow, (void *)host_training_weights->output_bias,	 	output_bias_size, hipMemcpyHostToDevice, hip_context.queue);
	hipMemcpyAsync(hip_mem.mem.output_bias_fast, hip_mem.mem.output_bias_slow,				  		output_bias_size, hipMemcpyDeviceToDevice, hip_context.queue);

	/** ZEROING **/
	//Gradients
	hipMemsetAsync(hip_mem.mem.factorizer_weights_gradient_sum,	 	0, factorizer_weights_size,	 	hip_context.queue);
	hipMemsetAsync(hip_mem.mem.accumulator_weights_gradient_sum,	0, accumulator_weights_size,	hip_context.queue);
	hipMemsetAsync(hip_mem.mem.accumulator_bias_gradient_sum,	   	0, accumulator_bias_size,	   	hip_context.queue);
	hipMemsetAsync(hip_mem.mem.output_weights_gradient_sum,		 	0, output_weights_size,		 	hip_context.queue);
	hipMemsetAsync(hip_mem.mem.output_bias_gradient_sum,			0, output_bias_size,			hip_context.queue);
	//First moments
	hipMemsetAsync(hip_mem.mem.m_factorizer_weights,	0, factorizer_weights_size,	 	hip_context.queue);
	hipMemsetAsync(hip_mem.mem.m_accumulator_weights,	0, accumulator_weights_size,	hip_context.queue);
	hipMemsetAsync(hip_mem.mem.m_accumulator_bias,	   	0, accumulator_bias_size,	   	hip_context.queue);
	hipMemsetAsync(hip_mem.mem.m_output_weights,		0, output_weights_size,		 	hip_context.queue);
	hipMemsetAsync(hip_mem.mem.m_output_bias,			0, output_bias_size,			hip_context.queue);
	//Second moments
	hipMemsetAsync(hip_mem.mem.v_factorizer_weights,	0, factorizer_weights_size,	 	hip_context.queue);
	hipMemsetAsync(hip_mem.mem.v_accumulator_weights,	0, accumulator_weights_size,	hip_context.queue);
	hipMemsetAsync(hip_mem.mem.v_accumulator_bias,	   	0, accumulator_bias_size,	   	hip_context.queue);
	hipMemsetAsync(hip_mem.mem.v_output_weights,		0, output_weights_size,		 	hip_context.queue);
	hipMemsetAsync(hip_mem.mem.v_output_bias,			0, output_bias_size,			hip_context.queue);

	hipStreamSynchronize(hip_context.queue);

	/** Set static arguments */
	//calculate_accumulator_args[0] = active inputs
	calculate_accumulator_args[1] = &hip_mem.mem.accumulator_weights_fast;
	calculate_accumulator_args[2] = &hip_mem.mem.factorizer_weights_fast;
	calculate_accumulator_args[3] = &hip_mem.mem.accumulator_bias_fast;
	calculate_accumulator_args[4] = &hip_mem.mem.accumulator_output;

	calculate_output_args[0] = &hip_mem.mem.accumulator_output;
	calculate_output_args[1] = &hip_mem.mem.output_weights_fast;
	calculate_output_args[2] = &hip_mem.mem.output_bias_fast;
	calculate_output_args[3] = &hip_mem.mem.final_output;
	//calculate_output_args[4] = output bucket

	calculate_delta_args[0] = &hip_mem.mem.final_output;
	//calculate_delta_args[1] = expected_output;
	calculate_delta_args[2] = &hip_mem.mem.accumulator_output;
	calculate_delta_args[3] = &hip_mem.mem.output_weights_fast;
	calculate_delta_args[4] = &hip_mem.mem.output_delta;
	calculate_delta_args[5] = &hip_mem.mem.accumulator_delta;
	calculate_delta_args[6] = &hip_mem.mem.loss;
	//calculate_delta_args[7] = output bucket

	output_gradient_args[0] = &hip_mem.mem.output_delta;
	output_gradient_args[1] = &hip_mem.mem.accumulator_output;
	output_gradient_args[2] = &hip_mem.mem.output_weights_gradient_sum;
	output_gradient_args[3] = &hip_mem.mem.output_bias_gradient_sum;
	//output_gradient_args[4] = output bucket

	//accumulator_gradient_args[0] = active inputs;
	accumulator_gradient_args[1] = &hip_mem.mem.accumulator_delta;
	accumulator_gradient_args[2] = &hip_mem.mem.factorizer_weights_gradient_sum;
	accumulator_gradient_args[3] = &hip_mem.mem.accumulator_weights_gradient_sum;

	accumulator_bias_gradient_args[0] = &hip_mem.mem.accumulator_delta;
	accumulator_bias_gradient_args[1] = &hip_mem.mem.accumulator_bias_gradient_sum;

	input_adam_args[0]  = &hip_mem.mem.accumulator_weights_fast;
	input_adam_args[1]  = &hip_mem.mem.accumulator_weights_gradient_sum;
	input_adam_args[2]  = &hip_mem.mem.m_accumulator_weights;
	input_adam_args[3]  = &hip_mem.mem.v_accumulator_weights;
	input_adam_args[4]  = &hip_mem.mem.factorizer_weights_fast;
	input_adam_args[5]  = &hip_mem.mem.factorizer_weights_gradient_sum;
	input_adam_args[6]  = &hip_mem.mem.m_factorizer_weights;
	input_adam_args[7]  = &hip_mem.mem.v_factorizer_weights;
	input_adam_args[8]  = &lr;
	input_adam_args[9]  = &bias_correction_1;
	input_adam_args[10] = &bias_correction_2;
	input_adam_args[11] = &rectification_term;

	dense_adam_args[0]  = &hip_mem.mem.output_weights_fast;
	dense_adam_args[1]  = &hip_mem.mem.output_weights_gradient_sum;
	dense_adam_args[2]  = &hip_mem.mem.m_output_weights;
	dense_adam_args[3]  = &hip_mem.mem.v_output_weights;
	dense_adam_args[4]  = &hip_mem.mem.accumulator_bias_fast;
	dense_adam_args[5]  = &hip_mem.mem.accumulator_bias_gradient_sum;
	dense_adam_args[6]  = &hip_mem.mem.m_accumulator_bias;
	dense_adam_args[7]  = &hip_mem.mem.v_accumulator_bias;
	dense_adam_args[8]  = &hip_mem.mem.output_bias_fast;
	dense_adam_args[9]  = &hip_mem.mem.output_bias_gradient_sum;
	dense_adam_args[10] = &hip_mem.mem.m_output_bias;
	dense_adam_args[11] = &hip_mem.mem.v_output_bias;
	dense_adam_args[12] = &lr;
	dense_adam_args[13] = &bias_correction_1;
	dense_adam_args[14] = &bias_correction_2;
	dense_adam_args[15] = &rectification_term;

	return hipSuccess;
}

void free_kernels(void)
{
	hipEventDestroy(hip_events.read_loss);
	hip_events.read_loss = NULL;
	for (int i = 0; i < EVENT_TRACKED_KERNELS; i++) {
		if (hip_events.start_events.arr[i]) {
			hipEventDestroy(hip_events.start_events.arr[i]);
			hip_events.start_events.arr[i] = NULL;
		}
		if (hip_events.end_events.arr[i]) {
			hipEventDestroy(hip_events.end_events.arr[i]);
			hip_events.end_events.arr[i] = NULL;
		}
	}

	if (hip_context.module) {
		hipModuleUnload(hip_context.module);
		hip_context.module = NULL;
	}
	for (int i = 0; i < KERNEL_COUNT; i++)
		hip_context.kernels.arr[i] = NULL;

	if (host_active_inputs_A)
		hipHostFree(host_active_inputs_A);
	if (host_active_inputs_B)
		hipHostFree(host_active_inputs_B);
	if (host_expected_outputs_A)
		hipHostFree(host_expected_outputs_A);
	if (host_expected_outputs_B)
		hipHostFree(host_expected_outputs_B);
	if (host_output_buckets_A)
		hipHostFree(host_output_buckets_A);
	if (host_output_buckets_B)
		hipHostFree(host_output_buckets_B);
	if (host_lossbuffer)
		hipHostFree(host_lossbuffer);

	if (host_quantized_weights)
		free(host_quantized_weights);
	if (host_training_weights)
		free(host_training_weights);
	if (host_first_moments)
		free(host_first_moments);
	if (host_second_moments)
		free(host_second_moments);

	host_active_inputs_A = NULL;
	host_active_inputs_B = NULL;
	host_expected_outputs_A = NULL;
	host_expected_outputs_B = NULL;
	host_output_buckets_A = NULL;
	host_output_buckets_B = NULL;
	host_lossbuffer = NULL;

	for (int i = 0; i < MEM_COUNT; i++) {
		if (hip_mem.mem.arr[i]) {
			hipFree(hip_mem.mem.arr[i]);
			hip_mem.mem.arr[i] = NULL;
		}
	}

	if (hip_context.queue) {
		hipStreamDestroy(hip_context.queue);
		hip_context.queue = NULL;
	}
}

float print_prof(const char *name, hipEvent_t start, hipEvent_t stop)
{
	float execute_ms = 0.0f;
	hipError_t err = hipEventElapsedTime(&execute_ms, start, stop);

	if (err == hipSuccess) {
		printf("%-30s | %0.4f ms\n", name, execute_ms);
		return execute_ms;
	} else {
		printf("%-30s | Profiling Error\n", name);
		return 0.0f;
	}
}

void wait_for_loss_read(void)
{
	hipEventSynchronize(hip_events.read_loss);
}

void enqueue_kernels(int buffer_side)
{
	//cosine annealing
	cosine_timestamp = _min(cosine_timestamp + 1, max_cosine_anneal_timestamp);
	lr = min_lr + 0.5 * (max_lr - min_lr) * (1.0 + cos(PI * (cosine_timestamp) / max_cosine_anneal_timestamp));
	lr = _min(max_lr, _max(min_lr, lr));

	hipMemcpyAsync((buffer_side == INPUT_GROUP_A) ? hip_mem.mem.active_inputs_A  : hip_mem.mem.active_inputs_B,
					(buffer_side == INPUT_GROUP_A) ? host_active_inputs_A		 : host_active_inputs_B,
					active_inputs_size, hipMemcpyHostToDevice, hip_context.queue);
	hipMemcpyAsync((buffer_side == INPUT_GROUP_A) ? hip_mem.mem.expected_output_A : hip_mem.mem.expected_output_B,
					(buffer_side == INPUT_GROUP_A) ? host_expected_outputs_A	   : host_expected_outputs_B,
					expected_output_size, hipMemcpyHostToDevice, hip_context.queue);
	hipMemcpyAsync((buffer_side == INPUT_GROUP_A) ? hip_mem.mem.output_bucket_A  : hip_mem.mem.output_bucket_B,
					(buffer_side == INPUT_GROUP_A) ? host_output_buckets_A		: host_output_buckets_B,
					output_bucket_size, hipMemcpyHostToDevice, hip_context.queue);

	hipMemsetAsync(hip_mem.mem.loss, 0, loss_size, hip_context.queue);

	calculate_accumulator_args[0] = (buffer_side == INPUT_GROUP_A) ? &hip_mem.mem.active_inputs_A : &hip_mem.mem.active_inputs_B;
	ENQUEUE_EVENT(hip_events.start_events.calc_accum, hip_context.queue);
	hipModuleLaunchKernel(hip_context.kernels.calculate_accumulator,
							MINIBATCH_SIZE * 2, 1, 1,
							32,				 1, 1,
							0, hip_context.queue, calculate_accumulator_args, NULL);

	ENQUEUE_EVENT(hip_events.end_events.calc_accum, hip_context.queue);

	calculate_output_args[4] = (buffer_side == INPUT_GROUP_A) ? &hip_mem.mem.output_bucket_A : &hip_mem.mem.output_bucket_B;
	ENQUEUE_EVENT(hip_events.start_events.calculate_output, hip_context.queue);
	hipModuleLaunchKernel(hip_context.kernels.calculate_output,
							MINIBATCH_SIZE, 1, 1,
							32,			 1, 1,
							0, hip_context.queue, calculate_output_args, NULL);
	ENQUEUE_EVENT(hip_events.end_events.calculate_output, hip_context.queue);


	calculate_delta_args[1] = (buffer_side == INPUT_GROUP_A) ? &hip_mem.mem.expected_output_A : &hip_mem.mem.expected_output_B;
	calculate_delta_args[7] = (buffer_side == INPUT_GROUP_A) ? &hip_mem.mem.output_bucket_A   : &hip_mem.mem.output_bucket_B;
	ENQUEUE_EVENT(hip_events.start_events.calculate_delta, hip_context.queue);
	hipModuleLaunchKernel(hip_context.kernels.calculate_deltas,
							MINIBATCH_SIZE,			 1, 1,
							ACCUMULATOR_NODES_PER_SIDE, 1, 1,
							0, hip_context.queue, calculate_delta_args, NULL);
	ENQUEUE_EVENT(hip_events.end_events.calculate_delta, hip_context.queue);


	hipMemcpyAsync(host_lossbuffer, hip_mem.mem.loss, loss_size, hipMemcpyDeviceToHost, hip_context.queue);
	hipEventRecord(hip_events.read_loss, hip_context.queue);

	output_gradient_args[4] = (buffer_side == INPUT_GROUP_A) ? &hip_mem.mem.output_bucket_A : &hip_mem.mem.output_bucket_B;
	ENQUEUE_EVENT(hip_events.start_events.calculate_output_gradient, hip_context.queue);
	hipModuleLaunchKernel(hip_context.kernels.calculate_output_gradient,
							OUTPUT_BUCKETS * ACCUMULATOR_NODES, 1, 1,
							32,								 1, 1,
							0, hip_context.queue, output_gradient_args, NULL);
	ENQUEUE_EVENT(hip_events.end_events.calculate_output_gradient, hip_context.queue);

	ENQUEUE_EVENT(hip_events.start_events.calculate_accumulator_bias_gradient, hip_context.queue);
	hipModuleLaunchKernel(hip_context.kernels.calculate_accumulator_bias_gradient,
							ACCUMULATOR_NODES_PER_SIDE,	 1, 1,
							32,							 1, 1,
							0, hip_context.queue, accumulator_bias_gradient_args, NULL);
	ENQUEUE_EVENT(hip_events.end_events.calculate_accumulator_bias_gradient, hip_context.queue);

	accumulator_gradient_args[0] = (buffer_side == INPUT_GROUP_A) ? &hip_mem.mem.active_inputs_A : &hip_mem.mem.active_inputs_B;
	ENQUEUE_EVENT(hip_events.start_events.calculate_accumulator_gradient, hip_context.queue);
	hipModuleLaunchKernel(hip_context.kernels.calculate_accumulator_gradient,
							MINIBATCH_SIZE, 1, 1,
							64,			1, 1,
							0, hip_context.queue, accumulator_gradient_args, NULL);
	ENQUEUE_EVENT(hip_events.end_events.calculate_accumulator_gradient, hip_context.queue);


	timestamp++;
	bias_correction_1 *= ADAM_BETA1;
	bias_correction_2 *= ADAM_BETA2;
	rho_timestamp = rho_inf - (2.0f * timestamp * bias_correction_2) / (1.0f - bias_correction_2);
	if (rho_timestamp > 4.0f)
		rectification_term = sqrtf(((rho_timestamp - 4.0f) * (rho_timestamp - 2.0f) * rho_inf) / ((rho_inf - 4.0f) * (rho_inf - 2.0f) * rho_timestamp));
	else
		rectification_term = 0.0f;

	ENQUEUE_EVENT(hip_events.start_events.dense_update, hip_context.queue);
	hipModuleLaunchKernel(hip_context.kernels.adamw,
							output_weights_count / 32, 1, 1,
							32, 1, 1,
							0, hip_context.queue, dense_adam_args, NULL);
	ENQUEUE_EVENT(hip_events.end_events.dense_update, hip_context.queue);


	ENQUEUE_EVENT(hip_events.start_events.input_update, hip_context.queue);
	hipModuleLaunchKernel(hip_context.kernels.inputadamw,
							accumulator_weights_count / 32, 1, 1,
							32, 1, 1,
							0, hip_context.queue, input_adam_args, NULL);
	ENQUEUE_EVENT(hip_events.end_events.input_update, hip_context.queue);


	if (timestamp % LOOKAHEAD_RANGE == 0) {
		LOOKAHEAD_UPDATE(hip_mem.mem.factorizer_weights_fast, hip_mem.mem.factorizer_weights_slow, factorizer_weights_count);

		LOOKAHEAD_UPDATE(hip_mem.mem.accumulator_weights_fast, hip_mem.mem.accumulator_weights_slow, accumulator_weights_count);
		LOOKAHEAD_UPDATE(hip_mem.mem.accumulator_bias_fast, hip_mem.mem.accumulator_bias_slow, accumulator_bias_count);

		LOOKAHEAD_UPDATE(hip_mem.mem.output_weights_fast, hip_mem.mem.output_weights_slow, output_weights_count);
		LOOKAHEAD_UPDATE(hip_mem.mem.output_bias_fast, hip_mem.mem.output_bias_slow, output_bias_count);
	}

	#ifdef PERFT_KERNELS
		//Profiling disallows the efficient usage of ping-pong buffers, which slows down execution.
		//It doesn't really matter since profiling only gets run for a short period.
		hipEventSynchronize(hip_events.end_events.input_update);

		//Overlap is prevented with explicit synchronization.
		//This increases overhead while perfting, so the true time is somewhere between sum and total.
		float total_time_ms = 0.0f;
		float sum_time_ms = 0.0f;

		hipEventElapsedTime(&total_time_ms, hip_events.start_events.calc_accum, hip_events.end_events.input_update);

		printf("\n--- Profiling ---\n");
		sum_time_ms += print_prof("Calculate Accumulator", hip_events.start_events.calc_accum, hip_events.end_events.calc_accum);
		sum_time_ms += print_prof("Calculate Output", hip_events.start_events.calculate_output, hip_events.end_events.calculate_output);
		sum_time_ms += print_prof("Calculate Deltas", hip_events.start_events.calculate_delta, hip_events.end_events.calculate_delta);
		sum_time_ms += print_prof("Output Gradient", hip_events.start_events.calculate_output_gradient, hip_events.end_events.calculate_output_gradient);
		sum_time_ms += print_prof("Accumulator Bias Gradient", hip_events.start_events.calculate_accumulator_bias_gradient, hip_events.end_events.calculate_accumulator_bias_gradient);
		sum_time_ms += print_prof("Accumulator Gradient", hip_events.start_events.calculate_accumulator_gradient, hip_events.end_events.calculate_accumulator_gradient);
		sum_time_ms += print_prof("Output Layer & Biases", hip_events.start_events.dense_update, hip_events.end_events.dense_update);
		sum_time_ms += print_prof("Input Layer", hip_events.start_events.input_update, hip_events.end_events.input_update);
		printf("%-30s | %0.4f ms\n", "Sum", sum_time_ms);
		printf("%-30s | %0.4f ms\n", "Total", total_time_ms);
		printf("-----------------\n");
	#endif
}

void save_gpu_weights(void)
{
	//Training Weights
	hipMemcpyDtoHAsync(host_training_weights->factorizer_weights,   hip_mem.mem.factorizer_weights_slow,	factorizer_weights_size,	hip_context.queue);
	hipMemcpyDtoHAsync(host_training_weights->accumulator_weights,  hip_mem.mem.accumulator_weights_slow,   accumulator_weights_size,   hip_context.queue);
	hipMemcpyDtoHAsync(host_training_weights->accumulator_bias,	 hip_mem.mem.accumulator_bias_slow,	  accumulator_bias_size,	  hip_context.queue);
	hipMemcpyDtoHAsync(host_training_weights->output_weights,	   hip_mem.mem.output_weights_slow,		output_weights_size,		hip_context.queue);
	hipMemcpyDtoHAsync(host_training_weights->output_bias,		  hip_mem.mem.output_bias_slow,		   output_bias_size,		   hip_context.queue);
	//First Moments
	hipMemcpyDtoHAsync(host_first_moments->factorizer_weights,   hip_mem.mem.m_factorizer_weights,	factorizer_weights_size,	hip_context.queue);
	hipMemcpyDtoHAsync(host_first_moments->accumulator_weights,  hip_mem.mem.m_accumulator_weights,   accumulator_weights_size,   hip_context.queue);
	hipMemcpyDtoHAsync(host_first_moments->accumulator_bias,	 hip_mem.mem.m_accumulator_bias,	  accumulator_bias_size,	  hip_context.queue);
	hipMemcpyDtoHAsync(host_first_moments->output_weights,	   hip_mem.mem.m_output_weights,		output_weights_size,		hip_context.queue);
	hipMemcpyDtoHAsync(host_first_moments->output_bias,		  hip_mem.mem.m_output_bias,		   output_bias_size,		   hip_context.queue);
	//Second Moments
	hipMemcpyDtoHAsync(host_second_moments->factorizer_weights,   hip_mem.mem.v_factorizer_weights,	factorizer_weights_size,	hip_context.queue);
	hipMemcpyDtoHAsync(host_second_moments->accumulator_weights,  hip_mem.mem.v_accumulator_weights,   accumulator_weights_size,   hip_context.queue);
	hipMemcpyDtoHAsync(host_second_moments->accumulator_bias,	 hip_mem.mem.v_accumulator_bias,	  accumulator_bias_size,	  hip_context.queue);
	hipMemcpyDtoHAsync(host_second_moments->output_weights,	   hip_mem.mem.v_output_weights,		output_weights_size,		hip_context.queue);
	hipMemcpyDtoHAsync(host_second_moments->output_bias,		  hip_mem.mem.v_output_bias,		   output_bias_size,		   hip_context.queue);

	hipStreamSynchronize(hip_context.queue);

	quantize_weights(host_training_weights, host_quantized_weights);
	save_quantize_weights(host_quantized_weights, 	"./import/quantized.bin");
	save_raw_weights(host_training_weights,   		"./import/raw.bin");
	save_raw_weights(host_first_moments,	  		"./import/first_moments.bin");
	save_raw_weights(host_second_moments,	 		"./import/second_moments.bin");
}
