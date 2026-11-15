from engine_wrapper import EngineProcess
import numpy as np
import re

ENGINE_PATH = "./target/Thom.exe"
SOURCE_FILE = "./scripts/nodes/middlegame.epd"
TARGET_FILE = SOURCE_FILE.replace(".epd", ".tmp")

engine = EngineProcess(ENGINE_PATH)
nodes_regex  = re.compile(r"(\d+)")

fen = ""
results = np.zeros(6, dtype=np.int64)

with open(TARGET_FILE, "w", encoding="utf-8") as outputFile:
    with open(SOURCE_FILE, "r", encoding="utf-8") as inputFile:
        for lineNum, line in enumerate(inputFile):
            if line.lstrip()[:1] == "#":
                print(f"{lineNum} | {line}", end='')
                outputFile.write(line)
                continue

            fen = line.rstrip()
            engine.setFEN(fen)
            for depth in range(1, 6):
                engine.send_command(f"perft {depth}\n")
                line = engine.process.stdout.readline()

                regex_info = nodes_regex.search(line)
                results[depth - 1] = int(regex_info.group(1))

            # depth 6 is too big for most middlegame positions. Billions of positions is the norm, and I don't want to spend an hour on this.
            print(f"{lineNum} | {fen} ;D1 {results[0]} ;D2 {results[1]} ;D3 {results[2]} ;D4 {results[3]} ;D5 {results[4]} ;D6 0")
            outputFile.write(f"{fen} ;D1 {results[0]} ;D2 {results[1]} ;D3 {results[2]} ;D4 {results[3]} ;D5 {results[4]} ;D6 0\n")
            outputFile.flush()
