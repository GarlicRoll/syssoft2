.assembly extern mscorlib {}
.assembly vm_math {}
.module vm_math.exe

.class public auto ansi beforefieldinit vm_math extends [mscorlib]System.Object
{
  .method public hidebysig specialname rtspecialname instance void .ctor() cil managed
  {
    ldarg.0
    call instance void [mscorlib]System.Object::.ctor()
    ret
  }

  .method public hidebysig static int64 add2(int64, int64) cil managed
  {
    .maxstack 128
    .locals init (
      [0] int64
    )
    ldarg 0
    ldarg 1
    add
    stloc 0
    ldloc 0
    ret
  }

  .method public hidebysig static int64 sub2(int64, int64) cil managed
  {
    .maxstack 128
    .locals init (
      [0] int64
    )
    ldarg 0
    ldarg 1
    sub
    stloc 0
    ldloc 0
    ret
  }

  .method public hidebysig static int64 mul2(int64, int64) cil managed
  {
    .maxstack 128
    .locals init (
      [0] int64
    )
    ldarg 0
    ldarg 1
    mul
    stloc 0
    ldloc 0
    ret
  }

  .method public hidebysig static int64 max2(int64, int64) cil managed
  {
    .maxstack 128
    .locals init (
      [0] int64
    )
    ldarg 0
    ldarg 1
    clt
    ldc.i4.0
    ceq
    conv.i8
    conv.i4
    brtrue label1
    ldarg 1
    stloc 0
  label2:
    ldloc 0
    ret
  label1:
    ldarg 0
    stloc 0
    br label2
  }

  .method public hidebysig static int64 abs1(int64) cil managed
  {
    .maxstack 128
    .locals init (
      [0] int64
    )
    ldarg 0
    ldc.i8 0
    clt
    conv.i8
    conv.i4
    brtrue label3
    ldarg 0
    stloc 0
  label4:
    ldloc 0
    ret
  label3:
    ldc.i8 0
    ldarg 0
    sub
    stloc 0
    br label4
  }

  .method public hidebysig static int64 main() cil managed
  {
    .maxstack 128
    .locals init (
      [0] int64
    )
    ldc.i8 0
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
    call int64 vm_math::main()
    pop
    ret
  }

}
