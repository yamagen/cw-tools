#ifndef EMIT_DOT_H
#define EMIT_DOT_H

#include <stdio.h>
#include "emit-types.h"

void emit_dot_write(FILE *stream, const EdgeVec *edges,
                    const NodeVec *nodes, const Config *config);

#endif
