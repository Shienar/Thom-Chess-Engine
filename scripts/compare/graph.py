import sys
import re
import threading
import bisect
import matplotlib.pyplot as plt
import matplotlib.animation as animation
import math

# Usage : 
# fastchess.exe <args> | py graph.py
# OR
# bash fastchess_script.sh | py graph.py

data_lock = threading.Lock()
games_played = []
elo_values = []
elo_errors = []

fig, ax = plt.subplots(1, 1, figsize=(10, 6))
fig.suptitle("Unknown Engine Match", fontsize=14, fontweight='bold')
receiving_input = True
def parse_fastchess_output():
    global fig
    global receiving_input
    title_regex = re.compile(r"Results of (.*?):")
    elo_regex = re.compile(r"Elo: ([-\d.]+) \+/- ([\d.]+),")
    games_regex = re.compile(r"Games: (\d+)")

    pending_elo = None
    pending_elo_err = None
    isTitleSet = False

    for line in sys.stdin:
        if "Started game" in line or "Finished game" in line:
            continue
        print(line, end='', flush=True)

        # Set graph title based on fastchess output summary
        if not isTitleSet:
            title_info = title_regex.search(line)
            if title_info:
                isTitleSet = True
                fig.suptitle(title_info.group(1), fontsize=14, fontweight='bold')

        # pending_elo is None if ELO is inf or nan, hence the buffering of elo values.
        elo_info = elo_regex.search(line)
        if elo_info:
            pending_elo = float(elo_info.group(1))
            pending_elo_err = float(elo_info.group(2))

        # Games get printed after corresponding elo line
        game_info = games_regex.search(line)
        if game_info:
            current_games = int(game_info.group(1))
            if pending_elo is not None and pending_elo_err is not None:
                with data_lock:
                    games_played.append(current_games)
                    elo_values.append(pending_elo)
                    elo_errors.append(pending_elo_err)
                pending_elo = None
                pending_elo_err = None
    receiving_input = False


last_seen_length = 0
data_labels = []
def update_label_visibility():
    x_min, x_max, _, _ = plt.axis()
    x_len = x_max - x_min
    last_point = (-1000, -1000)
    for i, label in enumerate(data_labels):
        current_point = label.get_position()
        if(math.dist(last_point, current_point) > x_len / 4):
            last_point = current_point
            label.set_visible(True)
        else:
            label.set_visible(False)

def update_graph(frame):
    global last_seen_length
    global data_labels
    if len(games_played) == last_seen_length or len(games_played) != len(elo_errors):
        if not receiving_input:
            ani.event_source.stop()
        return
    last_seen_length = len(games_played)
    
    with data_lock:
        ax.cla()

        x = list(games_played)
        y_elo = list(elo_values)
        y_err_elo = list(elo_errors)
        
        ax.plot(x, y_elo, color='b', marker='o', markersize=5, linewidth=2, label="Elo Delta")
    
        elo_lower = [y - err for y, err in zip(y_elo, y_err_elo)]
        elo_upper = [y + err for y, err in zip(y_elo, y_err_elo)]
        ax.fill_between(x, elo_lower, elo_upper, color='b', alpha=0.15, label="Confidence Interval")

        ax.axhline(0, color='gray', linestyle='--', linewidth=0.8)
        ax.set_xlabel("Games Played", fontweight='bold')
        ax.set_ylabel("Elo Rating", fontweight='bold')
        ax.set_title(f"Elo: {y_elo[-1]:.2f} +/- {y_err_elo[-1]:.2f}", loc='left', fontsize=11)
        ax.grid(True, linestyle=':', alpha=0.6)
        ax.legend(loc="upper left")

        # Crop
        if(games_played[-1] > 200):
            index = bisect.bisect_right(games_played, games_played[-1] - 200)
            ax.set_xlim(games_played[index], games_played[-1], emit=False)
            max_y_elo = max(y_elo[index :])
            min_y_elo = min(y_elo[index :])
            min_y_err_elo = min(y_err_elo[index :])
            if min_y_err_elo == 0:
                min_y_err_elo = 10
            ax.set_ylim(min_y_elo - min_y_err_elo, max_y_elo + min_y_err_elo, emit=False)
        else:
            max_y_elo = max(y_elo)
            min_y_elo = min(y_elo)
            min_y_err_elo = min(y_err_elo)
            if min_y_err_elo == 0:
                min_y_err_elo = 10
            ax.set_ylim(min_y_elo - min_y_err_elo, max_y_elo + min_y_err_elo, emit=False)

        # Annotate
        data_labels = [ax.text(x, y + 1, str(y), fontsize=9, clip_on=True) for i, (x, y) in enumerate(zip(x, y_elo))]
        update_label_visibility()


    plt.tight_layout()

    return (ax,)

event_zoom_x = None
event_zoom_y = None
def on_draw(event):
    update_label_visibility()

fig.canvas.mpl_connect('draw_event', on_draw)

worker = threading.Thread(target=parse_fastchess_output, daemon=True)
worker.start()

ani = animation.FuncAnimation(fig, update_graph, interval=500, cache_frame_data=False)
plt.show()