readByte:
    push bp
    mov bp, sp
    in r7
    mov r0, r7
    mov sp, bp
    pop bp
    ret
