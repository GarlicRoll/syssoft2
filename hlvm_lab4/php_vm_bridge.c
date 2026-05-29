#include "php_vm_bridge.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static MonoDomain *g_domain = NULL;
static MonoAssembly *g_assembly = NULL;
static MonoImage *g_image = NULL;
static MonoClass *g_class = NULL;

static char g_error[1024];

static void set_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_error, sizeof(g_error), fmt, ap);
    va_end(ap);
}

static MonoMethod *find_static_method(const char *method_name, int param_count) {
    void *iter = NULL;
    MonoMethod *m = NULL;
    while ((m = mono_class_get_methods(g_class, &iter)) != NULL) {
        const char *name = mono_method_get_name(m);
        MonoMethodSignature *sig = mono_method_signature(m);
        int cnt = (int)mono_signature_get_param_count(sig);
        if (name && strcmp(name, method_name) == 0 && cnt == param_count) {
            return m;
        }
    }
    return NULL;
}

static int invoke_method(const char *method_name, int param_count, int64_t *in_args, int64_t *out_value) {
    MonoMethod *m;
    MonoObject *exc = NULL;
    MonoObject *result;
    void *args[2] = {0};

    if (!g_class) {
        set_error("Runtime is not initialized");
        return 0;
    }
    if (!method_name || !out_value) {
        set_error("Invalid arguments");
        return 0;
    }

    m = find_static_method(method_name, param_count);
    if (!m) {
        set_error("Method not found: %s/%d", method_name, param_count);
        return 0;
    }

    if (param_count > 0) args[0] = &in_args[0];
    if (param_count > 1) args[1] = &in_args[1];

    result = mono_runtime_invoke(m, NULL, param_count ? args : NULL, &exc);
    if (exc) {
        MonoString *s = mono_object_to_string(exc, NULL);
        char *utf8 = s ? mono_string_to_utf8(s) : NULL;
        set_error("Managed exception: %s", utf8 ? utf8 : "unknown");
        if (utf8) mono_free(utf8);
        return 0;
    }

    if (!result) {
        *out_value = 0;
        return 1;
    }

    *out_value = *(int64_t *)mono_object_unbox(result);
    return 1;
}

int hlvm4_init(const char *assembly_path, const char *class_name) {
    if (!assembly_path || !class_name) {
        set_error("assembly_path/class_name is null");
        return 0;
    }

    hlvm4_shutdown();

    g_domain = mono_jit_init_version("hlvm_lab4_php_bridge", "v4.0.30319");
    if (!g_domain) {
        set_error("mono_jit_init_version failed");
        return 0;
    }

    g_assembly = mono_domain_assembly_open(g_domain, assembly_path);
    if (!g_assembly) {
        set_error("Failed to load assembly: %s", assembly_path);
        hlvm4_shutdown();
        return 0;
    }

    g_image = mono_assembly_get_image(g_assembly);
    if (!g_image) {
        set_error("Failed to obtain assembly image");
        hlvm4_shutdown();
        return 0;
    }

    g_class = mono_class_from_name(g_image, "", class_name);
    if (!g_class) {
        set_error("Class not found: %s", class_name);
        hlvm4_shutdown();
        return 0;
    }

    g_error[0] = '\0';
    return 1;
}

void hlvm4_shutdown(void) {
    g_class = NULL;
    g_image = NULL;
    g_assembly = NULL;
    if (g_domain) {
        mono_jit_cleanup(g_domain);
        g_domain = NULL;
    }
}

int hlvm4_call0(const char *method_name, int64_t *out_value) {
    return invoke_method(method_name, 0, NULL, out_value);
}

int hlvm4_call1(const char *method_name, int64_t arg0, int64_t *out_value) {
    int64_t args[1];
    args[0] = arg0;
    return invoke_method(method_name, 1, args, out_value);
}

int hlvm4_call2(const char *method_name, int64_t arg0, int64_t arg1, int64_t *out_value) {
    int64_t args[2];
    args[0] = arg0;
    args[1] = arg1;
    return invoke_method(method_name, 2, args, out_value);
}

const char *hlvm4_last_error(void) {
    return g_error;
}
