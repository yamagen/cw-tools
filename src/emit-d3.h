#ifndef EMIT_D3_H
#define EMIT_D3_H

#include <stdio.h>
#include "emit-types.h"

void emit_d3_write(FILE *stream, const EdgeVec *edges,
                   const NodeVec *nodes, const Config *config);

#endif
