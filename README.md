UCI-compliant NNUE chess engine created in C.

### BUILDING:

A release version of the engine should be compiled with 'make -j RELEASE=1'. It can exist outside of the compiled folder and won't attempt any file opening/read/writing for debug logs, opening books, or network weights.
- 'make clean' will remove target
- 'make -j all' will compile everything.
- 'make -j NEW=1' will compile with target '.\target\Thom_new.exe'. This is used for comparing changes within the engine.
- 'make -j TRAIN=1' will compile the trainer into the main program (requires HIP SDK).
- 'make -j TRAIN=1 KPERFT=1' will compile the trainer with kernel profiling mode enabled.
- 'make -j TRAIN=1 NVIDIA=1' will attempt to compile the trainer on NVIDIA GPUs (untested).
- 'make -j DEBUG=1' will compile with fewer compiler optimizations to allow for easier gdb debugging.
- 'make -j SPSA=1' will compile with uci options for Simultaneous Perturbation Stochastic Approximation (SPSA)
- 'make -j RELEASE=1' will compile quantized weights & book into the executable. Disables debug log file.

### COMPATIBLITY:

The engine in its current form will only work on CPUs that support AVX2 instructions. 
The training kernels require HIP SDK to run. They were created specifically for my use and were ported from some OpenCL kernels that I made before I was able to find community support for running HIP on gfx1034. I put a nvcc compiler option into the makefile, but the program is untested on NVIDIA GPUs.

### ACKNOWLEDGEMENTS:

- The engine uses the gm2600.bin polyglot opening book from Scid vs PC.
- Ronald de Man's sygyzy tablebase is used for endgame analysis. A tablebase generator is available at https://github.com/syzygy1/tb
- Andy Grant's sygyzy probing tool Pyrrhic is used to probe the sygyzy files. It is available at https://github.com/AndyGrant/Pyrrhic
- The chess programming wiki (https://www.chessprogramming.org/) contains a lot of valuable information that was very helpful to me when I started this project.
- Dominik Klein's book "Neural Networks for Chess" helped me get started on understanding chess NNUEs. You can find it at https://github.com/asdfjkl/neural_network_chess
- Training data from https://robotmoon.com/nnue-training-data/ was used during the extensive debugging & optimization of the training kernels. Uninitialized weights received stockfish data training as a kickstart, but the aim is to create data for self-improvement, although this is taking some time.