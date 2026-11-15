from engine_wrapper import EngineProcess
import re
import random

ENGINE_PATH = "./target/Thom.exe"
BOOK_FILE = "./scripts/compare/UHO_Lichess_4852_v1.epd"
OUTPUT_FILE = "./scripts/nodes/middlegame.epd"
DEPTH = 8
MAX_FENS = 64

engine = EngineProcess(ENGINE_PATH)

WHITE = 0
BLACK = 1
stm = WHITE

score_regex = re.compile(r"score cp (-?\d+)")

with open(BOOK_FILE, "r", encoding="utf-8") as bookFile:
    with open(OUTPUT_FILE, "a", encoding="utf-8") as outputFile:

        lines = bookFile.readlines()
        random.shuffle(lines)

        # Add a white-to-move FEN on even, else black.
        fensWritten = 0

        for openingFEN in lines:
            if fensWritten >= MAX_FENS:
                break
            
            openingFEN = openingFEN.rstrip("\n")

            engine.send_command("ucinewgame\n")
            moves = []
            engine.setFEN(openingFEN, moves)

            words = openingFEN.split()
            stm = WHITE if words[-5] == 'w' else BLACK
            halfMoveCount = 2 * int(words[-1])
            initialHalfMoveCount = halfMoveCount
            if stm == BLACK:
                halfMoveCount = halfMoveCount + 1

            score = 0
            while abs(score) < 35:
                cmd = engine.setFEN(openingFEN, moves)
                engine.send_command(f"go depth {DEPTH}\n")
                output_lines = engine.read_until("bestmove")
                moves.append(output_lines[-1].split()[-1])
                
                score_info = score_regex.search(output_lines[-2])
                if not score_info:
                    break
                score = int(score_info.group(1))

                if len(moves) > 6 and abs(score) < 25 and stm == fensWritten % 2:
                    fenStr = engine.getFEN()
                    fensWritten += 1
                    print(f"\033[2K\r[{fensWritten}] - {fenStr}")
                    outputFile.write(f"\n{fenStr}")
                    outputFile.flush()
                    break


                halfMoveCount = halfMoveCount + 1
                stm = halfMoveCount % 2

