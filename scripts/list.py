#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path


def main():
    script_dir = Path(__file__).resolve().parent
    cmd = [str(script_dir / "remote_task.py")]
    sys.exit(subprocess.call(cmd))


if __name__ == "__main__":
    main()
