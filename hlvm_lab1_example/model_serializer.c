#include "model_serializer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_indent(FILE *out, int indent) {
    for (int i = 0; i < indent; i++) {
        fputs("  ", out);
    }
}

static void write_json_string(FILE *out, const char *s) {
    fputc('"', out);
    if (s) {
        for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
            switch (*p) {
                case '"':
                    fputs("\\\"", out);
                    break;
                case '\\':
                    fputs("\\\\", out);
                    break;
                case '\b':
                    fputs("\\b", out);
                    break;
                case '\f':
                    fputs("\\f", out);
                    break;
                case '\n':
                    fputs("\\n", out);
                    break;
                case '\r':
                    fputs("\\r", out);
                    break;
                case '\t':
                    fputs("\\t", out);
                    break;
                default:
                    if (*p < 0x20) {
                        fprintf(out, "\\u%04x", *p);
                    } else {
                        fputc((char)*p, out);
                    }
                    break;
            }
        }
    }
    fputc('"', out);
}

static const char *register_to_string(Register reg) {
    static const char *names[] = {
        "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
        "sp", "bp", "ip", "flags", "port", "stdin", "stdout"
    };

    if (reg >= 0 && reg < (int)(sizeof(names) / sizeof(names[0]))) {
        return names[reg];
    }
    return "unknown";
}

static const char *instruction_to_string(InstructionCode opcode) {
    switch (opcode) {
        case NOP: return "nop";
        case HLT: return "hlt";
        case LDC: return "ldc";
        case MOV_REG_REG: return "mov";
        case MOV_REG_MEM_IMM: return "mov";
        case MOV_MEM_IMM_REG: return "mov";
        case MOV_REG_MEM_REG: return "mov";
        case MOV_MEM_REG_REG: return "mov";
        case ADD: return "add";
        case SUB: return "sub";
        case MUL: return "mul";
        case DIV: return "div";
        case MOD: return "mod";
        case BAND: return "band";
        case BOR: return "bor";
        case BXOR: return "bxor";
        case BNOT: return "bnot";
        case SHL: return "shl";
        case SHR: return "shr";
        case CMP: return "cmp";
        case JMP: return "jmp";
        case JE: return "je";
        case JNE: return "jne";
        case JL: return "jl";
        case JLE: return "jle";
        case JG: return "jg";
        case JGE: return "jge";
        case PUSH: return "push";
        case POP: return "pop";
        case CALL: return "call";
        case RET: return "ret";
        case SETPORT: return "setport";
        case IN: return "in";
        case OUT: return "out";
        default: return "unknown";
    }
}

static const char *jvm_analogue(InstructionCode opcode) {
    switch (opcode) {
        case NOP: return "nop";
        case HLT: return "return";
        case LDC: return "ldc";
        case MOV_REG_REG:
        case MOV_REG_MEM_IMM:
        case MOV_MEM_IMM_REG:
        case MOV_REG_MEM_REG:
        case MOV_MEM_REG_REG:
            return "iload/istore";
        case ADD: return "iadd";
        case SUB: return "isub";
        case MUL: return "imul";
        case DIV: return "idiv";
        case MOD: return "irem";
        case BAND: return "iand";
        case BOR: return "ior";
        case BXOR: return "ixor";
        case BNOT: return "iconst_m1 + ixor";
        case SHL: return "ishl";
        case SHR: return "ishr";
        case CMP: return "if_icmp*";
        case JMP: return "goto";
        case JE: return "ifeq";
        case JNE: return "ifne";
        case JL: return "iflt";
        case JLE: return "ifle";
        case JG: return "ifgt";
        case JGE: return "ifge";
        case PUSH: return "push";
        case POP: return "pop";
        case CALL: return "invokestatic";
        case RET: return "ireturn/return";
        case SETPORT: return "getstatic/putstatic";
        case IN: return "invokestatic(read)";
        case OUT: return "invokestatic(write)";
        default: return "unknown";
    }
}

static const char *operand_type_to_string(OperandType type) {
    switch (type) {
        case OP_NONE: return "none";
        case OP_REGISTER: return "register";
        case OP_IMMEDIATE64: return "imm64";
        case OP_IMMEDIATE16: return "imm16";
        case OP_ADDRESS: return "address";
        case OP_LABEL: return "label";
        default: return "unknown";
    }
}

static const char *operation_type_to_string(OperationType type) {
    switch (type) {
        case OP_CALL: return "call";
        case OP_ASSIGN: return "assign";
        case OP_VAR_DECL: return "var_decl";
        case OP_BINARY_OP: return "binary_op";
        case OP_LITERAL: return "literal";
        case OP_RETURN: return "return";
        case OP_BREAK: return "break";
        case OP_CONTINUE: return "continue";
        case OP_NOOP: return "noop";
        default: return "unknown";
    }
}

static void write_operand(FILE *out, const Operand *op, int indent) {
    write_indent(out, indent);
    fputs("{\n", out);

    write_indent(out, indent + 1);
    fputs("\"type\": ", out);
    write_json_string(out, operand_type_to_string(op->type));
    fputs(",\n", out);

    write_indent(out, indent + 1);
    fputs("\"value\": ", out);

    switch (op->type) {
        case OP_REGISTER:
            write_json_string(out, register_to_string(op->value.reg));
            break;
        case OP_IMMEDIATE64:
            fprintf(out, "%lld", (long long)op->value.imm64);
            break;
        case OP_IMMEDIATE16:
            fprintf(out, "%d", op->value.imm16);
            break;
        case OP_ADDRESS:
            fprintf(out, "%u", (unsigned int)op->value.addr);
            break;
        case OP_LABEL:
            write_json_string(out, op->value.label ? op->value.label : "");
            break;
        case OP_NONE:
        default:
            write_json_string(out, "");
            break;
    }

    fputc('\n', out);
    write_indent(out, indent);
    fputc('}', out);
}

static int has_operand(const Operand *op) {
    return op && op->type != OP_NONE;
}

static void write_instruction(FILE *out, const Instruction *instr, int index, int indent) {
    write_indent(out, indent);
    fputs("{\n", out);

    write_indent(out, indent + 1);
    fprintf(out, "\"index\": %d,\n", index);

    write_indent(out, indent + 1);
    fputs("\"opcode\": ", out);
    write_json_string(out, instruction_to_string(instr->opcode));
    fputs(",\n", out);

    write_indent(out, indent + 1);
    fputs("\"jvmAnalogue\": ", out);
    write_json_string(out, jvm_analogue(instr->opcode));
    fputs(",\n", out);

    write_indent(out, indent + 1);
    fputs("\"operands\": [\n", out);

    int operand_written = 0;
    for (int i = 0; i < 2; i++) {
        if (!has_operand(&instr->operands[i])) continue;
        if (operand_written > 0) {
            fputs(",\n", out);
        }
        write_operand(out, &instr->operands[i], indent + 2);
        operand_written++;
    }
    if (operand_written > 0) {
        fputc('\n', out);
    }

    write_indent(out, indent + 1);
    fputs("],\n", out);

    write_indent(out, indent + 1);
    fputs("\"label\": ", out);
    write_json_string(out, instr->label ? instr->label : "");
    fputs(",\n", out);

    write_indent(out, indent + 1);
    fputs("\"comment\": ", out);
    write_json_string(out, instr->comment ? instr->comment : "");
    fputc('\n', out);

    write_indent(out, indent);
    fputc('}', out);
}

static void write_cfg_functions(FILE *out, const CFG *cfg, int indent) {
    write_indent(out, indent);
    fputs("\"cfgFunctions\": [\n", out);

    for (int i = 0; i < cfg->func_count; i++) {
        FunctionCFG *func = cfg->functions[i];

        write_indent(out, indent + 1);
        fputs("{\n", out);

        write_indent(out, indent + 2);
        fputs("\"name\": ", out);
        write_json_string(out, (func && func->info && func->info->name) ? func->info->name : "anonymous");
        fputs(",\n", out);

        write_indent(out, indent + 2);
        fputs("\"sourceFile\": ", out);
        write_json_string(out, (func && func->info && func->info->source_file) ? func->info->source_file : "");
        fputs(",\n", out);

        write_indent(out, indent + 2);
        fputs("\"parameters\": [\n", out);

        int param_count = (func && func->info) ? func->info->param_count : 0;
        for (int p = 0; p < param_count; p++) {
            write_indent(out, indent + 3);
            fputs("{\n", out);

            write_indent(out, indent + 4);
            fputs("\"name\": ", out);
            write_json_string(out, func->info->params[p] ? func->info->params[p] : "");
            fputs(",\n", out);

            write_indent(out, indent + 4);
            fputs("\"type\": ", out);
            write_json_string(out, func->info->param_types[p] ? func->info->param_types[p] : "int");
            fputc('\n', out);

            write_indent(out, indent + 3);
            fputs((p + 1 < param_count) ? "},\n" : "}\n", out);
        }

        write_indent(out, indent + 2);
        fputs("],\n", out);

        write_indent(out, indent + 2);
        fputs("\"basicBlocks\": [\n", out);

        int node_count = func ? func->node_count : 0;
        for (int n = 0; n < node_count; n++) {
            CFGNode *node = func->nodes[n];

            write_indent(out, indent + 3);
            fputs("{\n", out);

            write_indent(out, indent + 4);
            fprintf(out, "\"id\": %d,\n", node ? node->id : -1);

            write_indent(out, indent + 4);
            fputs("\"label\": ", out);
            write_json_string(out, (node && node->label) ? node->label : "");
            fputs(",\n", out);

            write_indent(out, indent + 4);
            fprintf(out, "\"trueBranchId\": %d,\n", (node && node->true_branch) ? node->true_branch->id : -1);

            write_indent(out, indent + 4);
            fprintf(out, "\"nextId\": %d,\n", (node && node->next) ? node->next->id : -1);

            write_indent(out, indent + 4);
            fputs("\"operations\": [\n", out);

            int op_index = 0;
            for (Operation *op = node ? node->operations : NULL; op; op = op->next) {
                write_indent(out, indent + 5);
                fputs("{\n", out);

                write_indent(out, indent + 6);
                fputs("\"index\": ", out);
                fprintf(out, "%d,\n", op_index++);

                write_indent(out, indent + 6);
                fputs("\"type\": ", out);
                write_json_string(out, operation_type_to_string(op->type));
                fputs(",\n", out);

                write_indent(out, indent + 6);
                fputs("\"text\": ", out);
                write_json_string(out, op->text ? op->text : "");
                fputc('\n', out);

                write_indent(out, indent + 5);
                fputs(op->next ? "},\n" : "}\n", out);
            }

            write_indent(out, indent + 4);
            fputs("]\n", out);

            write_indent(out, indent + 3);
            fputs((n + 1 < node_count) ? "},\n" : "}\n", out);
        }

        write_indent(out, indent + 2);
        fputs("]\n", out);

        write_indent(out, indent + 1);
        fputs((i + 1 < cfg->func_count) ? "},\n" : "}\n", out);
    }

    write_indent(out, indent);
    fputs("],\n", out);
}

static void write_call_graph(FILE *out, const CFG *cfg, int indent) {
    write_indent(out, indent);
    fputs("\"callGraph\": [\n", out);

    for (int i = 0; i < cfg->call_edge_count; i++) {
        const char *edge = cfg->call_edges[i] ? cfg->call_edges[i] : "";
        const char *sep = strstr(edge, "->");

        char caller[256] = {0};
        char callee[256] = {0};

        if (sep) {
            size_t caller_len = (size_t)(sep - edge);
            if (caller_len >= sizeof(caller)) caller_len = sizeof(caller) - 1;
            memcpy(caller, edge, caller_len);
            caller[caller_len] = '\0';

            strncpy(callee, sep + 2, sizeof(callee) - 1);
            callee[sizeof(callee) - 1] = '\0';
        }

        write_indent(out, indent + 1);
        fputs("{\n", out);

        write_indent(out, indent + 2);
        fputs("\"caller\": ", out);
        write_json_string(out, sep ? caller : edge);
        fputs(",\n", out);

        write_indent(out, indent + 2);
        fputs("\"callee\": ", out);
        write_json_string(out, sep ? callee : "");
        fputc('\n', out);

        write_indent(out, indent + 1);
        fputs((i + 1 < cfg->call_edge_count) ? "},\n" : "}\n", out);
    }

    write_indent(out, indent);
    fputs("],\n", out);
}

static void write_user_types(FILE *out, const CFG *cfg, int indent) {
    write_indent(out, indent);
    fputs("\"userTypes\": [\n", out);

    for (int i = 0; i < cfg->type_count; i++) {
        const UserTypeInfo *type = &cfg->user_types[i];

        write_indent(out, indent + 1);
        fputs("{\n", out);

        write_indent(out, indent + 2);
        fputs("\"name\": ", out);
        write_json_string(out, type->name ? type->name : "");
        fputs(",\n", out);

        write_indent(out, indent + 2);
        fputs("\"baseType\": ", out);
        write_json_string(out, type->base_name ? type->base_name : "");
        fputs(",\n", out);

        write_indent(out, indent + 2);
        fputs("\"fields\": [\n", out);

        for (int f = 0; f < type->field_count; f++) {
            write_indent(out, indent + 3);
            fputs("{\n", out);

            write_indent(out, indent + 4);
            fputs("\"name\": ", out);
            write_json_string(out, type->fields[f].name ? type->fields[f].name : "");
            fputs(",\n", out);

            write_indent(out, indent + 4);
            fputs("\"type\": ", out);
            write_json_string(out, type->fields[f].type_name ? type->fields[f].type_name : "int");
            fputc('\n', out);

            write_indent(out, indent + 3);
            fputs((f + 1 < type->field_count) ? "},\n" : "}\n", out);
        }

        write_indent(out, indent + 2);
        fputs("]\n", out);

        write_indent(out, indent + 1);
        fputs((i + 1 < cfg->type_count) ? "},\n" : "}\n", out);
    }

    write_indent(out, indent);
    fputs("],\n", out);
}

static void write_linear_functions(FILE *out, const ProgramImage *image, int indent) {
    write_indent(out, indent);
    fputs("\"linearFunctions\": [\n", out);

    for (int i = 0; i < image->func_count; i++) {
        const CodeBlock *block = &image->functions[i];

        write_indent(out, indent + 1);
        fputs("{\n", out);

        write_indent(out, indent + 2);
        fputs("\"name\": ", out);
        write_json_string(out, block->name ? block->name : "");
        fputs(",\n", out);

        write_indent(out, indent + 2);
        fputs("\"instructions\": [\n", out);

        for (int j = 0; j < block->instruction_count; j++) {
            write_instruction(out, &block->instructions[j], j, indent + 3);
            fputs((j + 1 < block->instruction_count) ? ",\n" : "\n", out);
        }

        write_indent(out, indent + 2);
        fputs("]\n", out);

        write_indent(out, indent + 1);
        fputs((i + 1 < image->func_count) ? "},\n" : "}\n", out);
    }

    write_indent(out, indent);
    fputs("]\n", out);
}

static const char *detect_entry_point(const CFG *cfg, const ProgramImage *image) {
    if (image) {
        for (int i = 0; i < image->func_count; i++) {
            if (image->functions[i].name && strcmp(image->functions[i].name, "main") == 0) {
                return "main";
            }
        }
    }
    if (cfg) {
        for (int i = 0; i < cfg->func_count; i++) {
            if (cfg->functions[i] && cfg->functions[i]->info &&
                cfg->functions[i]->info->name &&
                strcmp(cfg->functions[i]->info->name, "main") == 0) {
                return "main";
            }
        }
    }
    if (image && image->entry_point && *image->entry_point) {
        return image->entry_point;
    }
    if (cfg && cfg->func_count > 0 && cfg->functions[0] && cfg->functions[0]->info &&
        cfg->functions[0]->info->name && *cfg->functions[0]->info->name) {
        return cfg->functions[0]->info->name;
    }
    return "main";
}

int write_program_model_json(const CFG *cfg, const ProgramImage *image, const char *path) {
    if (!cfg || !image || !path || !*path) {
        return 1;
    }

    FILE *out = fopen(path, "w");
    if (!out) {
        return 2;
    }

    fputs("{\n", out);

    write_indent(out, 1);
    fputs("\"lab\": ", out);
    write_json_string(out, "HLVM Task 1");
    fputs(",\n", out);

    write_indent(out, 1);
    fputs("\"variant\": 5,\n", out);

    write_indent(out, 1);
    fputs("\"targetVm\": {\n", out);

    write_indent(out, 2);
    fputs("\"name\": ", out);
    write_json_string(out, "JVM");
    fputs(",\n", out);

    write_indent(out, 2);
    fputs("\"executionModel\": ", out);
    write_json_string(out, "Stack-based virtual machine");
    fputs(",\n", out);

    write_indent(out, 2);
    fputs("\"binaryFormat\": ", out);
    write_json_string(out, "Class file (conceptual mapping)");
    fputc('\n', out);

    write_indent(out, 1);
    fputs("},\n", out);

    write_indent(out, 1);
    fputs("\"memoryModel\": {\n", out);

    write_indent(out, 2);
    fputs("\"frames\": ", out);
    write_json_string(out, "Per-method stack frames");
    fputs(",\n", out);

    write_indent(out, 2);
    fputs("\"operandStack\": ", out);
    write_json_string(out, "Expression evaluation stack");
    fputs(",\n", out);

    write_indent(out, 2);
    fputs("\"locals\": ", out);
    write_json_string(out, "Local variable slots");
    fputs(",\n", out);

    write_indent(out, 2);
    fputs("\"heap\": ", out);
    write_json_string(out, "Object/array instances");
    fputs(",\n", out);

    write_indent(out, 2);
    fputs("\"metaArea\": ", out);
    write_json_string(out, "Class metadata and constants");
    fputc('\n', out);

    write_indent(out, 1);
    fputs("},\n", out);

    write_indent(out, 1);
    fputs("\"program\": {\n", out);

    write_indent(out, 2);
    fputs("\"entryPoint\": ", out);
    write_json_string(out, detect_entry_point(cfg, image));
    fputs(",\n", out);

    write_cfg_functions(out, cfg, 2);
    write_call_graph(out, cfg, 2);
    write_user_types(out, cfg, 2);
    write_linear_functions(out, image, 2);

    write_indent(out, 1);
    fputs("}\n", out);

    fputs("}\n", out);

    fclose(out);
    return 0;
}
