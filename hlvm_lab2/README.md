# HLVM Lab 2 (CLR / ECMA-335)

Лабораторная 2 расширяет `hlvm_lab1` поддержкой:

- `first-order function type` (объявления `dim f as func(...) -> ...`),
- `local functions` (вложенные `function ... end function`),
- `closures` (захват переменных внешней функции).

## Как реализовано

В `hlvm_lab2/source_transformer.c` добавлен этап трансформации исходного текста перед существующим пайплайном 1 семестра (`parse -> CFG -> CLR model -> IL`):

1. **Lambda lifting**:
   - локальные функции выносятся на верхний уровень,
   - имя преобразуется в `Outer__Local`.

2. **Closure conversion**:
   - свободные переменные локальной функции вычисляются как захваты,
   - захваты добавляются как первые параметры в поднятую функцию,
   - при присваивании функции в переменную сохраняются `fnid + captured values`.

3. **Defunctionalization / dispatch**:
   - вызов `f(args)` для функциональной переменной преобразуется в условный dispatch по `f__fnid`.

## Сборка

```bash
cmake -S . -B build_local
cmake --build build_local -j4 --target HLVM_LAB2_CLR
```

## Использование

```bash
./build_local/HLVM_LAB2_CLR hlvm_lab2/out.il <input1> [input2 ...]
ilasm /exe /output:hlvm_lab2/out.exe hlvm_lab2/out.il
mono hlvm_lab2/out.exe
```

## Тесты

### 1) Closure + local function + function type

Файл: `hlvm_lab2/closure_demo.txt`

```bash
./build_local/HLVM_LAB2_CLR hlvm_lab2/closure_demo.il hlvm_lab2/closure_demo.txt
ilasm /exe /output:hlvm_lab2/closure_demo.exe hlvm_lab2/closure_demo.il
mono hlvm_lab2/closure_demo.exe
```

Ожидаемый вывод: `9`

### 2) Переприсваивание функциональной переменной (dispatch)

Файл: `hlvm_lab2/closure_dispatch_demo.txt`

```bash
./build_local/HLVM_LAB2_CLR hlvm_lab2/closure_dispatch_demo.il hlvm_lab2/closure_dispatch_demo.txt
ilasm /exe /output:hlvm_lab2/closure_dispatch_demo.exe hlvm_lab2/closure_dispatch_demo.il
mono hlvm_lab2/closure_dispatch_demo.exe
```

Ожидаемый вывод: `78`

## Ограничения текущей версии

- Поддерживаются локальные функции в теле функции (основной сценарий задания).
- Вызовы функциональных переменных поддержаны для форм:
  - `x = f(a, b);`
  - `f(a, b);`
- Поддержка сделана для first-order сценариев (без передачи функциональных значений через сложные выражения).
