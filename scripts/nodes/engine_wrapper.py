import subprocess
import re
import numpy as np

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
            re.compile(r"historyBonusScale type spin default (\d+)"),
            re.compile(r"historyBonusOffset type spin default (\d+)"),
            re.compile(r"historyPenaltyScale type spin default (\d+)"),
            re.compile(r"historyPenaltyOffset type spin default (\d+)"),
            re.compile(r"captureHistoryBonusScale type spin default (\d+)"),
            re.compile(r"captureHistoryBonusOffset type spin default (\d+)"),
            re.compile(r"captureHistoryPenaltyScale type spin default (\d+)"),
            re.compile(r"captureHistoryPenaltyOffset type spin default (\d+)")
        ]


        self.defaultHistoryValues = np.zeros(8, dtype=np.float32)
        for line in output:
            for index, regex_line in enumerate(history_regex_list):
                info = regex_line.search(line)
                if info:
                    self.defaultHistoryValues[index] = float(info.group(1))

    def send_command(self, cmd):
        self.process.stdin.write(cmd)
        self.process.stdin.flush()

    def updateHistories(self, values):
        self.process.stdin.write(f"setoption name historyBonusScale value {values[0]}\n")
        self.process.stdin.write(f"setoption name historyBonusOffset value {values[1]}\n")
        self.process.stdin.write(f"setoption name historyPenaltyScale value {values[2]}\n")
        self.process.stdin.write(f"setoption name historyPenaltyOffset value {values[3]}\n")
        self.process.stdin.write(f"setoption name captureHistoryBonusScale value {values[4]}\n")
        self.process.stdin.write(f"setoption name captureHistoryBonusOffset value {values[5]}\n")
        self.process.stdin.write(f"setoption name captureHistoryPenaltyScale value {values[6]}\n")
        self.process.stdin.write(f"setoption name captureHistoryPenaltyOffset value {values[7]}\n")
        self.process.stdin.flush()

    def read_until(self, keyword):
        output_lines = []
        while True:
            line = self.process.stdout.readline()
            if not line:
                break
            output_lines.append(line)
            if keyword in line:
                break
        return output_lines

    def close(self):
        try:
            self.send_command("quit\n")
            self.process.wait(1)
        except:
            self.process.kill()
    
    def __del__(self):
        self.close()