#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/hlvm_lab4"

cc -shared -fPIC \
  "$OUT_DIR/php_vm_bridge.c" \
  -o "$OUT_DIR/libhlvm4_bridge.so" \
  $(pkg-config --cflags --libs mono-2)

echo "Generated: $OUT_DIR/libhlvm4_bridge.so"
