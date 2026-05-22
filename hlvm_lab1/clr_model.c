#include "clr_model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "listing.h"

typedef struct {
    CFGNode **items;
    int count;
    int capacity;
} NodeSet;

typedef struct {
    char *name;
    int is_arg;
    int arg_index;
    int local_index;
} VarSymbol;

typedef struct {
    VarSymbol *items;
    int count;
    int capacity;
    int next_arg_index;
    int next_local_index;
} VarTable;

typedef struct {
    ClrInstruction *items;
    int count;
    int capacity;
} InstructionVec;

typedef struct {
    const ClrProgramModel *program;
    ClrMethodModel *method;
    VarTable vars;
    InstructionVec instructions;
    int return_local_index;
    int generated_terminal_ret;
} EmitContext;

static char *dup_string(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *copy = (char *)malloc(n);
    if (!copy) return NULL;
    memcpy(copy, s, n);
    return copy;
}

static int str_eq(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

static int str_ieq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        char ca = *a;
        char cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static void free_instruction(ClrInstruction *ins) {
    if (!ins) return;
    free(ins->label);
    free(ins->opcode);
    free(ins->operand);
}

static void instruction_vec_free(InstructionVec *vec) {
    if (!vec) return;
    for (int i = 0; i < vec->count; i++) {
        free_instruction(&vec->items[i]);
    }
    free(vec->items);
    vec->items = NULL;
    vec->count = 0;
    vec->capacity = 0;
}

static int instruction_vec_push(InstructionVec *vec, const char *label, const char *opcode, const char *operand) {
    if (vec->count >= vec->capacity) {
        int new_cap = vec->capacity ? vec->capacity * 2 : 64;
        ClrInstruction *new_items = (ClrInstruction *)realloc(vec->items, sizeof(ClrInstruction) * (size_t)new_cap);
        if (!new_items) return -1;
        vec->items = new_items;
        vec->capacity = new_cap;
    }

    ClrInstruction *dst = &vec->items[vec->count++];
    dst->label = dup_string(label);
    dst->opcode = dup_string(opcode);
    dst->operand = dup_string(operand);
    return 0;
}

static int node_set_has(NodeSet *set, CFGNode *node) {
    for (int i = 0; i < set->count; i++) {
        if (set->items[i] == node) return 1;
    }
    return 0;
}

static int node_set_add(NodeSet *set, CFGNode *node) {
    if (!node) return 0;
    if (node_set_has(set, node)) return 0;

    if (set->count >= set->capacity) {
        int new_cap = set->capacity ? set->capacity * 2 : 64;
        CFGNode **new_items = (CFGNode **)realloc(set->items, sizeof(CFGNode *) * (size_t)new_cap);
        if (!new_items) return -1;
        set->items = new_items;
        set->capacity = new_cap;
    }

    set->items[set->count++] = node;
    return 0;
}

static void node_set_free(NodeSet *set) {
    if (!set) return;
    free(set->items);
    set->items = NULL;
    set->count = 0;
    set->capacity = 0;
}

static VarSymbol *find_symbol(VarTable *table, const char *name) {
    if (!table || !name || !*name) return NULL;
    for (int i = 0; i < table->count; i++) {
        if (table->items[i].name && strcmp(table->items[i].name, name) == 0) {
            return &table->items[i];
        }
    }
    return NULL;
}

static VarSymbol *add_symbol(VarTable *table, const char *name, int is_arg) {
    if (!table || !name || !*name) return NULL;

    VarSymbol *existing = find_symbol(table, name);
    if (existing) return existing;

    if (table->count >= table->capacity) {
        int new_cap = table->capacity ? table->capacity * 2 : 32;
        VarSymbol *new_items = (VarSymbol *)realloc(table->items, sizeof(VarSymbol) * (size_t)new_cap);
        if (!new_items) return NULL;
        table->items = new_items;
        table->capacity = new_cap;
    }

    VarSymbol *dst = &table->items[table->count++];
    memset(dst, 0, sizeof(*dst));
    dst->name = dup_string(name);
    if (!dst->name) return NULL;

    if (is_arg) {
        dst->is_arg = 1;
        dst->arg_index = table->next_arg_index++;
        dst->local_index = -1;
    } else {
        dst->is_arg = 0;
        dst->arg_index = -1;
        dst->local_index = table->next_local_index++;
    }

    return dst;
}

static void free_var_table(VarTable *table) {
    if (!table) return;
    for (int i = 0; i < table->count; i++) {
        free(table->items[i].name);
    }
    free(table->items);
    table->items = NULL;
    table->count = 0;
    table->capacity = 0;
    table->next_arg_index = 0;
    table->next_local_index = 0;
}

static void collect_arg_defs(TreeNode *list_arg_def, VarTable *vars) {
    if (!list_arg_def || !vars) return;
    if (!list_arg_def->type || strcmp(list_arg_def->type, "listArgDef") != 0) return;

    if (list_arg_def->childrenNumber > 0) {
        TreeNode *arg_def = list_arg_def->childNodes[0];
        if (arg_def && arg_def->type && strcmp(arg_def->type, "argDef") == 0 && arg_def->childrenNumber > 0) {
            TreeNode *name_node = arg_def->childNodes[0];
            if (name_node && name_node->value) {
                add_symbol(vars, name_node->value, 1);
            }
        }
    }

    if (list_arg_def->childrenNumber > 1) {
        collect_arg_defs(list_arg_def->childNodes[1], vars);
    }
}

static void collect_symbols_from_operation(TreeNode *op, VarTable *vars) {
    if (!op || !vars || !op->type) return;

    if (str_eq(op->type, "ARG")) {
        if (op->childrenNumber > 1 && op->childNodes[1] && op->childNodes[1]->value) {
            add_symbol(vars, op->childNodes[1]->value, 1);
        }
        return;
    }

    if (str_eq(op->type, "AS")) {
        if (op->childrenNumber > 1 && op->childNodes[1] && op->childNodes[1]->value) {
            add_symbol(vars, op->childNodes[1]->value, 0);
        }
        return;
    }

    if (str_eq(op->type, "READ")) {
        if (op->childrenNumber > 0 && op->childNodes[0] && op->childNodes[0]->value) {
            add_symbol(vars, op->childNodes[0]->value, 0);
        }
        return;
    }

    if (str_eq(op->type, "SET")) {
        if (op->childrenNumber > 0 && op->childNodes[0] && op->childNodes[0]->value) {
            add_symbol(vars, op->childNodes[0]->value, 0);
        }
        if (op->childrenNumber > 1) {
            collect_symbols_from_operation(op->childNodes[1], vars);
        }
        return;
    }

    if (str_eq(op->type, "EXECUTE")) {
        for (int i = 1; i < op->childrenNumber; i++) {
            collect_symbols_from_operation(op->childNodes[i], vars);
        }
        return;
    }

    for (int i = 0; i < op->childrenNumber; i++) {
        collect_symbols_from_operation(op->childNodes[i], vars);
    }
}

static void collect_symbols_from_cfg(CFGNode *node, VarTable *vars, NodeSet *visited) {
    if (!node || !vars || !visited) return;
    if (node_set_has(visited, node)) return;
    if (node_set_add(visited, node) != 0) return;

    if (node->operationTree) {
        collect_symbols_from_operation(node->operationTree, vars);
    }

    collect_symbols_from_cfg(node->definitely, vars, visited);
    collect_symbols_from_cfg(node->conditionally, vars, visited);
}

static int emit_instruction(EmitContext *ctx, const char *opcode, const char *operand) {
    return instruction_vec_push(&ctx->instructions, NULL, opcode, operand);
}

static int emit_label(EmitContext *ctx, const char *label) {
    return instruction_vec_push(&ctx->instructions, label, NULL, NULL);
}

static int emit_load_symbol(EmitContext *ctx, const char *name) {
    char index_buf[32];
    VarSymbol *sym = find_symbol(&ctx->vars, name);
    if (!sym) {
        sym = add_symbol(&ctx->vars, name, 0);
        if (!sym) return -1;
    }

    if (sym->is_arg) {
        snprintf(index_buf, sizeof(index_buf), "%d", sym->arg_index);
        return emit_instruction(ctx, "ldarg", index_buf);
    }

    snprintf(index_buf, sizeof(index_buf), "%d", sym->local_index);
    return emit_instruction(ctx, "ldloc", index_buf);
}

static int emit_store_symbol(EmitContext *ctx, const char *name) {
    char index_buf[32];
    VarSymbol *sym = find_symbol(&ctx->vars, name);
    if (!sym) {
        sym = add_symbol(&ctx->vars, name, 0);
        if (!sym) return -1;
    }

    if (sym->is_arg) {
        snprintf(index_buf, sizeof(index_buf), "%d", sym->arg_index);
        return emit_instruction(ctx, "starg", index_buf);
    }

    snprintf(index_buf, sizeof(index_buf), "%d", sym->local_index);
    return emit_instruction(ctx, "stloc", index_buf);
}

static int emit_binary_operation(EmitContext *ctx, const char *op_type);
static int emit_operation_tree(EmitContext *ctx, TreeNode *op);

static int emit_const(EmitContext *ctx, TreeNode *op) {
    if (!op || op->childrenNumber < 2) return -1;

    const char *type_name = op->childNodes[0] ? op->childNodes[0]->value : NULL;
    const char *raw = op->childNodes[1] ? op->childNodes[1]->value : NULL;

    long long value = 0;
    if (type_name && str_ieq(type_name, "bool")) {
        value = (raw && str_ieq(raw, "true")) ? 1 : 0;
    } else if (type_name && str_ieq(type_name, "char")) {
        value = (raw && raw[0]) ? (unsigned char)raw[0] : 0;
    } else {
        value = raw ? atoll(raw) : 0;
    }

    char imm[64];
    snprintf(imm, sizeof(imm), "%lld", value);
    return emit_instruction(ctx, "ldc.i8", imm);
}

static int emit_call(EmitContext *ctx, TreeNode *op) {
    if (!op || op->childrenNumber < 1) return -1;

    TreeNode *callee_node = op->childNodes[0];
    const char *callee = callee_node ? callee_node->value : NULL;
    if (!callee || !*callee) return -1;

    for (int i = 1; i < op->childrenNumber; i++) {
        if (emit_operation_tree(ctx, op->childNodes[i]) < 0) return -1;
    }

    char call_sig[512];
    char args_sig[256];
    args_sig[0] = '\0';
    for (int i = 1; i < op->childrenNumber; i++) {
        if (i > 1) strncat(args_sig, ", ", sizeof(args_sig) - strlen(args_sig) - 1);
        strncat(args_sig, "int64", sizeof(args_sig) - strlen(args_sig) - 1);
    }

    snprintf(call_sig, sizeof(call_sig), "int64 %s::%s(%s)", ctx->program->class_name, callee, args_sig);
    return emit_instruction(ctx, "call", call_sig);
}

static int emit_binary_operation(EmitContext *ctx, const char *op_type) {
    if (!op_type) return -1;

    if (str_eq(op_type, "SUM")) return emit_instruction(ctx, "add", NULL);
    if (str_eq(op_type, "SUB")) return emit_instruction(ctx, "sub", NULL);
    if (str_eq(op_type, "MUL")) return emit_instruction(ctx, "mul", NULL);
    if (str_eq(op_type, "DIV") || str_eq(op_type, "SLASH")) return emit_instruction(ctx, "div", NULL);
    if (str_eq(op_type, "PERCENT")) return emit_instruction(ctx, "rem", NULL);
    if (str_eq(op_type, "AND")) return emit_instruction(ctx, "and", NULL);
    if (str_eq(op_type, "OR")) return emit_instruction(ctx, "or", NULL);

    if (str_eq(op_type, "EQUALITY")) {
        if (emit_instruction(ctx, "ceq", NULL) < 0) return -1;
        return emit_instruction(ctx, "conv.i8", NULL);
    }

    if (str_eq(op_type, "NOTEQUAL")) {
        if (emit_instruction(ctx, "ceq", NULL) < 0) return -1;
        if (emit_instruction(ctx, "ldc.i4.0", NULL) < 0) return -1;
        if (emit_instruction(ctx, "ceq", NULL) < 0) return -1;
        return emit_instruction(ctx, "conv.i8", NULL);
    }

    if (str_eq(op_type, "LESSTHAN")) {
        if (emit_instruction(ctx, "clt", NULL) < 0) return -1;
        return emit_instruction(ctx, "conv.i8", NULL);
    }

    if (str_eq(op_type, "GREATERTHAN")) {
        if (emit_instruction(ctx, "cgt", NULL) < 0) return -1;
        return emit_instruction(ctx, "conv.i8", NULL);
    }

    if (str_eq(op_type, "LESSTHANEQ")) {
        if (emit_instruction(ctx, "cgt", NULL) < 0) return -1;
        if (emit_instruction(ctx, "ldc.i4.0", NULL) < 0) return -1;
        if (emit_instruction(ctx, "ceq", NULL) < 0) return -1;
        return emit_instruction(ctx, "conv.i8", NULL);
    }

    if (str_eq(op_type, "GREATERTHANEQ")) {
        if (emit_instruction(ctx, "clt", NULL) < 0) return -1;
        if (emit_instruction(ctx, "ldc.i4.0", NULL) < 0) return -1;
        if (emit_instruction(ctx, "ceq", NULL) < 0) return -1;
        return emit_instruction(ctx, "conv.i8", NULL);
    }

    return -1;
}

static int emit_operation_tree(EmitContext *ctx, TreeNode *op) {
    if (!op || !op->type) return -1;

    if (str_eq(op->type, "ARG")) {
        return 0;
    }

    if (str_eq(op->type, "AS")) {
        if (emit_instruction(ctx, "ldc.i8", "0") < 0) return -1;
        if (op->childrenNumber > 1 && op->childNodes[1] && op->childNodes[1]->value) {
            return emit_store_symbol(ctx, op->childNodes[1]->value);
        }
        return 0;
    }

    if (str_eq(op->type, "CONST")) {
        return emit_const(ctx, op);
    }

    if (str_eq(op->type, "READ")) {
        if (op->childrenNumber > 0 && op->childNodes[0] && op->childNodes[0]->value) {
            return emit_load_symbol(ctx, op->childNodes[0]->value);
        }
        return -1;
    }

    if (str_eq(op->type, "SET")) {
        if (op->childrenNumber < 2) return -1;
        if (emit_operation_tree(ctx, op->childNodes[1]) < 0) return -1;
        if (emit_instruction(ctx, "dup", NULL) < 0) return -1;
        if (op->childNodes[0] && op->childNodes[0]->value) {
            return emit_store_symbol(ctx, op->childNodes[0]->value);
        }
        return -1;
    }

    if (str_eq(op->type, "EXECUTE")) {
        return emit_call(ctx, op);
    }

    if (str_eq(op->type, "NOT")) {
        if (op->childrenNumber < 1) return -1;
        if (emit_operation_tree(ctx, op->childNodes[0]) < 0) return -1;
        if (emit_instruction(ctx, "ldc.i8", "0") < 0) return -1;
        if (emit_instruction(ctx, "ceq", NULL) < 0) return -1;
        return emit_instruction(ctx, "conv.i8", NULL);
    }

    if (op->childrenNumber == 2) {
        if (emit_operation_tree(ctx, op->childNodes[0]) < 0) return -1;
        if (emit_operation_tree(ctx, op->childNodes[1]) < 0) return -1;
        return emit_binary_operation(ctx, op->type);
    }

    return -1;
}

static int is_value_statement(TreeNode *op) {
    if (!op || !op->type) return 0;
    return !str_eq(op->type, "ARG") && !str_eq(op->type, "AS");
}

static void reset_checked_recursive(CFGNode *node, NodeSet *visited) {
    if (!node || !visited) return;
    if (node_set_has(visited, node)) return;
    if (node_set_add(visited, node) != 0) return;

    if (node->listingNode) {
        node->listingNode->checked = 1;
    }

    reset_checked_recursive(node->definitely, visited);
    reset_checked_recursive(node->conditionally, visited);
}

static int emit_cfg_node(EmitContext *ctx, CFGNode *node) {
    if (!ctx || !node || !node->listingNode) return -1;

    if (node->listingNode->label && *node->listingNode->label) {
        if (emit_label(ctx, node->listingNode->label) < 0) return -1;
    }

    node->listingNode->checked++;

    if (node->operationTree) {
        if (emit_operation_tree(ctx, node->operationTree) < 0) return -1;

        if (node->conditionally) {
            if (emit_instruction(ctx, "conv.i4", NULL) < 0) return -1;
            if (emit_instruction(ctx, "brtrue", node->conditionally->listingNode->label) < 0) return -1;
        } else if (is_value_statement(node->operationTree)) {
            char ret_idx[16];
            snprintf(ret_idx, sizeof(ret_idx), "%d", ctx->return_local_index);
            if (emit_instruction(ctx, "stloc", ret_idx) < 0) return -1;
        }
    }

    if (!node->definitely) {
        char ret_idx[16];
        snprintf(ret_idx, sizeof(ret_idx), "%d", ctx->return_local_index);
        if (emit_instruction(ctx, "ldloc", ret_idx) < 0) return -1;
        if (emit_instruction(ctx, "ret", NULL) < 0) return -1;
        ctx->generated_terminal_ret = 1;
        return 0;
    }

    if (node->definitely->listingNode && node->definitely->listingNode->checked > 1) {
        if (emit_instruction(ctx, "br", node->definitely->listingNode->label) < 0) return -1;
    } else {
        if (emit_cfg_node(ctx, node->definitely) < 0) return -1;
    }

    if (node->conditionally && node->conditionally->listingNode && node->conditionally->listingNode->checked <= 1) {
        if (emit_cfg_node(ctx, node->conditionally) < 0) return -1;
    }

    return 0;
}

static int fill_method_model(const ClrProgramModel *program,
                             FunExecution *fun,
                             ClrMethodModel *out_method,
                             char **error_text) {
    if (!program || !fun || !out_method) return -1;

    memset(out_method, 0, sizeof(*out_method));
    out_method->name = dup_string(fun->name ? fun->name : "anonymous");
    if (!out_method->name) return -1;

    VarTable vars;
    memset(&vars, 0, sizeof(vars));
    vars.next_arg_index = 0;
    vars.next_local_index = 1;

    if (fun->signature) {
        for (int i = 0; i < fun->signature->childrenNumber; i++) {
            TreeNode *child = fun->signature->childNodes[i];
            if (child && child->type && strcmp(child->type, "listArgDef") == 0) {
                collect_arg_defs(child, &vars);
            }
        }
    }

    NodeSet visited_collect = {0};
    collect_symbols_from_cfg(fun->nodes, &vars, &visited_collect);
    node_set_free(&visited_collect);

    EmitContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.program = program;
    ctx.method = out_method;
    ctx.vars = vars;
    ctx.return_local_index = 0;

    NodeSet visited_reset = {0};
    reset_checked_recursive(fun->nodes, &visited_reset);
    node_set_free(&visited_reset);

    if (emit_cfg_node(&ctx, fun->nodes) < 0) {
        if (error_text) {
            *error_text = dup_string("Failed to emit CIL instructions from CFG");
        }
        free_var_table(&ctx.vars);
        instruction_vec_free(&ctx.instructions);
        return -1;
    }

    if (!ctx.generated_terminal_ret) {
        char ret_idx[16];
        snprintf(ret_idx, sizeof(ret_idx), "%d", ctx.return_local_index);
        if (emit_instruction(&ctx, "ldloc", ret_idx) < 0 || emit_instruction(&ctx, "ret", NULL) < 0) {
            free_var_table(&ctx.vars);
            instruction_vec_free(&ctx.instructions);
            return -1;
        }
    }

    out_method->param_count = ctx.vars.next_arg_index;
    out_method->local_count = ctx.vars.next_local_index;
    out_method->local_names = (char **)calloc((size_t)out_method->local_count, sizeof(char *));
    if (!out_method->local_names) {
        free_var_table(&ctx.vars);
        instruction_vec_free(&ctx.instructions);
        return -1;
    }

    out_method->local_names[0] = dup_string("__ret");
    for (int i = 0; i < ctx.vars.count; i++) {
        VarSymbol *sym = &ctx.vars.items[i];
        if (!sym->is_arg && sym->local_index >= 0 && sym->local_index < out_method->local_count) {
            free(out_method->local_names[sym->local_index]);
            out_method->local_names[sym->local_index] = dup_string(sym->name);
        }
    }

    out_method->instruction_count = ctx.instructions.count;
    out_method->instructions = ctx.instructions.items;

    ctx.instructions.items = NULL;
    ctx.instructions.count = 0;
    ctx.instructions.capacity = 0;

    free_var_table(&ctx.vars);
    return 0;
}

int clr_build_program_model(Array *fun_executions,
                            const char *assembly_name,
                            const char *class_name,
                            ClrProgramModel *out_model,
                            char **error_text) {
    if (!fun_executions || !out_model) return -1;

    memset(out_model, 0, sizeof(*out_model));
    out_model->assembly_name = dup_string(assembly_name && *assembly_name ? assembly_name : "HlvmLab1Clr");
    out_model->class_name = dup_string(class_name && *class_name ? class_name : "Program");
    out_model->entry_index = -1;

    if (!out_model->assembly_name || !out_model->class_name) {
        clr_free_program_model(out_model);
        return -1;
    }

    placeLabels(fun_executions);

    out_model->method_count = fun_executions->nextPosition;
    out_model->methods = (ClrMethodModel *)calloc((size_t)out_model->method_count, sizeof(ClrMethodModel));
    if (!out_model->methods) {
        clr_free_program_model(out_model);
        return -1;
    }

    for (int i = 0; i < fun_executions->nextPosition; i++) {
        FunExecution *fun = (FunExecution *)fun_executions->elements[i];
        if (fill_method_model(out_model, fun, &out_model->methods[i], error_text) < 0) {
            clr_free_program_model(out_model);
            return -1;
        }
        if (fun && fun->name && strcmp(fun->name, "main") == 0) {
            out_model->entry_index = i;
        }
    }

    if (out_model->entry_index < 0 && out_model->method_count > 0) {
        out_model->entry_index = 0;
    }

    return 0;
}

void clr_free_program_model(ClrProgramModel *model) {
    if (!model) return;

    free(model->assembly_name);
    free(model->class_name);

    for (int i = 0; i < model->method_count; i++) {
        ClrMethodModel *method = &model->methods[i];
        free(method->name);

        for (int j = 0; j < method->local_count; j++) {
            free(method->local_names ? method->local_names[j] : NULL);
        }
        free(method->local_names);

        for (int j = 0; j < method->instruction_count; j++) {
            free_instruction(&method->instructions[j]);
        }
        free(method->instructions);
    }

    free(model->methods);
    memset(model, 0, sizeof(*model));
}
