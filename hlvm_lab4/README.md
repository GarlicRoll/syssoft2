# HLVM Lab 4 (PHP + FFI + CLR VM)

Лаба 4: консольное приложение на PHP, вызывающее подпрограммы, сгенерированные вашим комплексом из лабы 1 (CLR/ECMA-335).

## Идея решения

PHP FFI умеет вызывать C ABI, но не CLR-методы напрямую. Поэтому добавлен промежуточный слой:

1. `vm_math.txt` -> (`HLVM_LAB1_CLR`) -> `vm_math.il` -> (`ilasm`) -> `vm_math.exe`
2. C shared library `libhlvm4_bridge.so` (Mono embedding API) загружает `vm_math.exe` и вызывает методы.
3. PHP через `FFI::cdef(...)` вызывает функции моста.

## Файлы

- `vm_math.txt` — подпрограммы на вашем входном языке (реализованы в рамках комплекса лабы 1)
- `php_vm_bridge.h/.c` — C-библиотека-мост (Mono runtime embedding)
- `app.php` — консольное PHP приложение
- `build_vm.sh` — генерация `vm_math.il` и `vm_math.exe`
- `build_bridge.sh` — сборка `libhlvm4_bridge.so`

## Сборка VM-подпрограмм

```bash
./hlvm_lab4/build_vm.sh
```

Скрипт использует цель `HLVM_LAB1_CLR` и `ilasm`.

## Сборка C bridge

```bash
./hlvm_lab4/build_bridge.sh
```

Альтернатива через CMake target:

```bash
cmake -S . -B build_local
cmake --build build_local -j4 --target HLVM_LAB4_BRIDGE
cp build_local/libhlvm4_bridge.so hlvm_lab4/libhlvm4_bridge.so
```

## Запуск PHP-приложения

```bash
php -d ffi.enable=1 hlvm_lab4/app.php
```

Быстрая неинтерактивная проверка:

```bash
printf "demo\nexit\n" | php -d ffi.enable=1 hlvm_lab4/app.php
```

Приложение запускается в интерактивном режиме:

- `op> add2` / `sub2` / `mul2` / `max2` (запросит `a` и `b`)
- `op> abs1` (запросит `x`)
- `op> demo` (покажет демонстрационный набор вызовов)
- `op> exit`

Ожидаемые значения в `demo`:

- `add2(7, 5) = 12`
- `sub2(7, 5) = 2`
- `mul2(7, 5) = 35`
- `max2(7, 5) = 7`
- `abs1(-42) = 42`

## Что изучено по FFI

1. PHP FFI (`FFI::cdef`, указатели, C-типы в PHP).
2. FFI на стороне высокоуровневой VM: Mono embedding API (`mono_jit_init_version`, `mono_domain_assembly_open`, `mono_runtime_invoke`).

## Источники

- PHP Manual: FFI — https://www.php.net/manual/en/book.ffi.php
- Mono Embedding — https://www.mono-project.com/docs/advanced/embedding/
- ECMA-335 overview (CLI/CIL context) — https://www.ecma-international.org/publications-and-standards/standards/ecma-335/
