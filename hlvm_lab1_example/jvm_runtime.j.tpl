.field private static __in Ljava/io/BufferedInputStream;
.field private static __out Ljava/io/PrintStream;

.method static <clinit>()V
  .limit stack 4
  .limit locals 0
  new java/io/BufferedInputStream
  dup
  getstatic java/lang/System/in Ljava/io/InputStream;
  invokespecial java/io/BufferedInputStream/<init>(Ljava/io/InputStream;)V
  putstatic {{CLASS_NAME}}/__in Ljava/io/BufferedInputStream;
  getstatic java/lang/System/out Ljava/io/PrintStream;
  putstatic {{CLASS_NAME}}/__out Ljava/io/PrintStream;
  return
.end method

.method private static __readByte()J
  .limit stack 2
  .limit locals 0
  getstatic {{CLASS_NAME}}/__in Ljava/io/BufferedInputStream;
  invokevirtual java/io/BufferedInputStream/read()I
  i2l
  lreturn
.end method

.method private static __readInt()J
  .limit stack 6
  .limit locals 6
  lconst_0
  lstore 0
  lconst_0
  lstore 2
RDI_LOOP:
  invokestatic {{CLASS_NAME}}/__readByte()J
  lstore 4
  lload 4
  lconst_0
  lcmp
  iflt RDI_RET
  lload 4
  ldc2_w 48
  lcmp
  iflt RDI_CHECK_END
  lload 4
  ldc2_w 57
  lcmp
  ifgt RDI_CHECK_END
  lload 0
  ldc2_w 10
  lmul
  lload 4
  ldc2_w 48
  lsub
  ladd
  lstore 0
  lconst_1
  lstore 2
  goto RDI_LOOP
RDI_CHECK_END:
  lload 2
  lconst_0
  lcmp
  ifle RDI_LOOP
  lload 4
  ldc2_w 10
  lcmp
  ifeq RDI_RET
  lload 4
  ldc2_w 13
  lcmp
  ifeq RDI_RET
  lload 4
  ldc2_w 32
  lcmp
  ifeq RDI_RET
  lload 4
  ldc2_w 9
  lcmp
  ifeq RDI_RET
  goto RDI_LOOP
RDI_RET:
  lload 0
  lreturn
.end method

.method private static __print(J)J
  .limit stack 3
  .limit locals 2
  getstatic {{CLASS_NAME}}/__out Ljava/io/PrintStream;
  lload 0
  invokevirtual java/io/PrintStream/print(J)V
  lconst_0
  lreturn
.end method

.method private static __printStr(Ljava/lang/String;)J
  .limit stack 2
  .limit locals 1
  getstatic {{CLASS_NAME}}/__out Ljava/io/PrintStream;
  aload 0
  invokevirtual java/io/PrintStream/print(Ljava/lang/String;)V
  lconst_0
  lreturn
.end method

