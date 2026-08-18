UCI-compliant HCE chess engine created in C.
The NNUE requires AVX2 SIMD instructions.

## BUILDING:

A release version of the engine should be compiled with 'make -j'.
- 'make -j' will compile everything.
- 'make clean' will remove target
- 'make -j NEW=1' will compile with target '.\target\Thom_new.exe'. This is used to easily conduct SPRT tests against a copy.
- 'make -j DEBUG=1' will compile with fewer compiler optimizations to allow for easier gdb debugging.
- 'make -j VERIFY=1' will include assertions to double-check the validity of efficient accumulator updates. Requires DEBUG=1.
- 'make -j SPSA=1' will compile with uci options for SPSA tuning

## COMMANDS:

### UCI:

- Options:
    - Threads, default 1 [1, 64]
    - Hash, default 4 [1, 4096]
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

### Non-UCI:
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
- generate "&lt;outputFilePath&gt;"
    - Viriformat binpack data generation
- binpackinfo "&lt;binpackFilePath&gt;"
    - Get information about a generated binpack

## FEATURES:

### NNUE:
- 2 x (768 -> 256) -> 1
    - 10 King Input Buckets
        - Accumulator Refresh Tables
        - Horizontal Mirroring
    - 8 Output Buckets
- Lizard SCReLU
- Accumulator Stack

### HCE:
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

### Search:
- Iterative Deepening
- Transposition Table
- Syzygy
- Aspiration Windows
- LazySMP
- Principal Variation Search
    - Mate Distance Pruning
    - Improving Heuristic
    - Killer Move Heuristic
    - History Heuristic
    - Countermove Heuristic
    - Futility Pruning
    - Reverse Futility Pruning
    - Null Move Pruning
    - Probcut
    - TT Reductions
    - Singular Extensions
    - Multicut Pruning
    - Check Extensions
    - Late Move Pruning
    - Late Move Reductions
    - SEE Pruning
- Quiescent Search
    - Delta Pruning
    - SEE Pruning

## ACKNOWLEDGEMENTS:

- The engine currently uses the [komodo.bin](https://komodochess.com/downloads.htm) polyglot book truncated to ply 6
- Ronald de Man's syzygy tablebase is used for endgame analysis.
- Andrew Grant's syzygy probing tool [Pyrrhic](https://github.com/AndyGrant/Pyrrhic) is used to probe the syzygy files.
- [Andrew Grant's modernized texel tuning method](https://github.com/AndyGrant/Ethereal/blob/master/Tuning.pdf) was used for tuning the HCE weights. 
- The lichess-big3-resolved.book dataset was used for tuning the HCE.
- The open-sourced nature of the chess programming community was very helpful for me to get past various barriers. I'd like to mention the following in particular that were the most impactful:
    - [Ethereal](https://github.com/AndyGrant/Ethereal) (For search & syzygy implementation)
    - [Alexandria](https://github.com/PGG106/Alexandria) (For search implementation)
    - [Stash](https://gitlab.com/mhouppin/stash-bot) (For search implementation)
    - [Surge](https://github.com/nkarve/surge) (For movegen)