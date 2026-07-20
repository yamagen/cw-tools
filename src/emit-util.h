#ifndef EMIT_UTIL_H
#define EMIT_UTIL_H

#include <stddef.h>
#include "emit-types.h"

typedef struct {
    const char *start;
    size_t length;
} EmitTokenField;

typedef struct {
    double min;
    double max;
} EmitNodeWeightRange;

void *emit_xmalloc(size_t size);
void *emit_xrealloc(void *ptr, size_t size);
char *emit_xstrdup(const char *text);

size_t emit_token_fields(const char *token, EmitTokenField fields[4]);
char *emit_make_label(const char *token, const size_t *field_numbers,
                      size_t field_count, const char *separator);
char *emit_make_node_label(const char *token, const Config *config);
char *emit_make_table_label(const char *token, const Config *config);

const char *emit_format_name(OutputFormat format);
const char *emit_font_size_by_name(FontSizeBy value);

void emit_validate_font_size_data(const NodeVec *nodes,
                                  const Config *config,
                                  const char *source);
EmitNodeWeightRange emit_node_weight_range(const NodeVec *nodes,
                                           const Config *config);
double emit_node_font_size(const NodeRef *node,
                           EmitNodeWeightRange range,
                           const Config *config);

#endif
