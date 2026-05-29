#!/usr/bin/env python3
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SYSSOFT = ROOT / "build" / "SYSSOFT"


def main() -> int:
    if len(sys.argv) < 3:
        print("Usage: build_txt_to_asm.py <input.txt> <output.asm> [tmp-output-dir]", file=sys.stderr)
        return 1

    input_txt = Path(sys.argv[1]).resolve()
    output_asm = Path(sys.argv[2]).resolve()
    tmp_dir = Path(sys.argv[3]).resolve() if len(sys.argv) > 3 else (ROOT / "output_new")

    if not SYSSOFT.is_file():
        print(f"Error: compiler not found: {SYSSOFT}", file=sys.stderr)
        return 1
    if not input_txt.is_file():
        print(f"Error: input txt not found: {input_txt}", file=sys.stderr)
        return 1
    if not tmp_dir.is_dir():
        print(f"Error: output dir not found: {tmp_dir}", file=sys.stderr)
        return 1

    cmd = [str(SYSSOFT), str(tmp_dir), str(input_txt)]
    rc = subprocess.call(cmd, cwd=ROOT)
    if rc != 0:
        return rc

    listing = tmp_dir / "listing.osm"
    if not listing.is_file():
        print(f"Error: expected listing not found: {listing}", file=sys.stderr)
        return 1

    output_asm.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(listing, output_asm)
    print(f"Generated: {output_asm}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
