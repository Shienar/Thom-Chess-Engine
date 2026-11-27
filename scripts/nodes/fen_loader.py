import numpy as np
import os
import re

class FEN():
    def __init__(self, fen, perft_results):
        self.fen_string = fen
        self.perft_results = perft_results

    def __str__(self):
        return f"{self.fen} ;D1 {self.perft_results[1]} ;D2 {self.perft_results[2]} ;D3 {self.perft_results[3]} ;D4 {self.perft_results[4]} ;D5 {self.perft_results[5]} ;D6 {self.perft_results[6]}"

fen_regex = re.compile(r"(.*?) ;D1 (\d+) ;D2 (\d+) ;D3 (\d+) ;D4 (\d+) ;D5 (\d+) ;D6 (\d+)")


def getFENList(fen_path):
    fen_list = []
    with open(fen_path, "r", encoding="utf-8") as file:
        for line in file:
            fen_string = None
            perft_results = np.zeros(7, dtype=np.int64)

            regex_info = fen_regex.search(line)
            if regex_info:
                fen_string = regex_info.group(1)
                for d in range(1, 7):
                    perft_results[d] = regex_info.group(1 + d)

            if fen_string:
                fen_list.append(FEN(fen_string, perft_results))
    return fen_list