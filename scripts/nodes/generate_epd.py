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

with open(BOOK_FILE, "r", encoding="utf-8") as book_file:
    with open(OUTPUT_FILE, "a", encoding="utf-8") as output_file:

        lines = book_file.readlines()
        random.shuffle(lines)

        # Add a white-to-move FEN on even, else black.
        fens_written = 0

        for opening_fen in lines:
            if fens_written >= MAX_FENS:
                break

            opening_fen = opening_fen.rstrip("\n")

            engine.send_command("ucinewgame\n")
            moves = []
            engine.setFEN(opening_fen, moves)

            words = opening_fen.split()
            stm = WHITE if words[-5] == 'w' else BLACK
            half_move_count = 2 * int(words[-1])
            if stm == BLACK:
                half_move_count = half_move_count + 1

            score = 0
            while abs(score) < 35:
                cmd = engine.setFEN(opening_fen, moves)
                engine.send_command(f"go depth {DEPTH}\n")
                output_lines = engine.read_until("bestmove")
                moves.append(output_lines[-1].split()[-1])

                score_info = score_regex.search(output_lines[-2])
                if not score_info:
                    break
                score = int(score_info.group(1))

                if len(moves) > 6 and abs(score) < 25 and stm == fens_written % 2:
                    fen_string = engine.getFEN()
                    fens_written += 1
                    print(f"\033[2K\r[{fens_written}] - {fen_string}")
                    output_file.write(f"\n{fen_string}")
                    output_file.flush()
                    break


                half_move_count = half_move_count + 1
                stm = half_move_count % 2

