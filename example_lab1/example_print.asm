[section code]

start:

    PUSH 65
    CALL print

    PUSH 10
    CALL print

    HLT

print:
    LOAD_BP -2
    PUSH 0xDD00
    STORE8
    RET 1