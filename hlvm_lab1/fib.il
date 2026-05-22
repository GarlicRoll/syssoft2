.assembly extern mscorlib {}
.assembly fib {}
.module fib.exe

.class public auto ansi beforefieldinit fib extends [mscorlib]System.Object
{
  .method public hidebysig specialname rtspecialname instance void .ctor() cil managed
  {
    ldarg.0
    call instance void [mscorlib]System.Object::.ctor()
    ret
  }

  .method public hidebysig static int64 fibonacci(int64) cil managed
  {
    .maxstack 128
    .locals init (
      [0] int64
    )
    ldarg 0
    ldc.i8 1
    cgt
    ldc.i4.0
    ceq
    conv.i8
    conv.i4
    brtrue label1
    ldarg 0
    ldc.i8 1
    sub
    call int64 fib::fibonacci(int64)
    ldarg 0
    ldc.i8 2
    sub
    call int64 fib::fibonacci(int64)
    add
    stloc 0
  label2:
    ldloc 0
    ret
  label1:
    ldarg 0
    stloc 0
    br label2
  }

  .method public hidebysig static int64 read_num() cil managed
  {
    .maxstack 128
    .locals init (
      [0] int64,
      [1] int64,
      [2] int64,
      [3] int64,
      [4] int64,
      [5] int64
    )
    ldc.i8 0
    stloc 1
    ldc.i8 0
    stloc 2
    ldc.i8 0
    stloc 3
    ldc.i8 0
    stloc 4
    ldc.i8 0
    stloc 5
    ldc.i8 0
    dup
    stloc 2
    stloc 0
    ldc.i8 1
    dup
    stloc 3
    stloc 0
    ldc.i8 0
    dup
    stloc 4
    stloc 0
    ldc.i8 1
    dup
    stloc 5
    stloc 0
  label6:
    ldc.i8 1
    conv.i4
    brtrue label3
  label4:
    ldloc 2
    ldloc 5
    mul
    stloc 0
    ldloc 0
    ret
  label3:
    call int64 fib::input()
    dup
    stloc 1
    stloc 0
    ldloc 1
    ldc.i8 13
    ceq
    conv.i8
    conv.i4
    brtrue label14
  label15:
    ldloc 1
    ldc.i8 10
    ceq
    conv.i8
    conv.i4
    brtrue label11
  label12:
    ldloc 1
    ldc.i8 48
    clt
    ldc.i4.0
    ceq
    conv.i8
    conv.i4
    brtrue label8
    ldloc 4
    ldc.i8 0
    ceq
    conv.i8
    conv.i4
    brtrue label5
    br label4
  label5:
    ldloc 1
    ldc.i8 45
    ceq
    conv.i8
    conv.i4
    brtrue label7
    br label6
  label7:
    ldc.i8 0
    ldc.i8 1
    sub
    dup
    stloc 5
    stloc 0
    br label6
  label8:
    ldloc 1
    ldc.i8 57
    cgt
    ldc.i4.0
    ceq
    conv.i8
    conv.i4
    brtrue label10
    ldloc 4
    ldc.i8 1
    ceq
    conv.i8
    conv.i4
    brtrue label9
    br label6
  label9:
    br label4
  label10:
    ldloc 2
    ldc.i8 10
    mul
    ldloc 1
    ldc.i8 48
    sub
    add
    dup
    stloc 2
    stloc 0
    ldloc 3
    ldc.i8 1
    add
    dup
    stloc 3
    stloc 0
    ldc.i8 1
    dup
    stloc 4
    stloc 0
    br label6
  label11:
    ldloc 4
    ldc.i8 1
    ceq
    conv.i8
    conv.i4
    brtrue label13
    br label12
  label13:
    br label4
  label14:
    ldloc 4
    ldc.i8 1
    ceq
    conv.i8
    conv.i4
    brtrue label16
    br label15
  label16:
    br label4
  }

  .method public hidebysig static int64 number_length(int64) cil managed
  {
    .maxstack 128
    .locals init (
      [0] int64,
      [1] int64,
      [2] int64
    )
    ldc.i8 0
    stloc 1
    ldc.i8 0
    stloc 2
    ldc.i8 1
    dup
    stloc 2
    stloc 0
    ldarg 0
    ldc.i8 10
    div
    dup
    stloc 1
    stloc 0
  label18:
    ldloc 1
    ldc.i8 0
    ceq
    ldc.i4.0
    ceq
    conv.i8
    conv.i4
    brtrue label17
    ldloc 2
    stloc 0
    ldloc 0
    ret
  label17:
    ldloc 1
    ldc.i8 10
    div
    dup
    stloc 1
    stloc 0
    ldloc 2
    ldc.i8 1
    add
    dup
    stloc 2
    stloc 0
    br label18
  }

  .method public hidebysig static int64 print_num(int64) cil managed
  {
    .maxstack 128
    .locals init (
      [0] int64,
      [1] int64,
      [2] int64,
      [3] int64
    )
    ldc.i8 0
    stloc 1
    ldc.i8 0
    stloc 2
    ldc.i8 0
    stloc 3
    ldarg 0
    dup
    stloc 1
    stloc 0
    ldarg 0
    call int64 fib::number_length(int64)
    ldc.i8 1
    sub
    dup
    stloc 2
    stloc 0
    ldc.i8 1
    dup
    stloc 3
    stloc 0
  label22:
    ldloc 2
    ldc.i8 0
    ceq
    ldc.i4.0
    ceq
    conv.i8
    conv.i4
    brtrue label21
  label20:
    ldloc 3
    ldc.i8 0
    ceq
    ldc.i4.0
    ceq
    conv.i8
    conv.i4
    brtrue label19
    ldc.i8 10
    call int64 fib::output(int64)
    stloc 0
    ldc.i8 13
    call int64 fib::output(int64)
    stloc 0
    ldc.i8 0
    stloc 0
    ldloc 0
    ret
  label19:
    ldloc 1
    ldloc 3
    div
    ldc.i8 48
    add
    call int64 fib::output(int64)
    stloc 0
    ldloc 1
    ldloc 3
    rem
    dup
    stloc 1
    stloc 0
    ldloc 3
    ldc.i8 10
    div
    dup
    stloc 3
    stloc 0
    br label20
  label21:
    ldloc 3
    ldc.i8 10
    mul
    dup
    stloc 3
    stloc 0
    ldloc 2
    ldc.i8 1
    sub
    dup
    stloc 2
    stloc 0
    br label22
  }

  .method public hidebysig static int64 main() cil managed
  {
    .maxstack 128
    .locals init (
      [0] int64,
      [1] int64,
      [2] int64
    )
    ldc.i8 0
    stloc 1
    ldc.i8 0
    stloc 2
    call int64 fib::read_num()
    dup
    stloc 1
    stloc 0
  label24:
    ldloc 1
    ldc.i8 0
    clt
    ldc.i4.0
    ceq
    conv.i8
    conv.i4
    brtrue label23
    ldc.i8 0
    stloc 0
    ldloc 0
    ret
  label23:
    ldloc 1
    ldc.i8 1
    sub
    call int64 fib::fibonacci(int64)
    dup
    stloc 2
    stloc 0
    ldloc 2
    call int64 fib::print_num(int64)
    stloc 0
    call int64 fib::read_num()
    dup
    stloc 1
    stloc 0
    br label24
  }

  .method private hidebysig static int64 input() cil managed
  {
    .maxstack 1
    call int32 [mscorlib]System.Console::Read()
    conv.i8
    ret
  }

  .method private hidebysig static int64 output(int64) cil managed
  {
    .maxstack 2
    ldarg.0
    conv.u2
    call void [mscorlib]System.Console::Write(char)
    ldc.i8 0
    ret
  }

  .method public hidebysig static void Main() cil managed
  {
    .entrypoint
    .maxstack 8
    call int64 fib::main()
    pop
    ret
  }

}
