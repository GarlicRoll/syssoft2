[section code]

start:
    ; Set TicksSignalPeriod
    PUSH 50000000
    PUSH 0xDC38
    STORE64
    ; stack: balanced (push+push consumed by STORE64)

    ; Install handler
    PUSH handler
    PUSH 0xD840
    STORE64
    ; stack: balanced

    ; Enable QueueInterrupts
    PUSH 1
    PUSH 0xD810
    STORE8
    ; stack: balanced

    ; === fall through into handler for first run (teacher pattern) ===

handler:
    ; At handler entry SP is at same level as after start: balanced
    ; Each PUSH costs -8, each STORE8 pops 2 slots = +16, net 0

    ; Print 'A'
    PUSH 65       ; SP-8
    PUSH 0xDD00   ; SP-8
    STORE8        ; SP+16  net=0

    ; Print newline
    PUSH 10       ; SP-8
    PUSH 0xDD00   ; SP-8
    STORE8        ; SP+16  net=0

    ; Re-enable InterruptsAllowed
    PUSH 1        ; SP-8
    PUSH 0xD80C   ; SP-8
    STORE8        ; SP+16  net=0

loop:
    JMP loop