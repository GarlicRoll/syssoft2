#include "clr_serializer.h"

#include <stdio.h>

static void write_runtime_methods(FILE *out, const char *class_name, const char *entry_name) {
    fprintf(out, "  .method private hidebysig static int64 input() cil managed\n");
    fprintf(out, "  {\n");
    fprintf(out, "    .maxstack 1\n");
    fprintf(out, "    call int32 [mscorlib]System.Console::Read()\n");
    fprintf(out, "    conv.i8\n");
    fprintf(out, "    ret\n");
    fprintf(out, "  }\n\n");

    fprintf(out, "  .method private hidebysig static int64 output(int64) cil managed\n");
    fprintf(out, "  {\n");
    fprintf(out, "    .maxstack 2\n");
    fprintf(out, "    ldarg.0\n");
    fprintf(out, "    conv.u2\n");
    fprintf(out, "    call void [mscorlib]System.Console::Write(char)\n");
    fprintf(out, "    ldc.i8 0\n");
    fprintf(out, "    ret\n");
    fprintf(out, "  }\n\n");

    fprintf(out, "  .method public hidebysig static void Main() cil managed\n");
    fprintf(out, "  {\n");
    fprintf(out, "    .entrypoint\n");
    fprintf(out, "    .maxstack 8\n");
    fprintf(out, "    call int64 %s::%s()\n", class_name, entry_name ? entry_name : "main");
    fprintf(out, "    pop\n");
    fprintf(out, "    ret\n");
    fprintf(out, "  }\n\n");
}

static void write_method(FILE *out, const ClrMethodModel *method) {
    fprintf(out, "  .method public hidebysig static int64 %s(", method->name);
    for (int i = 0; i < method->param_count; i++) {
        if (i > 0) fprintf(out, ", ");
        fprintf(out, "int64");
    }
    fprintf(out, ") cil managed\n");
    fprintf(out, "  {\n");
    fprintf(out, "    .maxstack 128\n");

    if (method->local_count > 0) {
        fprintf(out, "    .locals init (\n");
        for (int i = 0; i < method->local_count; i++) {
            fprintf(out, "      [%d] int64%s\n", i, (i + 1 < method->local_count) ? "," : "");
        }
        fprintf(out, "    )\n");
    }

    for (int i = 0; i < method->instruction_count; i++) {
        const ClrInstruction *ins = &method->instructions[i];
        if (ins->label && *ins->label) {
            fprintf(out, "  %s:\n", ins->label);
        }
        if (ins->opcode && *ins->opcode) {
            if (ins->operand && *ins->operand) {
                fprintf(out, "    %s %s\n", ins->opcode, ins->operand);
            } else {
                fprintf(out, "    %s\n", ins->opcode);
            }
        }
    }

    fprintf(out, "  }\n\n");
}

int clr_serialize_il(const ClrProgramModel *model, const char *output_path) {
    if (!model || !output_path) return -1;

    FILE *out = fopen(output_path, "w");
    if (!out) return -1;

    fprintf(out, ".assembly extern mscorlib {}\n");
    fprintf(out, ".assembly %s {}\n", model->assembly_name ? model->assembly_name : "HlvmLab2Clr");
    fprintf(out, ".module %s.exe\n\n", model->assembly_name ? model->assembly_name : "HlvmLab2Clr");

    fprintf(out, ".class public auto ansi beforefieldinit %s extends [mscorlib]System.Object\n", model->class_name);
    fprintf(out, "{\n");

    fprintf(out, "  .method public hidebysig specialname rtspecialname instance void .ctor() cil managed\n");
    fprintf(out, "  {\n");
    fprintf(out, "    ldarg.0\n");
    fprintf(out, "    call instance void [mscorlib]System.Object::.ctor()\n");
    fprintf(out, "    ret\n");
    fprintf(out, "  }\n\n");

    for (int i = 0; i < model->method_count; i++) {
        write_method(out, &model->methods[i]);
    }

    const char *entry_name = "main";
    if (model->entry_index >= 0 && model->entry_index < model->method_count &&
        model->methods[model->entry_index].name) {
        entry_name = model->methods[model->entry_index].name;
    }
    write_runtime_methods(out, model->class_name ? model->class_name : "Program", entry_name);

    fprintf(out, "}\n");

    fclose(out);
    return 0;
}
