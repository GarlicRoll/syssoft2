.assembly extern mscorlib {}
.assembly closure_dispatch_demo {}
.module closure_dispatch_demo.exe

.class public auto ansi beforefieldinit closure_dispatch_demo extends [mscorlib]System.Object
{
  .method public hidebysig specialname rtspecialname instance void .ctor() cil managed
  {
    ldarg.0
    call instance void [mscorlib]System.Object::.ctor()
    ret
  }

  .method public hidebysig static int64 main() cil managed
  {
    .maxstack 128
    .locals init (
      [0] int64,
      [1] int64,
      [2] int64,
      [3] int64,
      [4] int64,
      [5] int64,
      [6] int64,
      [7] int64,
      [8] int64,
      [9] int64,
      [10] int64,
      [11] int64,
      [12] int64
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
    stloc 6
    ldc.i8 0
    stloc 7
    ldc.i8 0
    stloc 8
    ldc.i8 0
    stloc 9
    ldc.i8 0
    stloc 10
    ldc.i8 0
    stloc 11
    ldc.i8 0
    stloc 12
    ldc.i8 2
    dup
    stloc 2
    stloc 0
    ldc.i8 1
    dup
    stloc 3
    stloc 0
    ldloc 2
    dup
    stloc 4
    stloc 0
    ldloc 3
    ldc.i8 1
    ceq
    conv.i8
    conv.i4
    brtrue label7
  label8:
    ldloc 3
    ldc.i8 2
    ceq
    conv.i8
    conv.i4
    brtrue label5
  label6:
    ldloc 12
    ldc.i8 48
    add
    call int64 closure_dispatch_demo::output(int64)
    stloc 0
    ldc.i8 2
    dup
    stloc 3
    stloc 0
    ldloc 2
    dup
    stloc 4
    stloc 0
    ldloc 3
    ldc.i8 1
    ceq
    conv.i8
    conv.i4
    brtrue label3
  label4:
    ldloc 3
    ldc.i8 2
    ceq
    conv.i8
    conv.i4
    brtrue label1
  label2:
    ldloc 12
    ldc.i8 48
    add
    call int64 closure_dispatch_demo::output(int64)
    stloc 0
    ldc.i8 10
    call int64 closure_dispatch_demo::output(int64)
    stloc 0
    ldc.i8 13
    call int64 closure_dispatch_demo::output(int64)
    stloc 0
    ldc.i8 0
    stloc 0
    ldloc 0
    ret
  label1:
    ldloc 4
    ldc.i8 4
    call int64 closure_dispatch_demo::main__mulb(int64, int64)
    dup
    stloc 12
    stloc 0
    br label2
  label3:
    ldloc 4
    ldc.i8 4
    call int64 closure_dispatch_demo::main__addb(int64, int64)
    dup
    stloc 12
    stloc 0
    br label4
  label5:
    ldloc 4
    ldc.i8 5
    call int64 closure_dispatch_demo::main__mulb(int64, int64)
    dup
    stloc 12
    stloc 0
    br label6
  label7:
    ldloc 4
    ldc.i8 5
    call int64 closure_dispatch_demo::main__addb(int64, int64)
    dup
    stloc 12
    stloc 0
    br label8
  }

  .method public hidebysig static int64 main__addb(int64, int64) cil managed
  {
    .maxstack 128
    .locals init (
      [0] int64
    )
    ldarg 1
    ldarg 0
    add
    stloc 0
    ldloc 0
    ret
  }

  .method public hidebysig static int64 main__mulb(int64, int64) cil managed
  {
    .maxstack 128
    .locals init (
      [0] int64
    )
    ldarg 1
    ldarg 0
    mul
    stloc 0
    ldloc 0
    ret
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
    call int64 closure_dispatch_demo::main()
    pop
    ret
  }

}
