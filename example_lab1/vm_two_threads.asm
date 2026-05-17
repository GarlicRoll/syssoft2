[section code]

; Cooperative two-thread demo based on clock0 cycles polling.
; Device map used here:
;   0xDC30 - SimpleClock cycles counter
;   0xDC40 - SimpleClock CyclesSignalPeriod
;   0xDD00 - SimplePipe SyncSend
;
; RAM cells:
;   0xE100 - last observed cycles value (u64)
;   0xE108 - current thread id (u8), 0 = A, 1 = B

start:
    ; last_cycles = clock_cycles
    PUSH 0xDC30
    LOAD64
    PUSH 0xE100
    STORE64

    ; current_thread = 0 (thread A)
    PUSH 0
    PUSH 0xE108
    STORE8

scheduler_loop:
    ; if current_thread == 0 -> run A, else run B
    PUSH 1
    PUSH 0xE108
    LOAD8
    PUSH 1
    PUSH 0
    EQ
    JNZ run_thread_A
    JMP run_thread_B

run_thread_A:
    PUSH 65
    PUSH 0xDD00
    STORE8
    PUSH 10
    PUSH 0xDD00
    STORE8
    JMP maybe_switch

run_thread_B:
    PUSH 66
    PUSH 0xDD00
    STORE8
    PUSH 10
    PUSH 0xDD00
    STORE8

maybe_switch:
    ; if (clock_cycles - last_cycles) >= CyclesSignalPeriod -> switch thread
    PUSH 1
    PUSH 0xDC30
    LOAD64
    PUSH 1
    PUSH 0xE100
    LOAD64
    SUB
    PUSH 1
    PUSH 0xDC40
    LOAD64
    GTE
    JNZ do_switch
    JMP scheduler_loop

do_switch:
    ; last_cycles = clock_cycles
    PUSH 0xDC30
    LOAD64
    PUSH 0xE100
    STORE64

    ; current_thread = (current_thread == 0) ? 1 : 0
    PUSH 1
    PUSH 0xE108
    LOAD8
    PUSH 1
    PUSH 0
    EQ
    JNZ set_thread_B

set_thread_A:
    PUSH 0
    PUSH 0xE108
    STORE8
    JMP scheduler_loop

set_thread_B:
    PUSH 1
    PUSH 0xE108
    STORE8
    JMP scheduler_loop
