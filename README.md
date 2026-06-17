UCI-compliant NNUE chess engine created in C.

### COMPATIBLITY:

The engine in its current form will only work on CPUs that support AVX2 instructions. 
The training kernels require HIP SDK to run. They were created specifically for my use and were ported from some OpenCL kernels that I made before I was able to find community support for running HIP on gfx1034. I put a nvcc compiler option into the makefile, but the program is untested on NVIDIA GPUs.

### ACKNOWLEDGEMENTS:

- The engine uses komodo's opening book binary file by Salvo Spitaleri, which is available at https://komodochess.com/downloads.htm
- Ronald de Man's sygyzy tablebase is used for endgame analysis. A tablebase generator is available at https://github.com/syzygy1/tb
- Andy Grant's sygyzy probing tool Pyrrhic is used to probe the sygyzy files. It is available at https://github.com/AndyGrant/Pyrrhic
- The chess programming wiki (https://www.chessprogramming.org/) contains a lot of valuable information that was very helpful to me when I started this project.
- Dominik Klein's book "Neural Networks for Chess" helped me get started on understanding chess NNUEs. You can find it at https://github.com/asdfjkl/neural_network_chess
- Training data from https://robotmoon.com/nnue-training-data/ was used during the extensive debugging & optimization of the training kernels. The first version of the engine was made using this data, and subsequent versions are going to be created with self-generated data.