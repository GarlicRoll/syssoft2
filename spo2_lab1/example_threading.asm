[section code]

CALL main
POP
HLT


main:
    PUSH 0
    PUSH 0
    PUSH 0
    PUSH 0
    PUSH 56360    

    PUSH 1
    PUSH 200
    GTE
    
    JNZ print_B
    PUSH 1
    PUSH 65
    CALL print
    POP

    JMP end

print_B:
    PUSH 1
    PUSH 66
    CALL print
    POP

end:

    JMP main


print:
    PUSH 0
    PUSH 0
    LOAD_BP -2
    PUSH 56576
    STORE8
    SAVE_BP 1
    RET 1
