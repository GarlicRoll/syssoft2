#!/usr/bin/env python3
import os
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT = SCRIPT_DIR.parent
DEF_FILE = Path(os.environ.get("DEF_FILE", ROOT / "arch" / "arch.pdsl"))
ARCH_NAME = os.environ.get("ARCH_NAME", "Arch")


def to_win(path: Path) -> str:
    return subprocess.check_output(["winepath", "-w", str(path)]).decode().strip()


def main():
    cmd = [
        str(SCRIPT_DIR / "remote_task.py"),
        "-s",
        "ResolveArchModel",
        "-w",
        "-q",
        f"definitionFile={to_win(DEF_FILE)}",
        f"archName={ARCH_NAME}",
    ]
    sys.exit(subprocess.call(cmd))


if __name__ == "__main__":
    main()
