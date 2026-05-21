; Пример тестовой программы, выводящей буковку в исходящий поток каждые 5 секунд 
  [section pram, code]
start:
  ldc8 r10, 50000000 ; 5 seconds expressed in "ticks"
  ldc8 r11, 0x13038  ; address of the TicksSignalPeriod register
  store8 d[r11],r10  ; set ticks timeout period to 5 seconds

  ldc8 r10, handler  ; set handler address to the first entry
  ldc8 r11, 0x10140  ; of the handlers-map of the SimplePic
  store8 d[r11],r10  ; while on-ticks signal has interrupt id 0

  ldc8 r10, 1
  ldc8 r11, 0x10110
  store8 d[r11],r10  ; enable interrupt signals queueing

handler:
  ldc8 r10, 0x13028  ; get value of seconds from the io register
  load8 r11, d[r10]  ; of the SimpleClockDevice
  mov r0, r11        ; add 0A30 to it for the bytes to look 
  add r0, 0x0A30     ; like an observable character and a newline
  mov r11, r0        ; r0 is of 2 and r11 is of 4 bytes length
  ldc8 r10, 0x020000 ; write four bytes to the SyncSend register
  store8 d[r10], r11 ; of the SimplePipe device to deliver the output

  ldc8 r10, 1
  ldc8 r11, 0x1010c
  store8 d[r11],r10  ; enable raizing of the queued interrupts 
loop:
  sjmp loop          ; loop to itself so that the only reason for 
                     ; something to happen is when the interrupt 
                     ; handler breaks the loop's control flow