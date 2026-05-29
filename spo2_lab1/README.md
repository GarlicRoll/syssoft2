# SPO2 Lab 1 (Variant 14: RR(2) and SRT)

Реализованы:

- два алгоритма планирования по варианту: `RR(2)` и `SRT`
- сбор метрик `avg_wait` и `avg_turnaround`
- таймерное вытеснение/переключение контекста (по аналогии с `spo2_lab1_example/sys`)

## Где что реализовано

- `scheduler.asm`:
  - `run_rr` — RR с квантом
  - `run_srt` — SRT (вытесняющий выбор минимального остатка)
  - `compute_metrics`, `scheduler_get_avg_wait_x100`, `scheduler_get_avg_turnaround_x100`
- `vm_runtime.asm`:
  - контексты потоков, переключение, обработчик тиков
  - `setupSchedulerInterrupts`
  - `threadInterruptPoint`
  - `setThreadQuantumTicks` (таймерный квант для переключения)
- `vm_two_threads.asm`:
  - тестовый сценарий варианта 14 с 20 процессами
  - печать метрик RR/SRT и выбора предпочтительного алгоритма
- `vm_two_threads_live_cycles.txt`:
  - живая демонстрация таймерного переключения между потоками
- `devices-1.xml`:
  - конфигурация устройств ВМ

## Таймерное переключение (как в sys-примере)

В `vm_runtime.asm` переключение в `vmco_interrupt_point` выполняется по правилу:

- если `current_tick - quantum_start_tick >= quantum_ticks`, выполняется переключение потока

Для этого добавлены/используются:

- `setThreadQuantumTicks(q)` — задаёт квант в тиках
- `vmco_quantum_ticks` и `vmco_quantum_start_tick` (runtime-поля)
- `threadInterruptPoint()` в рабочих циклах потоков

## Данные варианта 14

Используется 20 пар `<arrival, burst>`:

- arrivals:
  `0 7 15 21 28 37 44 52 58 65 73 80 89 95 102 110 117 123 131 140`
- bursts:
  `4 9 6 11 5 8 3 10 7 4 9 6 11 5 8 3 10 7 4 9`

Проверка условий:

- число пар: `20`
- диапазон длительностей: `3..11`
- средний интервал прибытия: `~7.37` (требование: `7.3`)

## Запуск метрик RR/SRT

```bash
python3 scripts/run_vm_two_threads.py --execute-timeout 60
```

Ожидаемые итоговые строки:

```text
RR_WAIT_X100=0305
RR_TAT_X100=1000
SRT_WAIT_X100=0190
SRT_TAT_X100=0885
PREFERRED=SRT
```

## Минимальный тест 2 потоков (таймерное переключение)

Два минимальных примера на ассемблере:

- `minimal_timer_abab.asm` — переключение на каждом тике таймера (`ABABAB...`)
- `minimal_timer_aabb.asm` — переключение через один тик (`AABBAABB...`)

Запуск:

```bash
python3 scripts/run_vm_two_threads.py --asm-file spo2_lab1/minimal_timer_abab.asm --execute-timeout 20
python3 scripts/run_vm_two_threads.py --asm-file spo2_lab1/minimal_timer_aabb.asm --execute-timeout 20
```

Оба примера используют тик таймера (`0xDC40`) как триггер вытеснения: логика переключения полностью находится в asm, а не в пользовательском `threadInterruptPoint()`.

## Минимальные программы в `.txt` (через `build/SYSSOFT`)

Добавлены два минимальных high-level примера (`.txt`):

- `minimal_timer_abab.txt` — два потока `A/B`, квант `setThreadQuantumTicks(1)`
- `minimal_timer_aabb.txt` — два потока `A/B`, квант `setThreadQuantumTicks(2)`

Сгенерировать `.asm` через твой билд можно так:

```bash
./build/SYSSOFT output_new spo2_lab1/minimal_timer_abab.txt
cp output_new/listing.osm spo2_lab1/minimal_timer_abab.generated.asm

./build/SYSSOFT output_new spo2_lab1/minimal_timer_aabb.txt
cp output_new/listing.osm spo2_lab1/minimal_timer_aabb.generated.asm
```

Или одной командой через вспомогательный скрипт:

```bash
python3 scripts/build_txt_to_asm.py spo2_lab1/minimal_timer_abab.txt spo2_lab1/minimal_timer_abab.generated.asm
python3 scripts/build_txt_to_asm.py spo2_lab1/minimal_timer_aabb.txt spo2_lab1/minimal_timer_aabb.generated.asm
```

Важный момент: `*.generated.asm` — это результат твоего frontend-компилятора (`SYSSOFT`) в стековом формате.
Они используются как промежуточный артефакт генерации. Для демонстрации таймерного переключения в этом каталоге используй готовые
`minimal_timer_abab.asm` и `minimal_timer_aabb.asm`, где логика переключения полностью находится внутри asm.

## Что показывать преподавателю

1. Таймер и контекст:
   показать `setThreadQuantumTicks`, `threadInterruptPoint`, `setupSchedulerInterrupts` в `vm_runtime.asm`.
2. Алгоритмы:
   показать `run_rr` и `run_srt` в `scheduler.asm`.
3. Данные:
   показать 20 пар и проверку ограничений варианта.
4. Результат:
   показать метрики RR/SRT и вывод `PREFERRED=SRT` по критерию минимального среднего ожидания.
5. Минимальная демонстрация таймерного вытеснения:
   запустить `minimal_timer_abab.asm` и `minimal_timer_aabb.asm`, показать разницу шаблонов вывода.

## Важная ремарка для защиты

`vm_two_threads.asm` даёт итоговое сравнение метрик RR/SRT по варианту, а `vm_two_threads_live_cycles.txt` демонстрирует именно таймерное вытеснение и переключение контекста в многопоточной среде.
