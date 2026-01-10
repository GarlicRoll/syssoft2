#!/usr/bin/env python3
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
KNOWN_RESULTS = [

    "out.ptptb"
]

    # "stdout.txt",
    # "stderr.txt",
    # "syntaxTree.txt",
    # "archInfoModel.xml",
    # "trace.txt",
    # "archInfoGraph.dgml",

def main():
    if len(sys.argv) < 2:
        print("Usage: show_task.py <task-id> [result-dir]", file=sys.stderr)
        sys.exit(1)

    task_id = sys.argv[1]
    result_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("./output/remote_tasks")

    if result_dir.exists():
        shutil.rmtree(result_dir)
    result_dir.mkdir(parents=True, exist_ok=True)

    print(f"Fetching task {task_id} into {result_dir}")

    subprocess.call([str(SCRIPT_DIR / "remote_task.py"), "-g", task_id])

    for name in KNOWN_RESULTS:
        out_path = result_dir / name
        cmd = [
            str(SCRIPT_DIR / "remote_task.py"),
            "-g",
            task_id,
            "-r",
            name,
            "-o",
            str(out_path),
            "-q",
        ]
        subprocess.call(cmd)

    print(f"Results placed in {result_dir}")


if __name__ == "__main__":
    main()
