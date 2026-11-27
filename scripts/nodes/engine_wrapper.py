import subprocess
import re
import numpy as np

fen_regex = re.compile(r"FEN:\s*(.*)")
class EngineProcess:
    def __init__(self, path):
        self.process = subprocess.Popen(
            [path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1)

        self.send_command(f"uci\n")
        self.send_command(f"setoption name Hash value 4\n")
        self.send_command(f"isready\n")

        output = self.read_until("readyok")
        history_regex_list = [
            re.compile(r"name history_bonus_scale type spin default (\d+)"),
            re.compile(r"name history_bonus_offset type spin default (\d+)"),
            re.compile(r"name history_penalty_scale type spin default (\d+)"),
            re.compile(r"name history_penalty_offset type spin default (\d+)"),
            re.compile(r"name capture_history_bonus_scale type spin default (\d+)"),
            re.compile(r"name capture_history_bonus_offset type spin default (\d+)"),
            re.compile(r"name capture_history_penalty_scale type spin default (\d+)"),
            re.compile(r"name capture_history_penalty_offset type spin default (\d+)")
        ]


        self.default_history_values = np.zeros(8, dtype=np.float32)
        for line in output:
            for index, regex_line in enumerate(history_regex_list):
                info = regex_line.search(line)
                if info:
                    self.default_history_values[index] = float(info.group(1))

    def send_command(self, cmd):
        self.process.stdin.write(cmd)
        self.process.stdin.flush()

    def read_until(self, keyword):
        output_lines = []
        while True:
            line = self.process.stdout.readline().rstrip("\n")
            if not line:
                break
            output_lines.append(line)
            if keyword in line:
                break
        return output_lines

    def updateHistories(self, values):
        self.process.stdin.write(f"setoption name history_bonus_scale value {values[0]}\n")
        self.process.stdin.write(f"setoption name history_bonus_offset value {values[1]}\n")
        self.process.stdin.write(f"setoption name history_penalty_scale value {values[2]}\n")
        self.process.stdin.write(f"setoption name history_penalty_offset value {values[3]}\n")
        self.process.stdin.write(f"setoption name capture_history_bonus_scale value {values[4]}\n")
        self.process.stdin.write(f"setoption name capture_history_bonus_offset value {values[5]}\n")
        self.process.stdin.write(f"setoption name capture_history_penalty_scale value {values[6]}\n")
        self.process.stdin.write(f"setoption name capture_history_penalty_offset value {values[7]}\n")
        self.process.stdin.flush()

    def setFEN(self, fen, move_list=None):
        cmd = f"position fen {fen}"
        if move_list is not None and len(move_list) > 0:
            cmd += " moves"
            for move in move_list:
                cmd += f" {move}"
        self.process.stdin.write(f"{cmd}\n")
        self.process.stdin.flush()
        return cmd

    def getFEN(self):
        self.send_command("print\n")
        output_lines = self.read_until("FEN")
        fen_info = fen_regex.search(output_lines[-1])
        if not fen_info:
            return "Invalid FEN"
        return fen_info.group(1)

    def close(self):
        try:
            self.send_command("quit\n")
            self.process.wait(1)
        except:
            self.process.kill()

    def __del__(self):
        self.close()