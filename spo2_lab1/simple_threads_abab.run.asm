[section code]
CALL main
POP
HLT
print_a:
PUSH 0
PUSH 0
label2:
PUSH 1
PUSH 1
JNZ label1
SAVE_BP 1
RET 1
label1:
PUSH 1
PUSH 65
CALL print
JMP label2
print_b:
PUSH 0
PUSH 0
label4:
PUSH 1
PUSH 1
JNZ label3
SAVE_BP 1
RET 1
label3:
PUSH 1
PUSH 66
CALL print
JMP label4
main:
PUSH 0
PUSH 0
PUSH 0
PUSH 0
PUSH 0
PUSH 0
PUSH 0
PUSH 0
PUSH 0
PUSH 0
PUSH 0
PUSH 0
CALL threadingInit
PUSH 1
PUSH 1
CALL setThreadQuantumOps
SAVE_BP 3
PUSH 1
PUSH 2
CALL setThreadQuantumTicks
SAVE_BP 5
PUSH 1
PUSH 1
CALL setupEntropyTimer
SAVE_BP 7
PUSH 1
PUSH 0
LOAD_BP 9
CALL spawnThread
PUSH 1
PUSH 1
LOAD_BP 11
CALL spawnThread
label6:
PUSH 1
PUSH 1
JNZ label5
SAVE_BP 1
RET 0
label5:
PUSH 1
PUSH 0
JMP label6
; VM runtime helpers for scheduler lab (variant 13)
; Context table layout (entry size = 48 bytes):
;   +0  pc
;   +8  acc
;   +16 sp
;   +24 state
;   +32 remaining
;   +40 dispatch_count

; Global runtime memory (RAM):
;   0xD000 threads_total
;   0xD008 context_switches
;   0xD010 current_thread
;   0xD018 timer_ticks
;   0xD020 last_restored_pc
;   0xD028 last_restored_acc
;   0xD030 last_restored_remaining
;   0xD038 last_restored_state
;   0xD040 auto_switch_enabled
;   0xD048 scheduler_next_tid_tmp
;   0xD050 scheduler_quantum_ops (global RR quantum in operations)
;   0xD058 scheduler_dynamic_quantum (0/1)
;   0xD060 scheduler_seed
;   0xD4C0 thread_quantum_base  (8 bytes * 20 entries)
;   0xD560 thread_quantum_left  (8 bytes * 20 entries)
;   0xD100 contexts base (48 bytes per thread)

; Device map (devices_scheduler_noirq.xml):
;   PIC state base      0xD800
;   PIC handlers base   0xD840
;   CLOCK state base    0xDC00
; Common offsets (from device docs / example):
;   PIC queue enable    +0x10 => 0xD810
;   PIC queue raise     +0x0C => 0xD80C
;   CLOCK seconds       +0x28 => 0xDC28
;   CLOCK ticks period  +0x38 => 0xDC38

; High-level API wrappers (no vmrt_* in test sources)
runtimeInit:
    push bp
    mov bp, sp

    call vmrt_init

    mov sp, bp
    pop bp
    ret

threadingInit:
    push bp
    mov bp, sp

    call vmco_init

    mov sp, bp
    pop bp
    ret

setThreadQuantumOps:
    push bp
    mov bp, sp

    ; arg0: operations per quantum
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r0, [r6]
    push r0
    call vmco_set_quantum
    ldc r1, 8
    add sp, r1

    mov sp, bp
    pop bp
    ret

setThreadQuantumTicks:
    push bp
    mov bp, sp

    ; arg0: timer ticks per quantum
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r0, [r6]
    push r0
    call vmco_set_quantum_ticks
    ldc r1, 8
    add sp, r1

    mov sp, bp
    pop bp
    ret

spawnThread:
    push bp
    mov bp, sp

    ; spawnThread(tid, entryPc)
    ; vmco_create_thread(tid, entryPc)
    mov r6, bp
    ldc r1, 24
    add r6, r1
    mov r0, [r6]
    push r0

    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r0, [r6]
    push r0

    call vmco_create_thread
    ldc r1, 16
    add sp, r1

    mov sp, bp
    pop bp
    ret

threadInterruptPoint:
    ; cooperative preemption point
    jmp vmco_interrupt_point

startThreadScheduler:
    ; never returns in normal mode
    jmp vmco_start

setSchedulerQuantumOps:
    push bp
    mov bp, sp

    ; arg0: operations per quantum
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r0, [r6]
    push r0
    call vmrt_set_scheduler_quantum_ops
    ldc r1, 8
    add sp, r1

    mov sp, bp
    pop bp
    ret

setSchedulerDynamicQuantum:
    push bp
    mov bp, sp

    ; arg0: enabled (0/1)
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r0, [r6]
    push r0
    call vmrt_set_dynamic_quantum
    ldc r1, 8
    add sp, r1

    mov sp, bp
    pop bp
    ret

createThread:
    push bp
    mov bp, sp

    ; createThread(tid, worker)
    ; flattened params:
    ; [bp+16] tid
    ; [bp+24] worker.value
    ; [bp+32] worker.methodId

    ; vmrt_create_thread(tid, startPc=methodId, startAcc=value)
    mov r6, bp
    ldc r1, 24
    add r6, r1
    mov r0, [r6]
    push r0

    mov r6, bp
    ldc r1, 32
    add r6, r1
    mov r0, [r6]
    push r0

    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r0, [r6]
    push r0

    call vmrt_create_thread
    ldc r1, 24
    add sp, r1

    call vmrt_get_threads_total

    mov sp, bp
    pop bp
    ret

createThreadWithTick:
    push bp
    mov bp, sp

    ; createThreadWithTick(tid, worker, quantumTicks)
    ; flattened params:
    ; [bp+16] tid
    ; [bp+24] worker.value
    ; [bp+32] worker.methodId
    ; [bp+40] quantumTicks

    ; vmrt_create_thread(tid, startPc=methodId, startAcc=value)
    mov r6, bp
    ldc r1, 24
    add r6, r1
    mov r0, [r6]
    push r0

    mov r6, bp
    ldc r1, 32
    add r6, r1
    mov r0, [r6]
    push r0

    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r0, [r6]
    push r0

    call vmrt_create_thread
    ldc r1, 24
    add sp, r1

    ; vmrt_set_thread_quantum(tid, quantumTicks)
    mov r6, bp
    ldc r1, 40
    add r6, r1
    mov r0, [r6]
    push r0

    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r0, [r6]
    push r0

    call vmrt_set_thread_quantum
    ldc r1, 16
    add sp, r1

    call vmrt_get_threads_total

    mov sp, bp
    pop bp
    ret

interruptThread:
    push bp
    mov bp, sp

    ; interruptThread(tid): mark context.state = 0
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r2, [r6]
    ldc r1, 20
    mod r2, r1

    ; context_ptr = 0xD100 + tid * 48 + 24
    mov r3, r2
    ldc r1, 48
    mul r3, r1
    ldc r6, 53504
    add r6, r3
    ldc r1, 24
    add r6, r1

    ldc r0, 0
    mov [r6], r0

    ldc r0, 1
    mov sp, bp
    pop bp
    ret

dispatchThread:
    push bp
    mov bp, sp

    ; dispatchThread(prevTid, nextTid)
    ; [bp+16] prev
    ; [bp+24] next

    ; vmrt_mark_dispatch(next)
    mov r6, bp
    ldc r1, 24
    add r6, r1
    mov r0, [r6]
    push r0
    call vmrt_mark_dispatch
    ldc r1, 8
    add sp, r1

    ; vmrt_switch_context(prev, next)
    mov r6, bp
    ldc r1, 24
    add r6, r1
    mov r0, [r6]
    push r0

    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r0, [r6]
    push r0

    call vmrt_switch_context
    ldc r1, 16
    add sp, r1
    mov r4, r0

    ; vmrt_restore_context(next)
    mov r6, bp
    ldc r1, 24
    add r6, r1
    mov r0, [r6]
    push r0
    call vmrt_restore_context
    ldc r1, 8
    add sp, r1

    ; return context_switches captured before restore return value overwrite
    mov r0, r4

    mov sp, bp
    pop bp
    ret

getThreadsTotal:
    push bp
    mov bp, sp

    call vmrt_get_threads_total

    mov sp, bp
    pop bp
    ret

getContextSwitches:
    push bp
    mov bp, sp

    call vmrt_get_context_switches

    mov sp, bp
    pop bp
    ret

getClockSeconds:
    push bp
    mov bp, sp

    call vmrt_get_clock_seconds

    mov sp, bp
    pop bp
    ret

getClockCycles:
    push bp
    mov bp, sp

    call vmrt_get_clock_cycles

    mov sp, bp
    pop bp
    ret

setupEntropyTimer:
    push bp
    mov bp, sp

    ; arg0: period
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r0, [r6]
    push r0
    call vmrt_timer_set_period
    ldc r1, 8
    add sp, r1

    call vmrt_install_default_tick_handler
    call vmrt_enable_irq_queue
    call vmrt_raise_queued_irq

    ldc r0, 1
    mov sp, bp
    pop bp
    ret

setupSchedulerInterrupts:
    push bp
    mov bp, sp

    ; Uses timer/clock period from devices XML (e.g., CyclesSignalPeriod).
    ; Only wires handler + IRQ queueing on VM side.
    call vmrt_install_default_tick_handler
    call vmrt_enable_irq_queue
    call vmrt_raise_queued_irq

    ldc r0, 1
    mov sp, bp
    pop bp
    ret

getEntropyTicks:
    push bp
    mov bp, sp

    call vmrt_get_ticks

    mov sp, bp
    pop bp
    ret

enableAutoThreadSwitching:
    push bp
    mov bp, sp

    ; arg0: enabled
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r0, [r6]
    push r0
    call vmrt_set_auto_switch
    ldc r1, 8
    add sp, r1

    ldc r0, 1
    mov sp, bp
    pop bp
    ret

exitThread:
    ; Friend-style API: transfer control to scheduler loop.
    ; In this VM model there is no host thread exit primitive,
    ; so this never-returning loop acts as "current thread finished".
    jmp runAutoScheduler

onThreadInterrupted:
    push bp
    mov bp, sp

    call vmrt_timer_handler
    call vmrt_get_ticks

    mov sp, bp
    pop bp
    ret

runAutoScheduler:
    push bp
    mov bp, sp

runAutoScheduler_loop:
    call vmrt_timer_handler
    jmp runAutoScheduler_loop

vmrt_init:
    push bp
    mov bp, sp

    ldc r0, 0

    ldc r6, 53248
    mov [r6], r0

    ldc r6, 53256
    mov [r6], r0

    ldc r6, 53272
    mov [r6], r0

    ldc r6, 53280
    mov [r6], r0

    ldc r6, 53288
    mov [r6], r0

    ldc r6, 53296
    mov [r6], r0

    ldc r6, 53304
    mov [r6], r0

    ldc r6, 53312
    mov [r6], r0

    ldc r6, 53320
    mov [r6], r0

    ; scheduler_quantum_ops = 1
    ldc r6, 53328
    ldc r0, 1
    mov [r6], r0

    ; scheduler_dynamic_quantum = 0
    ldc r6, 53336
    ldc r0, 0
    mov [r6], r0

    ; scheduler_seed = 1
    ldc r6, 53344
    ldc r0, 1
    mov [r6], r0

    ; current_thread = 20 (none selected)
    ldc r6, 53264
    ldc r0, 20
    mov [r6], r0

    ldc r0, 1
    mov sp, bp
    pop bp
    ret

vmrt_timer_set_period:
    push bp
    mov bp, sp

    ; arg0: period at [bp+16]
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r0, [r6]

    ldc r6, 56376
    mov [r6], r0

    mov sp, bp
    pop bp
    ret

vmrt_install_default_tick_handler:
    push bp
    mov bp, sp

    ; handlers-map entry 0
    ldc r6, 55360
    ldc r0, vmrt_timer_handler
    mov [r6], r0

    ldc r0, 1
    mov sp, bp
    pop bp
    ret

vmrt_enable_irq_queue:
    push bp
    mov bp, sp

    ldc r6, 55312
    ldc r0, 1
    mov [r6], r0

    ldc r0, 1
    mov sp, bp
    pop bp
    ret

vmrt_raise_queued_irq:
    push bp
    mov bp, sp

    ldc r6, 55308
    ldc r0, 1
    mov [r6], r0

    ldc r0, 1
    mov sp, bp
    pop bp
    ret

vmrt_get_clock_seconds:
    push bp
    mov bp, sp

    ldc r6, 56360
    mov r0, [r6]

    mov sp, bp
    pop bp
    ret

vmrt_get_clock_cycles:
    push bp
    mov bp, sp

    ; CLOCK cycles register (state base + 0x40)
    ldc r6, 56384
    mov r0, [r6]

    mov sp, bp
    pop bp
    ret

vmrt_create_thread:
    push bp
    mov bp, sp

    ; arg0: thread_id
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r2, [r6]
    ldc r1, 20
    mod r2, r1

    ; arg1: start_pc
    mov r6, bp
    ldc r1, 24
    add r6, r1
    mov r4, [r6]

    ; arg2: start_acc
    mov r6, bp
    ldc r1, 32
    add r6, r1
    mov r5, [r6]

    ; context_ptr = 0xD100 + thread_id * 48
    mov r3, r2
    ldc r1, 48
    mul r3, r1
    ldc r6, 53504
    add r6, r3

    ; +0  pc
    mov [r6], r4

    ; +8  acc
    ldc r1, 8
    add r6, r1
    mov [r6], r5

    ; +16 sp (initially 0)
    ldc r1, 8
    add r6, r1
    ldc r0, 0
    mov [r6], r0

    ; +24 state (READY=1)
    ldc r1, 8
    add r6, r1
    ldc r0, 1
    mov [r6], r0

    ; +32 remaining (will be refreshed by vmrt_save_context)
    ldc r1, 8
    add r6, r1
    ldc r0, 0
    mov [r6], r0

    ; +40 dispatch_count
    ldc r1, 8
    add r6, r1
    ldc r0, 0
    mov [r6], r0

    ; default quantum_base[tid] = scheduler_quantum_ops
    mov r3, r2
    ldc r1, 8
    mul r3, r1

    ldc r6, 53328
    mov r0, [r6]

    ldc r6, 54464
    add r6, r3
    mov [r6], r0

    ldc r6, 54624
    add r6, r3
    mov [r6], r0

    ; threads_total++
    ldc r6, 53248
    mov r0, [r6]
    ldc r1, 1
    add r0, r1
    mov [r6], r0

    mov sp, bp
    pop bp
    ret

vmrt_mark_dispatch:
    push bp
    mov bp, sp

    ; arg0: thread_id
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r2, [r6]
    ldc r1, 20
    mod r2, r1

    ; context_ptr = 0xD100 + thread_id * 48 + 40
    mov r3, r2
    ldc r1, 48
    mul r3, r1
    ldc r6, 53504
    add r6, r3
    ldc r1, 40
    add r6, r1

    mov r0, [r6]
    ldc r1, 1
    add r0, r1
    mov [r6], r0

    mov sp, bp
    pop bp
    ret

vmrt_save_context:
    push bp
    mov bp, sp

    ; arg0: thread_id
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r2, [r6]
    ldc r1, 20
    mod r2, r1

    ; arg1: pc
    mov r6, bp
    ldc r1, 24
    add r6, r1
    mov r4, [r6]

    ; arg2: acc
    mov r6, bp
    ldc r1, 32
    add r6, r1
    mov r5, [r6]

    ; arg3: remaining
    mov r6, bp
    ldc r1, 40
    add r6, r1
    mov r7, [r6]

    ; context_ptr = 0xD100 + thread_id * 48
    mov r3, r2
    ldc r1, 48
    mul r3, r1
    ldc r6, 53504
    add r6, r3

    ; +0  pc
    mov [r6], r4

    ; +8  acc
    ldc r1, 8
    add r6, r1
    mov [r6], r5

    ; +16 sp
    ldc r1, 8
    add r6, r1
    mov r0, sp
    mov [r6], r0

    ; +24 state
    ldc r1, 8
    add r6, r1
    mov r2, bp
    ldc r1, 48
    add r2, r1
    mov r0, [r2]
    mov [r6], r0

    ; +32 remaining
    ldc r1, 8
    add r6, r1
    mov [r6], r7

    ldc r0, 1
    mov sp, bp
    pop bp
    ret

vmrt_restore_context:
    push bp
    mov bp, sp

    ; arg0: thread_id
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r3, [r6]
    ldc r1, 20
    mod r3, r1

    ; context_ptr = 0xD100 + thread_id * 48
    mov r2, r3
    ldc r1, 48
    mul r2, r1
    ldc r6, 53504
    add r6, r2

    ; +0  pc
    mov r4, [r6]

    ; +8  acc
    ldc r1, 8
    add r6, r1
    mov r5, [r6]

    ; +16 sp (ignored)
    ldc r1, 8
    add r6, r1

    ; +24 state
    ldc r1, 8
    add r6, r1
    mov r2, [r6]

    ; +32 remaining
    ldc r1, 8
    add r6, r1
    mov r7, [r6]

    ; publish restored context to globals
    ldc r6, 53280
    mov [r6], r4

    ldc r6, 53288
    mov [r6], r5

    ldc r6, 53296
    mov [r6], r7

    ldc r6, 53304
    mov [r6], r2

    ldc r6, 53264
    mov [r6], r3

    ldc r0, 1

    mov sp, bp
    pop bp
    ret

vmrt_switch_context:
    push bp
    mov bp, sp

    ; arg0: prev_tid
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r2, [r6]
    ldc r1, 20
    mod r2, r1

    ; arg1: next_tid
    mov r6, bp
    ldc r1, 24
    add r6, r1
    mov r3, [r6]
    ldc r1, 20
    mod r3, r1

    cmp r2, r3
    je vmrt_switch_noinc

    ; context_switches++
    ldc r6, 53256
    mov r0, [r6]
    ldc r1, 1
    add r0, r1
    mov [r6], r0

vmrt_switch_noinc:
    ; current_thread = next_tid
    ldc r6, 53264
    mov [r6], r3

    ; return current context_switches
    ldc r6, 53256
    mov r0, [r6]

    mov sp, bp
    pop bp
    ret

vmrt_get_context_switches:
    push bp
    mov bp, sp

    ldc r6, 53256
    mov r0, [r6]

    mov sp, bp
    pop bp
    ret

vmrt_get_threads_total:
    push bp
    mov bp, sp

    ldc r6, 53248
    mov r0, [r6]

    mov sp, bp
    pop bp
    ret

vmrt_get_ticks:
    push bp
    mov bp, sp

    ldc r6, 53272
    mov r0, [r6]

    mov sp, bp
    pop bp
    ret

vmrt_set_thread_quantum:
    push bp
    mov bp, sp

    ; arg0: thread_id
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r2, [r6]
    ldc r1, 20
    mod r2, r1

    ; arg1: quantum_ticks
    mov r6, bp
    ldc r1, 24
    add r6, r1
    mov r0, [r6]

    ; clamp to >= 1
    ldc r1, 1
    cmp r0, r1
    jg vmrt_set_quantum_ok
    je vmrt_set_quantum_ok
    ldc r0, 1

vmrt_set_quantum_ok:
    mov r3, r2
    ldc r1, 8
    mul r3, r1

    ; quantum_base[tid]
    ldc r6, 54464
    add r6, r3
    mov [r6], r0

    ; quantum_left[tid]
    ldc r6, 54624
    add r6, r3
    mov [r6], r0

    ldc r0, 1
    mov sp, bp
    pop bp
    ret

vmrt_set_scheduler_quantum_ops:
    push bp
    mov bp, sp

    ; arg0: quantum operations
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r0, [r6]

    ; clamp to >= 1
    ldc r1, 1
    cmp r0, r1
    jg vmrt_set_sched_q_ok
    je vmrt_set_sched_q_ok
    ldc r0, 1

vmrt_set_sched_q_ok:
    ; persist global scheduler quantum
    ldc r6, 53328
    mov [r6], r0

    ; apply to all runtime thread slots (0..19)
    ldc r2, 0

vmrt_set_sched_q_loop:
    ldc r1, 20
    cmp r2, r1
    jl vmrt_set_sched_q_body
    jmp vmrt_set_sched_q_done

vmrt_set_sched_q_body:
    mov r3, r2
    ldc r1, 8
    mul r3, r1

    ldc r6, 54464
    add r6, r3
    mov [r6], r0

    ldc r6, 54624
    add r6, r3
    mov [r6], r0

    ldc r1, 1
    add r2, r1
    jmp vmrt_set_sched_q_loop

vmrt_set_sched_q_done:
    ldc r0, 1
    mov sp, bp
    pop bp
    ret

vmrt_set_dynamic_quantum:
    push bp
    mov bp, sp

    ; arg0: enabled
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r0, [r6]

    ; normalize: <=0 -> 0, >0 -> 1
    ldc r1, 1
    cmp r0, r1
    jl vmrt_set_dyn_off
    ldc r0, 1
    jmp vmrt_set_dyn_store

vmrt_set_dyn_off:
    ldc r0, 0

vmrt_set_dyn_store:
    ldc r6, 53336
    mov [r6], r0

    ldc r0, 1
    mov sp, bp
    pop bp
    ret

vmrt_reload_quantum_left:
    push bp
    mov bp, sp

    ; arg0: thread_id
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r2, [r6]
    ldc r1, 20
    mod r2, r1

    ; offset = tid * 8
    mov r3, r2
    ldc r1, 8
    mul r3, r1

    ; base = quantum_base[tid]
    ldc r6, 54464
    add r6, r3
    mov r4, [r6]

    ; clamp base >= 1
    ldc r1, 1
    cmp r4, r1
    jg vmrt_q_base_ok
    je vmrt_q_base_ok
    ldc r4, 1

vmrt_q_base_ok:
    ; dynamic mode?
    ldc r6, 53336
    mov r5, [r6]
    ldc r1, 1
    cmp r5, r1
    jl vmrt_q_static

    ; seed = seed * 1664525 + 1013904223 + ticks*131 + switches*17
    ldc r6, 53344
    mov r0, [r6]

    ldc r1, 1664525
    mul r0, r1
    ldc r1, 1013904223
    add r0, r1

    ldc r6, 53272
    mov r5, [r6]
    ldc r1, 131
    mul r5, r1
    add r0, r5

    ldc r6, 53256
    mov r5, [r6]
    ldc r1, 17
    mul r5, r1
    add r0, r5

    ; absolute value to avoid negative modulo artifacts
    ldc r1, 0
    cmp r0, r1
    jge vmrt_q_seed_store
    ldc r1, 0
    sub r1, r0
    mov r0, r1

vmrt_q_seed_store:
    ldc r6, 53344
    mov [r6], r0

    ; q = base + (seed % base)  -> [base .. (2*base-1)]
    mov r5, r0
    mod r5, r4
    mov r0, r4
    add r0, r5
    jmp vmrt_q_store

vmrt_q_static:
    mov r0, r4

vmrt_q_store:
    ; quantum_left[tid] = q
    ldc r6, 54624
    add r6, r3
    mov [r6], r0

    mov sp, bp
    pop bp
    ret

vmrt_set_auto_switch:
    push bp
    mov bp, sp

    ; arg0: enabled
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r0, [r6]

    ldc r6, 53312
    mov [r6], r0

    ldc r0, 1
    mov sp, bp
    pop bp
    ret

vmrt_pipe_emit_thread_value:
    push bp
    mov bp, sp

    ; arg0: value (111/222)
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r0, [r6]

    ldc r6, 56576
    ldc r1, 111
    cmp r0, r1
    jne vmrt_pipe_emit_222

    ldc r7, 49
    out r7
    mov [r6], r7
    out r7
    mov [r6], r7
    out r7
    mov [r6], r7
    ldc r7, 10
    out r7
    mov [r6], r7
    jmp vmrt_pipe_emit_done

vmrt_pipe_emit_222:
    ldc r7, 50
    out r7
    mov [r6], r7
    out r7
    mov [r6], r7
    out r7
    mov [r6], r7
    ldc r7, 10
    out r7
    mov [r6], r7

vmrt_pipe_emit_done:
    ldc r0, 1
    mov sp, bp
    pop bp
    ret

vmrt_timer_handler:
    ; Track timer ticks.
    ldc r6, 53272
    mov r0, [r6]
    ldc r1, 1
    add r0, r1
    mov [r6], r0

    ; auto_switch_enabled?
    ldc r6, 53312
    mov r0, [r6]
    ldc r1, 1
    cmp r0, r1
    jl vmrt_timer_ack

    ; Require at least 2 active thread contexts.
    ldc r6, 53248
    mov r0, [r6]
    ldc r1, 2
    cmp r0, r1
    jl vmrt_timer_ack

    ; current = current_thread
    ldc r6, 53264
    mov r2, [r6]

    ; If no thread is running yet, start from tid=0.
    ldc r1, 20
    cmp r2, r1
    jne vmrt_timer_have_current

    ldc r3, 0
    push r3
    call vmrt_mark_dispatch
    ldc r1, 8
    add sp, r1

    ldc r3, 0
    push r3
    push r2
    call vmrt_switch_context
    ldc r1, 16
    add sp, r1

    ldc r3, 0
    push r3
    call vmrt_restore_context
    ldc r1, 8
    add sp, r1

    ; reload quantum_left for tid=0
    ldc r3, 0
    push r3
    call vmrt_reload_quantum_left
    ldc r1, 8
    add sp, r1

    jmp vmrt_timer_emit_current

vmrt_timer_have_current:
    ; left = quantum_left[current]
    mov r3, r2
    ldc r1, 8
    mul r3, r1
    ldc r6, 54624
    add r6, r3
    mov r4, [r6]

    ; left--
    ldc r1, 1
    sub r4, r1
    mov [r6], r4

    ; continue current while left >= 1
    ldc r1, 1
    cmp r4, r1
    jl vmrt_timer_switch
    jmp vmrt_timer_emit_current

vmrt_timer_switch:
    ; Round-robin for two threads: next = (current == 0) ? 1 : 0
    ldc r3, 0
    cmp r2, r3
    jne vmrt_timer_switch_to_zero
    ldc r3, 1
    jmp vmrt_timer_switch_have_next

vmrt_timer_switch_to_zero:
    ldc r3, 0

vmrt_timer_switch_have_next:
    ; save next_tid because calls may clobber registers
    ldc r6, 53320
    mov [r6], r3

    ; mark dispatch(next)
    ldc r6, 53320
    mov r3, [r6]
    push r3
    call vmrt_mark_dispatch
    ldc r1, 8
    add sp, r1

    ; switch(prev, next)
    ldc r6, 53320
    mov r3, [r6]
    push r3
    push r2
    call vmrt_switch_context
    ldc r1, 16
    add sp, r1

    ; restore(next)
    ldc r6, 53320
    mov r3, [r6]
    push r3
    call vmrt_restore_context
    ldc r1, 8
    add sp, r1

    ; reload quantum_left for selected next thread
    ldc r6, 53320
    mov r3, [r6]
    push r3
    call vmrt_reload_quantum_left
    ldc r1, 8
    add sp, r1

vmrt_timer_emit_current:
    ; emit current thread value through pipe
    ldc r6, 53288
    mov r0, [r6]
    push r0
    call vmrt_pipe_emit_thread_value
    ldc r1, 8
    add sp, r1

vmrt_timer_ack:
    ; acknowledge queued interrupt signals
    ldc r6, 55308
    ldc r0, 1
    mov [r6], r0

    ret

; ------------------------------------------------------------
; Cooperative thread runtime for real while-loop execution
; (switching by operation points, not by explicit user calls in loops)
;
; Memory map:
;   0xCAE8 vmco_last_tick_seen
;   0xCAF0 vmco_tick_ready
;   0xCB00 vmco_enabled
;   0xCB08 vmco_thread_count
;   0xCB10 vmco_current_tid
;   0xCB18 vmco_quantum_ops
;   0xCB20 vmco_ops_left[20] (8 bytes each)
;   0xCBC0 vmco_quantum_ticks
;   0xCBC8 vmco_quantum_start_tick
;   0xCC00 vmco_ctx[20] entry size 32:
;          +0 pc, +8 sp, +16 bp, +24 state (0 free,1 ready,2 running)
; ------------------------------------------------------------

vmco_init:
    push bp
    mov bp, sp

    ; last_tick_seen = 0
    ldc r6, 51944
    ldc r0, 0
    mov [r6], r0

    ; tick_ready = 0
    ldc r6, 51952
    mov [r6], r0

    ; enabled = 0
    ldc r6, 51968
    mov [r6], r0

    ; thread_count = 0
    ldc r6, 51976
    mov [r6], r0

    ; current_tid = 20 (none)
    ldc r6, 51984
    ldc r0, 20
    mov [r6], r0

    ; quantum_ops = 1
    ldc r6, 51992
    ldc r0, 1
    mov [r6], r0

    ; quantum_ticks = 1
    ldc r6, 52160
    mov [r6], r0

    ; quantum_start_tick = 0
    ldc r6, 52168
    ldc r0, 0
    mov [r6], r0

    ; init ops_left[i] = 1 and ctx[i].state = 0
    ldc r2, 0
vmco_init_loop:
    ldc r1, 20
    cmp r2, r1
    jl vmco_init_body
    jmp vmco_init_done

vmco_init_body:
    ; ops_left[i]
    mov r3, r2
    ldc r1, 8
    mul r3, r1
    ldc r6, 52000
    add r6, r3
    ldc r0, 1
    mov [r6], r0

    ; ctx[i].state = 0
    mov r4, r2
    ldc r1, 32
    mul r4, r1
    ldc r6, 52224
    add r6, r4
    ldc r1, 24
    add r6, r1
    ldc r0, 0
    mov [r6], r0

    ldc r1, 1
    add r2, r1
    jmp vmco_init_loop

vmco_init_done:
    ldc r0, 1
    mov sp, bp
    pop bp
    ret

vmco_set_quantum:
    push bp
    mov bp, sp

    ; arg0: quantum ops
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r0, [r6]

    ; clamp >= 1
    ldc r1, 1
    cmp r0, r1
    jg vmco_set_q_ok
    je vmco_set_q_ok
    ldc r0, 1

vmco_set_q_ok:
    ; store global quantum
    ldc r6, 51992
    mov [r6], r0

    ; reset ops_left for all slots
    ldc r2, 0
vmco_set_q_loop:
    ldc r1, 20
    cmp r2, r1
    jl vmco_set_q_body
    jmp vmco_set_q_done

vmco_set_q_body:
    mov r3, r2
    ldc r1, 8
    mul r3, r1
    ldc r6, 52000
    add r6, r3
    mov [r6], r0
    ldc r1, 1
    add r2, r1
    jmp vmco_set_q_loop

vmco_set_q_done:
    ldc r0, 1
    mov sp, bp
    pop bp
    ret

vmco_set_quantum_ticks:
    push bp
    mov bp, sp

    ; arg0: quantum ticks
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r0, [r6]

    ; clamp >= 1
    ldc r1, 1
    cmp r0, r1
    jg vmco_set_ticks_ok
    je vmco_set_ticks_ok
    ldc r0, 1

vmco_set_ticks_ok:
    ; store global timer quantum
    ldc r6, 52160
    mov [r6], r0

    ; restart quantum window from current global tick snapshot
    ldc r6, 53272
    mov r0, [r6]
    ldc r6, 52168
    mov [r6], r0

    ldc r0, 1
    mov sp, bp
    pop bp
    ret

vmco_create_thread:
    push bp
    mov bp, sp

    ; arg0: tid
    mov r6, bp
    ldc r1, 16
    add r6, r1
    mov r2, [r6]
    ldc r1, 20
    mod r2, r1

    ; arg1: entry_pc
    mov r6, bp
    ldc r1, 24
    add r6, r1
    mov r4, [r6]

    ; ctx = 0xCC00 + tid * 32
    mov r3, r2
    ldc r1, 32
    mul r3, r1
    ldc r6, 52224
    add r6, r3

    ; ctx.pc = entry_pc
    mov [r6], r4

    ; sp = 0xC000 - tid * 512
    mov r5, r2
    ldc r1, 512
    mul r5, r1
    ldc r0, 49152
    sub r0, r5

    ; ctx.sp = sp
    ldc r1, 8
    add r6, r1
    mov [r6], r0

    ; ctx.bp = sp
    ldc r1, 8
    add r6, r1
    mov [r6], r0

    ; ctx.state = READY
    ldc r1, 8
    add r6, r1
    ldc r0, 1
    mov [r6], r0

    ; ops_left[tid] = quantum_ops
    mov r3, r2
    ldc r1, 8
    mul r3, r1
    ldc r6, 52000
    add r6, r3
    ldc r7, 51992
    mov r0, [r7]
    mov [r6], r0

    ; enabled = 1
    ldc r6, 51968
    ldc r0, 1
    mov [r6], r0

    ; thread_count = max(thread_count, tid+1)
    mov r5, r2
    ldc r1, 1
    add r5, r1
    ldc r6, 51976
    mov r0, [r6]
    cmp r5, r0
    jg vmco_store_count
    jmp vmco_count_done

vmco_store_count:
    mov [r6], r5

vmco_count_done:
    ldc r6, 51976
    mov r0, [r6]

    mov sp, bp
    pop bp
    ret

vmco_start:
    ; if no threads -> halt
    ldc r6, 51976
    mov r5, [r6]
    ldc r1, 1
    cmp r5, r1
    jl vmco_start_halt

    ; current_tid = 0
    ldc r6, 51984
    ldc r3, 0
    mov [r6], r3

    ; baseline tick snapshot (updated by vmrt_timer_handler from IRQ path)
    ldc r6, 53272
    mov r0, [r6]
    ldc r6, 51944
    mov [r6], r0
    ldc r6, 52168
    mov [r6], r0
    ldc r6, 51952
    ldc r0, 0
    mov [r6], r0

    ; ops_left[0] = quantum_ops
    ldc r6, 52000
    ldc r7, 51992
    mov r0, [r7]
    mov [r6], r0

    ; load ctx[0]
    ldc r6, 52224
    mov r0, [r6]      ; pc
    ldc r1, 8
    add r6, r1
    mov r4, [r6]      ; sp
    ldc r1, 8
    add r6, r1
    mov r5, [r6]      ; bp
    ldc r1, 8
    add r6, r1
    ldc r2, 2
    mov [r6], r2      ; state = RUNNING

    mov sp, r4
    mov bp, r5
    push r0
    ret

vmco_start_halt:
    hlt
    jmp vmco_start_halt

vmco_interrupt_point:
    push bp
    mov bp, sp

    ; if cooperative runtime disabled -> return
    ldc r6, 51968
    mov r0, [r6]
    ldc r1, 1
    cmp r0, r1
    jl vmco_ip_ret

    ; current tid
    ldc r6, 51984
    mov r2, [r6]
    ldc r1, 20
    cmp r2, r1
    jge vmco_ip_ret

    ; ctx_cur = 0xCC00 + tid*32
    mov r3, r2
    ldc r1, 32
    mul r3, r1
    ldc r6, 52224
    add r6, r3

    ; save return pc (after call)
    mov r7, bp
    ldc r1, 8
    add r7, r1
    mov r4, [r7]
    mov [r6], r4

    ; save caller sp = bp + 16
    mov r4, bp
    ldc r1, 16
    add r4, r1
    ldc r1, 8
    add r6, r1
    mov [r6], r4

    ; save caller bp = [bp]
    mov r7, bp
    mov r4, [r7]
    ldc r1, 8
    add r6, r1
    mov [r6], r4

    ; state = READY
    ldc r1, 8
    add r6, r1
    ldc r4, 1
    mov [r6], r4

    ; Observe timer tick changes for diagnostics/entropy helpers.
    ; Switching decision itself is based on ops quantum.
    ldc r6, 53272
    mov r0, [r6]
    ldc r7, 51944
    mov r1, [r7]
    cmp r0, r1
    je vmco_ip_no_new_tick
    mov [r7], r0
    ldc r6, 51952
    ldc r0, 1
    mov [r6], r0

vmco_ip_no_new_tick:
    ; Timer-based preemption (as in sys example):
    ; switch when (current_tick - quantum_start_tick) >= quantum_ticks.
    ldc r6, 53272
    mov r0, [r6]          ; current tick
    ldc r6, 52168
    mov r1, [r6]          ; quantum window start tick
    mov r4, r0
    sub r4, r1            ; elapsed ticks

    ldc r6, 52160
    mov r5, [r6]          ; quantum_ticks
    ldc r1, 1
    cmp r5, r1
    jg vmco_ip_qticks_ok
    je vmco_ip_qticks_ok
    ldc r5, 1
    mov [r6], r5

vmco_ip_qticks_ok:
    cmp r4, r5
    jl vmco_ip_ret
    jmp vmco_ip_switch

vmco_ip_switch:
    ; consume tick-ready flag (diagnostic only)
    ldc r6, 51952
    ldc r0, 0
    mov [r6], r0

    ; restart quantum window at current tick
    ldc r6, 53272
    mov r0, [r6]
    ldc r6, 52168
    mov [r6], r0

    ; if less than 2 threads, just reload own quantum and continue
    ldc r6, 51976
    mov r5, [r6]
    ldc r1, 2
    cmp r5, r1
    jl vmco_ip_reload_same

    ; next = current + 1 (mod thread_count)
    mov r3, r2
    ldc r1, 1
    add r3, r1
    cmp r3, r5
    jl vmco_ip_next_ok
    ldc r3, 0

vmco_ip_next_ok:
    ; current_tid = next
    ldc r6, 51984
    mov [r6], r3

    ; context_switches++
    ldc r6, 53256
    mov r0, [r6]
    ldc r1, 1
    add r0, r1
    mov [r6], r0

    ; ops_left[next] = quantum_ops
    mov r4, r3
    ldc r1, 8
    mul r4, r1
    ldc r6, 52000
    add r6, r4
    ldc r7, 51992
    mov r0, [r7]
    mov [r6], r0

    ; load ctx_next
    mov r4, r3
    ldc r1, 32
    mul r4, r1
    ldc r6, 52224
    add r6, r4

    mov r0, [r6]      ; pc
    ldc r1, 8
    add r6, r1
    mov r4, [r6]      ; sp
    ldc r1, 8
    add r6, r1
    mov r5, [r6]      ; bp
    ldc r1, 8
    add r6, r1
    ldc r7, 2
    mov [r6], r7      ; state = RUNNING

    ; switch by jumping to next saved pc
    mov sp, r4
    mov bp, r5
    push r0
    ret

vmco_ip_reload_same:
    mov r3, r2
    ldc r1, 8
    mul r3, r1
    ldc r6, 52000
    add r6, r3
    ldc r7, 51992
    mov r0, [r7]
    mov [r6], r0

vmco_ip_ret:
    mov sp, bp
    pop bp
    ret
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
