UCI-compliant NNUE chess engine created in C. Named after Thom Merrilin from The Wheel of Time.

All NNUE data was self-generated, starting from HCE-generated data.
The NNUE requires compliance with AVX2 SIMD instructions.

<details>
<summary><h2>BUILDING</h2></summary>

A release version of the engine should be compiled with 'make -j'.
- 'make -j' will compile everything.
- 'make clean' will remove target
- 'make -j NEW=1' will compile with target '.\target\Thom_new.exe'. This is used to easily conduct SPRT tests against a copy.
- 'make -j DEBUG=1' will compile with fewer compiler optimizations to allow for easier gdb debugging.
- 'make -j SEARCHINFO=1' will enable the logging & printing of informative search information, such as cutoff rates and effective branching factor.
- 'make -j VERIFY=1' will include assertions to double-check the validity of efficient accumulator updates & hash code updates. Requires DEBUG=1.
- 'make -j SPSA=1' will compile with uci options for SPSA tuning

This repository also contains a trainer for the NNUE. It was designed specifically for my (AMD) hardware & network architecture as a fun side project, and I make no guarantees about its compilability or effectiveness on other GPUs.
- 'make -j TRAIN=1' will compile the trainer.
- 'make =j TRAIN=1 KPERFT=1' will allow for performance tracking of the training kernels.
</details>

<details>
<summary><h2>COMMANDS</h2></summary>

<details> 
<summary><h3>UCI</h3></summary>

- Options:
    - Threads, default 1 [1, 64]
    - Hash, default 256 [1, 4096]
    - Ponder, default off
    - OwnBook, default off
    - SyzygyPath, default ""
    - SyzygyProbeLimit, default 5 [3, 7]
    - SyzygyProbeDepth, default 6 [5, 32]
- ucinewgame
- isready
- debug \[on/off\]
- position \[startpos | fen &lt;FEN&gt;\]
- go
    - depth N
    - infinite
    - ponder
    - wtime
    - btime
    - winc
    - binc
    - nodes
    - movetime
    - searchmoves
- ponderhit
- stop
- quit
</details>

<details> 
<summary><h3>Non-UCI</h3></summary>

- Options:
    - LogFilePath, default ""
        - Save any debug error messages to a file at this path.
    - UseNNUE, default true
- perft &lt;depth&gt;
- perftv &lt;depth&gt;
    - verbose perft, shows positions per first move.
- print
    - prints board
- eval
    - prints eval
- tune &lt;forcedK (0 for auto)&gt; &lt;epochs&gt; &lt;max_lr&gt; &lt;min_lr&gt; "&lt;inputPath&gt;" "&lt;outputPath&gt;"
    - HCE Tuning.
- generate &lt;outputFilePath&gt;
    - Viriformat binpack data generation
- binpackinfo &lt;binpackFilePath&gt;
    - Get information about a generated binpack
- cleanbinpack &lt;binpackFilePath&gt;
    - Iterates through the "filename.extension" viriformat binpack and creates a "filename_new.extension" binpack copy. If any games within the original contained a move that caused an error during iteration, they will be omitted from the copy. This is not meant as a substitution for solid write methods, and a 0.000427485% erroneous position rate is probably caused by the process unexpectedly getting force-closed or interrupted in the middle of a write.
- train &lt;epochs&gt; &lt;min_lr&gt; &lt;max_lr&gt; &lt;binpack file path&gt; &lt;kernel file path&gt;
    - GPU training of the NNUE.
    - Requires HIP & must be compiled with 'TRAIN=1'
    - Not intended to be universally compatible or flexible. 
    - Not included in release binaries.
</details>
</details>

<details>
<summary><h2>FEATURES</h2></summary>

<details>
<summary><h3>NNUE</h3></summary>
    
- 2 x (768 -> 256) -> 1
    - 10 King Input Buckets
        - Horizontal Mirroring
    - 8 Output Buckets
- Lizard SCReLU
- Accumulator Refresh Tables
- Accumulator Stack
</details>

<details>
<summary><h3>HCE</h3></summary>
    
- Raw Piece Values
- Piece/Square Tables
- Mobility
- Virtual Mobility
- Pawn cover of minor piece
- Passed Pawns
- Connected Pawns
- Neighboring Pawns
- Doubled Pawns
- Isolated Pawns
- Knight Outpost
- Bishop Pair
- Bad (same-colored) pawns for bishop
- (Semi-)Open Rook Files
- Connected sliders
- King Pawn Shield
- King Pawn Storm
- Open File near King
- King Safety Table
- Tempo
</details>

<details>
<summary><h3>Search</h3></summary>
    
- Iterative Deepening
- Transposition Table
- Syzygy
- Aspiration Windows
- LazySMP
- Principal Variation Search
    - Mate Distance Pruning
    - Improving Heuristic
    - Killer Move Heuristic
    - Quiet History Heuristic
    - Capture History Heuristic
    - Countermove History Heuristic
    - Followup History Heuristic
    - Static Eval Correction History
    - Razoring
    - Futility Pruning
    - Reverse Futility Pruning
    - Null Move Pruning
    - TT Reductions
    - Stable Eval Reductions
    - Worsening Reductions
    - Singular Extensions
    - Multicut Pruning
    - Check Extensions
    - Late Move Pruning
    - Late Move Reductions
    - SEE Pruning
- Quiescent Search
    - Delta Pruning
    - SEE Pruning
</details>
</details>

## ACKNOWLEDGEMENTS:

- The engine currently uses the [komodo.bin](https://komodochess.com/downloads.htm) polyglot book truncated to ply 6
- Ronald de Man's syzygy tablebase is used for endgame analysis.
- Andrew Grant's syzygy probing tool [Pyrrhic](https://github.com/AndyGrant/Pyrrhic) is used to probe the syzygy files.
- [Andrew Grant's modernized texel tuning method](https://github.com/AndyGrant/Ethereal/blob/master/Tuning.pdf) was used for tuning the HCE weights. 
- The lichess-big3-resolved.book dataset was used for tuning the HCE.
- [Weather Factory](https://github.com/jnlt3/weather-factory) was used for SPSA tuning.
- [Bullet](https://github.com/jw1912/bullet) was used historically for NNUE training. While current NNUEs are from this repository's trainer, being able to easily train working networks was extremely helpful in developing a working quantized forward propagation.
- The open-sourced nature of the chess programming community was very helpful for me to get past various barriers. I'd like to mention the following in particular that were the most impactful:
    - [Ethereal](https://github.com/AndyGrant/Ethereal) (For search & syzygy implementation)
    - [Alexandria](https://github.com/PGG106/Alexandria) (For search implementation)
    - [Stash](https://gitlab.com/mhouppin/stash-bot) (For search implementation)
    - [Surge](https://github.com/nkarve/surge) (For movegen)
