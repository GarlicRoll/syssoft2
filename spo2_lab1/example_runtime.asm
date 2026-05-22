runtimeInit:

    ; threads_total = 0
    PUSH 0
    PUSH 0xD000
    STORE64

    ; context_switches = 0
    PUSH 0
    PUSH 0xD008
    STORE64

    ; current_thread = 0
    PUSH 0
    PUSH 0xD010
    STORE64

    ; timer_ticks = 0
    PUSH 0
    PUSH 0xD018
    STORE64

    ; auto_switch_enabled = 0
    PUSH 0
    PUSH 0xD020
    STORE64

    ; scheduler_quantum = 1
    PUSH 1
    PUSH 0xD028
    STORE64

    RET 0

; ============================================================
; getThreadsTotal()
; ============================================================

getThreadsTotal:
    PUSH 0xD000
    LOAD64
    RET 0

; ============================================================
; getContextSwitches()
; ============================================================

getContextSwitches:
    PUSH 0xD008
    LOAD64
    RET 0

; ============================================================
; enableAutoThreadSwitching(flag)
; ============================================================

enableAutoThreadSwitching:

    LOAD_BP 65534

    PUSH 0xD020
    STORE64

    RET 1

; ============================================================
; setSchedulerQuantumOps(q)
; ============================================================

setSchedulerQuantumOps:

    LOAD_BP 65534

    PUSH 0xD028
    STORE64

    RET 1

; ============================================================
; createThread(tid, pc)
; ============================================================
;
; layout:
; [bp+16] tid
; [bp+24] entry_pc
;
; ctx = 0xD100 + tid * 40
;
; ============================================================

createThread:

    ; --------------------------------------------------------
    ; compute ctx address
    ; --------------------------------------------------------

    LOAD_BP 65534
    PUSH 40
    MUL

    PUSH 0xD100
    SUM

    ; duplicate ctx base
    DUP

    ; --------------------------------------------------------
    ; store pc
    ; --------------------------------------------------------

    LOAD_BP 65533

    SWAP
    STORE64

    ; --------------------------------------------------------
    ; ctx + 8 -> sp
    ; --------------------------------------------------------

    LOAD_BP 65534
    PUSH 40
    MUL
    PUSH 0xD100
    SUM

    PUSH 8
    SUM

    PUSH 0
    SWAP
    STORE64

    ; --------------------------------------------------------
    ; ctx + 16 -> bp
    ; --------------------------------------------------------

    LOAD_BP 65534
    PUSH 40
    MUL
    PUSH 0xD100
    SUM

    PUSH 16
    SUM

    PUSH 0
    SWAP
    STORE64

    ; --------------------------------------------------------
    ; ctx + 24 -> state = READY(1)
    ; --------------------------------------------------------

    LOAD_BP 65534
    PUSH 40
    MUL
    PUSH 0xD100
    SUM

    PUSH 24
    SUM

    PUSH 1
    SWAP
    STORE64

    ; --------------------------------------------------------
    ; increment threads_total
    ; --------------------------------------------------------

    PUSH 0xD000
    LOAD64

    PUSH 1
    SUM

    PUSH 0xD000
    STORE64

    RET 2

; ============================================================
; interruptThread(tid)
; state = 0
; ============================================================

interruptThread:

    LOAD_BP 65534
    PUSH 40
    MUL

    PUSH 0xD100
    SUM

    PUSH 24
    SUM

    PUSH 0
    SWAP
    STORE64

    RET 1

; ============================================================
; dispatchThread(prev, next)
; ============================================================

dispatchThread:

    ; context_switches++

    PUSH 0xD008
    LOAD64

    PUSH 1
    SUM

    PUSH 0xD008
    STORE64

    ; current_thread = next

    LOAD_BP 65533

    PUSH 0xD010
    STORE64

    RET 2

; ============================================================
; threadInterruptPoint()
; cooperative switch point
; ============================================================

threadInterruptPoint:

    ; if auto switching disabled -> return

    PUSH 0xD020
    LOAD64

    PUSH 0
    EQ

    JNZ threadInterruptPoint_ret

    ; timer_ticks++

    PUSH 0xD018
    LOAD64

    PUSH 1
    SUM

    PUSH 0xD018
    STORE64

threadInterruptPoint_ret:
    RET 0

; ============================================================
; getClockCycles()
; ============================================================

getClockCycles:
    PUSH 0xD018
    LOAD64
    RET 0

; ============================================================
; getClockSeconds()
; fake implementation
; ============================================================

getClockSeconds:

    PUSH 0xD018
    LOAD64

    PUSH 1000
    DIV

    RET 0

; ============================================================
; exitThread()
; ============================================================

exitThread:

exitThread_loop:
    JMP exitThread_loop

; ============================================================
; startThreadScheduler()
; ============================================================

startThreadScheduler:

scheduler_loop:

    CALL threadInterruptPoint

    JMP scheduler_loop

; ============================================================
; setupSchedulerInterrupts()
; ============================================================

setupSchedulerInterrupts:
    RET 0

; ============================================================
; setupEntropyTimer(period)
; ============================================================

setupEntropyTimer:
    RET 1

; ============================================================
; getEntropyTicks()
; ============================================================

getEntropyTicks:
    PUSH 0xD018
    LOAD64
    RET 0

; ============================================================
; createThreadWithTick(tid, pc, quantum)
; ============================================================

createThreadWithTick:

    ; internally reuse createThread

    LOAD_BP 65534
    LOAD_BP 65533

    CALL createThread

    ; ignore quantum for simplified runtime

    RET 3

; ============================================================
; spawnThread(tid, entry)
; ============================================================

spawnThread:

    LOAD_BP 65534
    LOAD_BP 65533

    CALL createThread

    RET 2

; ============================================================
; threadingInit()
; ============================================================

threadingInit:
    RET 0

; ============================================================
; setThreadQuantumOps(q)
; ============================================================

setThreadQuantumOps:
    RET 1

; ============================================================
; setSchedulerDynamicQuantum(flag)
; ============================================================

setSchedulerDynamicQuantum:
    RET 1

; ============================================================
; onThreadInterrupted()
; ============================================================

onThreadInterrupted:

    PUSH 0xD018
    LOAD64

    PUSH 1
    SUM

    PUSH 0xD018
    STORE64

    RET 0

; ============================================================
; runAutoScheduler()
; ============================================================

runAutoScheduler:

runAutoScheduler_loop:

    CALL threadInterruptPoint

    JMP runAutoScheduler_loop

; ============================================================
; VM HALT
; ============================================================

halt_forever:
    HLT
    JMP halt_forever
