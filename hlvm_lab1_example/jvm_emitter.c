#include "jvm_emitter.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    JAVA_VAR_LONG,
    JAVA_VAR_STRING,
    JAVA_VAR_LONG_ARRAY,
    JAVA_VAR_STRING_ARRAY
} JavaVarKind;

typedef struct {
    char *orig;
    char *jname;
    JavaVarKind kind;
    int array_size;
    int is_param;
} JavaVar;

typedef struct {
    JavaVar *items;
    int count;
    int capacity;
} JavaVarTable;

typedef struct {
    const CFG *cfg;
    FunctionCFG *func;
    JavaVarTable vars;
    char func_name[256];
    char func_method[300];
} JavaFnCtx;

static char *strdup_safe(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *r = (char *)malloc(n);
    if (!r) return NULL;
    memcpy(r, s, n);
    return r;
}

static int is_ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_' || c == '$';
}

static int is_ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_' || c == '$';
}

static int is_token_ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_' || c == '$' || c == '.';
}

static void sanitize_ident(const char *src, char *dst, size_t dst_sz) {
    if (!dst || dst_sz == 0) return;
    dst[0] = '\0';
    if (!src || !*src) return;

    size_t di = 0;
    if (!is_ident_start(src[0])) {
        if (di + 1 < dst_sz) dst[di++] = '_';
    }

    for (size_t i = 0; src[i] && di + 1 < dst_sz; i++) {
        unsigned char c = (unsigned char)src[i];
        if (is_ident_char((char)c)) dst[di++] = (char)c;
        else dst[di++] = '_';
    }
    dst[di] = '\0';
}

static void trim_inplace(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = '\0';
    size_t i = 0;
    while (s[i] && isspace((unsigned char)s[i])) i++;
    if (i > 0) memmove(s, s + i, strlen(s + i) + 1);
}

static int str_ieq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int starts_with_ci(const char *s, const char *prefix) {
    if (!s || !prefix) return 0;
    while (*prefix) {
        if (!*s) return 0;
        if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix)) return 0;
        s++;
        prefix++;
    }
    return 1;
}

static int is_builtin_scalar_type(const char *type_name) {
    if (!type_name || !*type_name) return 1;
    return str_ieq(type_name, "int") ||
           str_ieq(type_name, "uint") ||
           str_ieq(type_name, "long") ||
           str_ieq(type_name, "ulong") ||
           str_ieq(type_name, "bool") ||
           str_ieq(type_name, "byte") ||
           str_ieq(type_name, "char") ||
           str_ieq(type_name, "string");
}

static const UserTypeInfo *find_user_type_info(const CFG *cfg, const char *name) {
    if (!cfg || !name || !*name) return NULL;
    for (int i = 0; i < cfg->type_count; i++) {
        if (cfg->user_types[i].name && strcmp(cfg->user_types[i].name, name) == 0) {
            return &cfg->user_types[i];
        }
    }
    return NULL;
}

static const char *find_ci(const char *s, const char *needle) {
    if (!s || !needle || !*needle) return s;
    size_t nlen = strlen(needle);
    for (const char *p = s; *p; p++) {
        size_t i = 0;
        while (i < nlen && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) {
            i++;
        }
        if (i == nlen) return p;
    }
    return NULL;
}

static void fn_method_name(const char *raw_name, char *out, size_t out_sz) {
    char tmp[256];
    sanitize_ident(raw_name ? raw_name : "anonymous", tmp, sizeof(tmp));
    snprintf(out, out_sz, "%s__sl", tmp[0] ? tmp : "anonymous");
}

static JavaVarKind java_kind_from_type(const char *type_name, int is_array) {
    int is_string = type_name && str_ieq(type_name, "string");
    if (is_array) return is_string ? JAVA_VAR_STRING_ARRAY : JAVA_VAR_LONG_ARRAY;
    return is_string ? JAVA_VAR_STRING : JAVA_VAR_LONG;
}

static const char *java_param_type(const char *type_name) {
    return (type_name && str_ieq(type_name, "string")) ? "String" : "long";
}

static void var_table_free(JavaVarTable *vt) {
    if (!vt) return;
    for (int i = 0; i < vt->count; i++) {
        free(vt->items[i].orig);
        free(vt->items[i].jname);
    }
    free(vt->items);
    vt->items = NULL;
    vt->count = 0;
    vt->capacity = 0;
}

static JavaVar *var_table_find(JavaVarTable *vt, const char *orig) {
    if (!vt || !orig || !*orig) return NULL;
    for (int i = 0; i < vt->count; i++) {
        if (vt->items[i].orig && strcmp(vt->items[i].orig, orig) == 0) return &vt->items[i];
    }
    return NULL;
}

static JavaVar *var_table_add(JavaVarTable *vt, const char *orig, JavaVarKind kind, int array_size, int is_param) {
    if (!vt || !orig || !*orig) return NULL;

    JavaVar *existing = var_table_find(vt, orig);
    if (existing) {
        if (is_param) existing->is_param = 1;
        return existing;
    }

    if (vt->count >= vt->capacity) {
        int new_cap = vt->capacity ? vt->capacity * 2 : 16;
        JavaVar *new_items = (JavaVar *)realloc(vt->items, sizeof(JavaVar) * (size_t)new_cap);
        if (!new_items) return NULL;
        vt->items = new_items;
        vt->capacity = new_cap;
    }

    JavaVar *v = &vt->items[vt->count++];
    memset(v, 0, sizeof(*v));
    v->orig = strdup_safe(orig);
    char tmp[256];
    sanitize_ident(orig, tmp, sizeof(tmp));
    if (!tmp[0]) snprintf(tmp, sizeof(tmp), "v%d", vt->count);
    v->jname = strdup_safe(tmp);
    v->kind = kind;
    v->array_size = array_size > 0 ? array_size : 1;
    v->is_param = is_param ? 1 : 0;
    return v;
}

static void add_user_type_fields_recursive(JavaFnCtx *ctx, const char *base_name, const char *type_name, int depth) {
    if (!ctx || !base_name || !*base_name || !type_name || !*type_name) return;
    if (depth > 12) return;

    const UserTypeInfo *type = find_user_type_info(ctx->cfg, type_name);
    if (!type) return;

    if (type->base_name && *type->base_name) {
        add_user_type_fields_recursive(ctx, base_name, type->base_name, depth + 1);
    }

    for (int i = 0; i < type->field_count; i++) {
        const TypeFieldInfo *f = &type->fields[i];
        if (!f->name || !*f->name) continue;

        char full[512];
        snprintf(full, sizeof(full), "%s.%s", base_name, f->name);
        var_table_add(&ctx->vars, full, java_kind_from_type(f->type_name, 0), 1, 0);

        if (f->type_name && !is_builtin_scalar_type(f->type_name)) {
            add_user_type_fields_recursive(ctx, full, f->type_name, depth + 1);
        }
    }
}

static int is_builtin_stub_function(const FunctionCFG *func) {
    if (!func || !func->info || !func->info->name) return 0;
    if (strcmp(func->info->name, "readByte") == 0) return 1;
    if (strcmp(func->info->name, "print") == 0) return 1;
    if (strcmp(func->info->name, "printf") == 0) return 1;
    return 0;
}

static void parse_decl_and_add_vars(JavaFnCtx *ctx, const char *decl_text) {
    if (!ctx || !decl_text) return;

    char work[1024];
    strncpy(work, decl_text, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';
    trim_inplace(work);

    if (!starts_with_ci(work, "dim")) return;

    char *p = work + 3;
    while (*p && isspace((unsigned char)*p)) p++;

    const char *as_kw = find_ci(p, " as ");
    if (!as_kw) return;

    char names[800];
    size_t names_len = (size_t)(as_kw - p);
    if (names_len >= sizeof(names)) names_len = sizeof(names) - 1;
    memcpy(names, p, names_len);
    names[names_len] = '\0';

    const char *type_p = as_kw + 4;
    while (*type_p && isspace((unsigned char)*type_p)) type_p++;
    char type_name[64];
    size_t tlen = 0;
    while (type_p[tlen] && !isspace((unsigned char)type_p[tlen])) tlen++;
    if (tlen >= sizeof(type_name)) tlen = sizeof(type_name) - 1;
    memcpy(type_name, type_p, tlen);
    type_name[tlen] = '\0';

    char *save = NULL;
    for (char *tok = strtok_r(names, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        trim_inplace(tok);
        if (!*tok) continue;

        char base[256];
        strncpy(base, tok, sizeof(base) - 1);
        base[sizeof(base) - 1] = '\0';

        int is_array = 0;
        int arr_size = 1;
        char *lb = strchr(base, '[');
        if (lb) {
            char *rb = strrchr(lb, ']');
            if (rb && rb > lb + 1) {
                is_array = 1;
                char numbuf[64];
                size_t nlen = (size_t)(rb - (lb + 1));
                if (nlen >= sizeof(numbuf)) nlen = sizeof(numbuf) - 1;
                memcpy(numbuf, lb + 1, nlen);
                numbuf[nlen] = '\0';
                trim_inplace(numbuf);
                int parsed = atoi(numbuf);
                if (parsed > 0) arr_size = parsed;
            }
            *lb = '\0';
            trim_inplace(base);
        }

        if (!*base) continue;
        var_table_add(&ctx->vars, base, java_kind_from_type(type_name, is_array), arr_size, 0);
        if (!is_array && !is_builtin_scalar_type(type_name)) {
            add_user_type_fields_recursive(ctx, base, type_name, 0);
        }
    }
}

static int parse_lhs_array(const char *lhs, char *base, size_t base_sz, char *index, size_t index_sz) {
    if (!lhs || !base || !index || base_sz == 0 || index_sz == 0) return 0;
    char tmp[512];
    strncpy(tmp, lhs, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    trim_inplace(tmp);

    char *lb = strchr(tmp, '[');
    if (!lb) return 0;
    char *rb = strrchr(lb, ']');
    if (!rb || rb <= lb + 1) return 0;

    *lb = '\0';
    trim_inplace(tmp);

    size_t bl = strlen(tmp);
    if (bl >= base_sz) bl = base_sz - 1;
    memcpy(base, tmp, bl);
    base[bl] = '\0';

    size_t il = (size_t)(rb - (lb + 1));
    if (il >= index_sz) il = index_sz - 1;
    memcpy(index, lb + 1, il);
    index[il] = '\0';
    trim_inplace(index);

    return base[0] != '\0' && index[0] != '\0';
}

static void collect_vars_for_function(JavaFnCtx *ctx) {
    if (!ctx || !ctx->func || !ctx->func->info) return;

    FunctionInfo *info = ctx->func->info;
    for (int i = 0; i < info->param_count; i++) {
        const char *pn = info->params[i] ? info->params[i] : "arg";
        const char *pt = (info->param_types && info->param_types[i]) ? info->param_types[i] : "int";
        var_table_add(&ctx->vars, pn, java_kind_from_type(pt, 0), 1, 1);
    }

    for (int i = 0; i < ctx->func->node_count; i++) {
        CFGNode *node = ctx->func->nodes[i];
        for (Operation *op = node ? node->operations : NULL; op; op = op->next) {
            if (op->type == OP_VAR_DECL && op->text) {
                parse_decl_and_add_vars(ctx, op->text);
            } else if (op->type == OP_ASSIGN && op->text) {
                char line[512];
                strncpy(line, op->text, sizeof(line) - 1);
                line[sizeof(line) - 1] = '\0';
                char *eq = strchr(line, '=');
                if (!eq) continue;
                *eq = '\0';
                trim_inplace(line);
                if (!*line) continue;

                char arr_base[256], arr_idx[256];
                if (parse_lhs_array(line, arr_base, sizeof(arr_base), arr_idx, sizeof(arr_idx))) {
                    if (!var_table_find(&ctx->vars, arr_base)) {
                        var_table_add(&ctx->vars, arr_base, JAVA_VAR_LONG_ARRAY, 16, 0);
                    }
                } else {
                    if (!var_table_find(&ctx->vars, line) && strcmp(line, ctx->func->info->name) != 0) {
                        var_table_add(&ctx->vars, line, JAVA_VAR_LONG, 1, 0);
                    }
                }
            }
        }
    }
}

static int cfg_has_function_named(const CFG *cfg, const char *name) {
    if (!cfg || !name || !*name) return 0;
    for (int i = 0; i < cfg->func_count; i++) {
        FunctionCFG *f = cfg->functions[i];
        if (f && f->info && f->info->name && strcmp(f->info->name, name) == 0) return 1;
    }
    return 0;
}

static const char *map_call_name(JavaFnCtx *ctx, const char *raw, char *buf, size_t buf_sz) {
    if (!raw || !*raw) return "unknown_call";

    if (strcmp(raw, "readByte") == 0) return "__readByte";
    if (strcmp(raw, "readInt") == 0) {
        if (ctx && ctx->cfg && cfg_has_function_named(ctx->cfg, "readInt")) {
            snprintf(buf, buf_sz, "readInt__sl");
            return buf;
        }
        return "__readInt";
    }
    if (strcmp(raw, "print") == 0 || strcmp(raw, "printf") == 0) return "__print";

    char tmp[256];
    sanitize_ident(raw, tmp, sizeof(tmp));
    if (!tmp[0]) strncpy(tmp, "unknown_call", sizeof(tmp) - 1);
    snprintf(buf, buf_sz, "%s__sl", tmp);
    return buf;
}

static void rewrite_expression(JavaFnCtx *ctx, const char *expr, char *out, size_t out_sz);

static void split_call_args(const char *args_src, char args[][1024], int *arg_count, int max_args) {
    *arg_count = 0;
    if (!args_src || !*args_src || max_args <= 0) return;

    int depth = 0;
    int in_str = 0;
    size_t start = 0;
    size_t n = strlen(args_src);

    for (size_t i = 0; i <= n; i++) {
        char c = (i < n) ? args_src[i] : '\0';

        if (in_str) {
            if (c == '\\' && i + 1 < n) {
                i++;
                continue;
            }
            if (c == '"') in_str = 0;
            continue;
        }

        if (c == '"') {
            in_str = 1;
            continue;
        }
        if (c == '(' || c == '[') depth++;
        if (c == ')' || c == ']') depth--;

        if ((c == ',' && depth == 0) || c == '\0') {
            if (*arg_count < max_args) {
                size_t len = i - start;
                if (len >= 1023) len = 1023;
                memcpy(args[*arg_count], args_src + start, len);
                args[*arg_count][len] = '\0';
                trim_inplace(args[*arg_count]);
                (*arg_count)++;
            }
            start = i + 1;
        }
    }
}

static int parse_call_expression(JavaFnCtx *ctx, const char *text, char *out, size_t out_sz) {
    if (!text || !*text || !out || out_sz == 0) return 0;

    char work[2048];
    strncpy(work, text, sizeof(work) - 1);
    work[sizeof(work) - 1] = '\0';
    trim_inplace(work);

    char *p = work;
    if (starts_with_ci(p, "call ")) p += 5;
    while (*p && isspace((unsigned char)*p)) p++;

    char name[256];
    size_t ni = 0;
    while (*p && is_token_ident_char(*p)) {
        if (ni + 1 < sizeof(name)) name[ni++] = *p;
        p++;
    }
    name[ni] = '\0';
    while (*p && isspace((unsigned char)*p)) p++;

    if (!*name || *p != '(') return 0;

    int depth = 0;
    int in_str = 0;
    char args_raw[2048] = {0};
    size_t ai = 0;
    p++; // after '(' 

    for (; *p; p++) {
        char c = *p;
        if (in_str) {
            if (c == '\\' && p[1]) {
                if (ai + 2 < sizeof(args_raw)) {
                    args_raw[ai++] = c;
                    args_raw[ai++] = p[1];
                }
                p++;
                continue;
            }
            if (c == '"') in_str = 0;
            if (ai + 1 < sizeof(args_raw)) args_raw[ai++] = c;
            continue;
        }

        if (c == '"') {
            in_str = 1;
            if (ai + 1 < sizeof(args_raw)) args_raw[ai++] = c;
            continue;
        }

        if (c == '(') depth++;
        if (c == ')') {
            if (depth == 0) break;
            depth--;
        }
        if (ai + 1 < sizeof(args_raw)) args_raw[ai++] = c;
    }
    args_raw[ai] = '\0';

    char mapped[300];
    const char *call_name = map_call_name(ctx, name, mapped, sizeof(mapped));

    char args[32][1024];
    int arg_count = 0;
    split_call_args(args_raw, args, &arg_count, 32);

    size_t used = 0;
    int w = snprintf(out, out_sz, "%s(", call_name);
    if (w < 0 || (size_t)w >= out_sz) return 1;
    used = (size_t)w;

    for (int i = 0; i < arg_count; i++) {
        char rewritten[2048];
        rewrite_expression(ctx, args[i], rewritten, sizeof(rewritten));
        const char *arg_expr = rewritten;

        if (i > 0) {
            if (used + 2 >= out_sz) break;
            out[used++] = ',';
            out[used++] = ' ';
            out[used] = '\0';
        }

        size_t rem = out_sz - used;
        w = snprintf(out + used, rem, "%s", arg_expr);
        if (w < 0 || (size_t)w >= rem) {
            used = out_sz - 1;
            out[used] = '\0';
            return 1;
        }
        used += (size_t)w;
    }

    if (used + 2 < out_sz) {
        out[used++] = ')';
        out[used] = '\0';
    }

    return 1;
}

static void rewrite_expression(JavaFnCtx *ctx, const char *expr, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!expr) return;

    char tmp[2048];
    strncpy(tmp, expr, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    trim_inplace(tmp);
    if (!tmp[0]) return;

    if (parse_call_expression(ctx, tmp, out, out_sz)) return;

    size_t oi = 0;
    size_t n = strlen(tmp);

    for (size_t i = 0; i < n && oi + 1 < out_sz; ) {
        char c = tmp[i];

        if (c == '"') {
            out[oi++] = tmp[i++];
            while (i < n && oi + 1 < out_sz) {
                out[oi++] = tmp[i];
                if (tmp[i] == '\\' && i + 1 < n) {
                    i++;
                    if (oi + 1 < out_sz) out[oi++] = tmp[i];
                } else if (tmp[i] == '"') {
                    i++;
                    break;
                }
                i++;
            }
            continue;
        }

        if (is_ident_start(c)) {
            char token[256];
            size_t ti = 0;
            size_t j = i;
            while (j < n && is_token_ident_char(tmp[j])) {
                if (ti + 1 < sizeof(token)) token[ti++] = tmp[j];
                j++;
            }
            token[ti] = '\0';

            size_t k = j;
            while (k < n && isspace((unsigned char)tmp[k])) k++;

            const char *rep = token;
            char call_mapped[300];

            if (str_ieq(token, "and")) rep = "&&";
            else if (str_ieq(token, "or")) rep = "||";
            else if (str_ieq(token, "not")) rep = "!";
            else if (k < n && tmp[k] == '(') {
                rep = map_call_name(ctx, token, call_mapped, sizeof(call_mapped));
            } else {
                JavaVar *v = var_table_find(&ctx->vars, token);
                if (v && v->jname) rep = v->jname;
            }

            size_t rl = strlen(rep);
            if (oi + rl >= out_sz) rl = out_sz - oi - 1;
            memcpy(out + oi, rep, rl);
            oi += rl;
            i = j;
            continue;
        }

        out[oi++] = c;
        i++;
    }

    out[oi] = '\0';
}

static int rewrite_lvalue(JavaFnCtx *ctx, const char *lhs, char *out, size_t out_sz) {
    if (!lhs || !*lhs || !out || out_sz == 0) return 0;

    char arr_base[256], arr_idx[512];
    if (parse_lhs_array(lhs, arr_base, sizeof(arr_base), arr_idx, sizeof(arr_idx))) {
        JavaVar *v = var_table_find(&ctx->vars, arr_base);
        const char *base = (v && v->jname) ? v->jname : arr_base;

        char idx_java[1024];
        rewrite_expression(ctx, arr_idx, idx_java, sizeof(idx_java));
        snprintf(out, out_sz, "%s[(int)(%s)]", base, idx_java[0] ? idx_java : "0");
        return 1;
    }

    char clean[256];
    strncpy(clean, lhs, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
    trim_inplace(clean);

    JavaVar *v = var_table_find(&ctx->vars, clean);
    if (v && v->jname) {
        snprintf(out, out_sz, "%s", v->jname);
        return 1;
    }

    char san[256];
    sanitize_ident(clean, san, sizeof(san));
    if (!san[0]) snprintf(san, sizeof(san), "tmp");
    snprintf(out, out_sz, "%s", san);
    return 1;
}

static const char *extract_condition_text(const CFGNode *node) {
    for (Operation *op = node ? node->operations : NULL; op; op = op->next) {
        if (op->type == OP_BINARY_OP && op->text && *op->text) return op->text;
    }

    if (node && node->label) {
        const char *lb = strchr(node->label, '(');
        const char *rb = strrchr(node->label, ')');
        if (lb && rb && rb > lb + 1) {
            static char cond_buf[512];
            size_t len = (size_t)(rb - (lb + 1));
            if (len >= sizeof(cond_buf)) len = sizeof(cond_buf) - 1;
            memcpy(cond_buf, lb + 1, len);
            cond_buf[len] = '\0';
            return cond_buf;
        }
    }

    return "0";
}

static void emit_operation_java(FILE *out, JavaFnCtx *ctx, Operation *op) {
    if (!out || !ctx || !op) return;

    switch (op->type) {
        case OP_VAR_DECL:
        case OP_BINARY_OP:
        case OP_LITERAL:
        case OP_NOOP:
        case OP_BREAK:
        case OP_CONTINUE:
            return;

        case OP_RETURN:
            fprintf(out, "                    return __ret;\n");
            return;

        case OP_CALL: {
            char expr_java[2048];
            rewrite_expression(ctx, op->text ? op->text : "", expr_java, sizeof(expr_java));
            if (expr_java[0]) {
                fprintf(out, "                    %s;\n", expr_java);
            }
            return;
        }

        case OP_ASSIGN: {
            if (!op->text) return;
            char line[2048];
            strncpy(line, op->text, sizeof(line) - 1);
            line[sizeof(line) - 1] = '\0';

            char *eq = strchr(line, '=');
            if (!eq) return;
            *eq = '\0';

            char lhs[1024];
            char rhs[1024];
            strncpy(lhs, line, sizeof(lhs) - 1);
            lhs[sizeof(lhs) - 1] = '\0';
            strncpy(rhs, eq + 1, sizeof(rhs) - 1);
            rhs[sizeof(rhs) - 1] = '\0';
            trim_inplace(lhs);
            trim_inplace(rhs);

            char rhs_java[2048];
            rewrite_expression(ctx, rhs, rhs_java, sizeof(rhs_java));

            if (ctx->func && ctx->func->info && ctx->func->info->name && strcmp(lhs, ctx->func->info->name) == 0) {
                fprintf(out, "                    __ret = (%s);\n", rhs_java[0] ? rhs_java : "0L");
                return;
            }

            char lhs_java[1024];
            rewrite_lvalue(ctx, lhs, lhs_java, sizeof(lhs_java));
            fprintf(out, "                    %s = (%s);\n", lhs_java, rhs_java[0] ? rhs_java : "0L");
            return;
        }

        default:
            return;
    }
}

static FunctionCFG *find_main_function(const CFG *cfg) {
    if (!cfg) return 0;
    for (int i = 0; i < cfg->func_count; i++) {
        FunctionCFG *f = cfg->functions[i];
        if (f && f->info && f->info->name && strcmp(f->info->name, "main") == 0) return f;
    }
    return NULL;
}

static int has_main_function(const CFG *cfg) {
    return find_main_function(cfg) != NULL;
}

static void jasmin_type_sig(const char *type_name, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    if (type_name && str_ieq(type_name, "string")) {
        snprintf(out, out_sz, "Ljava/lang/String;");
    } else {
        snprintf(out, out_sz, "J");
    }
}

static void jasmin_method_desc(const FunctionInfo *info, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    strncat(out, "(", out_sz - 1);
    if (info) {
        for (int i = 0; i < info->param_count; i++) {
            char ts[64];
            const char *pt = (info->param_types && info->param_types[i]) ? info->param_types[i] : "int";
            jasmin_type_sig(pt, ts, sizeof(ts));
            size_t rem = out_sz - strlen(out) - 1;
            if (rem > 0) strncat(out, ts, rem);
        }
    }
    size_t rem = out_sz - strlen(out) - 1;
    if (rem > 0) strncat(out, ")J", rem);
}

typedef struct {
    const char *name;
    JavaVarKind kind;
    int slot;
} JasminVarSlot;

typedef struct {
    FILE *out;
    const CFG *cfg;
    const char *class_name;
    JavaFnCtx *java_ctx;
    FunctionCFG *func;
    JasminVarSlot *slots;
    int slot_count;
    int slot_capacity;
    int ret_slot;
    int next_label_id;
} JasminFnEmitCtx;

typedef enum {
    JTOK_EOF,
    JTOK_NUM,
    JTOK_IDENT,
    JTOK_STR,
    JTOK_LP,
    JTOK_RP,
    JTOK_COMMA,
    JTOK_PLUS,
    JTOK_MINUS,
    JTOK_MUL,
    JTOK_DIV,
    JTOK_MOD,
    JTOK_LT,
    JTOK_GT,
    JTOK_LE,
    JTOK_GE,
    JTOK_EQ,
    JTOK_NE,
    JTOK_AND,
    JTOK_OR,
    JTOK_NOT
} JasTokType;

typedef enum {
    JVAL_LONG,
    JVAL_STRING
} JasValueType;

typedef struct {
    JasTokType type;
    char text[512];
    long long num;
} JasToken;

typedef struct {
    const char *src;
    size_t pos;
    JasToken tok;
    JasminFnEmitCtx *fctx;
} JasParser;

static int jas_new_label(JasminFnEmitCtx *ctx) {
    return ctx->next_label_id++;
}

static void jas_emit_push_long(FILE *out, long long v) {
    if (v == 0) {
        fputs("  lconst_0\n", out);
    } else if (v == 1) {
        fputs("  lconst_1\n", out);
    } else {
        fprintf(out, "  ldc2_w %lld\n", v);
    }
}

static void jas_emit_bool_from_int_cmp(FILE *out, JasminFnEmitCtx *ctx, const char *ifop) {
    int l_true = jas_new_label(ctx);
    int l_end = jas_new_label(ctx);
    fprintf(out, "  %s JLBL_%d\n", ifop, l_true);
    fputs("  lconst_0\n", out);
    fprintf(out, "  goto JLBL_%d\n", l_end);
    fprintf(out, "JLBL_%d:\n", l_true);
    fputs("  lconst_1\n", out);
    fprintf(out, "JLBL_%d:\n", l_end);
}

static void jas_slots_free(JasminFnEmitCtx *ctx) {
    if (!ctx) return;
    free(ctx->slots);
    ctx->slots = NULL;
    ctx->slot_count = 0;
    ctx->slot_capacity = 0;
}

static JasminVarSlot *jas_find_slot(JasminFnEmitCtx *ctx, const char *name) {
    if (!ctx || !name || !*name) return NULL;
    for (int i = 0; i < ctx->slot_count; i++) {
        if (ctx->slots[i].name && strcmp(ctx->slots[i].name, name) == 0) return &ctx->slots[i];
    }
    return NULL;
}

static int jas_add_slot(JasminFnEmitCtx *ctx, const char *name, JavaVarKind kind, int slot) {
    if (!ctx || !name || !*name) return 1;
    if (ctx->slot_count >= ctx->slot_capacity) {
        int new_cap = ctx->slot_capacity ? ctx->slot_capacity * 2 : 16;
        JasminVarSlot *n = (JasminVarSlot *)realloc(ctx->slots, sizeof(JasminVarSlot) * (size_t)new_cap);
        if (!n) return 1;
        ctx->slots = n;
        ctx->slot_capacity = new_cap;
    }
    ctx->slots[ctx->slot_count].name = name;
    ctx->slots[ctx->slot_count].kind = kind;
    ctx->slots[ctx->slot_count].slot = slot;
    ctx->slot_count++;
    return 0;
}

static const FunctionCFG *find_function_by_method(const CFG *cfg, const char *method_name) {
    if (!cfg || !method_name || !*method_name) return NULL;
    for (int i = 0; i < cfg->func_count; i++) {
        FunctionCFG *f = cfg->functions[i];
        if (!f || !f->info || !f->info->name) continue;
        char mn[300];
        fn_method_name(f->info->name, mn, sizeof(mn));
        if (strcmp(mn, method_name) == 0) return f;
    }
    return NULL;
}

static void jas_method_desc_from_name(const CFG *cfg, const char *method_name, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (strcmp(method_name, "__readByte") == 0 || strcmp(method_name, "__readInt") == 0) {
        snprintf(out, out_sz, "()J");
        return;
    }
    if (strcmp(method_name, "__print") == 0) {
        snprintf(out, out_sz, "(J)J");
        return;
    }
    if (strcmp(method_name, "__printStr") == 0) {
        snprintf(out, out_sz, "(Ljava/lang/String;)J");
        return;
    }

    const FunctionCFG *f = find_function_by_method(cfg, method_name);
    if (f && f->info) {
        jasmin_method_desc(f->info, out, out_sz);
    } else {
        snprintf(out, out_sz, "()J");
    }
}

static void jas_next_token(JasParser *p) {
    p->tok.type = JTOK_EOF;
    p->tok.text[0] = '\0';
    p->tok.num = 0;
    if (!p || !p->src) return;

    const char *s = p->src;
    while (s[p->pos] && isspace((unsigned char)s[p->pos])) p->pos++;

    char c = s[p->pos];
    if (!c) {
        p->tok.type = JTOK_EOF;
        return;
    }

    if (isdigit((unsigned char)c)) {
        size_t start = p->pos;
        while (isdigit((unsigned char)s[p->pos])) p->pos++;
        size_t len = p->pos - start;
        if (len >= sizeof(p->tok.text)) len = sizeof(p->tok.text) - 1;
        memcpy(p->tok.text, s + start, len);
        p->tok.text[len] = '\0';
        p->tok.num = strtoll(p->tok.text, NULL, 10);
        p->tok.type = JTOK_NUM;
        return;
    }

    if (is_ident_start(c)) {
        size_t start = p->pos;
        p->pos++;
        while (is_token_ident_char(s[p->pos])) p->pos++;
        size_t len = p->pos - start;
        if (len >= sizeof(p->tok.text)) len = sizeof(p->tok.text) - 1;
        memcpy(p->tok.text, s + start, len);
        p->tok.text[len] = '\0';
        p->tok.type = JTOK_IDENT;
        return;
    }

    if (c == '"') {
        size_t start = p->pos;
        p->pos++;
        while (s[p->pos]) {
            if (s[p->pos] == '\\' && s[p->pos + 1]) {
                p->pos += 2;
                continue;
            }
            if (s[p->pos] == '"') {
                p->pos++;
                break;
            }
            p->pos++;
        }
        size_t len = p->pos - start;
        if (len >= sizeof(p->tok.text)) len = sizeof(p->tok.text) - 1;
        memcpy(p->tok.text, s + start, len);
        p->tok.text[len] = '\0';
        p->tok.type = JTOK_STR;
        return;
    }

    if (c == '(') { p->tok.type = JTOK_LP; p->tok.text[0] = c; p->tok.text[1] = '\0'; p->pos++; return; }
    if (c == ')') { p->tok.type = JTOK_RP; p->tok.text[0] = c; p->tok.text[1] = '\0'; p->pos++; return; }
    if (c == ',') { p->tok.type = JTOK_COMMA; p->tok.text[0] = c; p->tok.text[1] = '\0'; p->pos++; return; }
    if (c == '+') { p->tok.type = JTOK_PLUS; p->tok.text[0] = c; p->tok.text[1] = '\0'; p->pos++; return; }
    if (c == '-') { p->tok.type = JTOK_MINUS; p->tok.text[0] = c; p->tok.text[1] = '\0'; p->pos++; return; }
    if (c == '*') { p->tok.type = JTOK_MUL; p->tok.text[0] = c; p->tok.text[1] = '\0'; p->pos++; return; }
    if (c == '/') { p->tok.type = JTOK_DIV; p->tok.text[0] = c; p->tok.text[1] = '\0'; p->pos++; return; }
    if (c == '%') { p->tok.type = JTOK_MOD; p->tok.text[0] = c; p->tok.text[1] = '\0'; p->pos++; return; }

    if (c == '&' && s[p->pos + 1] == '&') { p->tok.type = JTOK_AND; strcpy(p->tok.text, "&&"); p->pos += 2; return; }
    if (c == '|' && s[p->pos + 1] == '|') { p->tok.type = JTOK_OR; strcpy(p->tok.text, "||"); p->pos += 2; return; }
    if (c == '=' && s[p->pos + 1] == '=') { p->tok.type = JTOK_EQ; strcpy(p->tok.text, "=="); p->pos += 2; return; }
    if (c == '!' && s[p->pos + 1] == '=') { p->tok.type = JTOK_NE; strcpy(p->tok.text, "!="); p->pos += 2; return; }
    if (c == '<' && s[p->pos + 1] == '=') { p->tok.type = JTOK_LE; strcpy(p->tok.text, "<="); p->pos += 2; return; }
    if (c == '>' && s[p->pos + 1] == '=') { p->tok.type = JTOK_GE; strcpy(p->tok.text, ">="); p->pos += 2; return; }

    if (c == '<') { p->tok.type = JTOK_LT; p->tok.text[0] = c; p->tok.text[1] = '\0'; p->pos++; return; }
    if (c == '>') { p->tok.type = JTOK_GT; p->tok.text[0] = c; p->tok.text[1] = '\0'; p->pos++; return; }
    if (c == '!') { p->tok.type = JTOK_NOT; p->tok.text[0] = c; p->tok.text[1] = '\0'; p->pos++; return; }

    p->pos++;
    p->tok.type = JTOK_EOF;
}

static int jas_accept(JasParser *p, JasTokType t) {
    if (p->tok.type != t) return 0;
    jas_next_token(p);
    return 1;
}

static void jas_emit_expr(JasParser *p, JasValueType *out_type);
static void jas_emit_logic_or(JasParser *p, JasValueType *out_type);
static void jas_emit_logic_and(JasParser *p, JasValueType *out_type);
static void jas_emit_equality(JasParser *p, JasValueType *out_type);
static void jas_emit_relational(JasParser *p, JasValueType *out_type);
static void jas_emit_add(JasParser *p, JasValueType *out_type);
static void jas_emit_mul(JasParser *p, JasValueType *out_type);
static void jas_emit_unary(JasParser *p, JasValueType *out_type);
static void jas_emit_primary(JasParser *p, JasValueType *out_type);

static void jas_ensure_long(FILE *out, JasValueType *t) {
    if (*t == JVAL_STRING) {
        fputs("  pop\n", out);
        fputs("  lconst_0\n", out);
        *t = JVAL_LONG;
    }
}

static void jas_emit_expr(JasParser *p, JasValueType *out_type) {
    jas_emit_logic_or(p, out_type);
}

static void jas_emit_logic_or(JasParser *p, JasValueType *out_type) {
    jas_emit_logic_and(p, out_type);
    while (p->tok.type == JTOK_OR) {
        jas_ensure_long(p->fctx->out, out_type);
        jas_next_token(p);
        JasValueType rt;
        jas_emit_logic_and(p, &rt);
        jas_ensure_long(p->fctx->out, &rt);

        int l_true = jas_new_label(p->fctx);
        int l_end = jas_new_label(p->fctx);
        fputs("  ladd\n", p->fctx->out);
        fputs("  lconst_0\n", p->fctx->out);
        fputs("  lcmp\n", p->fctx->out);
        fprintf(p->fctx->out, "  ifne JLBL_%d\n", l_true);
        fputs("  lconst_0\n", p->fctx->out);
        fprintf(p->fctx->out, "  goto JLBL_%d\n", l_end);
        fprintf(p->fctx->out, "JLBL_%d:\n", l_true);
        fputs("  lconst_1\n", p->fctx->out);
        fprintf(p->fctx->out, "JLBL_%d:\n", l_end);
        *out_type = JVAL_LONG;
    }
}

static void jas_emit_logic_and(JasParser *p, JasValueType *out_type) {
    jas_emit_equality(p, out_type);
    while (p->tok.type == JTOK_AND) {
        jas_ensure_long(p->fctx->out, out_type);
        jas_next_token(p);
        JasValueType rt;
        jas_emit_equality(p, &rt);
        jas_ensure_long(p->fctx->out, &rt);

        int l_true = jas_new_label(p->fctx);
        int l_end = jas_new_label(p->fctx);
        fputs("  lmul\n", p->fctx->out);
        fputs("  lconst_0\n", p->fctx->out);
        fputs("  lcmp\n", p->fctx->out);
        fprintf(p->fctx->out, "  ifne JLBL_%d\n", l_true);
        fputs("  lconst_0\n", p->fctx->out);
        fprintf(p->fctx->out, "  goto JLBL_%d\n", l_end);
        fprintf(p->fctx->out, "JLBL_%d:\n", l_true);
        fputs("  lconst_1\n", p->fctx->out);
        fprintf(p->fctx->out, "JLBL_%d:\n", l_end);
        *out_type = JVAL_LONG;
    }
}

static void jas_emit_equality(JasParser *p, JasValueType *out_type) {
    jas_emit_relational(p, out_type);

    while (p->tok.type == JTOK_EQ || p->tok.type == JTOK_NE) {
        jas_ensure_long(p->fctx->out, out_type);
        JasTokType op = p->tok.type;
        jas_next_token(p);

        JasValueType rt;
        jas_emit_relational(p, &rt);
        jas_ensure_long(p->fctx->out, &rt);

        fputs("  lcmp\n", p->fctx->out);
        jas_emit_bool_from_int_cmp(p->fctx->out, p->fctx, (op == JTOK_EQ) ? "ifeq" : "ifne");
        *out_type = JVAL_LONG;
    }
}

static void jas_emit_relational(JasParser *p, JasValueType *out_type) {
    jas_emit_add(p, out_type);

    while (p->tok.type == JTOK_LT || p->tok.type == JTOK_LE || p->tok.type == JTOK_GT || p->tok.type == JTOK_GE) {
        jas_ensure_long(p->fctx->out, out_type);
        JasTokType op = p->tok.type;
        jas_next_token(p);

        JasValueType rt;
        jas_emit_add(p, &rt);
        jas_ensure_long(p->fctx->out, &rt);

        fputs("  lcmp\n", p->fctx->out);
        if (op == JTOK_LT) jas_emit_bool_from_int_cmp(p->fctx->out, p->fctx, "iflt");
        else if (op == JTOK_LE) jas_emit_bool_from_int_cmp(p->fctx->out, p->fctx, "ifle");
        else if (op == JTOK_GT) jas_emit_bool_from_int_cmp(p->fctx->out, p->fctx, "ifgt");
        else jas_emit_bool_from_int_cmp(p->fctx->out, p->fctx, "ifge");

        *out_type = JVAL_LONG;
    }
}

static void jas_emit_add(JasParser *p, JasValueType *out_type) {
    jas_emit_mul(p, out_type);

    while (p->tok.type == JTOK_PLUS || p->tok.type == JTOK_MINUS) {
        jas_ensure_long(p->fctx->out, out_type);
        JasTokType op = p->tok.type;
        jas_next_token(p);

        JasValueType rt;
        jas_emit_mul(p, &rt);
        jas_ensure_long(p->fctx->out, &rt);

        if (op == JTOK_PLUS) fputs("  ladd\n", p->fctx->out);
        else fputs("  lsub\n", p->fctx->out);
        *out_type = JVAL_LONG;
    }
}

static void jas_emit_mul(JasParser *p, JasValueType *out_type) {
    jas_emit_unary(p, out_type);

    while (p->tok.type == JTOK_MUL || p->tok.type == JTOK_DIV || p->tok.type == JTOK_MOD) {
        jas_ensure_long(p->fctx->out, out_type);
        JasTokType op = p->tok.type;
        jas_next_token(p);

        JasValueType rt;
        jas_emit_unary(p, &rt);
        jas_ensure_long(p->fctx->out, &rt);

        if (op == JTOK_MUL) fputs("  lmul\n", p->fctx->out);
        else if (op == JTOK_DIV) fputs("  ldiv\n", p->fctx->out);
        else fputs("  lrem\n", p->fctx->out);
        *out_type = JVAL_LONG;
    }
}

static void jas_emit_unary(JasParser *p, JasValueType *out_type) {
    if (p->tok.type == JTOK_MINUS) {
        jas_next_token(p);
        jas_emit_unary(p, out_type);
        jas_ensure_long(p->fctx->out, out_type);
        fputs("  lneg\n", p->fctx->out);
        *out_type = JVAL_LONG;
        return;
    }
    if (p->tok.type == JTOK_PLUS) {
        jas_next_token(p);
        jas_emit_unary(p, out_type);
        jas_ensure_long(p->fctx->out, out_type);
        *out_type = JVAL_LONG;
        return;
    }
    if (p->tok.type == JTOK_NOT) {
        jas_next_token(p);
        jas_emit_unary(p, out_type);
        jas_ensure_long(p->fctx->out, out_type);
        fputs("  lconst_0\n", p->fctx->out);
        fputs("  lcmp\n", p->fctx->out);
        jas_emit_bool_from_int_cmp(p->fctx->out, p->fctx, "ifeq");
        *out_type = JVAL_LONG;
        return;
    }
    jas_emit_primary(p, out_type);
}

static void jas_emit_call(JasParser *p, const char *fn_name, JasValueType *out_type) {
    char desc[512];
    JasValueType arg_types[32];
    int argc = 0;

    if (jas_accept(p, JTOK_RP)) {
        jas_method_desc_from_name(p->fctx->cfg, fn_name, desc, sizeof(desc));
        fprintf(p->fctx->out, "  invokestatic %s/%s%s\n", p->fctx->class_name, fn_name, desc);
        *out_type = JVAL_LONG;
        return;
    }

    while (1) {
        if (argc >= 32) break;
        jas_emit_expr(p, &arg_types[argc]);
        argc++;
        if (jas_accept(p, JTOK_COMMA)) continue;
        (void)jas_accept(p, JTOK_RP);
        break;
    }

    if (strcmp(fn_name, "__print") == 0 && argc == 1 && arg_types[0] == JVAL_STRING) {
        strcpy(desc, "(Ljava/lang/String;)J");
        fprintf(p->fctx->out, "  invokestatic %s/__printStr%s\n", p->fctx->class_name, desc);
    } else {
        if (strcmp(fn_name, "__print") == 0 && argc == 1 && arg_types[0] != JVAL_LONG) {
            fputs("  pop\n", p->fctx->out);
            fputs("  lconst_0\n", p->fctx->out);
        }
        jas_method_desc_from_name(p->fctx->cfg, fn_name, desc, sizeof(desc));
        fprintf(p->fctx->out, "  invokestatic %s/%s%s\n", p->fctx->class_name, fn_name, desc);
    }

    *out_type = JVAL_LONG;
}

static void jas_emit_primary(JasParser *p, JasValueType *out_type) {
    if (p->tok.type == JTOK_NUM) {
        jas_emit_push_long(p->fctx->out, p->tok.num);
        jas_next_token(p);
        *out_type = JVAL_LONG;
        return;
    }

    if (p->tok.type == JTOK_STR) {
        fprintf(p->fctx->out, "  ldc %s\n", p->tok.text);
        jas_next_token(p);
        *out_type = JVAL_STRING;
        return;
    }

    if (p->tok.type == JTOK_IDENT) {
        char ident[512];
        strncpy(ident, p->tok.text, sizeof(ident) - 1);
        ident[sizeof(ident) - 1] = '\0';
        jas_next_token(p);

        if (jas_accept(p, JTOK_LP)) {
            jas_emit_call(p, ident, out_type);
            return;
        }

        JasminVarSlot *v = jas_find_slot(p->fctx, ident);
        if (v) {
            if (v->kind == JAVA_VAR_STRING || v->kind == JAVA_VAR_STRING_ARRAY) {
                fprintf(p->fctx->out, "  aload %d\n", v->slot);
                *out_type = JVAL_STRING;
            } else {
                fprintf(p->fctx->out, "  lload %d\n", v->slot);
                *out_type = JVAL_LONG;
            }
            return;
        }

        fputs("  lconst_0\n", p->fctx->out);
        *out_type = JVAL_LONG;
        return;
    }

    if (jas_accept(p, JTOK_LP)) {
        jas_emit_expr(p, out_type);
        (void)jas_accept(p, JTOK_RP);
        return;
    }

    fputs("  lconst_0\n", p->fctx->out);
    *out_type = JVAL_LONG;
}

static void jas_emit_expression_text(JasminFnEmitCtx *ctx, const char *expr, JasValueType *out_type) {
    char normalized[2048];
    rewrite_expression(ctx->java_ctx, expr ? expr : "", normalized, sizeof(normalized));
    if (!normalized[0]) {
        fputs("  lconst_0\n", ctx->out);
        if (out_type) *out_type = JVAL_LONG;
        return;
    }

    JasParser p;
    memset(&p, 0, sizeof(p));
    p.src = normalized;
    p.fctx = ctx;
    jas_next_token(&p);
    JasValueType t = JVAL_LONG;
    jas_emit_expr(&p, &t);
    if (out_type) *out_type = t;
}

static int jas_write_line_with_class(FILE *out, const char *line, const char *class_name) {
    const char *token = "{{CLASS_NAME}}";
    const size_t token_len = strlen(token);
    const char *p = line;

    while (1) {
        const char *m = strstr(p, token);
        if (!m) {
            return fputs(p, out) == EOF ? 1 : 0;
        }

        size_t n = (size_t)(m - p);
        if (n > 0 && fwrite(p, 1, n, out) != n) return 1;
        if (fputs(class_name, out) == EOF) return 1;
        p = m + token_len;
    }
}

static int jas_emit_runtime(FILE *out, const char *class_name) {
    const char *template_paths[] = {
        "templates/jvm_runtime.j.tpl",
        "./templates/jvm_runtime.j.tpl",
        NULL
    };

    FILE *tpl = NULL;
    for (int i = 0; template_paths[i]; i++) {
        tpl = fopen(template_paths[i], "r");
        if (tpl) break;
    }
    if (!tpl) return 1;

    char line[4096];
    while (fgets(line, sizeof(line), tpl)) {
        if (jas_write_line_with_class(out, line, class_name) != 0) {
            fclose(tpl);
            return 2;
        }
    }

    if (ferror(tpl)) {
        fclose(tpl);
        return 3;
    }
    fclose(tpl);
    return 0;
}

static int jas_setup_slots(JasminFnEmitCtx *ctx) {
    if (!ctx || !ctx->func || !ctx->func->info || !ctx->java_ctx) return 1;

    int slot = 0;
    FunctionInfo *info = ctx->func->info;

    for (int p = 0; p < info->param_count; p++) {
        const char *pn = (info->params && info->params[p]) ? info->params[p] : "arg";
        JavaVar *v = var_table_find(&ctx->java_ctx->vars, pn);
        JavaVarKind k = v ? v->kind : JAVA_VAR_LONG;
        const char *jn = (v && v->jname) ? v->jname : pn;
        if (jas_add_slot(ctx, jn, k, slot) != 0) return 1;
        slot += (k == JAVA_VAR_STRING || k == JAVA_VAR_STRING_ARRAY) ? 1 : 2;
    }

    for (int i = 0; i < ctx->java_ctx->vars.count; i++) {
        JavaVar *v = &ctx->java_ctx->vars.items[i];
        if (v->is_param) continue;
        if (jas_add_slot(ctx, v->jname, v->kind, slot) != 0) return 1;
        slot += (v->kind == JAVA_VAR_STRING || v->kind == JAVA_VAR_STRING_ARRAY) ? 1 : 2;
    }

    ctx->ret_slot = slot;
    return 0;
}

static void jas_emit_var_inits(JasminFnEmitCtx *ctx) {
    if (!ctx || !ctx->java_ctx) return;
    for (int i = 0; i < ctx->java_ctx->vars.count; i++) {
        JavaVar *v = &ctx->java_ctx->vars.items[i];
        if (v->is_param) continue;
        JasminVarSlot *slot = jas_find_slot(ctx, v->jname);
        if (!slot) continue;

        if (v->kind == JAVA_VAR_LONG) {
            fputs("  lconst_0\n", ctx->out);
            fprintf(ctx->out, "  lstore %d\n", slot->slot);
        } else if (v->kind == JAVA_VAR_STRING) {
            fputs("  ldc \"\"\n", ctx->out);
            fprintf(ctx->out, "  astore %d\n", slot->slot);
        } else if (v->kind == JAVA_VAR_LONG_ARRAY) {
            fprintf(ctx->out, "  ldc %d\n", v->array_size > 0 ? v->array_size : 1);
            fputs("  newarray long\n", ctx->out);
            fprintf(ctx->out, "  astore %d\n", slot->slot);
        } else if (v->kind == JAVA_VAR_STRING_ARRAY) {
            fprintf(ctx->out, "  ldc %d\n", v->array_size > 0 ? v->array_size : 1);
            fputs("  anewarray java/lang/String\n", ctx->out);
            fprintf(ctx->out, "  astore %d\n", slot->slot);
        }
    }

    fputs("  lconst_0\n", ctx->out);
    fprintf(ctx->out, "  lstore %d\n", ctx->ret_slot);
}

static int jas_emit_assign(JasminFnEmitCtx *ctx, const char *text) {
    if (!ctx || !text) return 0;
    char line[2048];
    strncpy(line, text, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';

    char *eq = strchr(line, '=');
    if (!eq) return 0;
    *eq = '\0';
    char lhs[1024];
    char rhs[1024];
    strncpy(lhs, line, sizeof(lhs) - 1);
    lhs[sizeof(lhs) - 1] = '\0';
    strncpy(rhs, eq + 1, sizeof(rhs) - 1);
    rhs[sizeof(rhs) - 1] = '\0';
    trim_inplace(lhs);
    trim_inplace(rhs);

    if (!lhs[0]) return 0;

    char lhs_java[1024];
    rewrite_lvalue(ctx->java_ctx, lhs, lhs_java, sizeof(lhs_java));

    JasValueType rt = JVAL_LONG;
    jas_emit_expression_text(ctx, rhs, &rt);

    if (ctx->func && ctx->func->info && ctx->func->info->name && strcmp(lhs, ctx->func->info->name) == 0) {
        jas_ensure_long(ctx->out, &rt);
        fprintf(ctx->out, "  lstore %d\n", ctx->ret_slot);
        return 0;
    }

    JasminVarSlot *v = jas_find_slot(ctx, lhs_java);
    if (!v) {
        jas_ensure_long(ctx->out, &rt);
        fputs("  pop2\n", ctx->out);
        return 0;
    }

    if (v->kind == JAVA_VAR_STRING || v->kind == JAVA_VAR_STRING_ARRAY) {
        if (rt == JVAL_LONG) {
            fputs("  pop2\n", ctx->out);
            fputs("  ldc \"\"\n", ctx->out);
        }
        fprintf(ctx->out, "  astore %d\n", v->slot);
    } else {
        jas_ensure_long(ctx->out, &rt);
        fprintf(ctx->out, "  lstore %d\n", v->slot);
    }
    return 0;
}

static int jas_emit_call_stmt(JasminFnEmitCtx *ctx, const char *text) {
    if (!ctx || !text) return 0;
    JasValueType t = JVAL_LONG;
    jas_emit_expression_text(ctx, text, &t);
    if (t == JVAL_LONG) fputs("  pop2\n", ctx->out);
    else fputs("  pop\n", ctx->out);
    return 0;
}

static int jas_emit_operations(JasminFnEmitCtx *ctx, CFGNode *node) {
    for (Operation *op = node ? node->operations : NULL; op; op = op->next) {
        switch (op->type) {
            case OP_ASSIGN:
                jas_emit_assign(ctx, op->text);
                break;
            case OP_CALL:
                jas_emit_call_stmt(ctx, op->text);
                break;
            case OP_RETURN:
                fprintf(ctx->out, "  lload %d\n", ctx->ret_slot);
                fputs("  lreturn\n", ctx->out);
                return 1;
            default:
                break;
        }
    }
    return 0;
}

static void jas_emit_condition_branch(JasminFnEmitCtx *ctx, CFGNode *node) {
    const char *cond_text = extract_condition_text(node);
    JasValueType t = JVAL_LONG;
    jas_emit_expression_text(ctx, cond_text, &t);
    jas_ensure_long(ctx->out, &t);
    fputs("  lconst_0\n", ctx->out);
    fputs("  lcmp\n", ctx->out);
    fprintf(ctx->out, "  ifne N_%d\n", node->true_branch ? node->true_branch->id : -1);
    fprintf(ctx->out, "  goto N_%d\n", node->next ? node->next->id : -1);
}

static void jas_emit_function(FILE *out, const CFG *cfg, const char *class_name, FunctionCFG *func) {
    if (!out || !cfg || !func || !func->info || !func->info->name) return;

    char method[300];
    char desc[512];
    fn_method_name(func->info->name, method, sizeof(method));
    jasmin_method_desc(func->info, desc, sizeof(desc));

    if (is_builtin_stub_function(func)) {
        if (strcmp(func->info->name, "readByte") == 0) {
            fprintf(out, ".method public static %s()J\n", method);
            fputs("  .limit stack 2\n", out);
            fputs("  .limit locals 0\n", out);
            fprintf(out, "  invokestatic %s/__readByte()J\n", class_name);
            fputs("  lreturn\n", out);
            fputs(".end method\n\n", out);
            return;
        }
        if (strcmp(func->info->name, "print") == 0 || strcmp(func->info->name, "printf") == 0) {
            const char *pt = (func->info->param_count > 0 && func->info->param_types && func->info->param_types[0])
                                 ? func->info->param_types[0]
                                 : "int";
            fprintf(out, ".method public static %s%s\n", method, desc);
            fputs("  .limit stack 3\n", out);
            fputs("  .limit locals 3\n", out);
            if (pt && str_ieq(pt, "string")) {
                fputs("  aload 0\n", out);
                fprintf(out, "  invokestatic %s/__printStr(Ljava/lang/String;)J\n", class_name);
            } else {
                fputs("  lload 0\n", out);
                fprintf(out, "  invokestatic %s/__print(J)J\n", class_name);
            }
            fputs("  lreturn\n", out);
            fputs(".end method\n\n", out);
            return;
        }
    }

    JavaFnCtx jctx;
    memset(&jctx, 0, sizeof(jctx));
    jctx.cfg = cfg;
    jctx.func = func;
    sanitize_ident(func->info->name, jctx.func_name, sizeof(jctx.func_name));
    if (!jctx.func_name[0]) strncpy(jctx.func_name, "anonymous", sizeof(jctx.func_name) - 1);
    fn_method_name(func->info->name, jctx.func_method, sizeof(jctx.func_method));
    collect_vars_for_function(&jctx);

    JasminFnEmitCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.out = out;
    ctx.cfg = cfg;
    ctx.class_name = class_name;
    ctx.java_ctx = &jctx;
    ctx.func = func;

    if (jas_setup_slots(&ctx) != 0) {
        var_table_free(&jctx.vars);
        jas_slots_free(&ctx);
        return;
    }

    int max_locals = ctx.ret_slot + 2;
    if (max_locals < 1) max_locals = 1;

    fprintf(out, ".method public static %s%s\n", method, desc);
    fputs("  .limit stack 128\n", out);
    fprintf(out, "  .limit locals %d\n", max_locals);

    jas_emit_var_inits(&ctx);

    for (int i = 0; i < func->node_count; i++) {
        CFGNode *node = func->nodes[i];
        if (!node) continue;
        fprintf(out, "N_%d:\n", node->id);

        int terminated = jas_emit_operations(&ctx, node);
        if (terminated) continue;

        if (node->true_branch && node->next) {
            jas_emit_condition_branch(&ctx, node);
        } else if (node->next) {
            fprintf(out, "  goto N_%d\n", node->next->id);
        } else {
            fprintf(out, "  lload %d\n", ctx.ret_slot);
            fputs("  lreturn\n", out);
        }
    }

    fprintf(out, "  lload %d\n", ctx.ret_slot);
    fputs("  lreturn\n", out);
    fputs(".end method\n\n", out);

    jas_slots_free(&ctx);
    var_table_free(&jctx.vars);
}

static void emit_java_runtime(FILE *out) {
    fputs("    private static final java.io.BufferedInputStream __in = new java.io.BufferedInputStream(System.in);\n", out);
    fputs("    private static final java.io.PrintStream __out = System.out;\n\n", out);

    fputs("    private static boolean __toBool(long v) { return v != 0L; }\n", out);
    fputs("    private static boolean __toBool(boolean v) { return v; }\n\n", out);

    fputs("    private static long __readByte() {\n", out);
    fputs("        try {\n", out);
    fputs("            return __in.read();\n", out);
    fputs("        } catch (java.io.IOException e) {\n", out);
    fputs("            throw new RuntimeException(e);\n", out);
    fputs("        }\n", out);
    fputs("    }\n\n", out);

    fputs("    private static long __readInt() {\n", out);
    fputs("        long x = 0L;\n", out);
    fputs("        long b;\n", out);
    fputs("        boolean started = false;\n", out);
    fputs("        while (true) {\n", out);
    fputs("            b = __readByte();\n", out);
    fputs("            if (b < 0) return x;\n", out);
    fputs("            if (b >= 48 && b <= 57) {\n", out);
    fputs("                x = x * 10 + (b - 48);\n", out);
    fputs("                started = true;\n", out);
    fputs("                continue;\n", out);
    fputs("            }\n", out);
    fputs("            if (started && (b == 10 || b == 13 || b == 32 || b == 9)) {\n", out);
    fputs("                return x;\n", out);
    fputs("            }\n", out);
    fputs("        }\n", out);
    fputs("    }\n\n", out);

    fputs("    private static long __print(long v) {\n", out);
    fputs("        __out.print(v);\n", out);
    fputs("        return 0L;\n", out);
    fputs("    }\n\n", out);

    fputs("    private static long __print(String v) {\n", out);
    fputs("        __out.print(v);\n", out);
    fputs("        return 0L;\n", out);
    fputs("    }\n\n", out);
}

static void emit_builtin_function(FILE *out, const FunctionCFG *func) {
    if (!out || !func || !func->info || !func->info->name) return;

    char method[300];
    fn_method_name(func->info->name, method, sizeof(method));

    if (strcmp(func->info->name, "readByte") == 0) {
        fprintf(out, "    public static long %s() {\n", method);
        fputs("        return __readByte();\n", out);
        fputs("    }\n\n", out);
        return;
    }

    if (strcmp(func->info->name, "print") == 0 || strcmp(func->info->name, "printf") == 0) {
        const char *pt = (func->info->param_count > 0 && func->info->param_types && func->info->param_types[0])
                             ? func->info->param_types[0]
                             : "int";
        const char *ptype = java_param_type(pt);
        const char *pname = (func->info->param_count > 0 && func->info->params && func->info->params[0])
                             ? func->info->params[0]
                             : "x";
        char pident[128];
        sanitize_ident(pname, pident, sizeof(pident));
        if (!pident[0]) strncpy(pident, "x", sizeof(pident) - 1);

        fprintf(out, "    public static long %s(%s %s) {\n", method, ptype, pident);
        fprintf(out, "        return __print(%s);\n", pident);
        fputs("    }\n\n", out);
        return;
    }
}

static void emit_function_java(FILE *out, const CFG *cfg, FunctionCFG *func) {
    if (!out || !cfg || !func || !func->info || !func->info->name) return;

    if (is_builtin_stub_function(func)) {
        emit_builtin_function(out, func);
        return;
    }

    JavaFnCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.cfg = cfg;
    ctx.func = func;
    sanitize_ident(func->info->name, ctx.func_name, sizeof(ctx.func_name));
    if (!ctx.func_name[0]) strncpy(ctx.func_name, "anonymous", sizeof(ctx.func_name) - 1);
    fn_method_name(func->info->name, ctx.func_method, sizeof(ctx.func_method));

    collect_vars_for_function(&ctx);

    fprintf(out, "    public static long %s(", ctx.func_method);
    for (int p = 0; p < func->info->param_count; p++) {
        if (p > 0) fputs(", ", out);
        const char *pt = (func->info->param_types && func->info->param_types[p]) ? func->info->param_types[p] : "int";
        const char *ptype = java_param_type(pt);
        JavaVar *v = var_table_find(&ctx.vars, func->info->params[p]);
        const char *pname = (v && v->jname) ? v->jname : "arg";
        fprintf(out, "%s %s", ptype, pname);
    }
    fputs(") {\n", out);

    fprintf(out, "        // source function: %s\n", func->info->name);
    fprintf(out, "        // cfg_nodes: %d\n", func->node_count);

    for (int i = 0; i < ctx.vars.count; i++) {
        JavaVar *v = &ctx.vars.items[i];
        if (v->is_param) continue;
        switch (v->kind) {
            case JAVA_VAR_LONG:
                fprintf(out, "        long %s = 0L;\n", v->jname);
                break;
            case JAVA_VAR_STRING:
                fprintf(out, "        String %s = \"\";\n", v->jname);
                break;
            case JAVA_VAR_LONG_ARRAY:
                fprintf(out, "        long[] %s = new long[%d];\n", v->jname, v->array_size > 0 ? v->array_size : 1);
                break;
            case JAVA_VAR_STRING_ARRAY:
                fprintf(out, "        String[] %s = new String[%d];\n", v->jname, v->array_size > 0 ? v->array_size : 1);
                break;
        }
    }

    int entry_id = (func->entry != NULL) ? func->entry->id : ((func->node_count > 0 && func->nodes[0]) ? func->nodes[0]->id : -1);
    fprintf(out, "        long __ret = 0L;\n");
    fprintf(out, "        int __pc = %d;\n", entry_id);

    fputs("        while (true) {\n", out);
    fputs("            switch (__pc) {\n", out);

    for (int i = 0; i < func->node_count; i++) {
        CFGNode *node = func->nodes[i];
        if (!node) continue;

        fprintf(out, "                case %d: {\n", node->id);

        for (Operation *op = node->operations; op; op = op->next) {
            emit_operation_java(out, &ctx, op);
        }

        if (node->true_branch && node->next) {
            const char *cond_text = extract_condition_text(node);
            char cond_java[2048];
            rewrite_expression(&ctx, cond_text, cond_java, sizeof(cond_java));
            if (!cond_java[0]) strncpy(cond_java, "0", sizeof(cond_java) - 1);

            fprintf(out, "                    if (__toBool(%s)) {\n", cond_java);
            fprintf(out, "                        __pc = %d;\n", node->true_branch->id);
            fputs("                    } else {\n", out);
            fprintf(out, "                        __pc = %d;\n", node->next->id);
            fputs("                    }\n", out);
            fputs("                    continue;\n", out);
        } else if (node->next) {
            fprintf(out, "                    __pc = %d;\n", node->next->id);
            fputs("                    continue;\n", out);
        } else {
            fputs("                    return __ret;\n", out);
        }

        fputs("                }\n", out);
    }

    fputs("                default:\n", out);
    fputs("                    return __ret;\n", out);
    fputs("            }\n", out);
    fputs("        }\n", out);
    fputs("    }\n\n", out);

    var_table_free(&ctx.vars);
}

int emit_java_stub_model(const CFG *cfg, const char *filename, const char *class_name) {
    if (!cfg || !filename || !*filename || !class_name || !*class_name) return 1;

    FILE *out = fopen(filename, "w");
    if (!out) return 2;

    char cname[256];
    sanitize_ident(class_name, cname, sizeof(cname));
    if (!cname[0]) strncpy(cname, "ProgramGenerated", sizeof(cname) - 1);

    fprintf(out, "final class %s {\n", cname);
    fprintf(out, "    private %s() {}\n\n", cname);

    emit_java_runtime(out);

    for (int i = 0; i < cfg->func_count; i++) {
        emit_function_java(out, cfg, cfg->functions[i]);
    }

    if (has_main_function(cfg)) {
        fputs("    public static void main(String[] args) {\n", out);
        fputs("        try {\n", out);
        fputs("            java.lang.reflect.Method __entry = null;\n", out);
        fprintf(out, "            for (java.lang.reflect.Method m : %s.class.getDeclaredMethods()) {\n", cname);
        fputs("                if (m.getName().equals(\"main__sl\")) {\n", out);
        fputs("                    __entry = m;\n", out);
        fputs("                    break;\n", out);
        fputs("                }\n", out);
        fputs("            }\n", out);
        fputs("            if (__entry != null) {\n", out);
        fputs("                Class<?>[] __types = __entry.getParameterTypes();\n", out);
        fputs("                Object[] __argv = new Object[__types.length];\n", out);
        fputs("                for (int i = 0; i < __types.length; i++) {\n", out);
        fputs("                    if (__types[i] == String.class) {\n", out);
        fputs("                        __argv[i] = \"\";\n", out);
        fputs("                    } else if (i == 0) {\n", out);
        fputs("                        __argv[i] = Long.valueOf(args.length);\n", out);
        fputs("                    } else {\n", out);
        fputs("                        __argv[i] = Long.valueOf(0L);\n", out);
        fputs("                    }\n", out);
        fputs("                }\n", out);
        fputs("                __entry.setAccessible(true);\n", out);
        fputs("                __entry.invoke(null, __argv);\n", out);
        fputs("            }\n", out);
        fputs("        } catch (ReflectiveOperationException e) {\n", out);
        fputs("            throw new RuntimeException(e);\n", out);
        fputs("        }\n", out);
        fputs("    }\n", out);
    } else {
        fputs("    public static void main(String[] args) {\n", out);
        fputs("        // no source main() in this module\n", out);
        fputs("    }\n", out);
    }

    fputs("}\n", out);
    fclose(out);
    return 0;
}

int emit_jasmin_model(const CFG *cfg, const char *filename, const char *class_name) {
    if (!cfg || !filename || !*filename || !class_name || !*class_name) return 1;

    FILE *out = fopen(filename, "w");
    if (!out) return 2;

    char cname[256];
    sanitize_ident(class_name, cname, sizeof(cname));
    if (!cname[0]) strncpy(cname, "ProgramGenerated", sizeof(cname) - 1);

    fprintf(out, "; JVM model generated from CFG (standalone)\n");
    fprintf(out, "; Variant: 5 (JVM)\n\n");
    fprintf(out, ".class public %s\n", cname);
    fputs(".super java/lang/Object\n\n", out);

    if (jas_emit_runtime(out, cname) != 0) {
        fclose(out);
        return 3;
    }

    fputs(".method public <init>()V\n", out);
    fputs("  aload_0\n", out);
    fputs("  invokespecial java/lang/Object/<init>()V\n", out);
    fputs("  return\n", out);
    fputs(".end method\n\n", out);

    for (int i = 0; i < cfg->func_count; i++) {
        jas_emit_function(out, cfg, cname, cfg->functions[i]);
    }

    if (has_main_function(cfg)) {
        fputs(".method public static main([Ljava/lang/String;)V\n", out);
        fputs("  .limit stack 4\n", out);
        fputs("  .limit locals 1\n", out);
        fputs("  invokestatic ", out);
        fputs(cname, out);
        fputs("/main__sl()J\n", out);
        fputs("  pop2\n", out);
        fputs("  return\n", out);
        fputs(".end method\n", out);
    } else {
        fputs(".method public static main([Ljava/lang/String;)V\n", out);
        fputs("  .limit stack 1\n", out);
        fputs("  .limit locals 1\n", out);
        fputs("  return\n", out);
        fputs(".end method\n", out);
    }

    fclose(out);
    return 0;
}
