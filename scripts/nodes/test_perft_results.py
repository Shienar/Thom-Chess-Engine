from fen_loader import fen_list
from engine_wrapper import EngineProcess
import re

MIN_DEPTH = 1
MAX_DEPTH = 6
MAX_NODES = 10_000_000
ENGINE_PATH = "./target/Thom.exe"
engine = EngineProcess(ENGINE_PATH)

nodes_regex  = re.compile(r"(\d+)")
fail_count = 0
for index, info in enumerate(fen_list):
    print(f"\033[2K\rSearching FEN {index + 1}/{len(fen_list)}", end='')
    engine.send_command(f"position fen {info.fenStr}\n")
    
    for depth, expectedOutput in enumerate(info.perftResults):
        if expectedOutput != 0 and expectedOutput <= MAX_NODES and depth >= MIN_DEPTH and depth <= MAX_DEPTH :
            engine.send_command(f"perft {depth}\n")
            line = engine.process.stdout.readline()

            regex_info = nodes_regex.search(line)
            output = int(regex_info.group(1))

            if output != expectedOutput:
                fail_count = fail_count + 1
                print("\033[2K\rERROR:")
                print(f"\t{info.fenStr}")
                print(f"\tExpected {expectedOutput} nodes at depth {depth}, but got {output} in output.")
                


print(f"\033[2K\rPERFT TEST FINISHED, {fail_count} FAILS")