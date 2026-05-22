
# HLVM Lab 1 (CLR / ECMA-335)

Реализация в `hlvm_lab1` строит модель программы в терминах CLR и сериализует ее в CIL (`.il`) для `ilasm`.

## Состав

- `clr_model.h/.c` — модуль формирования модели программы CLR:
  - методы,
  - параметры/локальные переменные,
  - линейная последовательность CIL-инструкций,
  - точки ветвления по CFG.
- `clr_serializer.h/.c` — модуль сериализации модели в `.il`.
- `main.c` — основная программа:
  - принимает список входных файлов,
  - использует существующие парсер/CFG-модули,
  - строит CLR-модель,
  - пишет выходной `.il`.

## Сборка

```bash
cmake -S . -B build_local
cmake --build build_local -j4 --target HLVM_LAB1_CLR
```

## Генерация IL

```bash
./build_local/HLVM_LAB1_CLR hlvm_lab1/calc.il input/calc.txt
```

## Сборка и запуск через существующую CLR VM (Mono)

```bash
ilasm /exe /output:hlvm_lab1/calc.exe hlvm_lab1/calc.il
mono hlvm_lab1/calc.exe
```

Пример проверки (fibonacci):

```bash
./build_local/HLVM_LAB1_CLR hlvm_lab1/fib.il input/fib.txt
ilasm /exe /output:hlvm_lab1/fib.exe hlvm_lab1/fib.il
printf '5\r\n-1\r\n' | mono hlvm_lab1/fib.exe
```

Ожидаемый вывод: `3`.
