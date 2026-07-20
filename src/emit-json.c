#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emit-json.h"
#include "emit-util.h"

static void json_string_n(FILE *stream, const char *text, size_t length)
{
    static const char hex[] = "0123456789abcdef";
    fputc('"', stream);
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)text[i];
        switch (c) {
        case '"': fputs("\\\"", stream); break;
        case '\\': fputs("\\\\", stream); break;
        case '\b': fputs("\\b", stream); break;
        case '\f': fputs("\\f", stream); break;
        case '\n': fputs("\\n", stream); break;
        case '\r': fputs("\\r", stream); break;
        case '\t': fputs("\\t", stream); break;
        default:
            if (c < 0x20) {
                fputs("\\u00", stream);
                fputc(hex[c >> 4], stream);
                fputc(hex[c & 0x0f], stream);
            } else {
                fputc(c, stream);
            }
            break;
        }
    }
    fputc('"', stream);
}

static void json_string(FILE *stream, const char *text)
{
    json_string_n(stream, text, strlen(text));
}

static void json_token_fields(FILE *stream, const char *token)
{
    EmitTokenField fields[4];
    size_t count = emit_token_fields(token, fields);
    fputc('[', stream);
    for (size_t i = 0; i < count; i++) {
        if (i > 0)
            fputs(", ", stream);
        json_string_n(stream, fields[i].start, fields[i].length);
    }
    fputc(']', stream);
}

void emit_json_write(FILE *stream, const EdgeVec *edges,
                     const NodeVec *nodes, const Config *config)
{
    EmitNodeWeightRange range = emit_node_weight_range(nodes, config);

    fputs("{\n", stream);
    fputs("  \"format\": \"cw-tools/graph\",\n", stream);
    fputs("  \"version\": 1,\n", stream);
    fputs("  \"emit\": {\"config\": ", stream);
    json_string(stream, config->config_path != NULL ? config->config_path : "");
    fputs(", \"output_format\": ", stream);
    json_string(stream, emit_format_name(config->format));
    fputs(", \"label_fields\": [", stream);
    for (size_t i = 0; i < config->node_label_field_count; i++) {
        if (i > 0)
            fputs(", ", stream);
        fprintf(stream, "%zu", config->node_label_fields[i]);
    }
    fputs("], \"label_separator\": ", stream);
    json_string(stream, config->node_label_separator);
    fputs(", \"font_size_by\": ", stream);
    json_string(stream, emit_font_size_by_name(config->node_font_size_by));
    fputs("},\n", stream);
    fprintf(stream, "  \"directed\": %s,\n",
            config->directed ? "true" : "false");

    fputs("  \"nodes\": [\n", stream);
    for (size_t i = 0; i < nodes->len; i++) {
        const NodeRef *node = &nodes->items[i];
        char *label = emit_make_node_label(node->id, config);
        fputs("    {\"id\": ", stream);
        json_string(stream, node->id);
        fputs(", \"label\": ", stream);
        json_string(stream, label);
        fputs(", \"fields\": ", stream);
        json_token_fields(stream, node->id);
        fprintf(stream, ", \"df\": %zu, \"idf\": %.17g, \"fq\": ",
                node->df, node->idf);
        if (node->fq_available)
            fprintf(stream, "%zu", node->fq);
        else
            fputs("null", stream);
        fprintf(stream,
                ", \"degree\": %zu, \"font_size\": %.17g}%s\n",
                node->degree, emit_node_font_size(node, range, config),
                i + 1 == nodes->len ? "" : ",");
        free(label);
    }
    fputs("  ],\n", stream);

    fputs("  \"edges\": [\n", stream);
    for (size_t i = 0; i < edges->len; i++) {
        const Edge *edge = &edges->items[i];
        fputs("    {\"source\": ", stream);
        json_string(stream, edge->source);
        fputs(", \"target\": ", stream);
        json_string(stream, edge->target);
        fprintf(stream,
                ", \"ctf\": %zu, \"cdf\": %zu, \"df1\": %zu, "
                "\"idf1\": %.17g, \"fq1\": ",
                edge->ctf, edge->cdf, edge->df1, edge->idf1);
        if (edge->fq1_available)
            fprintf(stream, "%zu", edge->fq1);
        else
            fputs("null", stream);
        fprintf(stream, ", \"df2\": %zu, \"idf2\": %.17g, \"fq2\": ",
                edge->df2, edge->idf2);
        if (edge->fq2_available)
            fprintf(stream, "%zu", edge->fq2);
        else
            fputs("null", stream);
        fprintf(stream, ", \"cw\": %.17g, \"z\": %.17g, \"unit_ids\": [",
                edge->cw, edge->z);
        for (size_t j = 0; j < edge->unit_count; j++) {
            if (j > 0)
                fputs(", ", stream);
            json_string(stream, edge->unit_ids[j]);
        }
        fprintf(stream, "]}%s\n", i + 1 == edges->len ? "" : ",");
    }
    fputs("  ]\n", stream);
    fputs("}\n", stream);
}
