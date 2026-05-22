#!/usr/bin/env python3
import argparse
import os
import re
import signal
import subprocess
import sys
import time
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
ROOT = SCRIPT_DIR.parent

ASM_FILE = ROOT / "spo2_lab1" / "vm_two_threads.asm"
DEVICES_FILE = ROOT / "spo2_lab1" / "devices-1.xml"
BIN_FILE = SCRIPT_DIR / "output" / "remote_tasks" / "out.ptptb"


def run_and_print(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    proc = subprocess.run(cmd, cwd=SCRIPT_DIR, text=True, capture_output=True)
    if proc.stdout:
        print(proc.stdout, end="")
    if proc.stderr:
        print(proc.stderr, end="", file=sys.stderr)
    return proc


def run_with_timeout(cmd: list[str], timeout_sec: int) -> int:
    proc = subprocess.Popen(cmd, cwd=SCRIPT_DIR, start_new_session=True)
    deadline = time.monotonic() + timeout_sec

    def stop_group(sig: int) -> None:
        try:
            os.killpg(proc.pid, sig)
        except ProcessLookupError:
            pass

    try:
        while True:
            rc = proc.poll()
            if rc is not None:
                return rc
            if time.monotonic() >= deadline:
                raise subprocess.TimeoutExpired(cmd, timeout_sec)
            time.sleep(0.1)
    except KeyboardInterrupt:
        print("\nInterrupted by Ctrl+C, stopping execute step...")
        stop_group(signal.SIGINT)
        time.sleep(0.5)
        stop_group(signal.SIGTERM)
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            stop_group(signal.SIGKILL)
            proc.wait()
        return 130
    except subprocess.TimeoutExpired:
        print(f"\nExecution timeout reached ({timeout_sec}s), stopping execute step.")
        stop_group(signal.SIGTERM)
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            stop_group(signal.SIGKILL)
            proc.wait()
        return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--execute-timeout",
        type=int,
        default=20,
        help="How many seconds to run execute_binary_with_io before auto-stop (default: 20).",
    )
    args = parser.parse_args()

    if not ASM_FILE.is_file():
        print(f"Error: asm file not found: {ASM_FILE}", file=sys.stderr)
        return 1
    if not DEVICES_FILE.is_file():
        print(f"Error: devices file not found: {DEVICES_FILE}", file=sys.stderr)
        return 1

    assemble_cmd = [sys.executable, "assemble.py", str(ASM_FILE)]
    assemble_proc = run_and_print(assemble_cmd)
    if assemble_proc.returncode != 0:
        return assemble_proc.returncode

    match = re.search(r"Task Assemble started with id ([0-9a-fA-F-]{36})", assemble_proc.stdout or "")
    if not match:
        print("Error: could not parse assemble task id from output", file=sys.stderr)
        return 1
    task_id = match.group(1)

    show_cmd = [sys.executable, "show_task.py", task_id]
    show_proc = run_and_print(show_cmd)
    if show_proc.returncode != 0:
        return show_proc.returncode

    exec_cmd = [
        sys.executable,
        "execute_binary_with_io.py",
        str(BIN_FILE),
        str(DEVICES_FILE),
    ]
    return run_with_timeout(exec_cmd, args.execute_timeout)


if __name__ == "__main__":
    raise SystemExit(main())
