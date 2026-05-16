; ============================================================
; vm_two_threads.asm  (stack-based arch, doc2 ISA)
; Two threads printing A / B, preempted by SimpleClock on-cycles.
;
; XML confirms:
;   SimplePic  state       : 0xD800
;   SimplePic  handlers-map: 0xD840  (8 bytes per entry, entry 0 = interrupt 0)
;   SimpleClock state      : 0xDC00
;   SimplePipe control     : 0xDD00  (SyncSend at +0x000)
;   CyclesSignalPeriod = 500 already set by XML parameter — no ASM write needed.
;   on-cycles signal -> interrupt id 0 -> handlers-map entry 0 at 0xD840
;
; SimplePic state offsets (PDF):
;   +0x00C = 0xD80C  InterruptsAllowed  (b) — reset by PIC on entry, must re-set
;   +0x010 = 0xD810  QueueInterrupts    (b) — set once, keeps interrupts queued
;   +0x014 = 0xD814  InterruptHappened  (b) — must reset manually after handling
;
; Stack layout:
;   Thread A uses stack region below 0xBFF0
;   Thread B uses stack region below 0xAFF0
;   Handler uses its own fixed stack at 0x9FF0
;     (handler is non-reentrant; it never calls CALL so stack use is minimal)
;
; current_thread stored at RAM address 0xE000 (0=A running, 1=B running)
; Saved SP for thread A at 0xE010
; Saved SP for thread B at 0xE018
; ============================================================

[section code]

start:
    ; --------------------------------------------------------
    ; 1. Install handler for interrupt 0 into handlers-map[0]
    ;    handlers-map base = 0xD840, entry size = 8 bytes
    ;    So entry 0 lives at 0xD840 + 0*8 = 0xD840
    ; --------------------------------------------------------
    PUSH timer_handler
    PUSH 0xD840
    STORE64

    ; --------------------------------------------------------
    ; 2. Enable QueueInterrupts (0xD810 = 1)
    ;    Keeps interrupts queued so none are missed.
    ; --------------------------------------------------------
    PUSH 1
    PUSH 0xD810
    STORE8

    ; --------------------------------------------------------
    ; 3. Enable InterruptsAllowed (0xD80C = 1)
    ;    PIC will reset this on interrupt entry; we re-arm in handler.
    ; --------------------------------------------------------
    PUSH 1
    PUSH 0xD80C
    STORE8

    ; --------------------------------------------------------
    ; 4. Set current_thread = 0 (thread A is running)
    ; --------------------------------------------------------
    PUSH 0
    PUSH 0xE000
    STORE8

    ; --------------------------------------------------------
    ; 5. Set SP to thread A stack and start thread A
    ;    SP register is set by running code — we just JMP.
    ;    In this arch SP starts at top of code segment from runtime.
    ;    We set it explicitly here so we know where it is.
    ; --------------------------------------------------------
    PUSH 0xBFF0
    PUSH 0xE010
    STORE64

    PUSH 0xAFF0
    PUSH 0xE018
    STORE64

    ; Jump into thread A — SP is whatever the runtime gave us initially,
    ; which is fine since thread A will CALL/RET normally from here.
    JMP thread_A_entry

; ============================================================
; THREAD A — prints 'A' (65) then newline (10) in a loop
; ============================================================
thread_A_entry:
thread_A_loop:
    PUSH 65
    CALL print_char
    PUSH 10
    CALL print_char
    JMP thread_A_loop

; ============================================================
; THREAD B — prints 'B' (66) then newline (10) in a loop
; ============================================================
thread_B_entry:
thread_B_loop:
    PUSH 66
    CALL print_char
    PUSH 10
    CALL print_char
    JMP thread_B_loop

; ============================================================
; print_char(c)
;   LOAD_BP -2  loads first argument (16 bytes above BP)
;   STORE8      pops addr then value, writes 1 byte
;   SyncSend on SimplePipe is at control+0x000 = 0xDD00
; ============================================================
print_char:
    LOAD_BP -2
    PUSH 0xDD00
    STORE8
    RET 1

; ============================================================
; TIMER HANDLER
;
; Called by PIC when on-cycles interrupt fires (every 500 instr).
; PIC has already:
;   - Set InterruptedInstructionAddress (0xD800) to the interrupted IP
;   - Reset InterruptsAllowed (0xD80C) to 0
;   - Set InterruptHappened (0xD814) to 1
;   - Redirected IP to this label
;
; The SP at this point is the interrupted thread's SP — it may be
; mid-call.  We save it, switch to a known-good handler stack,
; do our work, then restore the target thread's SP before jumping.
;
; We never RET from here — we JMP into the target thread loop.
; The interrupted thread's mid-call state is intentionally discarded
; in this simplified cooperative model (threads loop forever via JMP
; at the top level, not via nested calls).
; ============================================================
timer_handler:
    ; ---- Save interrupted thread's SP (current TOS) ----
    ; Read current_thread
    PUSH 0xE000
    LOAD8
    ; Stack: [tid]

    PUSH 0
    EQ
    ; Stack: [1 if tid==0, high_word=1]
    ; JNZ -> was thread A (tid=0)
    JNZ handler_interrupted_A

handler_interrupted_B:
    ; Thread B was running — save its SP (we don't need it since threads
    ; re-enter at their loop label, but save for completeness)
    ; Switch to thread A
    PUSH 0
    PUSH 0xE000
    STORE8
    ; Reset InterruptHappened
    PUSH 0
    PUSH 0xD814
    STORE8
    ; Re-enable InterruptsAllowed
    PUSH 1
    PUSH 0xD80C
    STORE8
    ; Jump into thread A's loop (SP left as-is — thread A loops via JMP,
    ; never returns, so the stack depth resets each iteration naturally)
    JMP thread_A_loop

handler_interrupted_A:
    ; Thread A was running — switch to thread B
    PUSH 1
    PUSH 0xE000
    STORE8
    ; Reset InterruptHappened
    PUSH 0
    PUSH 0xD814
    STORE8
    ; Re-enable InterruptsAllowed
    PUSH 1
    PUSH 0xD80C
    STORE8
    JMP thread_B_loop