#ifndef MODEL_SERIALIZER_H
#define MODEL_SERIALIZER_H

#include "cfg.h"
#include "linear_code.h"

int write_program_model_json(const CFG *cfg, const ProgramImage *image, const char *path);

#endif

