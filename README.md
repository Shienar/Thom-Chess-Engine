UCI-compliant NNUE chess engine created in C.

### BUILDING:

Simply run 'make' in the main directory to compile the program.
- 'make clean' will remove target
- 'make all' will compile everything.
- 'make NEW=1' will compile with target '.\target\Thom_new.exe'. This is used for comparing changes within the engine.
- 'make TRAIN=1' will compile the trainer into the main program (requires HIP SDK).
- 'make TRAIN=1 KPERFT=1' will compile the trainer with kernel profiling mode enabled.
- 'make TRAIN=1 NVIDIA=1' will attempt to compile the trainer on NVIDIA GPUs (untested).
- 'make DEBUG=1' will compile with fewer compiler optimizations to allow for easier gdb debugging.
- 'make PROF=1' will compile with profiling flags for gmon.
- 'make SPSA=1' will compile with uci options for Simultaneous Perturbation Stochastic Approximation

The engine's executable can be moved away from the target folder. The project's directory gets compiled into the engine with make, and the absolute paths are used to open the weight/book files in the import folder.

### COMPATIBLITY:

The engine in its current form will only work on CPUs that support AVX2 instructions. 
The training kernels require HIP SDK to run. They were created specifically for my use and were ported from some OpenCL kernels that I made before I was able to find community support for running HIP on gfx1034. I put a nvcc compiler option into the makefile, but the program is untested on NVIDIA GPUs.

### ACKNOWLEDGEMENTS:

- The engine uses the gm2600.bin polyglot opening book from Scid vs PC.
- Ronald de Man's sygyzy tablebase is used for endgame analysis. A tablebase generator is available at https://github.com/syzygy1/tb
- Andy Grant's sygyzy probing tool Pyrrhic is used to probe the sygyzy files. It is available at https://github.com/AndyGrant/Pyrrhic
- The chess programming wiki (https://www.chessprogramming.org/) contains a lot of valuable information that was very helpful to me when I started this project.
- Dominik Klein's book "Neural Networks for Chess" helped me get started on understanding chess NNUEs. You can find it at https://github.com/asdfjkl/neural_network_chess
- Training data from https://robotmoon.com/nnue-training-data/ was used during the extensive debugging & optimization of the training kernels. Pre-release neural nets were trained solely from this data, with later versions using self-generated data.