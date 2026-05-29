<?php

declare(strict_types=1);

$libPath = __DIR__ . '/libhlvm4_bridge.so';
$asmPath = __DIR__ . '/vm_math.exe';
$className = 'vm_math';

if (!extension_loaded('FFI')) {
    fwrite(STDERR, "FFI extension is not loaded\n");
    exit(1);
}

if (!file_exists($libPath)) {
    fwrite(STDERR, "Bridge library not found: $libPath\n");
    exit(1);
}

if (!file_exists($asmPath)) {
    fwrite(STDERR, "VM assembly not found: $asmPath\n");
    exit(1);
}

$cdef = <<<'CDEF'
    typedef signed long long int64_t;

    int hlvm4_init(const char *assembly_path, const char *class_name);
    void hlvm4_shutdown(void);

    int hlvm4_call0(const char *method_name, int64_t *out_value);
    int hlvm4_call1(const char *method_name, int64_t arg0, int64_t *out_value);
    int hlvm4_call2(const char *method_name, int64_t arg0, int64_t arg1, int64_t *out_value);

    const char *hlvm4_last_error(void);
CDEF;

$ffi = FFI::cdef($cdef, $libPath);

if ($ffi->hlvm4_init($asmPath, $className) == 0) {
    $err = FFI::string($ffi->hlvm4_last_error());
    fwrite(STDERR, "Init failed: $err\n");
    exit(1);
}

function vmCall2(FFI $ffi, string $method, int $a, int $b): int
{
    $out = FFI::new('int64_t');
    if ($ffi->hlvm4_call2($method, $a, $b, FFI::addr($out)) == 0) {
        $err = FFI::string($ffi->hlvm4_last_error());
        throw new RuntimeException("Call $method failed: $err");
    }
    return (int)$out->cdata;
}

function vmCall1(FFI $ffi, string $method, int $a): int
{
    $out = FFI::new('int64_t');
    if ($ffi->hlvm4_call1($method, $a, FFI::addr($out)) == 0) {
        $err = FFI::string($ffi->hlvm4_last_error());
        throw new RuntimeException("Call $method failed: $err");
    }
    return (int)$out->cdata;
}

function readLinePrompt(string $prompt): string
{
    echo $prompt;
    $line = fgets(STDIN);
    if ($line === false) {
        return '';
    }
    return trim($line);
}

try {
    echo "PHP + FFI + CLR VM calculator\n";
    echo "Available operations: add2, sub2, mul2, max2, abs1, demo, exit\n";

    while (true) {
        $op = strtolower(readLinePrompt("op> "));
        if ($op === 'exit') {
            break;
        }
        if ($op === 'demo') {
            echo "add2(7,5) = " . vmCall2($ffi, 'add2', 7, 5) . "\n";
            echo "sub2(7,5) = " . vmCall2($ffi, 'sub2', 7, 5) . "\n";
            echo "mul2(7,5) = " . vmCall2($ffi, 'mul2', 7, 5) . "\n";
            echo "max2(7,5) = " . vmCall2($ffi, 'max2', 7, 5) . "\n";
            echo "abs1(-42) = " . vmCall1($ffi, 'abs1', -42) . "\n";
            continue;
        }

        if ($op === 'abs1') {
            $a = (int)readLinePrompt("x = ");
            echo "result = " . vmCall1($ffi, 'abs1', $a) . "\n";
            continue;
        }

        if (in_array($op, ['add2', 'sub2', 'mul2', 'max2'], true)) {
            $a = (int)readLinePrompt("a = ");
            $b = (int)readLinePrompt("b = ");
            echo "result = " . vmCall2($ffi, $op, $a, $b) . "\n";
            continue;
        }

        echo "Unknown operation\n";
    }
} finally {
    $ffi->hlvm4_shutdown();
}
