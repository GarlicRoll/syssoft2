#ifndef HLVM_LAB1_CLR_MODEL_H
#define HLVM_LAB1_CLR_MODEL_H

#include "cfg.h"

typedef struct {
    char *label;
    char *opcode;
    char *operand;
} ClrInstruction;

typedef struct {
    char *name;
    int param_count;
    int local_count;
    char **local_names;
    int instruction_count;
    ClrInstruction *instructions;
} ClrMethodModel;

typedef struct {
    char *assembly_name;
    char *class_name;
    int entry_index;
    int method_count;
    ClrMethodModel *methods;
} ClrProgramModel;

int clr_build_program_model(Array *fun_executions,
                            const char *assembly_name,
                            const char *class_name,
                            ClrProgramModel *out_model,
                            char **error_text);

void clr_free_program_model(ClrProgramModel *model);

#endif
