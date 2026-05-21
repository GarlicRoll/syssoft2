#!/usr/bin/env python3
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def read_cycles_signal_period(devices_xml: Path) -> int:
    tree = ET.parse(devices_xml)
    root = tree.getroot()

    ns = {}
    if root.tag.startswith("{"):
        ns_uri = root.tag[1:].split("}", 1)[0]
        ns = {"vm": ns_uri}
        params = root.findall(".//vm:Parameter", ns)
    else:
        params = root.findall(".//Parameter")

    for p in params:
        if p.attrib.get("Name") == "CyclesSignalPeriod":
            val = p.attrib.get("Value")
            if val is None:
                raise ValueError("CyclesSignalPeriod has no Value attribute")
            period = int(val, 0)
            if period <= 0:
                raise ValueError("CyclesSignalPeriod must be > 0")
            return period

    raise ValueError("CyclesSignalPeriod parameter not found")


def patch_asm_threshold(asm_file: Path, period: int) -> None:
    text = asm_file.read_text(encoding="utf-8")

    text = re.sub(
        r"(;\s*if \(clock_cycles - last_cycles\) >= )\d+(\s*-> switch thread)",
        rf"\g<1>{period}\2",
        text,
    )

    patched, count = re.subn(
        r"(PUSH 1\s*\n\s*PUSH )\d+(\s*\n\s*GTE)",
        rf"\g<1>{period}\2",
        text,
        count=1,
    )

    if count == 0:
        raise ValueError("Could not find threshold pattern in vm_two_threads.asm")

    asm_file.write_text(patched, encoding="utf-8")


def main() -> int:
    if len(sys.argv) != 3:
        print(
            "Usage: sync_vm_two_threads_period.py <devices.xml> <vm_two_threads.asm>",
            file=sys.stderr,
        )
        return 1

    devices_xml = Path(sys.argv[1])
    asm_file = Path(sys.argv[2])

    if not devices_xml.is_file():
        print(f"Error: devices xml not found: {devices_xml}", file=sys.stderr)
        return 1
    if not asm_file.is_file():
        print(f"Error: asm file not found: {asm_file}", file=sys.stderr)
        return 1

    period = read_cycles_signal_period(devices_xml)
    patch_asm_threshold(asm_file, period)
    print(f"Synchronized {asm_file.name}: CyclesSignalPeriod={period}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
