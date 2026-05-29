#include "source_transformer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { HLVM2_MAX_CAPTURES = 8 };

typedef struct {
    char **items;
    int count;
    int capacity;
} StrVec;

typedef struct {
    char *name;
    int arity;
} FunctionVar;

typedef struct {
    FunctionVar *items;
    int count;
    int capacity;
} FunctionVarVec;

typedef struct {
    char *name;
    char *lifted_name;
    int fn_id;
    int param_count;
    char **param_names;
    int capture_count;
    char **captures;
    char *ret_type;
    StrVec body_lines;
} LocalFunction;

typedef struct {
    LocalFunction *items;
    int count;
    int capacity;
} LocalFunctionVec;

typedef struct {
    char *name;
    char *signature;
    StrVec body_lines;
} FunctionBlock;

typedef struct {
    FunctionBlock *items;
    int count;
    int capacity;
} FunctionBlockVec;

static char *xstrdup(const char *s) {
    size_t n;
    char *r;
    if (!s) return NULL;
    n = strlen(s) + 1;
    r = (char *)malloc(n);
    if (!r) return NULL;
    memcpy(r, s, n);
    return r;
}

static void strvec_free(StrVec *v) {
    int i;
    if (!v) return;
    for (i = 0; i < v->count; i++) free(v->items[i]);
    free(v->items);
    v->items = NULL;
    v->count = 0;
    v->capacity = 0;
}

static int strvec_push(StrVec *v, const char *s) {
    char **new_items;
    int new_cap;
    if (v->count >= v->capacity) {
        new_cap = v->capacity ? v->capacity * 2 : 16;
        new_items = (char **)realloc(v->items, sizeof(char *) * (size_t)new_cap);
        if (!new_items) return -1;
        v->items = new_items;
        v->capacity = new_cap;
    }
    v->items[v->count] = xstrdup(s ? s : "");
    if (!v->items[v->count]) return -1;
    v->count++;
    return 0;
}

static int strvec_contains(const StrVec *v, const char *s) {
    int i;
    if (!v || !s) return 0;
    for (i = 0; i < v->count; i++) {
        if (strcmp(v->items[i], s) == 0) return 1;
    }
    return 0;
}

static int strvec_push_unique(StrVec *v, const char *s) {
    if (strvec_contains(v, s)) return 0;
    return strvec_push(v, s);
}

static char *trim_copy(const char *s) {
    const char *b = s;
    const char *e;
    size_t n;
    char *r;
    while (*b && isspace((unsigned char)*b)) b++;
    e = b + strlen(b);
    while (e > b && isspace((unsigned char)e[-1])) e--;
    n = (size_t)(e - b);
    r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, b, n);
    r[n] = '\0';
    return r;
}

static int starts_with_kw(const char *line, const char *kw) {
    size_t k;
    if (!line || !kw) return 0;
    while (*line && isspace((unsigned char)*line)) line++;
    k = strlen(kw);
    if (strncmp(line, kw, k) != 0) return 0;
    return line[k] == '\0' || isspace((unsigned char)line[k]);
}

static int starts_with_text(const char *line, const char *prefix) {
    size_t k;
    if (!line || !prefix) return 0;
    while (*line && isspace((unsigned char)*line)) line++;
    k = strlen(prefix);
    return strncmp(line, prefix, k) == 0;
}

static void functionvarvec_free(FunctionVarVec *v) {
    int i;
    if (!v) return;
    for (i = 0; i < v->count; i++) free(v->items[i].name);
    free(v->items);
    memset(v, 0, sizeof(*v));
}

static FunctionVar *functionvar_find(FunctionVarVec *v, const char *name) {
    int i;
    if (!v || !name) return NULL;
    for (i = 0; i < v->count; i++) {
        if (strcmp(v->items[i].name, name) == 0) return &v->items[i];
    }
    return NULL;
}

static int functionvar_add(FunctionVarVec *v, const char *name, int arity) {
    FunctionVar *f;
    FunctionVar *new_items;
    int new_cap;
    if (!v || !name) return -1;
    f = functionvar_find(v, name);
    if (f) {
        f->arity = arity;
        return 0;
    }
    if (v->count >= v->capacity) {
        new_cap = v->capacity ? v->capacity * 2 : 8;
        new_items = (FunctionVar *)realloc(v->items, sizeof(FunctionVar) * (size_t)new_cap);
        if (!new_items) return -1;
        v->items = new_items;
        v->capacity = new_cap;
    }
    v->items[v->count].name = xstrdup(name);
    if (!v->items[v->count].name) return -1;
    v->items[v->count].arity = arity;
    v->count++;
    return 0;
}

static void localfunction_free(LocalFunction *lf) {
    int i;
    if (!lf) return;
    free(lf->name);
    free(lf->lifted_name);
    for (i = 0; i < lf->param_count; i++) free(lf->param_names[i]);
    free(lf->param_names);
    for (i = 0; i < lf->capture_count; i++) free(lf->captures[i]);
    free(lf->captures);
    free(lf->ret_type);
    strvec_free(&lf->body_lines);
    memset(lf, 0, sizeof(*lf));
}

static void localfunctionvec_free(LocalFunctionVec *v) {
    int i;
    if (!v) return;
    for (i = 0; i < v->count; i++) localfunction_free(&v->items[i]);
    free(v->items);
    memset(v, 0, sizeof(*v));
}

static LocalFunction *localfunction_find(LocalFunctionVec *v, const char *name) {
    int i;
    if (!v || !name) return NULL;
    for (i = 0; i < v->count; i++) {
        if (strcmp(v->items[i].name, name) == 0) return &v->items[i];
    }
    return NULL;
}

static int localfunctionvec_push(LocalFunctionVec *v, const LocalFunction *lf) {
    LocalFunction *new_items;
    int new_cap;
    if (v->count >= v->capacity) {
        new_cap = v->capacity ? v->capacity * 2 : 8;
        new_items = (LocalFunction *)realloc(v->items, sizeof(LocalFunction) * (size_t)new_cap);
        if (!new_items) return -1;
        v->items = new_items;
        v->capacity = new_cap;
    }
    v->items[v->count] = *lf;
    v->count++;
    return 0;
}

static void functionblock_free(FunctionBlock *f) {
    if (!f) return;
    free(f->name);
    free(f->signature);
    strvec_free(&f->body_lines);
    memset(f, 0, sizeof(*f));
}

static void functionblockvec_free(FunctionBlockVec *v) {
    int i;
    if (!v) return;
    for (i = 0; i < v->count; i++) functionblock_free(&v->items[i]);
    free(v->items);
    memset(v, 0, sizeof(*v));
}

static int functionblockvec_push(FunctionBlockVec *v, const FunctionBlock *f) {
    FunctionBlock *new_items;
    int new_cap;
    if (v->count >= v->capacity) {
        new_cap = v->capacity ? v->capacity * 2 : 8;
        new_items = (FunctionBlock *)realloc(v->items, sizeof(FunctionBlock) * (size_t)new_cap);
        if (!new_items) return -1;
        v->items = new_items;
        v->capacity = new_cap;
    }
    v->items[v->count] = *f;
    v->count++;
    return 0;
}

static int read_lines(const char *path, StrVec *out) {
    FILE *f = fopen(path, "r");
    char buf[4096];
    if (!f) return -1;
    while (fgets(buf, sizeof(buf), f)) {
        size_t n = strlen(buf);
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = '\0';
        if (strvec_push(out, buf) != 0) {
            fclose(f);
            return -1;
        }
    }
    fclose(f);
    return 0;
}

static int write_lines(const char *path, const StrVec *lines) {
    FILE *f = fopen(path, "w");
    int i;
    if (!f) return -1;
    for (i = 0; i < lines->count; i++) {
        fprintf(f, "%s\n", lines->items[i]);
    }
    fclose(f);
    return 0;
}

static int split_top_level_csv(const char *text, StrVec *parts) {
    int depth = 0;
    const char *start = text;
    const char *p = text;
    while (*p) {
        if (*p == '(') depth++;
        else if (*p == ')' && depth > 0) depth--;
        else if (*p == ',' && depth == 0) {
            size_t n = (size_t)(p - start);
            char *tmp = (char *)malloc(n + 1);
            char *trimmed;
            if (!tmp) return -1;
            memcpy(tmp, start, n);
            tmp[n] = '\0';
            trimmed = trim_copy(tmp);
            free(tmp);
            if (!trimmed) return -1;
            if (strvec_push(parts, trimmed) != 0) {
                free(trimmed);
                return -1;
            }
            free(trimmed);
            start = p + 1;
        }
        p++;
    }
    {
        char *tail = trim_copy(start);
        if (!tail) return -1;
        if (strvec_push(parts, tail) != 0) {
            free(tail);
            return -1;
        }
        free(tail);
    }
    return 0;
}

static int parse_signature(const char *line, char **name, char **args, char **ret_type) {
    const char *p;
    const char *lp;
    const char *rp;
    char *trimmed = trim_copy(line);
    char *after;
    size_t n;
    char *ret;
    if (!trimmed) return -1;
    if (!starts_with_kw(trimmed, "function")) {
        free(trimmed);
        return -1;
    }
    p = trimmed;
    while (*p && isspace((unsigned char)*p)) p++;
    p += 8;
    while (*p && isspace((unsigned char)*p)) p++;
    lp = strchr(p, '(');
    rp = strrchr(p, ')');
    if (!lp || !rp || rp < lp) {
        free(trimmed);
        return -1;
    }
    n = (size_t)(lp - p);
    *name = (char *)malloc(n + 1);
    if (!*name) {
        free(trimmed);
        return -1;
    }
    memcpy(*name, p, n);
    (*name)[n] = '\0';
    {
        char *t = trim_copy(*name);
        free(*name);
        *name = t;
        if (!*name) {
            free(trimmed);
            return -1;
        }
    }

    n = (size_t)(rp - (lp + 1));
    *args = (char *)malloc(n + 1);
    if (!*args) {
        free(*name);
        free(trimmed);
        return -1;
    }
    memcpy(*args, lp + 1, n);
    (*args)[n] = '\0';

    after = trim_copy(rp + 1);
    if (!after) {
        free(*name);
        free(*args);
        free(trimmed);
        return -1;
    }
    ret = NULL;
    if (starts_with_kw(after, "as")) {
        const char *q = after;
        while (*q && isspace((unsigned char)*q)) q++;
        q += 2;
        while (*q && isspace((unsigned char)*q)) q++;
        ret = trim_copy(q);
    }
    if (!ret) ret = xstrdup("int");
    *ret_type = ret;
    free(after);
    free(trimmed);
    return ret ? 0 : -1;
}

static int count_func_arity(const char *type_text) {
    const char *lp = strchr(type_text, '(');
    const char *rp = strrchr(type_text, ')');
    StrVec parts = {0};
    int arity = 0;
    int i;
    char *inside;
    if (!lp || !rp || rp < lp) return 0;
    inside = (char *)malloc((size_t)(rp - lp));
    if (!inside) return 0;
    memcpy(inside, lp + 1, (size_t)(rp - lp - 1));
    inside[rp - lp - 1] = '\0';
    if (split_top_level_csv(inside, &parts) != 0) {
        free(inside);
        return 0;
    }
    free(inside);
    for (i = 0; i < parts.count; i++) {
        char *t = trim_copy(parts.items[i]);
        if (t && *t) arity++;
        free(t);
    }
    strvec_free(&parts);
    return arity;
}

static int parse_arg_names(const char *args_text, StrVec *names, FunctionVarVec *fn_args) {
    StrVec parts = {0};
    int i;
    if (split_top_level_csv(args_text, &parts) != 0) return -1;
    for (i = 0; i < parts.count; i++) {
        char *p = trim_copy(parts.items[i]);
        char *as_pos;
        char *name;
        char *type;
        if (!p) {
            strvec_free(&parts);
            return -1;
        }
        if (!*p) {
            free(p);
            continue;
        }
        as_pos = strstr(p, " as ");
        if (!as_pos) {
            as_pos = strstr(p, " AS ");
        }
        if (!as_pos) {
            free(p);
            continue;
        }
        *as_pos = '\0';
        name = trim_copy(p);
        type = trim_copy(as_pos + 4);
        free(p);
        if (!name || !type) {
            free(name);
            free(type);
            strvec_free(&parts);
            return -1;
        }
        if (strvec_push_unique(names, name) != 0) {
            free(name);
            free(type);
            strvec_free(&parts);
            return -1;
        }
        if (starts_with_text(type, "func(")) {
            if (functionvar_add(fn_args, name, count_func_arity(type)) != 0) {
                free(name);
                free(type);
                strvec_free(&parts);
                return -1;
            }
        }
        free(name);
        free(type);
    }
    strvec_free(&parts);
    return 0;
}

static int parse_dim_decl(const char *line, StrVec *names, char **type_out) {
    char *trimmed = trim_copy(line);
    char *as_pos;
    char *name_part;
    char *type_part;
    StrVec vars = {0};
    int i;
    if (!trimmed) return -1;
    if (!starts_with_kw(trimmed, "dim")) {
        free(trimmed);
        return 1;
    }
    name_part = trimmed + 3;
    while (*name_part && isspace((unsigned char)*name_part)) name_part++;
    as_pos = strstr(name_part, " as ");
    if (!as_pos) as_pos = strstr(name_part, " AS ");
    if (!as_pos) {
        free(trimmed);
        return -1;
    }
    *as_pos = '\0';
    type_part = trim_copy(as_pos + 4);
    if (!type_part) {
        free(trimmed);
        return -1;
    }
    if (split_top_level_csv(name_part, &vars) != 0) {
        free(type_part);
        free(trimmed);
        return -1;
    }
    for (i = 0; i < vars.count; i++) {
        char *v = trim_copy(vars.items[i]);
        if (!v) {
            free(type_part);
            strvec_free(&vars);
            free(trimmed);
            return -1;
        }
        if (*v) {
            if (strvec_push_unique(names, v) != 0) {
                free(v);
                free(type_part);
                strvec_free(&vars);
                free(trimmed);
                return -1;
            }
        }
        free(v);
    }
    strvec_free(&vars);
    *type_out = type_part;
    free(trimmed);
    return 0;
}

static int is_identifier_token(const char *tok) {
    int i;
    if (!tok || !*tok) return 0;
    if (!(isalpha((unsigned char)tok[0]) || tok[0] == '_')) return 0;
    for (i = 1; tok[i]; i++) {
        if (!(isalnum((unsigned char)tok[i]) || tok[i] == '_')) return 0;
    }
    return 1;
}

static int is_keyword_or_builtin(const char *tok) {
    static const char *kws[] = {
        "function", "end", "if", "then", "else", "while", "wend", "do", "loop", "until", "break",
        "dim", "as", "true", "false", "and", "or", "not", "input", "output", "return"
    };
    int i;
    for (i = 0; i < (int)(sizeof(kws) / sizeof(kws[0])); i++) {
        if (strcmp(tok, kws[i]) == 0) return 1;
    }
    return 0;
}

static int collect_identifiers_from_line(const char *line, StrVec *ids) {
    char tok[256];
    int ti = 0;
    int in_str = 0;
    int in_char = 0;
    int i;
    for (i = 0; line[i]; i++) {
        char c = line[i];
        if (c == '"' && !in_char) {
            in_str = !in_str;
            continue;
        }
        if (c == '\'' && !in_str) {
            in_char = !in_char;
            continue;
        }
        if (in_str || in_char) continue;

        if (isalnum((unsigned char)c) || c == '_') {
            if (ti + 1 < (int)sizeof(tok)) tok[ti++] = c;
        } else {
            if (ti > 0) {
                tok[ti] = '\0';
                if (is_identifier_token(tok) && !is_keyword_or_builtin(tok)) {
                    if (strvec_push_unique(ids, tok) != 0) return -1;
                }
                ti = 0;
            }
        }
    }
    if (ti > 0) {
        tok[ti] = '\0';
        if (is_identifier_token(tok) && !is_keyword_or_builtin(tok)) {
            if (strvec_push_unique(ids, tok) != 0) return -1;
        }
    }
    return 0;
}

static int find_matching_end_function(const StrVec *lines, int start, int *end_idx) {
    int depth = 0;
    int i;
    for (i = start; i < lines->count; i++) {
        char *t = trim_copy(lines->items[i]);
        if (!t) return -1;
        if (starts_with_kw(t, "function")) depth++;
        if (starts_with_text(t, "end function")) {
            depth--;
            if (depth == 0) {
                free(t);
                *end_idx = i;
                return 0;
            }
        }
        free(t);
    }
    return -1;
}

static int parse_top_functions(const StrVec *lines, FunctionBlockVec *out, StrVec *prologue) {
    int i = 0;
    while (i < lines->count) {
        char *t = trim_copy(lines->items[i]);
        int is_func;
        if (!t) return -1;
        is_func = starts_with_kw(t, "function");
        free(t);
        if (!is_func) {
            if (strvec_push(prologue, lines->items[i]) != 0) return -1;
            i++;
            continue;
        }
        {
            int end_idx;
            FunctionBlock f;
            char *name = NULL;
            char *args = NULL;
            char *ret = NULL;
            int j;
            memset(&f, 0, sizeof(f));
            if (find_matching_end_function(lines, i, &end_idx) != 0) return -1;
            if (parse_signature(lines->items[i], &name, &args, &ret) != 0) {
                free(name); free(args); free(ret);
                return -1;
            }
            f.name = name;
            f.signature = xstrdup(lines->items[i]);
            free(args);
            free(ret);
            if (!f.signature) {
                functionblock_free(&f);
                return -1;
            }
            for (j = i + 1; j < end_idx; j++) {
                if (strvec_push(&f.body_lines, lines->items[j]) != 0) {
                    functionblock_free(&f);
                    return -1;
                }
            }
            if (functionblockvec_push(out, &f) != 0) {
                functionblock_free(&f);
                return -1;
            }
            i = end_idx + 1;
        }
    }
    return 0;
}

static int build_local_functions(const FunctionBlock *fn,
                                 const StrVec *outer_vars,
                                 LocalFunctionVec *locals,
                                 StrVec *body_without_locals,
                                 int *id_seed) {
    int i = 0;
    while (i < fn->body_lines.count) {
        char *t = trim_copy(fn->body_lines.items[i]);
        int is_local = t && starts_with_kw(t, "function");
        free(t);
        if (!is_local) {
            if (strvec_push(body_without_locals, fn->body_lines.items[i]) != 0) return -1;
            i++;
            continue;
        }

        {
            int j;
            int depth = 0;
            LocalFunction lf;
            char *name = NULL;
            char *args = NULL;
            char *ret = NULL;
            StrVec local_vars = {0};
            StrVec used = {0};
            int k;
            memset(&lf, 0, sizeof(lf));

            if (parse_signature(fn->body_lines.items[i], &name, &args, &ret) != 0) {
                free(name); free(args); free(ret);
                return -1;
            }
            lf.name = name;
            lf.ret_type = ret;
            lf.fn_id = (*id_seed)++;

            if (parse_arg_names(args, &local_vars, &(FunctionVarVec){0}) != 0) {
                free(args);
                localfunction_free(&lf);
                strvec_free(&local_vars);
                return -1;
            }
            free(args);

            for (k = i; k < fn->body_lines.count; k++) {
                char *tk = trim_copy(fn->body_lines.items[k]);
                if (!tk) {
                    localfunction_free(&lf);
                    strvec_free(&local_vars);
                    return -1;
                }
                if (starts_with_kw(tk, "function")) depth++;
                if (starts_with_text(tk, "end function")) {
                    depth--;
                    free(tk);
                    if (depth == 0) {
                        j = k;
                        break;
                    }
                    continue;
                }
                if (k > i) {
                    if (strvec_push(&lf.body_lines, fn->body_lines.items[k]) != 0) {
                        free(tk);
                        localfunction_free(&lf);
                        strvec_free(&local_vars);
                        return -1;
                    }
                }
                free(tk);
            }

            if (depth != 0) {
                localfunction_free(&lf);
                strvec_free(&local_vars);
                return -1;
            }

            for (k = 0; k < lf.body_lines.count; k++) {
                char *decl_type = NULL;
                StrVec decl_vars = {0};
                if (parse_dim_decl(lf.body_lines.items[k], &decl_vars, &decl_type) == 0) {
                    int m;
                    for (m = 0; m < decl_vars.count; m++) {
                        if (strvec_push_unique(&local_vars, decl_vars.items[m]) != 0) {
                            free(decl_type);
                            strvec_free(&decl_vars);
                            localfunction_free(&lf);
                            strvec_free(&local_vars);
                            return -1;
                        }
                    }
                }
                free(decl_type);
                strvec_free(&decl_vars);
            }

            for (k = 0; k < lf.body_lines.count; k++) {
                if (collect_identifiers_from_line(lf.body_lines.items[k], &used) != 0) {
                    localfunction_free(&lf);
                    strvec_free(&local_vars);
                    strvec_free(&used);
                    return -1;
                }
            }

            lf.capture_count = 0;
            lf.captures = NULL;
            for (k = 0; k < outer_vars->count; k++) {
                const char *ov = outer_vars->items[k];
                if (strvec_contains(&used, ov) && !strvec_contains(&local_vars, ov)) {
                    char **new_caps = (char **)realloc(lf.captures, sizeof(char *) * (size_t)(lf.capture_count + 1));
                    if (!new_caps) {
                        localfunction_free(&lf);
                        strvec_free(&local_vars);
                        strvec_free(&used);
                        return -1;
                    }
                    lf.captures = new_caps;
                    lf.captures[lf.capture_count] = xstrdup(ov);
                    if (!lf.captures[lf.capture_count]) {
                        localfunction_free(&lf);
                        strvec_free(&local_vars);
                        strvec_free(&used);
                        return -1;
                    }
                    lf.capture_count++;
                }
            }

            lf.param_count = local_vars.count;
            lf.param_names = (char **)calloc((size_t)local_vars.count, sizeof(char *));
            if (!lf.param_names) {
                localfunction_free(&lf);
                strvec_free(&local_vars);
                strvec_free(&used);
                return -1;
            }
            for (k = 0; k < local_vars.count; k++) {
                lf.param_names[k] = xstrdup(local_vars.items[k]);
                if (!lf.param_names[k]) {
                    localfunction_free(&lf);
                    strvec_free(&local_vars);
                    strvec_free(&used);
                    return -1;
                }
            }

            {
                char lifted[512];
                snprintf(lifted, sizeof(lifted), "%s__%s", fn->name, lf.name);
                lf.lifted_name = xstrdup(lifted);
                if (!lf.lifted_name) {
                    localfunction_free(&lf);
                    strvec_free(&local_vars);
                    strvec_free(&used);
                    return -1;
                }
            }

            if (localfunctionvec_push(locals, &lf) != 0) {
                localfunction_free(&lf);
                strvec_free(&local_vars);
                strvec_free(&used);
                return -1;
            }
            strvec_free(&local_vars);
            strvec_free(&used);
            i = j + 1;
        }
    }
    return 0;
}

static int rewrite_func_types_in_signature(const char *line, char **out_line) {
    char *trimmed = trim_copy(line);
    char *name = NULL;
    char *args = NULL;
    char *ret = NULL;
    StrVec parts = {0};
    StrVec new_parts = {0};
    char buf[4096];
    int i;
    if (!trimmed) return -1;
    if (parse_signature(trimmed, &name, &args, &ret) != 0) {
        *out_line = trimmed;
        return 0;
    }
    if (split_top_level_csv(args, &parts) != 0) {
        free(trimmed); free(name); free(args); free(ret);
        return -1;
    }
    for (i = 0; i < parts.count; i++) {
        char *p = trim_copy(parts.items[i]);
        char *as_pos = p ? strstr(p, " as ") : NULL;
        if (!as_pos && p) as_pos = strstr(p, " AS ");
        if (p && as_pos) {
            char *nm;
            char *tp;
            *as_pos = '\0';
            nm = trim_copy(p);
            tp = trim_copy(as_pos + 4);
            if (nm && tp && starts_with_text(tp, "func(")) {
                char item[512];
                snprintf(item, sizeof(item), "%s as int", nm);
                if (strvec_push(&new_parts, item) != 0) {
                    free(nm); free(tp); free(p);
                    strvec_free(&parts); strvec_free(&new_parts);
                    free(trimmed); free(name); free(args); free(ret);
                    return -1;
                }
            } else if (strvec_push(&new_parts, parts.items[i]) != 0) {
                free(nm); free(tp); free(p);
                strvec_free(&parts); strvec_free(&new_parts);
                free(trimmed); free(name); free(args); free(ret);
                return -1;
            }
            free(nm);
            free(tp);
        } else if (strvec_push(&new_parts, parts.items[i]) != 0) {
            free(p);
            strvec_free(&parts); strvec_free(&new_parts);
            free(trimmed); free(name); free(args); free(ret);
            return -1;
        }
        free(p);
    }

    snprintf(buf, sizeof(buf), "function %s(", name);
    for (i = 0; i < new_parts.count; i++) {
        strncat(buf, new_parts.items[i], sizeof(buf) - strlen(buf) - 1);
        if (i + 1 < new_parts.count) strncat(buf, ", ", sizeof(buf) - strlen(buf) - 1);
    }
    strncat(buf, ")", sizeof(buf) - strlen(buf) - 1);
    if (ret && *ret) {
        strncat(buf, " as ", sizeof(buf) - strlen(buf) - 1);
        strncat(buf, ret, sizeof(buf) - strlen(buf) - 1);
    }
    *out_line = xstrdup(buf);

    strvec_free(&parts);
    strvec_free(&new_parts);
    free(trimmed); free(name); free(args); free(ret);
    return *out_line ? 0 : -1;
}

static int parse_assignment(const char *line, char **lhs, char **rhs, int *has_semicolon) {
    const char *eq;
    char *trimmed = trim_copy(line);
    char *left;
    char *right;
    size_t n;
    if (!trimmed) return -1;
    eq = strchr(trimmed, '=');
    if (!eq) {
        free(trimmed);
        return 1;
    }
    left = (char *)malloc((size_t)(eq - trimmed) + 1);
    if (!left) {
        free(trimmed);
        return -1;
    }
    memcpy(left, trimmed, (size_t)(eq - trimmed));
    left[eq - trimmed] = '\0';
    right = xstrdup(eq + 1);
    if (!right) {
        free(left);
        free(trimmed);
        return -1;
    }
    {
        char *lt = trim_copy(left);
        char *rt;
        free(left);
        left = lt;
        if (!left) {
            free(right);
            free(trimmed);
            return -1;
        }
        n = strlen(right);
        *has_semicolon = 0;
        if (n > 0 && right[n - 1] == ';') {
            right[n - 1] = '\0';
            *has_semicolon = 1;
        }
        rt = trim_copy(right);
        free(right);
        right = rt;
        if (!right) {
            free(left);
            free(trimmed);
            return -1;
        }
    }
    *lhs = left;
    *rhs = right;
    free(trimmed);
    return 0;
}

static int parse_simple_call(const char *expr, char **callee, StrVec *args) {
    const char *lp;
    const char *rp;
    char *trimmed = trim_copy(expr);
    char *name;
    char *argtext;
    if (!trimmed) return -1;
    lp = strchr(trimmed, '(');
    rp = strrchr(trimmed, ')');
    if (!lp || !rp || rp < lp) {
        free(trimmed);
        return 1;
    }
    {
        const char *q = rp + 1;
        while (*q) {
            if (!isspace((unsigned char)*q)) {
                free(trimmed);
                return 1;
            }
            q++;
        }
    }
    name = (char *)malloc((size_t)(lp - trimmed) + 1);
    if (!name) {
        free(trimmed);
        return -1;
    }
    memcpy(name, trimmed, (size_t)(lp - trimmed));
    name[lp - trimmed] = '\0';
    {
        char *nt = trim_copy(name);
        free(name);
        name = nt;
    }
    if (!name || !is_identifier_token(name)) {
        free(name);
        free(trimmed);
        return 1;
    }
    argtext = (char *)malloc((size_t)(rp - lp));
    if (!argtext) {
        free(name);
        free(trimmed);
        return -1;
    }
    memcpy(argtext, lp + 1, (size_t)(rp - lp - 1));
    argtext[rp - lp - 1] = '\0';
    if (split_top_level_csv(argtext, args) != 0) {
        free(argtext);
        free(name);
        free(trimmed);
        return -1;
    }
    free(argtext);
    free(trimmed);
    *callee = name;
    return 0;
}

static int emit_dispatch_call(StrVec *out,
                              const char *target_var,
                              const char *fnvar,
                              const StrVec *call_args,
                              const LocalFunctionVec *locals) {
    int i, j;
    char line[2048];
    for (i = 0; i < locals->count; i++) {
        const LocalFunction *lf = &locals->items[i];
        snprintf(line, sizeof(line), "if %s__fnid == %d then", fnvar, lf->fn_id);
        if (strvec_push(out, line) != 0) return -1;

        snprintf(line, sizeof(line), "    %s = %s(", target_var, lf->lifted_name);
        for (j = 0; j < lf->capture_count; j++) {
            char cap[64];
            snprintf(cap, sizeof(cap), "%s__cap%d", fnvar, j);
            strncat(line, cap, sizeof(line) - strlen(line) - 1);
            if (j + 1 < lf->capture_count || call_args->count > 0) {
                strncat(line, ", ", sizeof(line) - strlen(line) - 1);
            }
        }
        for (j = 0; j < call_args->count; j++) {
            strncat(line, call_args->items[j], sizeof(line) - strlen(line) - 1);
            if (j + 1 < call_args->count) strncat(line, ", ", sizeof(line) - strlen(line) - 1);
        }
        strncat(line, ");", sizeof(line) - strlen(line) - 1);
        if (strvec_push(out, line) != 0) return -1;
        if (strvec_push(out, "end if") != 0) return -1;
    }
    return 0;
}

static int transform_function_body(const FunctionBlock *fn,
                                   const LocalFunctionVec *locals,
                                   StrVec *out_body,
                                   FunctionVarVec *fn_vars,
                                   const StrVec *outer_vars) {
    int i;
    for (i = 0; i < fn->body_lines.count; i++) {
        char *decl_type = NULL;
        StrVec decl_vars = {0};
        int dim_res = parse_dim_decl(fn->body_lines.items[i], &decl_vars, &decl_type);
        if (dim_res == 0 && decl_type && starts_with_text(decl_type, "func(")) {
            int arity = count_func_arity(decl_type);
            int v;
            for (v = 0; v < decl_vars.count; v++) {
                char line[256];
                int c;
                if (functionvar_add(fn_vars, decl_vars.items[v], arity) != 0) {
                    free(decl_type);
                    strvec_free(&decl_vars);
                    return -1;
                }
                snprintf(line, sizeof(line), "dim %s__fnid as int", decl_vars.items[v]);
                if (strvec_push(out_body, line) != 0) {
                    free(decl_type);
                    strvec_free(&decl_vars);
                    return -1;
                }
                for (c = 0; c < HLVM2_MAX_CAPTURES; c++) {
                    snprintf(line, sizeof(line), "dim %s__cap%d as int", decl_vars.items[v], c);
                    if (strvec_push(out_body, line) != 0) {
                        free(decl_type);
                        strvec_free(&decl_vars);
                        return -1;
                    }
                }
            }
            free(decl_type);
            strvec_free(&decl_vars);
            continue;
        }
        free(decl_type);
        strvec_free(&decl_vars);

        {
            char *lhs = NULL;
            char *rhs = NULL;
            int has_semicolon = 0;
            int asg = parse_assignment(fn->body_lines.items[i], &lhs, &rhs, &has_semicolon);
            if (asg == 0) {
                FunctionVar *fv = functionvar_find(fn_vars, lhs);
                LocalFunction *lf = localfunction_find((LocalFunctionVec *)locals, rhs);
                if (fv && lf) {
                    char line[256];
                    int c;
                    snprintf(line, sizeof(line), "%s__fnid = %d;", lhs, lf->fn_id);
                    if (strvec_push(out_body, line) != 0) {
                        free(lhs); free(rhs);
                        return -1;
                    }
                    for (c = 0; c < lf->capture_count && c < HLVM2_MAX_CAPTURES; c++) {
                        snprintf(line, sizeof(line), "%s__cap%d = %s;", lhs, c, lf->captures[c]);
                        if (strvec_push(out_body, line) != 0) {
                            free(lhs); free(rhs);
                            return -1;
                        }
                    }
                    free(lhs); free(rhs);
                    continue;
                }

                if (fv) {
                    /* keep as regular assignment fallback */
                }

                {
                    char *callee = NULL;
                    StrVec args = {0};
                    int call_res = parse_simple_call(rhs, &callee, &args);
                    if (call_res == 0 && functionvar_find(fn_vars, callee)) {
                        if (emit_dispatch_call(out_body, lhs, callee, &args, locals) != 0) {
                            free(lhs); free(rhs); free(callee); strvec_free(&args);
                            return -1;
                        }
                        free(lhs); free(rhs); free(callee); strvec_free(&args);
                        continue;
                    }
                    free(callee);
                    strvec_free(&args);
                }
                free(lhs); free(rhs);
            }
        }

        {
            char *t = trim_copy(fn->body_lines.items[i]);
            char *callee = NULL;
            StrVec args = {0};
            int has_semicolon = 0;
            if (!t) return -1;
            if (t[0]) {
                size_t n = strlen(t);
                if (n > 0 && t[n - 1] == ';') {
                    t[n - 1] = '\0';
                    has_semicolon = 1;
                }
                if (parse_simple_call(t, &callee, &args) == 0 && functionvar_find(fn_vars, callee)) {
                    if (emit_dispatch_call(out_body, "__hlvm2_call_tmp", callee, &args, locals) != 0) {
                        free(t); free(callee); strvec_free(&args);
                        return -1;
                    }
                    if (!has_semicolon) {
                        if (strvec_push(out_body, "__hlvm2_call_tmp;") != 0) {
                            free(t); free(callee); strvec_free(&args);
                            return -1;
                        }
                    }
                    free(t); free(callee); strvec_free(&args);
                    continue;
                }
            }
            free(callee);
            strvec_free(&args);
            free(t);
        }

        if (strvec_push(out_body, fn->body_lines.items[i]) != 0) return -1;
    }

    (void)outer_vars;
    return 0;
}

static int emit_lifted_function(const FunctionBlock *parent,
                                const LocalFunction *lf,
                                StrVec *out_lines) {
    char sig[4096];
    int i;
    snprintf(sig, sizeof(sig), "function %s(", lf->lifted_name);
    for (i = 0; i < lf->capture_count; i++) {
        strncat(sig, lf->captures[i], sizeof(sig) - strlen(sig) - 1);
        strncat(sig, " as int", sizeof(sig) - strlen(sig) - 1);
        if (i + 1 < lf->capture_count || lf->param_count > 0) {
            strncat(sig, ", ", sizeof(sig) - strlen(sig) - 1);
        }
    }
    for (i = 0; i < lf->param_count; i++) {
        strncat(sig, lf->param_names[i], sizeof(sig) - strlen(sig) - 1);
        strncat(sig, " as int", sizeof(sig) - strlen(sig) - 1);
        if (i + 1 < lf->param_count) strncat(sig, ", ", sizeof(sig) - strlen(sig) - 1);
    }
    strncat(sig, ") as ", sizeof(sig) - strlen(sig) - 1);
    strncat(sig, lf->ret_type ? lf->ret_type : "int", sizeof(sig) - strlen(sig) - 1);

    if (strvec_push(out_lines, sig) != 0) return -1;
    for (i = 0; i < lf->body_lines.count; i++) {
        if (strvec_push(out_lines, lf->body_lines.items[i]) != 0) return -1;
    }
    if (strvec_push(out_lines, "end function") != 0) return -1;
    if (strvec_push(out_lines, "") != 0) return -1;

    (void)parent;
    return 0;
}

static int transform_top_function(const FunctionBlock *fn,
                                  StrVec *out_lines,
                                  StrVec *lifted_out,
                                  int *id_seed) {
    char *sig = NULL;
    char *name = NULL;
    char *args = NULL;
    char *ret = NULL;
    StrVec outer_vars = {0};
    FunctionVarVec fn_vars = {0};
    StrVec body_wo_locals = {0};
    LocalFunctionVec locals = {0};
    StrVec new_body = {0};
    int i;

    if (rewrite_func_types_in_signature(fn->signature, &sig) != 0) goto fail;
    if (parse_signature(sig, &name, &args, &ret) != 0) goto fail;
    if (parse_arg_names(args, &outer_vars, &fn_vars) != 0) goto fail;

    for (i = 0; i < fn->body_lines.count; i++) {
        char *decl_type = NULL;
        StrVec decl_vars = {0};
        int r = parse_dim_decl(fn->body_lines.items[i], &decl_vars, &decl_type);
        if (r == 0) {
            int j;
            for (j = 0; j < decl_vars.count; j++) {
                if (strvec_push_unique(&outer_vars, decl_vars.items[j]) != 0) {
                    free(decl_type);
                    strvec_free(&decl_vars);
                    goto fail;
                }
            }
            if (decl_type && starts_with_text(decl_type, "func(")) {
                int arity = count_func_arity(decl_type);
                for (j = 0; j < decl_vars.count; j++) {
                    if (functionvar_add(&fn_vars, decl_vars.items[j], arity) != 0) {
                        free(decl_type);
                        strvec_free(&decl_vars);
                        goto fail;
                    }
                }
            }
        }
        free(decl_type);
        strvec_free(&decl_vars);
    }

    if (build_local_functions(fn, &outer_vars, &locals, &body_wo_locals, id_seed) != 0) goto fail;

    {
        FunctionBlock tmp = *fn;
        tmp.body_lines = body_wo_locals;
        if (transform_function_body(&tmp, &locals, &new_body, &fn_vars, &outer_vars) != 0) goto fail;
        body_wo_locals.items = NULL;
        body_wo_locals.count = 0;
        body_wo_locals.capacity = 0;
    }

    if (functionvar_find(&fn_vars, "__hlvm2_call_tmp")) {
        /* unreachable; keep placeholder */
    }
    if (fn_vars.count > 0) {
        if (strvec_push(out_lines, sig) != 0) goto fail;
        if (strvec_push(out_lines, "dim __hlvm2_call_tmp as int") != 0) goto fail;
        for (i = 0; i < new_body.count; i++) {
            if (strvec_push(out_lines, new_body.items[i]) != 0) goto fail;
        }
        if (strvec_push(out_lines, "end function") != 0) goto fail;
        if (strvec_push(out_lines, "") != 0) goto fail;
    } else {
        if (strvec_push(out_lines, sig) != 0) goto fail;
        for (i = 0; i < new_body.count; i++) {
            if (strvec_push(out_lines, new_body.items[i]) != 0) goto fail;
        }
        if (strvec_push(out_lines, "end function") != 0) goto fail;
        if (strvec_push(out_lines, "") != 0) goto fail;
    }

    for (i = 0; i < locals.count; i++) {
        if (emit_lifted_function(fn, &locals.items[i], lifted_out) != 0) goto fail;
    }

    free(sig); free(name); free(args); free(ret);
    strvec_free(&outer_vars);
    functionvarvec_free(&fn_vars);
    strvec_free(&body_wo_locals);
    localfunctionvec_free(&locals);
    strvec_free(&new_body);
    return 0;

fail:
    free(sig); free(name); free(args); free(ret);
    strvec_free(&outer_vars);
    functionvarvec_free(&fn_vars);
    strvec_free(&body_wo_locals);
    localfunctionvec_free(&locals);
    strvec_free(&new_body);
    return -1;
}

int hlvm2_transform_source_file(const char *input_path,
                                const char *output_path,
                                char **error_text) {
    StrVec lines = {0};
    StrVec prologue = {0};
    FunctionBlockVec funcs = {0};
    StrVec out = {0};
    StrVec lifted = {0};
    int id_seed = 1;
    int i;

    if (read_lines(input_path, &lines) != 0) {
        if (error_text) *error_text = xstrdup("Cannot read input source file");
        return -1;
    }
    if (parse_top_functions(&lines, &funcs, &prologue) != 0) {
        if (error_text) *error_text = xstrdup("Failed to parse function blocks");
        goto fail;
    }

    for (i = 0; i < prologue.count; i++) {
        if (strvec_push(&out, prologue.items[i]) != 0) goto fail;
    }

    for (i = 0; i < funcs.count; i++) {
        if (transform_top_function(&funcs.items[i], &out, &lifted, &id_seed) != 0) {
            if (error_text) *error_text = xstrdup("Failed to transform local functions/closures");
            goto fail;
        }
    }

    for (i = 0; i < lifted.count; i++) {
        if (strvec_push(&out, lifted.items[i]) != 0) goto fail;
    }

    if (write_lines(output_path, &out) != 0) {
        if (error_text) *error_text = xstrdup("Cannot write transformed source file");
        goto fail;
    }

    strvec_free(&lines);
    strvec_free(&prologue);
    functionblockvec_free(&funcs);
    strvec_free(&out);
    strvec_free(&lifted);
    return 0;

fail:
    strvec_free(&lines);
    strvec_free(&prologue);
    functionblockvec_free(&funcs);
    strvec_free(&out);
    strvec_free(&lifted);
    return -1;
}
