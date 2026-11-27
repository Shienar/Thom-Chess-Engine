import re
from scipy.optimize import minimize
import numpy as np
from engine_wrapper import EngineProcess
from fen_loader import getFENList
import os

ENGINE_PATH = "./target/Thom.exe"
DEPTH = 8

engine = EngineProcess(ENGINE_PATH)

file_dir = os.path.dirname(os.path.abspath(__file__))
fen_path = os.path.join(file_dir, 'middlegame.epd')
fen_list = getFENList(fen_path)

values = np.copy(engine.default_history_values)
best_values = np.copy(values)
min_limits = np.clip(values - 50, min=1)
max_limits = np.clip(values + 50, max=600)
bounds = [(min, max) for min, max in zip(min_limits, max_limits)]

def testFEN(values, fen):
    global engine

    engine.updateHistories(values)
    engine.send_command("ucinewgame\n")
    engine.setFEN(fen)
    engine.send_command(f"go depth {DEPTH}\n")

    output_lines = engine.read_until("bestmove")

    nodes_regex = re.compile(r"nodes (\d+)")
    nodes = 0

    nodecount_info = nodes_regex.search(output_lines[-2])
    if nodecount_info:
        nodes = int(nodecount_info.group(1))

    return nodes

lowest_nodecount = 0
def get_node_count(values, func):
    global iteration
    global best_values
    global lowest_nodecount

    rounded_values = [round(p) for p in values]

    nodelist = np.zeros(len(fen_list))
    for index, fen in enumerate(fen_list):
        nodelist[index] = func(rounded_values, fen.fen_string)
    nodes = np.sum(nodelist)

    if lowest_nodecount != 0:
        print(f"\033[1A\033[2K[{iteration}]: Values {rounded_values} reached a nodecount of {nodes:,} | Best Values {best_values} reached a nodecount of {lowest_nodecount:,}")

    if nodes < lowest_nodecount:
        best_values = rounded_values[:]
        lowest_nodecount = nodes

    return nodes

iteration = 0
lowest_nodecount = get_node_count(best_values, testFEN)
rng = np.random.default_rng()
print(f"Default values: {values} reached a nodecount of {lowest_nodecount:,}\n")
while True:
    iteration = iteration + 1
    pre_nodecount = lowest_nodecount
    result = minimize(
        get_node_count,
        values,
        args=testFEN,
        method='Powell',
        bounds=bounds,
        options={
            'xtol': 1.0,
            'ftol': 1.0,
            'disp': False
        }
    )
    rounded_values = np.array([round(p) for p in result.x])
    if pre_nodecount <= lowest_nodecount:
        break

    min_limits = np.clip(rounded_values - 50, min=1)
    max_limits = np.clip(rounded_values + 50, max=600)
    bounds = [(min, max) for min, max in zip(min_limits, max_limits)]
    values = best_values[:]