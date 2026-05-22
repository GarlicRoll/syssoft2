#ifndef JVM_EMITTER_H
#define JVM_EMITTER_H

#include "cfg.h"

// Emit JVM-oriented textual representation (Jasmin assembly, .j).
// Returns 0 on success.
int emit_jasmin_model(const CFG *cfg, const char *filename, const char *class_name);

// Emit Java stub model (.java) that can be compiled by javac into .class.
// Returns 0 on success.
int emit_java_stub_model(const CFG *cfg, const char *filename, const char *class_name);

#endif
