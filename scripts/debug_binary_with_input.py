#!/usr/bin/env python3
import os
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT = SCRIPT_DIR.parent

DEF_FILE = Path(os.environ.get("DEF_FILE", ROOT / "arch" / "arch.pdsl"))
ARCH_NAME = os.environ.get("ARCH_NAME", "Arch")
CODE_BANK = os.environ.get("CODE_BANK", "code")
IP_REG = os.environ.get("IP_REG", "IP")
FINISH_MNEMONIC = os.environ.get("FINISH_MNEMONIC", "HLT")
STDIN_REG = os.environ.get("STDIN_REG", "IN_PORT")
STDOUT_REG = os.environ.get("STDOUT_REG", "OUT_PORT")


def to_win(path: Path) -> str:
    return subprocess.check_output(["winepath", "-w", str(path)]).decode().strip()


def main():
    if len(sys.argv) < 3:
        print("Usage: debug_binary_with_input.py <binaryFileToRun> <inputFile>", file=sys.stderr)
        sys.exit(1)
    bin_file = Path(sys.argv[1])
    input_file = Path(sys.argv[2])
    if not bin_file.is_file():
        print(f"Error: binary file not found: {bin_file}", file=sys.stderr)
        sys.exit(1)
    if not input_file.is_file():
        print(f"Error: input file not found: {input_file}", file=sys.stderr)
        sys.exit(1)

    cmd = [
        str(SCRIPT_DIR / "remote_task.py"),
        "-il",
        "-s",
        "DebugBinaryWithInput",
        "-w",
        f"stdinRegStName={STDIN_REG}",
        f"stdoutRegStName={STDOUT_REG}",
        f"inputFile={to_win(input_file)}",
        f"definitionFile={to_win(DEF_FILE)}",
        f"archName={ARCH_NAME}",
        f"binaryFileToRun={to_win(bin_file)}",
        f"codeRamBankName={CODE_BANK}",
        f"ipRegStorageName={IP_REG}",
        f"finishMnemonicName={FINISH_MNEMONIC}",
    ]
    sys.exit(subprocess.call(cmd))


if __name__ == "__main__":
    main()
