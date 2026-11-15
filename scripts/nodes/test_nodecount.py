from concurrent.futures import ThreadPoolExecutor
import re
import queue
from engine_wrapper import EngineProcess
from fen_loader import getFENList
import numpy as np
import os

ENGINE_PATH = "./target/Thom.exe"
DEPTH = 10
engine = EngineProcess(ENGINE_PATH)

file_dir = os.path.dirname(os.path.abspath(__file__))
fen_path = os.path.join(file_dir, 'middlegame.epd')
fen_list = getFENList(fen_path)


def testFEN(fen):
    global engine

    engine.send_command("ucinewgame\n")
    engine.setFEN(fen)
    engine.send_command(f"go depth {DEPTH}\n")
    
    output_lines = engine.read_until("bestmove")

    nodes_regex = re.compile(r"nodes\s*(\d+)")
    nodes = 0

    nodecount_info = nodes_regex.search(output_lines[-2])
    if nodecount_info:
        nodes = int(nodecount_info.group(1))

    return nodes

nodelist = np.zeros(len(fen_list))

for index, fen in enumerate(fen_list):
    print(f"\033[2K\r{index+1}/{len(fen_list)}", end = '')
    nodelist[index] = testFEN(fen.fenStr)
    
nodes = np.sum(nodelist)
avg = np.average(nodelist)
print(f"\033[2K\rTotal Nodes: {nodes:,}")
print(f"Average: {avg:,}")