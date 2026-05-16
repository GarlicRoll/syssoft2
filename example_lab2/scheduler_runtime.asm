[section code, code]

; scheduler globals
; 0x0000A000 tick
; 0x0000A004 current slot (-1 if idle)
; 0x0000A008 completed count
; 0x0000A00C boot flag
; 0x0000A010 algorithm (0 = FCFS, 1 = SRT)
; 0x0000A014 phase id  (0 = FCFS5, 1 = SRT5, 2 = FCFS4, 3 = SRT4)
; 0x0000A018 task count
; 0x0000A01C saved fp for run_pool
;
; TCB layout (128 bytes each, base = 0x0000A100 + slot * 128)
;   +00 state     (0 new, 1 ready, 2 running, 3 done)
;   +04 arrival
;   +08 duration
;   +12 remaining
;   +16 pid       (1..20)
;   +20 symbol    ('A'..'T')
;   +24 workload  (1..4)
;   +28 started   (0/1)
;   +32 saved r0
;   +36 saved r1
;   +40 saved r2
;   +44 saved r3
;   +48 saved r4
;   +52 saved r5
;   +56 saved r6
;   +60 saved r7
;   +64 saved r8
;   +68 saved r9

idle:
  cjmp idle

fn_boot_scheduler__v:
  sub r9, 4
  store d[r9], r4
  ldc r4, 0
  add r4, r9

  ldc r10, 0x0000A00C
  load r11, d[r10]
  eq r11, 1
  jnz r11, boot_scheduler_done

  ldc r10, 0x10110
  ldc r11, 1
  store d[r10], r11

  ldc r10, 0x10140
  ldc r11, timer_handler
  store d[r10], r11

  ldc r10, 0x0000A00C
  ldc r11, 1
  store d[r10], r11

boot_scheduler_done:
  ldc r10, 0x0000A004
  ldc r11, -1
  store d[r10], r11

  ldc r1, 0
  ldc r9, 0
  add r9, r4
  load r4, d[r9]
  add r9, 4
  ret

fn_create_pool__int_int:
  sub r9, 4
  store d[r9], r4
  ldc r4, 0
  add r4, r9

  ldc r10, 0x0000A010
  store d[r10], r0
  ldc r10, 0x0000A014
  store d[r10], r1

  call disarm_interrupts

  ldc r11, 0
  ldc r10, 0x0000A000
  store d[r10], r11
  ldc r10, 0x0000A008
  store d[r10], r11
  ldc r10, 0x0000A018
  store d[r10], r11

  ldc r10, 0x0000A004
  ldc r11, -1
  store d[r10], r11

  ldc r0, 0
  add r0, r1
  call send_phase_header

  ldc r1, 0
  ldc r9, 0
  add r9, r4
  load r4, d[r9]
  add r9, 4
  ret

fn_create_thread__int_int_int_int:
  sub r9, 4
  store d[r9], r4
  ldc r4, 0
  add r4, r9

  ldc r6, 0
  add r6, r0
  ldc r7, 0
  add r7, r1
  ldc r8, 0
  add r8, r2
  ldc r10, 8
  add r10, r4
  load r10, d[r10]

  call init_task

  ldc r10, 0x0000A018
  load r11, d[r10]
  add r11, 1
  store d[r10], r11

  ldc r1, 0
  ldc r9, 0
  add r9, r4
  load r4, d[r9]
  add r9, 4
  ret

fn_run_pool__v:
  sub r9, 4
  store d[r9], r4
  ldc r4, 0
  add r4, r9

  ldc r10, 0x0000A01C
  store d[r10], r4

  call admit_arrivals
  call dispatch_if_needed

  ldc r10, 0x13038
  ldc r11, 100000
  store d[r10], r11

  cjmp restore_or_idle

run_pool_done:
  call disarm_interrupts

  ldc r10, 0x0000A004
  ldc r11, -1
  store d[r10], r11

  ldc r10, 0x0000A01C
  load r4, d[r10]

  ldc r1, 0
  ldc r9, 0
  add r9, r4
  load r4, d[r9]
  add r9, 4
  ret

fn_finish_demo__v:
  sub r9, 4
  store d[r9], r4
  ldc r4, 0
  add r4, r9

  call send_done

  ldc r1, 0
  ldc r9, 0
  add r9, r4
  load r4, d[r9]
  add r9, 4
  ret

timer_handler:
  ldc r10, 0x0000A004
  load r10, d[r10]
  ldc r11, 0
  add r11, r10
  eq r11, -1
  jnz r11, timer_handler_saved

  ldc r12, 0
  add r12, r10
  mul r12, 128
  add r12, 0x0000A100

  ldc r11, 32
  add r11, r12
  store d[r11], r0
  ldc r11, 36
  add r11, r12
  store d[r11], r1
  ldc r11, 40
  add r11, r12
  store d[r11], r2
  ldc r11, 44
  add r11, r12
  store d[r11], r3
  ldc r11, 48
  add r11, r12
  store d[r11], r4
  ldc r11, 52
  add r11, r12
  store d[r11], r5
  ldc r11, 56
  add r11, r12
  store d[r11], r6
  ldc r11, 60
  add r11, r12
  store d[r11], r7
  ldc r11, 64
  add r11, r12
  store d[r11], r8
  ldc r11, 68
  add r11, r12
  store d[r11], r9

timer_handler_saved:
  ldc r9, 0x00009000

  call inc_tick
  call log_running_symbol
  call update_running_task
  call admit_arrivals
  call check_phase_complete
  call dispatch_if_needed
  cjmp restore_or_idle

send_char:
  ldc r10, 0x20000
  storeb d8[r10], r0
  ret

send_newline:
  ldc r0, 10
  call send_char
  ret

send_done:
  ldc r0, 68
  call send_char
  ldc r0, 79
  call send_char
  ldc r0, 78
  call send_char
  ldc r0, 69
  call send_char
  call send_newline
  ret

send_phase_header:
  ldc r1, 0
  add r1, r0
  eq r1, 0
  jnz r1, send_phase_fcfs5

  ldc r1, 0
  add r1, r0
  eq r1, 1
  jnz r1, send_phase_srt5

  ldc r1, 0
  add r1, r0
  eq r1, 2
  jnz r1, send_phase_fcfs4

  ldc r1, 0
  add r1, r0
  eq r1, 9
  jnz r1, send_phase_warm

  cjmp send_phase_srt4

send_phase_fcfs5:
  ldc r0, 70
  call send_char
  ldc r0, 67
  call send_char
  ldc r0, 70
  call send_char
  ldc r0, 83
  call send_char
  ldc r0, 53
  call send_char
  call send_newline
  ret

send_phase_srt5:
  ldc r0, 83
  call send_char
  ldc r0, 82
  call send_char
  ldc r0, 84
  call send_char
  ldc r0, 53
  call send_char
  call send_newline
  ret

send_phase_fcfs4:
  ldc r0, 70
  call send_char
  ldc r0, 67
  call send_char
  ldc r0, 70
  call send_char
  ldc r0, 83
  call send_char
  ldc r0, 52
  call send_char
  call send_newline
  ret

send_phase_srt4:
  ldc r0, 83
  call send_char
  ldc r0, 82
  call send_char
  ldc r0, 84
  call send_char
  ldc r0, 52
  call send_char
  call send_newline
  ret

send_phase_warm:
  ldc r0, 87
  call send_char
  ldc r0, 65
  call send_char
  ldc r0, 82
  call send_char
  ldc r0, 77
  call send_char
  call send_newline
  ret

arm_interrupts:
  ldc r13, 0
  ldc r14, 0x10114
  store d[r14], r13

  ldc r13, 1
  ldc r14, 0x1010c
  store d[r14], r13
  ret

disarm_interrupts:
  ldc r13, 0
  ldc r14, 0x1010c
  store d[r14], r13

  ldc r14, 0x13038
  store d[r14], r13
  ret

inc_tick:
  ldc r10, 0x0000A000
  load r11, d[r10]
  add r11, 1
  store d[r10], r11
  ret

compute_tcb_base:
  ldc r1, 0
  add r1, r0
  mul r1, 128
  add r1, 0x0000A100
  ret

init_task:
  ldc r0, 0
  add r0, r6
  call compute_tcb_base

  ldc r2, 0
  store d[r1], r2

  ldc r3, 4
  add r3, r1
  store d[r3], r7

  ldc r3, 8
  add r3, r1
  store d[r3], r8

  ldc r3, 12
  add r3, r1
  store d[r3], r8

  ldc r2, 1
  add r2, r6
  ldc r3, 16
  add r3, r1
  store d[r3], r2

  ldc r2, 65
  add r2, r6
  ldc r3, 20
  add r3, r1
  store d[r3], r2

  ldc r3, 24
  add r3, r1
  store d[r3], r10

  ldc r2, 0
  ldc r3, 28
  add r3, r1
  store d[r3], r2

  ldc r3, 32
  add r3, r1
  store d[r3], r2
  ldc r3, 36
  add r3, r1
  store d[r3], r2
  ldc r3, 40
  add r3, r1
  store d[r3], r2
  ldc r3, 44
  add r3, r1
  store d[r3], r2
  ldc r3, 48
  add r3, r1
  store d[r3], r2
  ldc r3, 52
  add r3, r1
  store d[r3], r2
  ldc r3, 56
  add r3, r1
  store d[r3], r2
  ldc r3, 60
  add r3, r1
  store d[r3], r2
  ldc r3, 64
  add r3, r1
  store d[r3], r2

  ldc r2, 0
  add r2, r6
  mul r2, 256
  add r2, 0x00003000
  ldc r3, 68
  add r3, r1
  store d[r3], r2
  ret

load_task_context:
  ldc r11, 32
  add r11, r10
  load r0, d[r11]
  ldc r11, 36
  add r11, r10
  load r1, d[r11]
  ldc r11, 40
  add r11, r10
  load r2, d[r11]
  ldc r11, 44
  add r11, r10
  load r3, d[r11]
  ldc r11, 48
  add r11, r10
  load r4, d[r11]
  ldc r11, 52
  add r11, r10
  load r5, d[r11]
  ldc r11, 56
  add r11, r10
  load r6, d[r11]
  ldc r11, 60
  add r11, r10
  load r7, d[r11]
  ldc r11, 64
  add r11, r10
  load r8, d[r11]
  ldc r11, 68
  add r11, r10
  load r9, d[r11]
  ret

log_running_symbol:
  ldc r10, 0x0000A004
  load r10, d[r10]
  ldc r11, 0
  add r11, r10
  eq r11, -1
  jnz r11, log_running_idle

  ldc r0, 0
  add r0, r10
  call compute_tcb_base
  ldc r2, 20
  add r2, r1
  load r0, d[r2]
  call send_char
  ret

log_running_idle:
  ldc r0, 46
  call send_char
  ret

admit_arrivals:
  ldc r8, 0x0000A018
  load r8, d[r8]
  ldc r7, 0x0000A000
  load r7, d[r7]
  ldc r6, 0

admit_arrivals_loop:
  ldc r1, 0
  add r1, r6
  lt r1, r8
  jz r1, admit_arrivals_done

  ldc r0, 0
  add r0, r6
  call compute_tcb_base
  load r2, d[r1]
  eq r2, 0
  jz r2, admit_arrivals_next

  ldc r3, 4
  add r3, r1
  load r2, d[r3]
  leqt r2, r7
  jz r2, admit_arrivals_next

  ldc r2, 1
  store d[r1], r2

admit_arrivals_next:
  add r6, 1
  cjmp admit_arrivals_loop

admit_arrivals_done:
  ret

dispatch_if_needed:
  ldc r10, 0x0000A004
  load r10, d[r10]
  ldc r11, 0
  add r11, r10
  eq r11, -1
  jz r11, dispatch_if_needed_done

  ldc r10, 0x0000A010
  load r10, d[r10]
  eq r10, 0
  jnz r10, dispatch_if_needed_fcfs
  call dispatch_srt
  ret

dispatch_if_needed_fcfs:
  call dispatch_fcfs
  ret

dispatch_if_needed_done:
  ret

dispatch_fcfs:
  ldc r8, 0x0000A018
  load r8, d[r8]
  ldc r6, 0

dispatch_fcfs_loop:
  ldc r1, 0
  add r1, r6
  lt r1, r8
  jz r1, dispatch_fcfs_none

  ldc r0, 0
  add r0, r6
  call compute_tcb_base
  load r2, d[r1]
  eq r2, 1
  jnz r2, dispatch_fcfs_found

  add r6, 1
  cjmp dispatch_fcfs_loop

dispatch_fcfs_found:
  ldc r10, 0x0000A004
  store d[r10], r6
  ldc r2, 2
  store d[r1], r2
  ret

dispatch_fcfs_none:
  ldc r10, 0x0000A004
  ldc r11, -1
  store d[r10], r11
  ret

dispatch_srt:
  ldc r6, 0x0000A018
  load r6, d[r6]
  ldc r7, -1
  ldc r8, 999999
  ldc r5, 0

dispatch_srt_loop:
  ldc r1, 0
  add r1, r5
  lt r1, r6
  jz r1, dispatch_srt_done

  ldc r0, 0
  add r0, r5
  call compute_tcb_base
  load r2, d[r1]
  eq r2, 1
  jz r2, dispatch_srt_next

  ldc r3, 12
  add r3, r1
  load r2, d[r3]

  ldc r3, 0
  add r3, r2
  lt r3, r8
  jnz r3, dispatch_srt_better

  ldc r3, 0
  add r3, r7
  eq r3, -1
  jnz r3, dispatch_srt_better
  cjmp dispatch_srt_next

dispatch_srt_better:
  ldc r8, 0
  add r8, r2
  ldc r7, 0
  add r7, r5

dispatch_srt_next:
  add r5, 1
  cjmp dispatch_srt_loop

dispatch_srt_done:
  ldc r10, 0x0000A004
  store d[r10], r7

  ldc r11, 0
  add r11, r7
  eq r11, -1
  jnz r11, dispatch_srt_ret

  ldc r0, 0
  add r0, r7
  call compute_tcb_base
  ldc r2, 2
  store d[r1], r2

dispatch_srt_ret:
  ret

update_running_task:
  ldc r10, 0x0000A004
  load r10, d[r10]
  ldc r11, 0
  add r11, r10
  eq r11, -1
  jnz r11, update_running_task_done

  ldc r0, 0
  add r0, r10
  call compute_tcb_base

  ldc r2, 12
  add r2, r1
  load r3, d[r2]
  sub r3, 1
  store d[r2], r3
  eq r3, 0
  jnz r3, update_running_finished

  ldc r10, 0x0000A010
  load r10, d[r10]
  eq r10, 1
  jnz r10, update_running_preempt

  ldc r2, 2
  store d[r1], r2
  ret

update_running_preempt:
  ldc r2, 1
  store d[r1], r2
  ldc r10, 0x0000A004
  ldc r11, -1
  store d[r10], r11
  ret

update_running_finished:
  ldc r2, 3
  store d[r1], r2

  ldc r10, 0x0000A008
  load r11, d[r10]
  add r11, 1
  store d[r10], r11

  ldc r10, 0x0000A004
  ldc r11, -1
  store d[r10], r11
  ret

update_running_task_done:
  ret

check_phase_complete:
  ldc r10, 0x0000A008
  load r10, d[r10]
  ldc r11, 0x0000A018
  load r11, d[r11]
  eq r10, r11
  jz r10, check_phase_complete_done

  call send_newline
  cjmp run_pool_done

check_phase_complete_done:
  ret

restore_or_idle:
  ldc r10, 0x0000A004
  load r10, d[r10]
  ldc r11, 0
  add r11, r10
  eq r11, -1
  jnz r11, restore_or_idle_idle

  ldc r0, 0
  add r0, r10
  call compute_tcb_base
  ldc r10, 0
  add r10, r1

  ldc r11, 24
  add r11, r10
  load r11, d[r11]
  ldc r12, 28
  add r12, r10
  load r12, d[r12]

  ldc r13, 0
  add r13, r11
  eq r13, 1
  jnz r13, restore_workload1

  ldc r13, 0
  add r13, r11
  eq r13, 2
  jnz r13, restore_workload2

  ldc r13, 0
  add r13, r11
  eq r13, 3
  jnz r13, restore_workload3

  cjmp restore_workload4

restore_or_idle_idle:
  call arm_interrupts
  cjmp idle

restore_workload1:
  ldc r13, 0
  add r13, r12
  eq r13, 0
  jnz r13, start_workload1
  call arm_interrupts
  call load_task_context
  cjmp fn_load1__v_L5_bb

start_workload1:
  ldc r13, 1
  ldc r14, 28
  add r14, r10
  store d[r14], r13
  call arm_interrupts
  call load_task_context
  cjmp fn_load1__v

restore_workload2:
  ldc r13, 0
  add r13, r12
  eq r13, 0
  jnz r13, start_workload2
  call arm_interrupts
  call load_task_context
  cjmp fn_load2__v_L5_bb

start_workload2:
  ldc r13, 1
  ldc r14, 28
  add r14, r10
  store d[r14], r13
  call arm_interrupts
  call load_task_context
  cjmp fn_load2__v

restore_workload3:
  ldc r13, 0
  add r13, r12
  eq r13, 0
  jnz r13, start_workload3
  call arm_interrupts
  call load_task_context
  cjmp fn_load3__v_L5_bb

start_workload3:
  ldc r13, 1
  ldc r14, 28
  add r14, r10
  store d[r14], r13
  call arm_interrupts
  call load_task_context
  cjmp fn_load3__v

restore_workload4:
  ldc r13, 0
  add r13, r12
  eq r13, 0
  jnz r13, start_workload4
  call arm_interrupts
  call load_task_context
  cjmp fn_load4__v_L5_bb

start_workload4:
  ldc r13, 1
  ldc r14, 28
  add r14, r10
  store d[r14], r13
  call arm_interrupts
  call load_task_context
  cjmp fn_load4__v
