import argparse
from datetime import datetime
from pathlib import Path
import re
import time

import serial


parser = argparse.ArgumentParser(description="Record W3E2 UART clock drift.")
parser.add_argument("--port", default="COM4")
args = parser.parse_args()

log_path = Path(__file__).with_name("ex2w3_drift.txt")
if log_path.exists():
    log_path = log_path.with_name(datetime.now().strftime("ex2w3_drift_%Y%m%d_%H%M%S.txt"))

start_pc = None
last_mcu_ms = 0

with serial.Serial(args.port, 115200, timeout=1) as uart, \
        log_path.open("x", encoding="utf-8") as log:
    print(f"Opened {args.port}, 115200 8N1. Press STM32 Reset ONCE.", flush=True)
    print(f"Log: {log_path}", flush=True)
    deadline = time.perf_counter() + 900

    while time.perf_counter() < deadline:
        line = uart.readline().decode("ascii", errors="replace").strip()
        received_pc = time.perf_counter()
        received_time = datetime.now().isoformat(timespec="milliseconds")
        match = re.search(r"\bmcu_ms=(\d+)\b", line)
        if match is None:
            continue

        mcu_ms = int(match.group(1))
        if start_pc is None:
            if mcu_ms != 0:
                continue
            start_pc = received_pc
            deadline = received_pc + 690
            print("Measurement started. Keep the board running for 10 minutes.", flush=True)
        elif mcu_ms < last_mcu_ms:
            raise RuntimeError("STM32 reset during measurement. This run is incomplete.")

        last_mcu_ms = mcu_ms
        elapsed_pc_ms = (received_pc - start_pc) * 1000
        drift_ms = mcu_ms - elapsed_pc_ms
        text = (f"{received_time} -> {line} "
                f"pc_elapsed_ms={elapsed_pc_ms:.3f} drift_ms={drift_ms:.3f}")
        print(text, flush=True)
        log.write(text + "\n")
        log.flush()

        if mcu_ms >= 600000:
            ppm = drift_ms / elapsed_pc_ms * 1000000
            result = f"COMPLETE drift_ms={drift_ms:.3f} drift_ppm={ppm:.3f} bound_ppm=65"
            print(result, flush=True)
            log.write(result + "\n")
            break
    else:
        raise TimeoutError("No complete 10-minute UART sequence was received. Log is incomplete.")
