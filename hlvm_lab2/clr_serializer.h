#ifndef HLVM_LAB1_CLR_SERIALIZER_H
#define HLVM_LAB1_CLR_SERIALIZER_H

#include "clr_model.h"

int clr_serialize_il(const ClrProgramModel *model, const char *output_path);

#endif
