#include "train/opencl/gpu_cl.h"
#include "compatibility.h"
#include "debug.h"
#include "analyze/nnue/neuralnet.h"
#include "train/train.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

opencl_context_struct opencl_context = {
	.platform = 0,
	.device = 0,
	.context = NULL,
	.queue = NULL,
	.program = NULL,
	.kernels = { { NULL } }
};
cl_kernel_args cl_args = { 0 };
opencl_events cl_events = { 0};

cl_float lr = 3e-3f;
uint64_t cosine_timestamp;
uint64_t timestamp;
float rho_inf = (2.0 / (1.0 - ADAM_BETA2)) - 1.0;
float rho_timestamp = 0.0;
cl_float rectification_term = 0.0;
cl_float bias_correction_1 = 1.0;
cl_float bias_correction_2 = 1.0;

const size_t factorizer_weights_size	= sizeof(((training_weights *)0)->factorizer_weights);
const size_t factorizer_weights_count   = sizeof(((training_weights *)0)->factorizer_weights) / sizeof(float);

const size_t accumulator_weights_size   = sizeof(((training_weights *)0)->accumulator_weights);
const size_t accumulator_weights_count  = sizeof(((training_weights *)0)->accumulator_weights) / sizeof(float);

const size_t accumulator_bias_size	  	= sizeof(((training_weights *)0)->accumulator_bias);
const size_t accumulator_bias_count	 	= sizeof(((training_weights *)0)->accumulator_bias) / sizeof(float);

const size_t output_weights_size		= sizeof(((training_weights *)0)->output_weights);
const size_t output_weights_count	   	= sizeof(((training_weights *)0)->output_weights) / sizeof(float);

const size_t output_bias_size		   	= sizeof(((training_weights *)0)->output_bias);
const size_t output_bias_count		  	= sizeof(((training_weights *)0)->output_bias) / sizeof(float);

const size_t active_inputs_size	  		= MINIBATCH_SIZE * 64 * sizeof(short);
const size_t expected_output_size		= MINIBATCH_SIZE * sizeof(float);
const size_t output_bucket_size	  		= MINIBATCH_SIZE * sizeof(char);
const size_t loss_size			  		= MINIBATCH_SIZE * sizeof(float);
const size_t accumulator_output_size 	= MINIBATCH_SIZE * ACCUMULATOR_NODES * sizeof(float);
const size_t final_output_size	   		= MINIBATCH_SIZE * sizeof(float);
const size_t output_delta_size	  		= MINIBATCH_SIZE * sizeof(float);
const size_t accum_delta_size	   		= MINIBATCH_SIZE * ACCUMULATOR_NODES * sizeof(float);

short *host_active_inputs_A	  = NULL;
short *host_active_inputs_B	  = NULL;
float *host_expected_outputs_A   = NULL;
float *host_expected_outputs_B   = NULL;
char *host_output_buckets_A	 = NULL;
char *host_output_buckets_B	 = NULL;
float *host_lossbuffer		  = NULL;

nnue_weights *host_quantized_weights 		= NULL;
training_weights *host_training_weights  	= NULL;
training_weights *host_first_moments	 	= NULL;
training_weights *host_second_moments		= NULL;

char *get_kernel_functions(const char *file_name)
{
	FILE *input = fopen(file_name, "rb");

	if (!input) {
		printf("Cannot locate gpu kernels at %s\n", file_name);
		return NULL;
	}

	fseek(input, 0, SEEK_END);
	size_t size = ftell(input);

	rewind(input);

	char *source = calloc(size + 1, sizeof(char));

	if (!source) {
		DEBUG_ERROR("Failed malloc.");
		fclose(input);
		return NULL;
	}

	size_t bytes_read = fread(source, 1, size, input);

	source[bytes_read] = '\0';

	fclose(input);

	return source;
}

int init_kernels(const char *kernel_path,
				short **h_active_A, float **h_expected_A, char **h_bucket_A,
				short **h_active_B, float **h_expected_B, char **h_bucket_B,
				float **h_lossbuffer)
{
	cosine_timestamp = 0;
	timestamp = 0;

	int err = 0;

	err = clGetPlatformIDs(1, &opencl_context.platform, NULL);
	err = clGetDeviceIDs(opencl_context.platform, CL_DEVICE_TYPE_GPU, 1, &opencl_context.device, NULL);
	opencl_context.context = clCreateContext(NULL, 1, &opencl_context.device, NULL, NULL, &err);

	#ifdef PERFT_KERNELS
		cl_queue_properties queueProperties[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};

		opencl_context.queue = clCreateCommandQueueWithProperties(opencl_context.context, opencl_context.device, queueProperties, &err);
	#else
		opencl_context.queue = clCreateCommandQueueWithProperties(opencl_context.context, opencl_context.device, NULL, &err);
	#endif

	char *kernel_source = get_kernel_functions(kernel_path);

	opencl_context.program = clCreateProgramWithSource(opencl_context.context, 1, (const char **)&kernel_source, NULL, &err);
	free(kernel_source);
	kernel_source = NULL;

	const char *options = "-cl-std=CL2.0";

	err = clBuildProgram(opencl_context.program, 1, &opencl_context.device, options, NULL, NULL);

	if (err) {
		char log[4096];

		clGetProgramBuildInfo(opencl_context.program, opencl_context.device, CL_PROGRAM_BUILD_LOG, 4096, log, NULL);
		printf("Build Error:\n%s\n", log);
		return err;
	}

	opencl_context.kernels.calculate_accumulator				= clCreateKernel(opencl_context.program, "calculate_accumulator", &err);
	opencl_context.kernels.calculate_output					  	= clCreateKernel(opencl_context.program, "calculate_output", &err);
	opencl_context.kernels.calculate_deltas					 	= clCreateKernel(opencl_context.program, "calculate_deltas", &err);
	opencl_context.kernels.calculate_output_gradient			= clCreateKernel(opencl_context.program, "calculate_output_gradient", &err);
	opencl_context.kernels.calculate_accumulator_bias_gradient  = clCreateKernel(opencl_context.program, "calculate_accumulator_bias_gradient", &err);
	opencl_context.kernels.calculate_accumulator_gradient	   	= clCreateKernel(opencl_context.program, "calculate_accumulator_gradient", &err);
	opencl_context.kernels.adamw								= clCreateKernel(opencl_context.program, "adam_w", &err);
	opencl_context.kernels.inputadamw						   	= clCreateKernel(opencl_context.program, "input_adam_w", &err);
	opencl_context.kernels.lookahead							= clCreateKernel(opencl_context.program, "lookahead_update", &err);

	/** ALLOCATING **/
	//Existing weights/moments from file.
	//Their copying is infrequent so I'm just doing malloc.
	alignedAlloc(host_quantized_weights, 4096, sizeof(nnue_weights));
	alignedAlloc(host_training_weights, 4096, sizeof(training_weights));
	alignedAlloc(host_first_moments, 4096, sizeof(training_weights));
	alignedAlloc(host_second_moments, 4096, sizeof(training_weights));
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
	//Host pinning (Staging buffers)
	cl_args.mem.host_active_inputs_A	 = clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, active_inputs_size,   NULL, NULL);
	cl_args.mem.host_active_inputs_B	 = clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, active_inputs_size,   NULL, NULL);
	cl_args.mem.host_expected_output_A   = clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, expected_output_size, NULL, NULL);
	cl_args.mem.host_expected_output_B   = clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, expected_output_size, NULL, NULL);
	cl_args.mem.host_output_bucket_A	 = clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, output_bucket_size,   NULL, NULL);
	cl_args.mem.host_output_bucket_B	 = clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, output_bucket_size,   NULL, NULL);
	//Save & export pointers
	*h_active_A	 	= host_active_inputs_A		= (short *) clEnqueueMapBuffer(opencl_context.queue, cl_args.mem.host_active_inputs_A,   CL_TRUE, CL_MAP_WRITE | CL_MAP_READ, 0, active_inputs_size,   0, NULL, NULL, &err);
	*h_active_B	 	= host_active_inputs_B		= (short *) clEnqueueMapBuffer(opencl_context.queue, cl_args.mem.host_active_inputs_B,   CL_TRUE, CL_MAP_WRITE | CL_MAP_READ, 0, active_inputs_size,   0, NULL, NULL, &err);
	*h_expected_A   = host_expected_outputs_A 	= (float *) clEnqueueMapBuffer(opencl_context.queue, cl_args.mem.host_expected_output_A, CL_TRUE, CL_MAP_WRITE | CL_MAP_READ, 0, expected_output_size, 0, NULL, NULL, &err);
	*h_expected_B   = host_expected_outputs_B 	= (float *) clEnqueueMapBuffer(opencl_context.queue, cl_args.mem.host_expected_output_B, CL_TRUE, CL_MAP_WRITE | CL_MAP_READ, 0, expected_output_size, 0, NULL, NULL, &err);
	*h_bucket_A	 	= host_output_buckets_A   	= (char *)  clEnqueueMapBuffer(opencl_context.queue, cl_args.mem.host_output_bucket_A,   CL_TRUE, CL_MAP_WRITE | CL_MAP_READ, 0, output_bucket_size,   0, NULL, NULL, &err);
	*h_bucket_B	 	= host_output_buckets_B   	= (char *)  clEnqueueMapBuffer(opencl_context.queue, cl_args.mem.host_output_bucket_B,   CL_TRUE, CL_MAP_WRITE | CL_MAP_READ, 0, output_bucket_size,   0, NULL, NULL, &err);
	alignedAlloc(host_lossbuffer, 4096, loss_size);
	*h_lossbuffer   = host_lossbuffer;
	//gpu buffers (copied into from host-pinned memory)
	cl_args.mem.active_inputs_A	  	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, active_inputs_size,	 NULL, NULL);
	cl_args.mem.active_inputs_B	  	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, active_inputs_size,	 NULL, NULL);
	cl_args.mem.expected_output_A	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, expected_output_size,   NULL, NULL);
	cl_args.mem.expected_output_B	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, expected_output_size,   NULL, NULL);
	cl_args.mem.output_bucket_A	  	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, output_bucket_size,	 NULL, NULL);
	cl_args.mem.output_bucket_B	  	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, output_bucket_size,	 NULL, NULL);
	cl_args.mem.loss				= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, loss_size,			 	NULL, NULL);
	//Fast weights
	cl_args.mem.factorizer_weights_fast	 	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, factorizer_weights_size,   	(void *)host_training_weights->factorizer_weights, 	NULL);
	cl_args.mem.accumulator_weights_fast	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, accumulator_weights_size,  	(void *)host_training_weights->accumulator_weights, NULL);
	cl_args.mem.accumulator_bias_fast	   	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, accumulator_bias_size,	 	(void *)host_training_weights->accumulator_bias,	NULL);
	cl_args.mem.output_weights_fast		 	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, output_weights_size,	   	(void *)host_training_weights->output_weights, 		NULL);
	cl_args.mem.output_bias_fast			= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, output_bias_size,		  	(void *)host_training_weights->output_bias, 		NULL);
	//Slow Weights
	cl_args.mem.factorizer_weights_slow	 	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, factorizer_weights_size,   	(void *)host_training_weights->factorizer_weights,	NULL);
	cl_args.mem.accumulator_weights_slow	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, accumulator_weights_size,  	(void *)host_training_weights->accumulator_weights,	NULL);
	cl_args.mem.accumulator_bias_slow	   	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, accumulator_bias_size,	 	(void *)host_training_weights->accumulator_bias, 	NULL);
	cl_args.mem.output_weights_slow		 	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, output_weights_size,	   	(void *)host_training_weights->output_weights,		NULL);
	cl_args.mem.output_bias_slow			= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, output_bias_size,		  	(void *)host_training_weights->output_bias, 		NULL);
	//Intermediate Outputs
	cl_args.mem.accumulator_output   = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, accumulator_output_size, NULL, NULL);
	cl_args.mem.final_output		 = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, final_output_size, NULL, NULL);
	//Deltas
	cl_args.mem.output_delta		  = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, output_delta_size, NULL, NULL);
	cl_args.mem.accumulator_delta	  = clCreateBuffer(opencl_context.context, CL_MEM_READ_WRITE, accum_delta_size, NULL, NULL);
	//Gradient Sums
	cl_args.mem.factorizer_weights_gradient_sum	 	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, factorizer_weights_size,   NULL, NULL);
	cl_args.mem.accumulator_weights_gradient_sum	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, accumulator_weights_size,  NULL, NULL);
	cl_args.mem.accumulator_bias_gradient_sum	   	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, accumulator_bias_size,	 	NULL, NULL);
	cl_args.mem.output_weights_gradient_sum		 	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, output_weights_size,	   	NULL, NULL);
	cl_args.mem.output_bias_gradient_sum			= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, output_bias_size,		  	NULL, NULL);
	//Adam First moments
	cl_args.mem.m_factorizer_weights	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, factorizer_weights_size,   NULL, NULL);
	cl_args.mem.m_accumulator_weights   = clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, accumulator_weights_size,  NULL, NULL);
	cl_args.mem.m_accumulator_bias	  	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, accumulator_bias_size,	 	NULL, NULL);
	cl_args.mem.m_output_weights		= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, output_weights_size,	   	NULL, NULL);
	cl_args.mem.m_output_bias		   	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, output_bias_size,		  	NULL, NULL);
	//Adam Second moments
	cl_args.mem.v_factorizer_weights	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, factorizer_weights_size,   NULL, NULL);
	cl_args.mem.v_accumulator_weights   = clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, accumulator_weights_size,  NULL, NULL);
	cl_args.mem.v_accumulator_bias	  	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, accumulator_bias_size,	 	NULL, NULL);
	cl_args.mem.v_output_weights		= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, output_weights_size,	   	NULL, NULL);
	cl_args.mem.v_output_bias		   	= clCreateBuffer(opencl_context.context,  CL_MEM_READ_WRITE, output_bias_size,		  	NULL, NULL);

	/** ZEROING **/
	float zero = 0.0f;
	//Gradients
	clEnqueueFillBuffer(opencl_context.queue, cl_args.mem.factorizer_weights_gradient_sum,  &zero, sizeof(float), 0, factorizer_weights_size,   0, NULL, NULL);
	clEnqueueFillBuffer(opencl_context.queue, cl_args.mem.accumulator_weights_gradient_sum, &zero, sizeof(float), 0, accumulator_weights_size,  0, NULL, NULL);
	clEnqueueFillBuffer(opencl_context.queue, cl_args.mem.accumulator_bias_gradient_sum,	&zero, sizeof(float), 0, accumulator_bias_size,	 	0, NULL, NULL);
	clEnqueueFillBuffer(opencl_context.queue, cl_args.mem.output_weights_gradient_sum,	  	&zero, sizeof(float), 0, output_weights_size,	   	0, NULL, NULL);
	clEnqueueFillBuffer(opencl_context.queue, cl_args.mem.output_bias_gradient_sum,		 	&zero, sizeof(float), 0, output_bias_size,		  	0, NULL, NULL);
	//First moments
	clEnqueueFillBuffer(opencl_context.queue, cl_args.mem.m_factorizer_weights,  	&zero, sizeof(float), 0, factorizer_weights_size,   0, NULL, NULL);
	clEnqueueFillBuffer(opencl_context.queue, cl_args.mem.m_accumulator_weights, 	&zero, sizeof(float), 0, accumulator_weights_size,  0, NULL, NULL);
	clEnqueueFillBuffer(opencl_context.queue, cl_args.mem.m_accumulator_bias,		&zero, sizeof(float), 0, accumulator_bias_size,	 	0, NULL, NULL);
	clEnqueueFillBuffer(opencl_context.queue, cl_args.mem.m_output_weights,	  		&zero, sizeof(float), 0, output_weights_size,	   	0, NULL, NULL);
	clEnqueueFillBuffer(opencl_context.queue, cl_args.mem.m_output_bias,		 	&zero, sizeof(float), 0, output_bias_size,		  	0, NULL, NULL);
	//Second moments
	clEnqueueFillBuffer(opencl_context.queue, cl_args.mem.v_factorizer_weights,  	&zero, sizeof(float), 0, factorizer_weights_size,   0, NULL, NULL);
	clEnqueueFillBuffer(opencl_context.queue, cl_args.mem.v_accumulator_weights, 	&zero, sizeof(float), 0, accumulator_weights_size,  0, NULL, NULL);
	clEnqueueFillBuffer(opencl_context.queue, cl_args.mem.v_accumulator_bias,		&zero, sizeof(float), 0, accumulator_bias_size,	 	0, NULL, NULL);
	clEnqueueFillBuffer(opencl_context.queue, cl_args.mem.v_output_weights,	  		&zero, sizeof(float), 0, output_weights_size,	   	0, NULL, NULL);
	clEnqueueFillBuffer(opencl_context.queue, cl_args.mem.v_output_bias,			&zero, sizeof(float), 0, output_bias_size,		  	0, NULL, NULL);

	clFinish(opencl_context.queue);

	/** Set static arguments */

	//clSetKernelArg(opencl_context.kernels.calculate_accumulator, 0, sizeof(cl_mem), active input);
	clSetKernelArg(opencl_context.kernels.calculate_accumulator, 1, sizeof(cl_mem), &cl_args.mem.accumulator_weights_fast);
	clSetKernelArg(opencl_context.kernels.calculate_accumulator, 2, sizeof(cl_mem), &cl_args.mem.factorizer_weights_fast);
	clSetKernelArg(opencl_context.kernels.calculate_accumulator, 3, sizeof(cl_mem), &cl_args.mem.accumulator_bias_fast);
	clSetKernelArg(opencl_context.kernels.calculate_accumulator, 4, sizeof(cl_mem), &cl_args.mem.accumulator_output);

	clSetKernelArg(opencl_context.kernels.calculate_output, 0, sizeof(cl_mem), &cl_args.mem.accumulator_output);
	clSetKernelArg(opencl_context.kernels.calculate_output, 1, sizeof(cl_mem), &cl_args.mem.output_weights_fast);
	clSetKernelArg(opencl_context.kernels.calculate_output, 2, sizeof(cl_mem), &cl_args.mem.output_bias_fast);
	clSetKernelArg(opencl_context.kernels.calculate_output, 3, sizeof(cl_mem), &cl_args.mem.final_output);
	// clSetKernelArg(opencl_context.kernels.calculate_output, 4, sizeof(cl_mem), output bucket);

	clSetKernelArg(opencl_context.kernels.calculate_deltas, 0, sizeof(cl_mem), &cl_args.mem.final_output);
	//clSetKernelArg(opencl_context.kernels.calculate_deltas, 1, sizeof(cl_mem), expected output);
	clSetKernelArg(opencl_context.kernels.calculate_deltas, 2, sizeof(cl_mem), &cl_args.mem.accumulator_output);
	clSetKernelArg(opencl_context.kernels.calculate_deltas, 3, sizeof(cl_mem), &cl_args.mem.output_weights_fast);
	clSetKernelArg(opencl_context.kernels.calculate_deltas, 4, sizeof(cl_mem), &cl_args.mem.output_delta);
	clSetKernelArg(opencl_context.kernels.calculate_deltas, 5, sizeof(cl_mem), &cl_args.mem.accumulator_delta);
	clSetKernelArg(opencl_context.kernels.calculate_deltas, 6, sizeof(cl_mem), &cl_args.mem.loss);
	//clSetKernelArg(opencl_context.kernels.calculate_deltas, 7, sizeof(cl_mem), output buckets);

	clSetKernelArg(opencl_context.kernels.calculate_output_gradient, 0, sizeof(cl_mem), &cl_args.mem.output_delta);
	clSetKernelArg(opencl_context.kernels.calculate_output_gradient, 1, sizeof(cl_mem), &cl_args.mem.accumulator_output);
	clSetKernelArg(opencl_context.kernels.calculate_output_gradient, 2, sizeof(cl_mem), &cl_args.mem.output_weights_gradient_sum);
	clSetKernelArg(opencl_context.kernels.calculate_output_gradient, 3, sizeof(cl_mem), &cl_args.mem.output_bias_gradient_sum);
	//clSetKernelArg(opencl_context.kernels.calculate_output_gradient, 4, sizeof(cl_mem), output bucket);

	//clSetKernelArg(opencl_context.kernels.calculate_accumulator_gradient, 0, sizeof(cl_mem), active inputs);
	clSetKernelArg(opencl_context.kernels.calculate_accumulator_gradient, 1, sizeof(cl_mem), &cl_args.mem.accumulator_delta);
	clSetKernelArg(opencl_context.kernels.calculate_accumulator_gradient, 2, sizeof(cl_mem), &cl_args.mem.factorizer_weights_gradient_sum);
	clSetKernelArg(opencl_context.kernels.calculate_accumulator_gradient, 3, sizeof(cl_mem), &cl_args.mem.accumulator_weights_gradient_sum);

	clSetKernelArg(opencl_context.kernels.calculate_accumulator_bias_gradient, 0, sizeof(cl_mem), &cl_args.mem.accumulator_delta);
	clSetKernelArg(opencl_context.kernels.calculate_accumulator_bias_gradient, 1, sizeof(cl_mem), &cl_args.mem.accumulator_bias_gradient_sum);

	clSetKernelArg(opencl_context.kernels.inputadamw, 0, sizeof(cl_mem), &cl_args.mem.accumulator_weights_fast);
	clSetKernelArg(opencl_context.kernels.inputadamw, 1, sizeof(cl_mem), &cl_args.mem.accumulator_weights_gradient_sum);
	clSetKernelArg(opencl_context.kernels.inputadamw, 2, sizeof(cl_mem), &cl_args.mem.m_accumulator_weights);
	clSetKernelArg(opencl_context.kernels.inputadamw, 3, sizeof(cl_mem), &cl_args.mem.v_accumulator_weights);
	clSetKernelArg(opencl_context.kernels.inputadamw, 4, sizeof(cl_mem), &cl_args.mem.factorizer_weights_fast);
	clSetKernelArg(opencl_context.kernels.inputadamw, 5, sizeof(cl_mem), &cl_args.mem.factorizer_weights_gradient_sum);
	clSetKernelArg(opencl_context.kernels.inputadamw, 6, sizeof(cl_mem), &cl_args.mem.m_factorizer_weights);
	clSetKernelArg(opencl_context.kernels.inputadamw, 7, sizeof(cl_mem), &cl_args.mem.v_factorizer_weights);
	clSetKernelArg(opencl_context.kernels.inputadamw, 8, sizeof(float), &lr);
	clSetKernelArg(opencl_context.kernels.inputadamw, 9, sizeof(float), &bias_correction_1);
	clSetKernelArg(opencl_context.kernels.inputadamw, 10, sizeof(float), &bias_correction_2);
	clSetKernelArg(opencl_context.kernels.inputadamw, 11, sizeof(float), &rectification_term);

	clSetKernelArg(opencl_context.kernels.adamw, 0, sizeof(cl_mem), &cl_args.mem.output_weights_fast);
	clSetKernelArg(opencl_context.kernels.adamw, 1, sizeof(cl_mem), &cl_args.mem.output_weights_gradient_sum);
	clSetKernelArg(opencl_context.kernels.adamw, 2, sizeof(cl_mem), &cl_args.mem.m_output_weights);
	clSetKernelArg(opencl_context.kernels.adamw, 3, sizeof(cl_mem), &cl_args.mem.v_output_weights);
	clSetKernelArg(opencl_context.kernels.adamw, 4, sizeof(cl_mem), &cl_args.mem.accumulator_bias_fast);
	clSetKernelArg(opencl_context.kernels.adamw, 5, sizeof(cl_mem), &cl_args.mem.accumulator_bias_gradient_sum);
	clSetKernelArg(opencl_context.kernels.adamw, 6, sizeof(cl_mem), &cl_args.mem.m_accumulator_bias);
	clSetKernelArg(opencl_context.kernels.adamw, 7, sizeof(cl_mem), &cl_args.mem.v_accumulator_bias);
	clSetKernelArg(opencl_context.kernels.adamw, 8, sizeof(cl_mem), &cl_args.mem.output_bias_fast);
	clSetKernelArg(opencl_context.kernels.adamw, 9, sizeof(cl_mem), &cl_args.mem.output_bias_gradient_sum);
	clSetKernelArg(opencl_context.kernels.adamw, 10, sizeof(cl_mem), &cl_args.mem.m_output_bias);
	clSetKernelArg(opencl_context.kernels.adamw, 11, sizeof(cl_mem), &cl_args.mem.v_output_bias);
	clSetKernelArg(opencl_context.kernels.adamw, 12, sizeof(float), &lr);
	clSetKernelArg(opencl_context.kernels.adamw, 13, sizeof(float), &bias_correction_1);
	clSetKernelArg(opencl_context.kernels.adamw, 14, sizeof(float), &bias_correction_2);
	clSetKernelArg(opencl_context.kernels.adamw, 15, sizeof(float), &rectification_term);

	return 0;
}

void free_kernels(void)
{

	clEnqueueUnmapMemObject(opencl_context.queue, cl_args.mem.host_active_inputs_A, host_active_inputs_A, 0, NULL, NULL);
	clEnqueueUnmapMemObject(opencl_context.queue, cl_args.mem.host_active_inputs_B, host_active_inputs_B, 0, NULL, NULL);
	clEnqueueUnmapMemObject(opencl_context.queue, cl_args.mem.host_expected_output_A, host_expected_outputs_A, 0, NULL, NULL);
	clEnqueueUnmapMemObject(opencl_context.queue, cl_args.mem.host_expected_output_B, host_expected_outputs_B, 0, NULL, NULL);
	clEnqueueUnmapMemObject(opencl_context.queue, cl_args.mem.host_output_bucket_A, host_output_buckets_A, 0, NULL, NULL);
	clEnqueueUnmapMemObject(opencl_context.queue, cl_args.mem.host_output_bucket_B, host_output_buckets_B, 0, NULL, NULL);
	clFinish(opencl_context.queue);

	for (int i = 0; i < KERNEL_COUNT; i++)
		clReleaseKernel(opencl_context.kernels.arr[i]);

	clReleaseProgram(opencl_context.program);
	clReleaseCommandQueue(opencl_context.queue);

	if (host_quantized_weights)
		alignedFree(host_quantized_weights);
	if (host_training_weights)
		alignedFree(host_training_weights);
	if (host_first_moments)
		alignedFree(host_first_moments);
	if (host_second_moments)
		alignedFree(host_second_moments);
	if (host_lossbuffer)
		alignedFree(host_lossbuffer);

	for (int i = 0; i < MEM_COUNT; i++) {
		if (cl_args.mem.arr[i]) {
			clReleaseMemObject(cl_args.mem.arr[i]);
			cl_args.mem.arr[i] = NULL;
		}
	}

	clReleaseContext(opencl_context.context);
}

double print_prof(const char *name, cl_event e)
{
	cl_ulong start, end;

	clGetEventProfilingInfo(e, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
	clGetEventProfilingInfo(e, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);

	double execute_ms = (end - start) / 1000000.0;

	printf("%-30s | %0.4f ms\n", name, execute_ms);
	clReleaseEvent(e);
	return execute_ms;
}

size_t calc_accumulator_global_size[3]  = { 32 * MINIBATCH_SIZE * 2, 1, 1};
size_t calc_accumulator_local_size[3]   = { 32,					  1, 1};

size_t calc_output_global_size[3]  = { 32 * MINIBATCH_SIZE, 1, 1};
size_t calc_output_local_size[3]   = { 32,				  1, 1};

size_t calc_deltas_global_size[3]  = { ACCUMULATOR_NODES_PER_SIDE * MINIBATCH_SIZE,   1, 1};
size_t calc_deltas_localsize[3]   = { ACCUMULATOR_NODES_PER_SIDE,					1, 1};

size_t calc_output_gradient_global_size[3]  = { 32 * OUTPUT_BUCKETS * ACCUMULATOR_NODES, 1, 1};
size_t calc_output_gradient_localsize[3]   = { 32,									  1, 1};

size_t calc_accumulator_bias_gradient_global_size[3]  = { 32 * ACCUMULATOR_NODES_PER_SIDE, 1, 1};
size_t calc_accumulator_bias_gradient_localsize[3]   = { 32,							  1, 1};

size_t calc_accumulator_gradient_global_size[3]  = { 64 * MINIBATCH_SIZE, 1, 1};
size_t calc_accumulator_gradient_local_size[3]   = { 64,				  1, 1};

//weights are guaranteed divisible by 32
size_t adamw_global_size[3]  = { output_weights_count, 1, 1};
size_t adamw_localsize[3]   = { 32,						1, 1};

size_t input_adamw_global_size[3]  = { accumulator_weights_count, 1, 1};
size_t input_adamw_localsize[3] = { 32,							 1, 1};

void wait_for_loss_read(void)
{
	clWaitForEvents(1, &cl_events.read_loss);
	clReleaseEvent(cl_events.read_loss);
}

void enqueue_kernels(int buffer_side)
{
	//cosine annealing
	cosine_timestamp = _min(cosine_timestamp + 1, max_cosine_anneal_timestamp);
	lr = min_lr + 0.5 * (max_lr - min_lr) * (1.0 + cos(PI * (cosine_timestamp) / max_cosine_anneal_timestamp));
	lr = _min(max_lr, _max(min_lr, lr));

	clEnqueueWriteBuffer(opencl_context.queue, (buffer_side == INPUT_GROUP_A) ? cl_args.mem.active_inputs_A : cl_args.mem.active_inputs_B, CL_FALSE, 0, active_inputs_size,
							(buffer_side == INPUT_GROUP_A) ? host_active_inputs_A : host_active_inputs_B, 0, NULL, NULL);
	clEnqueueWriteBuffer(opencl_context.queue, (buffer_side == INPUT_GROUP_A) ? cl_args.mem.expected_output_A : cl_args.mem.expected_output_B, CL_FALSE, 0, expected_output_size,
							(buffer_side == INPUT_GROUP_A) ? host_expected_outputs_A : host_expected_outputs_B, 0, NULL, NULL);
	clEnqueueWriteBuffer(opencl_context.queue, (buffer_side == INPUT_GROUP_A) ? cl_args.mem.output_bucket_A : cl_args.mem.output_bucket_B, CL_FALSE, 0, output_bucket_size,
							(buffer_side == INPUT_GROUP_A) ? host_output_buckets_A : host_output_buckets_B, 0, NULL, NULL);

	float zero = 0.0f;

	clEnqueueFillBuffer(opencl_context.queue, cl_args.mem.loss, &zero, sizeof(float), 0, loss_size, 0, NULL, NULL);

	clSetKernelArg(opencl_context.kernels.calculate_accumulator, 0, sizeof(cl_mem), (buffer_side == INPUT_GROUP_A) ? &cl_args.mem.active_inputs_A : &cl_args.mem.active_inputs_B);
	clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.kernels.calculate_accumulator, 3, NULL, calc_accumulator_global_size, calc_accumulator_local_size, 0, NULL, ENQUEUE_EVENT(cl_events.calc_accum));

	clSetKernelArg(opencl_context.kernels.calculate_output, 4, sizeof(cl_mem), (buffer_side == INPUT_GROUP_A) ? &cl_args.mem.output_bucket_A : &cl_args.mem.output_bucket_B);
	clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.kernels.calculate_output, 3, NULL, calc_output_global_size, calc_output_local_size, 0, NULL, ENQUEUE_EVENT(cl_events.calculate_output));

	clSetKernelArg(opencl_context.kernels.calculate_deltas, 1, sizeof(cl_mem), (buffer_side == INPUT_GROUP_A) ? &cl_args.mem.expected_output_A : &cl_args.mem.expected_output_B);
	clSetKernelArg(opencl_context.kernels.calculate_deltas, 7, sizeof(cl_mem), (buffer_side == INPUT_GROUP_A) ? &cl_args.mem.output_bucket_A : &cl_args.mem.output_bucket_B);
	clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.kernels.calculate_deltas, 3, NULL, calc_deltas_global_size, calc_deltas_localsize, 0, NULL, ENQUEUE_EVENT(cl_events.calculate_delta));

	clEnqueueReadBuffer(opencl_context.queue, cl_args.mem.loss, CL_FALSE, 0, loss_size, (void *) host_lossbuffer, 0, NULL, &cl_events.read_loss);

	clSetKernelArg(opencl_context.kernels.calculate_output_gradient, 4, sizeof(cl_mem), (buffer_side == INPUT_GROUP_A) ? &cl_args.mem.output_bucket_A : &cl_args.mem.output_bucket_B);
	clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.kernels.calculate_output_gradient, 3, NULL, calc_output_gradient_global_size, calc_output_gradient_localsize, 0, NULL, ENQUEUE_EVENT(cl_events.calculate_output_gradient));

	clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.kernels.calculate_accumulator_bias_gradient, 3, NULL, calc_accumulator_bias_gradient_global_size, calc_accumulator_bias_gradient_localsize, 0, NULL, ENQUEUE_EVENT(cl_events.calculate_accumulator_bias_gradient));

	clSetKernelArg(opencl_context.kernels.calculate_accumulator_gradient, 0, sizeof(cl_mem), (buffer_side == INPUT_GROUP_A) ? &cl_args.mem.active_inputs_A : &cl_args.mem.active_inputs_B);
	clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.kernels.calculate_accumulator_gradient, 3, NULL, calc_accumulator_gradient_global_size, calc_accumulator_gradient_local_size, 0, NULL, ENQUEUE_EVENT(cl_events.calculate_accumulator_gradient));

	timestamp++;
	bias_correction_1 *= ADAM_BETA1;
	bias_correction_2 *= ADAM_BETA2;
	rho_timestamp = rho_inf - (2.0f * timestamp * bias_correction_2) / (1.0f - bias_correction_2);
	if (rho_timestamp > 4.0f)
		rectification_term = sqrtf(((rho_timestamp - 4.0f) * (rho_timestamp - 2.0f) * rho_inf) / ((rho_inf - 4.0f) * (rho_inf - 2.0f) * rho_timestamp));
	else
		rectification_term = 0.0f;

	clSetKernelArg(opencl_context.kernels.adamw, 12, sizeof(cl_float), &lr);
	clSetKernelArg(opencl_context.kernels.adamw, 13, sizeof(cl_float), &bias_correction_1);
	clSetKernelArg(opencl_context.kernels.adamw, 14, sizeof(cl_float), &bias_correction_2);
	clSetKernelArg(opencl_context.kernels.adamw, 15, sizeof(cl_float), &rectification_term);
	clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.kernels.adamw,	  3, NULL, adamw_global_size,	  adamw_localsize,	   0, NULL, ENQUEUE_EVENT(cl_events.dense_update));

	clSetKernelArg(opencl_context.kernels.inputadamw, 8,  sizeof(cl_float), &lr);
	clSetKernelArg(opencl_context.kernels.inputadamw, 9,  sizeof(cl_float), &bias_correction_1);
	clSetKernelArg(opencl_context.kernels.inputadamw, 10, sizeof(cl_float), &bias_correction_2);
	clSetKernelArg(opencl_context.kernels.inputadamw, 11, sizeof(cl_float), &rectification_term);
	clEnqueueNDRangeKernel(opencl_context.queue, opencl_context.kernels.inputadamw, 3, NULL, input_adamw_global_size, input_adamw_localsize,  0, NULL, ENQUEUE_EVENT(cl_events.input_update));

	if (timestamp % LOOKAHEAD_RANGE == 0) {
		LOOKAHEAD_UPDATE(cl_args.mem.factorizer_weights_fast, cl_args.mem.factorizer_weights_slow, factorizer_weights_count);

		LOOKAHEAD_UPDATE(cl_args.mem.accumulator_weights_fast, cl_args.mem.accumulator_weights_slow, accumulator_weights_count);
		LOOKAHEAD_UPDATE(cl_args.mem.accumulator_bias_fast, cl_args.mem.accumulator_bias_slow, accumulator_bias_count);

		LOOKAHEAD_UPDATE(cl_args.mem.output_weights_fast, cl_args.mem.output_weights_slow, output_weights_count);
		LOOKAHEAD_UPDATE(cl_args.mem.output_bias_fast, cl_args.mem.output_bias_slow, output_bias_count);
	}

	#ifdef PERFT_KERNELS
		//Profiling disallows the efficient usage of ping-pong buffers, which slows down execution.
		//It doesn't really matter since profiling only gets run for a short period.
		clFinish(opencl_context.queue);

		//Overlap is prevented with explicit synchronization.
		//This increases overhead while perfting, so the true time is somewhere between sum and total.
		cl_ulong start, end;

		clGetEventProfilingInfo(cl_events.calc_accum, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
		clGetEventProfilingInfo(cl_events.input_update, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
		float sum_time_ms = 0.0f;
		float total_time_ms = (end - start) / 1000000.0;

		printf("\n--- Profiling ---\n");
		sum_time_ms += print_prof("Calculate Accumulator", cl_events.calc_accum);
		sum_time_ms += print_prof("Calculate Output", cl_events.calculate_output);
		sum_time_ms += print_prof("Calculate Deltas", cl_events.calculate_delta);
		sum_time_ms += print_prof("Output Gradient", cl_events.calculate_output_gradient);
		sum_time_ms += print_prof("Accumulator Bias Gradient", cl_events.calculate_accumulator_bias_gradient);
		sum_time_ms += print_prof("Accumulator Gradient", cl_events.calculate_accumulator_gradient);
		sum_time_ms += print_prof("Output Layer & Biases", cl_events.dense_update);
		sum_time_ms += print_prof("Input Layer", cl_events.input_update);
		printf("%-30s | %0.4f ms\n", "Sum", sum_time_ms);
		printf("%-30s | %0.4f ms\n", "Total", total_time_ms);
		printf("-----------------\n");
	#endif
}

void save_gpu_weights(void)
{
	clFinish(opencl_context.queue);

	//Training Weights
	clEnqueueReadBuffer(opencl_context.queue, cl_args.mem.factorizer_weights_slow,  CL_FALSE, 0, factorizer_weights_size,   host_training_weights->factorizer_weights,  0, NULL, NULL);
	clEnqueueReadBuffer(opencl_context.queue, cl_args.mem.accumulator_weights_slow, CL_FALSE, 0, accumulator_weights_size,  host_training_weights->accumulator_weights, 0, NULL, NULL);
	clEnqueueReadBuffer(opencl_context.queue, cl_args.mem.accumulator_bias_slow,	CL_FALSE, 0, accumulator_bias_size,	 	host_training_weights->accumulator_bias,	0, NULL, NULL);
	clEnqueueReadBuffer(opencl_context.queue, cl_args.mem.output_weights_slow,	 	CL_FALSE, 0, output_weights_size,	   	host_training_weights->output_weights,	   	0, NULL, NULL);
	clEnqueueReadBuffer(opencl_context.queue, cl_args.mem.output_bias_slow,		 	CL_FALSE, 0, output_bias_size,		  	host_training_weights->output_bias,		  	0, NULL, NULL);

	//First Moments
	clEnqueueReadBuffer(opencl_context.queue, cl_args.mem.m_factorizer_weights,   	CL_FALSE, 0, factorizer_weights_size,	host_first_moments->factorizer_weights,   0, NULL, NULL);
	clEnqueueReadBuffer(opencl_context.queue, cl_args.mem.m_accumulator_weights,  	CL_FALSE, 0, accumulator_weights_size,	host_first_moments->accumulator_weights,  0, NULL, NULL);
	clEnqueueReadBuffer(opencl_context.queue, cl_args.mem.m_accumulator_bias,	 	CL_FALSE, 0, accumulator_bias_size,	    host_first_moments->accumulator_bias,	 0, NULL, NULL);
	clEnqueueReadBuffer(opencl_context.queue, cl_args.mem.m_output_weights,	   		CL_FALSE, 0, output_weights_size,		host_first_moments->output_weights,	   0, NULL, NULL);
	clEnqueueReadBuffer(opencl_context.queue, cl_args.mem.m_output_bias,		  	CL_FALSE, 0, output_bias_size,			host_first_moments->output_bias,		  0, NULL, NULL);

	//Second Moments
	clEnqueueReadBuffer(opencl_context.queue, cl_args.mem.v_factorizer_weights,   	CL_FALSE, 0, factorizer_weights_size,	host_second_moments->factorizer_weights,   	0, NULL, NULL);
	clEnqueueReadBuffer(opencl_context.queue, cl_args.mem.v_accumulator_weights,  	CL_FALSE, 0, accumulator_weights_size,	host_second_moments->accumulator_weights,  	0, NULL, NULL);
	clEnqueueReadBuffer(opencl_context.queue, cl_args.mem.v_accumulator_bias,	 	CL_FALSE, 0, accumulator_bias_size,	    host_second_moments->accumulator_bias,	 	0, NULL, NULL);
	clEnqueueReadBuffer(opencl_context.queue, cl_args.mem.v_output_weights,	   		CL_FALSE, 0, output_weights_size,		host_second_moments->output_weights,	   	0, NULL, NULL);
	clEnqueueReadBuffer(opencl_context.queue, cl_args.mem.v_output_bias,		  	CL_TRUE,  0, output_bias_size,			host_second_moments->output_bias,		  	0, NULL, NULL);

	clFinish(opencl_context.queue);

	quantize_weights(host_training_weights, host_quantized_weights);
	save_quantize_weights(host_quantized_weights, "./import/quantized.bin");
	save_raw_weights(host_training_weights,   "./import/raw.bin");
	save_raw_weights(host_first_moments,	  "./import/first_moments.bin");
	save_raw_weights(host_second_moments,	 "./import/second_moments.bin");
}
