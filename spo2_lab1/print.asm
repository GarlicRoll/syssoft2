writeByte:
    LOAD_BP -2
    PUSH 0xDD00
    STORE8
    ; Context switching logic is inside asm runtime path.
    CALL threadInterruptPoint
    RET 1

print:
    LOAD_BP -2
    CALL writeByte
    RET 1
