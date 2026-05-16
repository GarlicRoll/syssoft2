#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
TASKS_DIR="$ROOT_DIR/RemoteTasks"
WORK_DIR="$ROOT_DIR/scheduler_lab"
APP_BIN="$ROOT_DIR/app"
SOURCE_TEXT="$WORK_DIR/scheduler_program.txt"

LOGIN="${PORTABLE_REMOTE_LOGIN:-XXXXXXXXX}"
PASSWORD="${PORTABLE_REMOTE_PASSWORD:-XXXXXXXXXXXXXXXXXXXxx}"

TEMP_DIR="$(mktemp -d -t nikita_scheduler)"
SOURCE_ASM="$TEMP_DIR/scheduler_program.asm"
COMBINED_ASM="$TEMP_DIR/scheduler_combined.asm"
BIN_FILE="$TEMP_DIR/scheduler_demo.ptptb"
COMPILE_LOG="$TEMP_DIR/compiler.log"
STREAM_FIFO="$TEMP_DIR/stream.fifo"
RUN_PID=""

cleanup() {
  if [ -n "$RUN_PID" ]; then
    kill "$RUN_PID" 2>/dev/null || true
    wait "$RUN_PID" 2>/dev/null || true
  fi
  rm -rf "$TEMP_DIR"
}
trap cleanup EXIT

echo "=== Build compiler ==="
make -s -C "$ROOT_DIR" build >/dev/null

: > "$COMPILE_LOG"
echo "=== Compile scheduler_program.txt ==="
if ! "$APP_BIN" --no-ast --no-cfg --no-pdf -o "$SOURCE_ASM" "$SOURCE_TEXT" >>"$COMPILE_LOG" 2>&1; then
  cat "$COMPILE_LOG" >&2
  exit 1
fi

cat "$SOURCE_ASM" "$WORK_DIR/scheduler_runtime.asm" > "$COMBINED_ASM"

echo "=== Assemble scheduler listing ==="
ASM_TASK_ID="$(
  cd "$TASKS_DIR"
  mono Portable.RemoteTasks.Manager.exe \
    -ul "$LOGIN" \
    -up "$PASSWORD" \
    -id -w \
    -s Assemble \
    asmListing "$COMBINED_ASM" \
    definitionFile "$TASKS_DIR/definition.target.pdsl" \
    archName zns \
    | tr -d '\r' \
    | awk 'NF { last = $0 } END { gsub(/^[[:space:]]+|[[:space:]]+$/, "", last); print last }'
)"

if [[ ! "$ASM_TASK_ID" =~ ^[0-9a-fA-F-]{36}$ ]]; then
  echo "Assemble failed: invalid task id: $ASM_TASK_ID" >&2
  exit 1
fi

(
  cd "$TASKS_DIR"
  mono Portable.RemoteTasks.Manager.exe \
    -ul "$LOGIN" \
    -up "$PASSWORD" \
    -g "$ASM_TASK_ID" \
    -r out.ptptb \
    -o "$BIN_FILE"
)

echo "=== Run scheduler ==="
mkfifo "$STREAM_FIFO"

(
  cd "$TASKS_DIR"
  script -qF /dev/null \
    env TERM=dumb mono Portable.RemoteTasks.Manager.exe \
      -ul "$LOGIN" \
      -up "$PASSWORD" \
      -ip -w \
      -s ExecuteBinaryWithIo \
      devices.xml "$WORK_DIR/devices.xml" \
      definitionFile "$TASKS_DIR/definition.target.pdsl" \
      archName zns \
      binaryFileToRun "$BIN_FILE" \
      codeRamBankName code \
      ipRegStorageName inp \
      finishMnemonicName hlt \
    </dev/null \
    > "$STREAM_FIFO"
) &
RUN_PID=$!

python3 -u - "$STREAM_FIFO" "$SOURCE_TEXT" <<'PY'
import re
import sys
from pathlib import Path

fifo_path = sys.argv[1]
source_path = sys.argv[2]
source_text = Path(source_path).read_text(encoding="utf-8")

PHASE_ID_TO_HEADER = {
    0: "FCFS5",
    1: "SRT5",
    2: "FCFS4",
    3: "SRT4",
    9: "WARM",
}
ALGO_CODE_TO_NAME = {
    0: "FCFS",
    1: "SRT",
}

def fail(message: str):
    raise SystemExit(message)

def parse_methods(text: str):
    methods = []
    current_name = None
    current_lines = []
    for line in text.splitlines():
        match = re.match(r"\s*method\s+([A-Za-z_][A-Za-z_0-9]*)\s*\(", line)
        if match:
            if current_name is not None:
                methods.append((current_name, "\n".join(current_lines)))
            current_name = match.group(1)
            current_lines = [line]
            continue
        if current_name is not None:
            current_lines.append(line)
    if current_name is not None:
        methods.append((current_name, "\n".join(current_lines)))
    return methods

def parse_phase_methods(text: str):
    phase_methods = {}
    method_order = []
    for method_name, body in parse_methods(text):
        method_order.append(method_name)
        pool_match = re.search(r"create_pool\s*\(\s*(\d+)\s*,\s*(\d+)\s*\)\s*;", body)
        if not pool_match:
            continue

        algorithm_code = int(pool_match.group(1))
        phase_id = int(pool_match.group(2))
        if algorithm_code not in ALGO_CODE_TO_NAME:
            fail(f"{method_name}: unsupported algorithm code {algorithm_code}")
        if phase_id not in PHASE_ID_TO_HEADER:
            fail(
                f"{method_name}: unsupported phase id {phase_id}; "
                "runtime and parser know only ids 0, 1, 2, 3 and 9"
            )

        tasks = []
        seen_slots = set()
        for task_match in re.finditer(
            r"create_thread\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)\s*;",
            body,
        ):
            slot, arrival, duration, workload = map(int, task_match.groups())
            if slot in seen_slots:
                fail(f"{method_name}: duplicate slot {slot}")
            if duration <= 0:
                fail(f"{method_name}: slot {slot} has non-positive duration")
            if workload < 1 or workload > 4:
                fail(f"{method_name}: slot {slot} workload must be in range 1..4")
            seen_slots.add(slot)
            tasks.append(
                {
                    "slot": slot,
                    "arrival": arrival,
                    "duration": duration,
                    "workload": workload,
                }
            )

        tasks.sort(key=lambda task: task["slot"])
        if not tasks:
            fail(f"{method_name}: phase must contain at least one create_thread(...) call")
        if len(tasks) > 20:
            fail(f"{method_name}: more than 20 tasks is not supported by the current trace format")
        expected_slots = list(range(len(tasks)))
        actual_slots = [task["slot"] for task in tasks]
        if actual_slots != expected_slots:
            fail(
                f"{method_name}: slots must be contiguous and start from 0, got {actual_slots}"
            )

        phase_methods[method_name] = {
            "method_name": method_name,
            "algorithm": ALGO_CODE_TO_NAME[algorithm_code],
            "phase_id": phase_id,
            "header_name": PHASE_ID_TO_HEADER[phase_id],
            "tasks": tasks,
        }

    return phase_methods, method_order

def phase_call_order(text: str, phase_methods, method_order):
    methods = dict(parse_methods(text))
    main_body = methods.get("main", "")
    calls = []
    for line in main_body.splitlines():
        match = re.match(r"\s*([A-Za-z_][A-Za-z_0-9]*)\s*\([^;]*\)\s*;\s*$", line)
        if match and match.group(1) in phase_methods:
            calls.append(match.group(1))
    if calls:
        return calls
    return [name for name in method_order if name in phase_methods]

phase_methods, method_order = parse_phase_methods(source_text)
call_order = phase_call_order(source_text, phase_methods, method_order)

phase_instances = []
header_counts = {}
for method_name in call_order:
    phase_def = phase_methods[method_name]
    if phase_def["phase_id"] == 9:
        continue
    header_name = phase_def["header_name"]
    header_counts[header_name] = header_counts.get(header_name, 0) + 1
    display_name = (
        header_name
        if header_counts[header_name] == 1
        else f"{header_name}#{header_counts[header_name]}"
    )
    phase_instances.append(
        {
            "display_name": display_name,
            "header_name": header_name,
            "algorithm": phase_def["algorithm"],
            "tasks": phase_def["tasks"],
        }
    )

if not phase_instances:
    fail("scheduler_program.txt does not contain any runnable scheduler phases")

display_order = [item["display_name"] for item in phase_instances]
phase_by_display = {item["display_name"]: item for item in phase_instances}
max_task_count = max(len(item["tasks"]) for item in phase_instances)
symbol_to_pid = {chr(ord("A") + i): i + 1 for i in range(max_task_count)}
trace = {name: "" for name in display_order}
phase = None
tick = 0
line = []
collecting_trace = False
done_stream = False
next_phase_index = 0

NORMAL = 0
ESC = 1
CSI = 2
OSC = 3
OSC_ESC = 4
state = NORMAL

def emit_trace_char(char: str):
    global tick
    trace[phase] += char
    tick += 1
    if char == ".":
        print(f"{phase} PID idle - TICK {tick}")
    else:
        pid = symbol_to_pid[char]
        print(f"{phase} PID {pid:02d} - TICK {tick}")

def finalize_line(text: str):
    global phase, tick, collecting_trace, done_stream, next_phase_index
    normalized = text.strip()
    if not normalized:
        return
    if normalized == "DONE":
        done_stream = True
        return

    matched_header = None
    for header_name in PHASE_ID_TO_HEADER.values():
        if normalized == header_name or normalized.startswith(header_name):
            matched_header = header_name
            break

    if matched_header is None:
        return

    if matched_header == "WARM":
        return

    while next_phase_index < len(phase_instances):
        candidate = phase_instances[next_phase_index]
        next_phase_index += 1
        if candidate["header_name"] != matched_header:
            fail(
                f"Unexpected phase header {matched_header}; "
                f"expected {candidate['header_name']}"
            )
        phase = candidate["display_name"]
        tick = 0
        collecting_trace = True
        print(f"=== {phase} ===")
        suffix = normalized[len(matched_header):]
        if suffix:
            for char in suffix:
                if char == "." or char in symbol_to_pid:
                    emit_trace_char(char)
        return

    fail(f"Unexpected extra phase header {matched_header}")

with open(fifo_path, "rb", buffering=0) as stream:
    while not done_stream:
        chunk = stream.read(1)
        if not chunk:
            break
        ch = chunk[0]

        if state == ESC:
            if ch == ord("["):
                state = CSI
                continue
            if ch == ord("]"):
                state = OSC
                continue
            state = NORMAL
            continue

        if state == CSI:
            if 0x40 <= ch <= 0x7E:
                state = NORMAL
            continue

        if state == OSC:
            if ch == 0x07:
                state = NORMAL
                continue
            if ch == 0x1B:
                state = OSC_ESC
                continue
            continue

        if state == OSC_ESC:
            if ch == ord("\\"):
                state = NORMAL
            else:
                state = OSC
            continue

        if ch == 0x1B:
            state = ESC
            continue

        if ch in (0x00, 0x04, 0x08):
            continue

        if ch == 0x0D:
            continue

        if ch == 0x0A:
            if collecting_trace:
                collecting_trace = False
            else:
                finalize_line("".join(line))
                line = []
            continue

        char = chr(ch)
        if collecting_trace and phase and (char == "." or char in symbol_to_pid):
            emit_trace_char(char)
            continue

        line.append(char)

def phase_metrics(phase_name: str):
    tasks = phase_by_display[phase_name]["tasks"]
    arrivals = [task["arrival"] for task in tasks]
    durations = [task["duration"] for task in tasks]
    ticks = trace[phase_name]
    waiting_total = 0.0
    turnaround_total = 0.0
    response_total = 0.0
    task_count = len(arrivals)

    for pid in range(1, task_count + 1):
        symbol = chr(ord("A") + pid - 1)
        first = ticks.find(symbol)
        last = ticks.rfind(symbol)
        if first == -1 or last == -1:
            raise SystemExit(f"Trace for {phase_name} is incomplete: missing PID {pid:02d}")
        arrival = arrivals[pid - 1]
        duration = durations[pid - 1]
        turnaround = (last + 1) - arrival
        waiting = turnaround - duration
        response = first - arrival
        waiting_total += waiting
        turnaround_total += turnaround
        response_total += response

    count = float(task_count)
    return (
        waiting_total / count,
        turnaround_total / count,
        response_total / count,
    )

metrics = {}
for name in display_order:
    metrics[name] = phase_metrics(name)

phase_width = max(len("Phase"), max(len(name) for name in display_order))

print("=== Metrics ===")
print(f"+-{'-' * phase_width}-+-------------+----------------+--------------+")
print(f"| {'Phase':<{phase_width}} | avg waiting | avg turnaround | avg response |")
print(f"+-{'-' * phase_width}-+-------------+----------------+--------------+")
for name in display_order:
    waiting, turnaround, response = metrics[name]
    print(f"| {name:<{phase_width}} | {waiting:11.2f} | {turnaround:14.2f} | {response:12.2f} |")
print(f"+-{'-' * phase_width}-+-------------+----------------+--------------+")

criterion = "avg waiting"
metric_index = {
    "avg waiting": 0,
    "avg turnaround": 1,
    "avg response": 2,
}[criterion]

comparison_groups = {}
algorithm_totals = {}
algorithm_samples = {}
for item in phase_instances:
    display_name = item["display_name"]
    algorithm = item["algorithm"]
    algorithm_totals[algorithm] = algorithm_totals.get(algorithm, 0.0) + metrics[display_name][metric_index]
    algorithm_samples[algorithm] = algorithm_samples.get(algorithm, 0) + 1

    match = re.match(r"^(FCFS|SRT)(.*)$", item["header_name"])
    if match:
        scenario = match.group(2) or item["header_name"]
        comparison_groups.setdefault(scenario, {})[match.group(1)] = display_name

PY

kill "$RUN_PID" 2>/dev/null || true
wait "$RUN_PID" 2>/dev/null || true
RUN_PID=""
