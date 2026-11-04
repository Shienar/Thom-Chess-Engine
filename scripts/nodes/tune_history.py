import re
from scipy.optimize import minimize
import numpy as np
from engine_wrapper import EngineProcess
from fen_loader import fen_list

ENGINE_PATH = "./target/Thom.exe"
DEPTH = 8

engine = EngineProcess(ENGINE_PATH)

values = np.copy(engine.defaultHistoryValues)
best_values = np.copy(values)
min_limits = np.clip(values - 50, min=1)
max_limits = np.clip(values + 50, max=600)
bounds = [(min, max) for min, max in zip(min_limits, max_limits)]

print(f"Default values: {values}\n")

lowest_nodecount = int(1e9)

def testFEN(values, fen):
    global engine

    engine.updateHistories(values)
    engine.send_command("ucinewgame\n")
    engine.send_command(f"position fen {fen}\n")
    engine.send_command(f"go depth {DEPTH}\n")
    
    output_lines = engine.read_until("bestmove")

    nodes_regex = re.compile(r"nodes (\d+)")
    nodes = 0
    
    nodecount_info = nodes_regex.search(output_lines[-2])
    if nodecount_info:
        nodes = int(nodecount_info.group(1))
            
    return nodes

def getNodeCount(values, func):
    global iteration
    global best_values
    global lowest_nodecount

    rounded_values = [round(p) for p in values]
    
    nodelist = np.zeros(len(fen_list))
    for index, fen in enumerate(fen_list):
        nodelist[index] = func(rounded_values, fen.fenStr)
    nodes = np.sum(nodelist)

    print(f"\033[1A\033[2K[{iteration}]: Values {rounded_values} reached a nodecount of {nodes} | Best Values {best_values} reached a nodecount of {lowest_nodecount}")

    return nodes

iterations_since_improvement = 0
iteration = 0
lowest_nodecount = getNodeCount(best_values, testFEN)
rng = np.random.default_rng()
while True:
    iteration = iteration + 1
    result = minimize(
        getNodeCount,
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
    if result.fun < lowest_nodecount:
        lowest_nodecount = result.fun
        best_values = rounded_values[:]
        iterations_since_improvement = 0

        min_limits = np.clip(rounded_values - 50, min=1)
        max_limits = np.clip(rounded_values + 50, max=600)
        bounds = [(min, max) for min, max in zip(min_limits, max_limits)]
    else:
        iterations_since_improvement = iterations_since_improvement + 1

    if iterations_since_improvement > 10:
        break

    values = rounded_values[:] + rng.normal(loc=0, scale=5, size=values.shape)
    values = np.clip(values, min_limits, max_limits)