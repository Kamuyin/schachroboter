#!/usr/bin/env python3
import argparse
import json
import threading
import queue
import sys
import time

import paho.mqtt.client as mqtt
from rich.console import Console
from rich.table import Table
from rich.live import Live

BROKER_DEFAULT = "localhost"
TOPIC = "chess/board/fullstate"

class BoardState:
    def __init__(self):
        self.board = [[0]*8 for _ in range(8)]
        self.timestamp = 0

    def update_from_json(self, payload):
        try:
            data = json.loads(payload)
            if "board" in data and isinstance(data["board"], list):
                self.board = data["board"]
            if "timestamp" in data:
                self.timestamp = data["timestamp"]
        except Exception:
            pass

    def render_table(self):
        table = Table(title=f"Chess Board (timestamp: {self.timestamp})", show_header=True, box=None, pad_edge=False)
        # Add column labels (A-H)
        col_labels = [chr(ord('A')+i) for i in range(8)]
        table.add_column("", justify="right", no_wrap=True)
        for label in col_labels:
            table.add_column(f"[bold]{label}[/bold]", justify="center", no_wrap=True)

        # Unicode chessboard squares
        light = "[on #f0d9b5]"
        dark = "[on #b58863]"
        piece = "[bold black]♟[/bold black]"
        empty = " "

        for r in range(8):
            row_cells = [f"[bold]{8-r}[/bold]"]
            for c in range(8):
                color = light if (r+c)%2==0 else dark
                cell = piece if self.board[r][c] else empty
                row_cells.append(f"{color}{cell}[/]")
            table.add_row(*row_cells)
        return table

def mqtt_thread(broker, state, q, stop_event):
    def on_connect(client, userdata, flags, rc):
        client.subscribe(TOPIC)
    def on_message(client, userdata, msg):
        q.put(msg.payload.decode())
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    try:
        client.connect(broker)
    except Exception as e:
        print(f"Failed to connect to MQTT broker: {e}")
        stop_event.set()
        return
    client.loop_start()
    while not stop_event.is_set():
        time.sleep(0.1)
    client.loop_stop()
    client.disconnect()

def main():
    parser = argparse.ArgumentParser(description="Chess Board TUI from MQTT stream")
    parser.add_argument("--broker", type=str, default=BROKER_DEFAULT, help="MQTT broker address")
    args = parser.parse_args()

    state = BoardState()
    q = queue.Queue()
    stop_event = threading.Event()

    t = threading.Thread(target=mqtt_thread, args=(args.broker, state, q, stop_event), daemon=True)
    t.start()

    console = Console()
    with Live(state.render_table(), console=console, refresh_per_second=4) as live:
        try:
            while not stop_event.is_set():
                try:
                    payload = q.get(timeout=0.5)
                    state.update_from_json(payload)
                    live.update(state.render_table())
                except queue.Empty:
                    pass
        except KeyboardInterrupt:
            stop_event.set()
            t.join()
            print("\nExiting.")

if __name__ == "__main__":
    main()
