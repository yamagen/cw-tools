#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emit-util.h"

void *emit_xmalloc(size_t size)
{
    void *ptr = malloc(size == 0 ? 1 : size);
    if (ptr == NULL) {
        fputs("emit: out of memory\n", stderr);
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *emit_xrealloc(void *ptr, size_t size)
{
    void *replacement = realloc(ptr, size == 0 ? 1 : size);
    if (replacement == NULL) {
        fputs("emit: out of memory\n", stderr);
        exit(EXIT_FAILURE);
    }
    return replacement;
}

char *emit_xstrdup(const char *text)
{
    char *copy = strdup(text);
    if (copy == NULL) {
        fputs("emit: out of memory\n", stderr);
        exit(EXIT_FAILURE);
    }
    return copy;
}

size_t emit_token_fields(const char *token, EmitTokenField fields[4])
{
    size_t count = 0;
    const char *start = token;

    for (const char *p = token;; p++) {
        if (*p == '/' || *p == '\0') {
            if (count < 4) {
                fields[count].start = start;
                fields[count].length = (size_t)(p - start);
                count++;
            }
            if (*p == '\0' || count == 4)
                break;
            start = p + 1;
        }
    }
    return count;
}

char *emit_make_label(const char *token, const size_t *field_numbers,
                      size_t field_count, const char *separator)
{
    EmitTokenField fields[4];
    size_t available = emit_token_fields(token, fields);
    size_t separator_length = strlen(separator);
    size_t length = 0;
    size_t included = 0;

    for (size_t i = 0; i < field_count; i++) {
        size_t number = field_numbers[i];
        if (number < 1 || number > available)
            continue;
        if (included > 0)
            length += separator_length;
        length += fields[number - 1].length;
        included++;
    }

    if (included == 0)
        return emit_xstrdup(token);

    char *result = emit_xmalloc(length + 1);
    char *out = result;
    included = 0;
    for (size_t i = 0; i < field_count; i++) {
        size_t number = field_numbers[i];
        if (number < 1 || number > available)
            continue;
        if (included > 0) {
            memcpy(out, separator, separator_length);
            out += separator_length;
        }
        memcpy(out, fields[number - 1].start, fields[number - 1].length);
        out += fields[number - 1].length;
        included++;
    }
    *out = '\0';
    return result;
}

char *emit_make_node_label(const char *token, const Config *config)
{
    return emit_make_label(token, config->node_label_fields,
                           config->node_label_field_count,
                           config->node_label_separator);
}

char *emit_make_table_label(const char *token, const Config *config)
{
    return emit_make_label(token, config->table_label_fields,
                           config->table_label_field_count,
                           config->table_label_separator);
}

const char *emit_format_name(OutputFormat format)
{
    switch (format) {
    case FORMAT_JSON: return "json";
    case FORMAT_DOT: return "dot";
    case FORMAT_MD: return "md";
    case FORMAT_TEX: return "tex";
    case FORMAT_HTML: return "html";
    case FORMAT_D3: return "d3";
    }
    return "dot";
}

const char *emit_font_size_by_name(FontSizeBy value)
{
    switch (value) {
    case FONT_SIZE_FQ: return "fq";
    case FONT_SIZE_IDF: return "idf";
    case FONT_SIZE_DEGREE: return "degree";
    }
    return "fq";
}

static double node_weight(const NodeRef *node, const Config *config)
{
    switch (config->node_font_size_by) {
    case FONT_SIZE_FQ: return (double)node->fq;
    case FONT_SIZE_IDF: return node->idf;
    case FONT_SIZE_DEGREE: return (double)node->degree;
    }
    return 0.0;
}

void emit_validate_font_size_data(const NodeVec *nodes,
                                  const Config *config,
                                  const char *source)
{
    if (config->node_font_size_by != FONT_SIZE_FQ)
        return;

    for (size_t i = 0; i < nodes->len; i++) {
        if (!nodes->items[i].fq_available) {
            fprintf(stderr,
                    "emit: %s: fq is unavailable for token '%s'; "
                    "use pair 0.2.0 or choose font_size_by idf/degree\n",
                    source, nodes->items[i].id);
            exit(EXIT_FAILURE);
        }
    }
}

EmitNodeWeightRange emit_node_weight_range(const NodeVec *nodes,
                                           const Config *config)
{
    EmitNodeWeightRange range = {0.0, 0.0};
    if (nodes->len == 0)
        return range;

    range.min = node_weight(&nodes->items[0], config);
    range.max = range.min;
    for (size_t i = 1; i < nodes->len; i++) {
        double value = node_weight(&nodes->items[i], config);
        if (value < range.min)
            range.min = value;
        if (value > range.max)
            range.max = value;
    }
    return range;
}

double emit_node_font_size(const NodeRef *node,
                           EmitNodeWeightRange range,
                           const Config *config)
{
    if (range.max == range.min)
        return (config->node_min_font_size +
                config->node_max_font_size) / 2.0;

    double normalized =
        (node_weight(node, config) - range.min) /
        (range.max - range.min);
    return config->node_min_font_size +
           normalized * (config->node_max_font_size -
                         config->node_min_font_size);
}
