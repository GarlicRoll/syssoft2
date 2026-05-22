.assembly extern mscorlib {}
.assembly calc {}
.module calc.exe

.class public auto ansi beforefieldinit calc extends [mscorlib]System.Object
{
  .method public hidebysig specialname rtspecialname instance void .ctor() cil managed
  {
    ldarg.0
    call instance void [mscorlib]System.Object::.ctor()
    ret
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
  label4:
    ldc.i8 1
    conv.i4
    brtrue label1
  label2:
    ldloc 2
    ldloc 5
    mul
    stloc 0
    ldloc 0
    ret
  label1:
    call int64 calc::input()
    dup
    stloc 1
    stloc 0
    ldloc 1
    ldc.i8 13
    ceq
    conv.i8
    conv.i4
    brtrue label12
  label13:
    ldloc 1
    ldc.i8 10
    ceq
    conv.i8
    conv.i4
    brtrue label9
  label10:
    ldloc 1
    ldc.i8 48
    clt
    ldc.i4.0
    ceq
    conv.i8
    conv.i4
    brtrue label6
    ldloc 4
    ldc.i8 0
    ceq
    conv.i8
    conv.i4
    brtrue label3
    br label2
  label3:
    ldloc 1
    ldc.i8 45
    ceq
    conv.i8
    conv.i4
    brtrue label5
    br label4
  label5:
    ldc.i8 0
    ldc.i8 1
    sub
    dup
    stloc 5
    stloc 0
    br label4
  label6:
    ldloc 1
    ldc.i8 57
    cgt
    ldc.i4.0
    ceq
    conv.i8
    conv.i4
    brtrue label8
    ldloc 4
    ldc.i8 1
    ceq
    conv.i8
    conv.i4
    brtrue label7
    br label4
  label7:
    br label2
  label8:
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
    br label4
  label9:
    ldloc 4
    ldc.i8 1
    ceq
    conv.i8
    conv.i4
    brtrue label11
    br label10
  label11:
    br label2
  label12:
    ldloc 4
    ldc.i8 1
    ceq
    conv.i8
    conv.i4
    brtrue label14
    br label13
  label14:
    br label2
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
  label16:
    ldloc 1
    ldc.i8 0
    ceq
    ldc.i4.0
    ceq
    conv.i8
    conv.i4
    brtrue label15
    ldloc 2
    stloc 0
    ldloc 0
    ret
  label15:
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
    br label16
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
    call int64 calc::number_length(int64)
    ldc.i8 1
    sub
    dup
    stloc 2
    stloc 0
    ldc.i8 1
    dup
    stloc 3
    stloc 0
  label20:
    ldloc 2
    ldc.i8 0
    ceq
    ldc.i4.0
    ceq
    conv.i8
    conv.i4
    brtrue label19
  label18:
    ldloc 3
    ldc.i8 0
    ceq
    ldc.i4.0
    ceq
    conv.i8
    conv.i4
    brtrue label17
    ldc.i8 10
    call int64 calc::output(int64)
    stloc 0
    ldc.i8 13
    call int64 calc::output(int64)
    stloc 0
    ldc.i8 0
    stloc 0
    ldloc 0
    ret
  label17:
    ldloc 1
    ldloc 3
    div
    ldc.i8 48
    add
    call int64 calc::output(int64)
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
    br label18
  label19:
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
    br label20
  }

  .method public hidebysig static int64 main() cil managed
  {
    .maxstack 128
    .locals init (
      [0] int64,
      [1] int64,
      [2] int64,
      [3] int64,
      [4] int64
    )
    ldc.i8 0
    stloc 1
    ldc.i8 0
    stloc 2
    ldc.i8 0
    stloc 3
    ldc.i8 0
    stloc 4
    call int64 calc::read_num()
    dup
    stloc 1
    stloc 0
  label22:
    ldc.i8 1
    conv.i4
    brtrue label21
    ldloc 0
    ret
  label21:
  label32:
    ldc.i8 1
    conv.i4
    brtrue label31
  label35:
    call int64 calc::read_num()
    dup
    stloc 2
    stloc 0
    ldloc 3
    ldc.i8 43
    ceq
    conv.i8
    conv.i4
    brtrue label29
  label30:
    ldloc 3
    ldc.i8 45
    ceq
    conv.i8
    conv.i4
    brtrue label27
  label28:
    ldloc 3
    ldc.i8 42
    ceq
    conv.i8
    conv.i4
    brtrue label25
  label26:
    ldloc 3
    ldc.i8 47
    ceq
    conv.i8
    conv.i4
    brtrue label23
  label24:
    ldloc 4
    call int64 calc::print_num(int64)
    stloc 0
    ldloc 4
    dup
    stloc 1
    stloc 0
    br label22
  label23:
    ldloc 1
    ldloc 2
    div
    dup
    stloc 4
    stloc 0
    br label24
  label25:
    ldloc 1
    ldloc 2
    mul
    dup
    stloc 4
    stloc 0
    br label26
  label27:
    ldloc 1
    ldloc 2
    sub
    dup
    stloc 4
    stloc 0
    br label28
  label29:
    ldloc 1
    ldloc 2
    add
    dup
    stloc 4
    stloc 0
    br label30
  label31:
    call int64 calc::input()
    dup
    stloc 3
    stloc 0
    ldloc 3
    ldc.i8 13
    ceq
    ldc.i4.0
    ceq
    conv.i8
    conv.i4
    brtrue label33
    br label32
  label33:
    ldloc 3
    ldc.i8 10
    ceq
    ldc.i4.0
    ceq
    conv.i8
    conv.i4
    brtrue label34
    br label32
  label34:
    br label35
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
    call int64 calc::main()
    pop
    ret
  }

}
