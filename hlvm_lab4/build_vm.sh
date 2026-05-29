#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/hlvm_lab4"
BUILD_DIR="$ROOT_DIR/build_local"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j4 --target HLVM_LAB1_CLR

"$BUILD_DIR/HLVM_LAB1_CLR" "$OUT_DIR/vm_math.il" "$OUT_DIR/vm_math.txt"
ilasm /exe /output:"$OUT_DIR/vm_math.exe" "$OUT_DIR/vm_math.il"

echo "Generated: $OUT_DIR/vm_math.il"
echo "Generated: $OUT_DIR/vm_math.exe"
