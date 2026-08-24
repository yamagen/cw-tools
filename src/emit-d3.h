#ifndef EMIT_D3_H
#define EMIT_D3_H

#include <stdbool.h>
#include <stdio.h>
#include "emit-types.h"

void emit_d3_set_data_only(bool enabled);
void emit_d3_write(FILE *stream, const EdgeVec *edges,
                   const NodeVec *nodes, const Config *config);

#endif
