#ifndef EMIT_TABLES_H
#define EMIT_TABLES_H

#include <stdio.h>
#include "emit-types.h"

void emit_tables_write(FILE *stream, const EdgeVec *edges,
                       const Config *config);

const char *emit_table_default_header(TableColumn column);
bool emit_table_column_is_numeric(TableColumn column);

#endif
