UCI-compliant NNUE chess engine created in C.

The engine currently uses its own hand-crafted evaluation with the intent to switch over to a NNUE as soon as data generation finishes. All the NNUE-related stuff is unimplemented and unfinished.

### BUILDING:

A release version of the engine should be compiled with 'make -j RELEASE=1'. It can exist outside of the compiled folder and won't attempt any file opening/read/writing for debug logs, opening books, or network weights.
- 'make clean' will remove target
- 'make -j all' will compile everything.
- 'make -j NEW=1' will compile with target '.\target\Thom_new.exe'. This is used to easily conduct SPRT tests.
- 'make -j NNUE=1 TRAIN=1' will compile the trainer into the main program (requires HIP SDK).
- 'make -j NNUE=1 TRAIN=1 KPERFT=1' will compile the trainer with kernel profiling mode enabled.
- 'make -j NNUE=1 TRAIN=1 NVIDIA=1' will attempt to compile the trainer on NVIDIA GPUs (untested).
- 'make -j DEBUG=1' will compile with fewer compiler optimizations to allow for easier gdb debugging.
- 'make -j SPSA=1' will compile with uci options for SPSA tuning
- 'make -j RELEASE=1' will compile quantized weights & book into the executable. Disables debug log file.

### COMPATIBLITY:

The NNUE will only work on CPUs that support AVX2 instructions. 
The training kernels require HIP SDK to run. Their compatibility was not really a priority or a concern.

### ACKNOWLEDGEMENTS:

- The engine is currently using the gm2600.bin polyglot opening book from Scid vs PC.
- Ronald de Man's sygyzy tablebase is used for endgame analysis.
- Andy Grant's sygyzy probing tool [Pyrrhic](https://github.com/AndyGrant/Pyrrhic) is used to probe the sygyzy files.
- Dominik Klein's book "Neural Networks for Chess" helped me get started on understanding chess NNUEs. You can find it at https://github.com/asdfjkl/neural_network_chess
- [Stockfish-converted Leela training data](https://robotmoon.com/nnue-training-data/) was used during the development of the training pipeline. Networks that were trained in the debugging phase were discarded. In the interests of originality, the engine will train its uninitialized weights on data generated from its own HCE.
- [Andrew Grant's modernized texel tuning method](https://github.com/AndyGrant/Ethereal/blob/master/Tuning.pdf) was used for tuning the HCE weights. 
- The lichess-big3-resolved.book dataset was used for tuning the HCE.
- The open-sourced nature of the chess programming community was very helpful for me to get past various barriers. I'd like to mention the following in particular that were the most impactful:
    - [Ethereal](https://github.com/AndyGrant/Ethereal)
    - [Alexandria](https://github.com/PGG106/Alexandria)
    - [Surge](https://github.com/nkarve/surge)

### 