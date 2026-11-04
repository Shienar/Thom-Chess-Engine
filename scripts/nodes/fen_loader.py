# Reformatted data from https://github.com/AndyGrant/Ethereal/blob/master/src/perft/standard.epd
import numpy as np
import os
import re

class FEN():
    def __init__(self, fen, perftResults):
        self.fenStr = fen
        self.perftResults = perftResults

    def __str__(self):
        return f"{self.fen} ;D1 {self.perftResults[1]} ;D2 {self.perftResults[2]} ;D3 {self.perftResults[3]} ;D4 {self.perftResults[4]} ;D5 {self.perftResults[5]} ;D6 {self.perftResults[6]}"
    
fen_list = []
file_dir = os.path.dirname(os.path.abspath(__file__))
fen_path = os.path.join(file_dir, 'standard.epd')
fen_regex = re.compile(r"(.*?) ;D1 (\d+) ;D2 (\d+) ;D3 (\d+) ;D4 (\d+) ;D5 (\d+) ;D6 (\d+)")

with open(fen_path, "r", encoding="utf-8") as file:
    for line in file:
        fenStr = None
        perftResults = np.zeros(7, dtype=np.int64)

        regex_info = fen_regex.search(line)
        if regex_info:
            fenStr = regex_info.group(1)
            for d in range(1, 7):
                perftResults[d] = regex_info.group(1 + d)

        if fenStr:
            fen_list.append(FEN(fenStr, perftResults))