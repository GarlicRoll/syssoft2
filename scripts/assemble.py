#!/usr/bin/env python3
import os
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT = SCRIPT_DIR.parent

DEF_FILE = Path(os.environ.get("DEF_FILE", ROOT / "arch.pdsl"))
ARCH_NAME = os.environ.get("ARCH_NAME", "Arch")


def to_win(path: Path) -> str:
    return subprocess.check_output(["winepath", "-w", str(path)]).decode().strip()


def main():
    if len(sys.argv) < 2:
        print("Usage: assemble.py <asmListing>", file=sys.stderr)
        sys.exit(1)
    asm_file = Path(sys.argv[1])
    if not asm_file.is_file():
        print(f"Error: asm listing not found: {asm_file}", file=sys.stderr)
        sys.exit(1)

    cmd = [
        str(SCRIPT_DIR / "remote_task.py"),
        "-s",
        "Assemble",
        "-w",
        "-q",
        "-v",
        f"definitionFile={to_win(DEF_FILE)}",
        f"archName={ARCH_NAME}",
        f"asmListing={to_win(asm_file)}",
    ]
    sys.exit(subprocess.call(cmd))


if __name__ == "__main__":
    main()
