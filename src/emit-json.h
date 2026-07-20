#ifndef EMIT_JSON_H
#define EMIT_JSON_H

#include <stdio.h>
#include "emit-types.h"

void emit_json_write(FILE *stream, const EdgeVec *edges,
                     const NodeVec *nodes, const Config *config);

#endif
