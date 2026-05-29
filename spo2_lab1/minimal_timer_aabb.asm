[section code]

; Minimal timer-driven 2-thread demo.
; quantum=2 ticks => AABBAABB...
;
; NOTE: arithmetic/compare/jumps in this VM consume typed pairs:
; pair encoding on stack is [value, type], where type=1 for int.

start:
    ; current_tid = 0
    PUSH 0
    PUSH 0x2000
    STORE64

    ; budget_left = 2
    PUSH 2
    PUSH 0x2008
    STORE64

    ; quantum_ticks = 2
    PUSH 2
    PUSH 0x2010
    STORE64

    ; last_tick = -1
    PUSH 0
    PUSH 1
    SUB
    NIP
    PUSH 0x2018
    STORE64

    JMP thread_a

thread_a:
    ; if current_tid == 0 -> run A else go to B
    PUSH 0x2000
    LOAD64
    PUSH 1
    SWAP
    PUSH 1
    PUSH 0
    EQ
    JNZ thread_a_active
    JMP thread_b

thread_a_active:
    ; wait until clock tick changes: (clock != last_tick)
    PUSH 0xDC40
    LOAD64
    PUSH 1
    SWAP
    PUSH 0x2018
    LOAD64
    PUSH 1
    SWAP
    NEQ
    JNZ thread_a_tick
    JMP thread_a

thread_a_tick:
    ; last_tick = clock
    PUSH 0xDC40
    LOAD64
    DUP
    PUSH 0x2018
    STORE64
    DROP

    ; print 'A'
    PUSH 65
    PUSH 0xDD00
    STORE8

    ; budget_left = budget_left - 1
    PUSH 0x2008
    LOAD64
    PUSH 1
    SWAP
    PUSH 1
    PUSH 1
    SUB
    NIP
    DUP
    PUSH 0x2008
    STORE64

    ; if budget_left == 0 -> switch to B
    PUSH 1
    SWAP
    PUSH 1
    PUSH 0
    EQ
    JNZ switch_to_b
    JMP thread_a

switch_to_b:
    PUSH 1
    PUSH 0x2000
    STORE64

    PUSH 0x2010
    LOAD64
    PUSH 0x2008
    STORE64
    JMP thread_b

thread_b:
    ; if current_tid == 1 -> run B else go to A
    PUSH 0x2000
    LOAD64
    PUSH 1
    SWAP
    PUSH 1
    PUSH 1
    EQ
    JNZ thread_b_active
    JMP thread_a

thread_b_active:
    ; wait until clock tick changes: (clock != last_tick)
    PUSH 0xDC40
    LOAD64
    PUSH 1
    SWAP
    PUSH 0x2018
    LOAD64
    PUSH 1
    SWAP
    NEQ
    JNZ thread_b_tick
    JMP thread_b

thread_b_tick:
    ; last_tick = clock
    PUSH 0xDC40
    LOAD64
    DUP
    PUSH 0x2018
    STORE64
    DROP

    ; print 'B'
    PUSH 66
    PUSH 0xDD00
    STORE8

    ; budget_left = budget_left - 1
    PUSH 0x2008
    LOAD64
    PUSH 1
    SWAP
    PUSH 1
    PUSH 1
    SUB
    NIP
    DUP
    PUSH 0x2008
    STORE64

    ; if budget_left == 0 -> switch to A
    PUSH 1
    SWAP
    PUSH 1
    PUSH 0
    EQ
    JNZ switch_to_a
    JMP thread_b

switch_to_a:
    PUSH 0
    PUSH 0x2000
    STORE64

    PUSH 0x2010
    LOAD64
    PUSH 0x2008
    STORE64
    JMP thread_a
