#ifndef HLVM_LAB4_PHP_VM_BRIDGE_H
#define HLVM_LAB4_PHP_VM_BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int hlvm4_init(const char *assembly_path, const char *class_name);
void hlvm4_shutdown(void);

int hlvm4_call0(const char *method_name, int64_t *out_value);
int hlvm4_call1(const char *method_name, int64_t arg0, int64_t *out_value);
int hlvm4_call2(const char *method_name, int64_t arg0, int64_t arg1, int64_t *out_value);

const char *hlvm4_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
