#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emit-dot.h"
#include "emit-url.h"
#include "emit-util.h"

static void dot_string(FILE *stream, const char *text)
{
    fputc('"', stream);
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stream); break;
        case '\\': fputs("\\\\", stream); break;
        case '\n': fputs("\\n", stream); break;
        case '\r': fputs("\\r", stream); break;
        case '\t': fputs("\\t", stream); break;
        default:
            if (*p < 0x20)
                fprintf(stream, "\\x%02x", *p);
            else
                fputc(*p, stream);
            break;
        }
    }
    fputc('"', stream);
}

static bool tooltip_enabled(const Config *config)
{
    return config->tooltip_ctf || config->tooltip_cdf ||
           config->tooltip_cw || config->tooltip_z ||
           config->tooltip_unit_ids;
}

static void tooltip_append(char **buffer, size_t *length, size_t *capacity,
                           const char *text)
{
    size_t add = strlen(text);
    if (*length + add + 1 > *capacity) {
        size_t new_capacity = *capacity == 0 ? 128 : *capacity * 2;
        while (new_capacity < *length + add + 1)
            new_capacity *= 2;
        *buffer = emit_xrealloc(*buffer, new_capacity);
        *capacity = new_capacity;
    }
    memcpy(*buffer + *length, text, add);
    *length += add;
    (*buffer)[*length] = '\0';
}

static char *edge_tooltip(const Edge *edge, const Config *config)
{
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;
    char number[128];
    bool first = true;

#define ADD_FIELD(TEXT) do { \
    if (!first) tooltip_append(&buffer, &length, &capacity, "\n"); \
    tooltip_append(&buffer, &length, &capacity, (TEXT)); \
    first = false; \
} while (0)

    if (config->tooltip_ctf) {
        snprintf(number, sizeof(number), "ctf: %zu", edge->ctf);
        ADD_FIELD(number);
    }
    if (config->tooltip_cdf) {
        snprintf(number, sizeof(number), "cdf: %zu", edge->cdf);
        ADD_FIELD(number);
    }
    if (config->tooltip_cw) {
        snprintf(number, sizeof(number), "cw: %.17g", edge->cw);
        ADD_FIELD(number);
    }
    if (config->tooltip_z) {
        snprintf(number, sizeof(number), "z: %.17g", edge->z);
        ADD_FIELD(number);
    }
    if (config->tooltip_unit_ids) {
        if (!first)
            tooltip_append(&buffer, &length, &capacity, "\n");
        tooltip_append(&buffer, &length, &capacity, "unit_ids: ");
        for (size_t i = 0; i < edge->unit_count; i++) {
            if (i > 0)
                tooltip_append(&buffer, &length, &capacity, ", ");
            tooltip_append(&buffer, &length, &capacity, edge->unit_ids[i]);
        }
        first = false;
    }
#undef ADD_FIELD

    if (buffer == NULL)
        buffer = emit_xstrdup("");
    return buffer;
}

static void write_edge_label(FILE *stream, const Edge *edge,
                             const Config *config)
{
    switch (config->edge_label) {
    case EDGE_LABEL_NONE:
        break;
    case EDGE_LABEL_CTF:
        fprintf(stream, "%zu", edge->ctf);
        break;
    case EDGE_LABEL_CDF:
        fprintf(stream, "%zu", edge->cdf);
        break;
    case EDGE_LABEL_CW:
        fprintf(stream, "%.6g", edge->cw);
        break;
    case EDGE_LABEL_Z:
        fprintf(stream, "%.6g", edge->z);
        break;
    }
}

static void write_attr_string(FILE *stream, const char *name,
                              const char *value, bool *first)
{
    if (value == NULL)
        return;
    if (!*first)
        fputs(", ", stream);
    fprintf(stream, "%s=", name);
    dot_string(stream, value);
    *first = false;
}

static const char *z_sign_name(double z)
{
    if (z < 0.0)
        return "negative";
    if (z > 0.0)
        return "positive";
    return "zero";
}

void emit_dot_write(FILE *stream, const EdgeVec *edges,
                    const NodeVec *nodes, const Config *config)
{
    EmitNodeWeightRange range = emit_node_weight_range(nodes, config);
    EmitValueRange z_range = emit_edge_z_magnitude_range(edges);
    EmitUrlConfig url_config = emit_url_config_load(config->config_path);
    const char *kind = config->directed ? "digraph" : "graph";
    const char *connector = config->directed ? " -> " : " -- ";
    const char *direction_class = config->directed ? "directed" : "undirected";
    const char *font_source = emit_font_size_by_name(config->node_font_size_by);
    char graph_classes[96];

    snprintf(graph_classes, sizeof(graph_classes), "font-by-%s %s",
             font_source, direction_class);

    fprintf(stream, "%s ", kind);
    dot_string(stream, config->graph_name);
    fputs(" {\n", stream);

    fputs("  graph [", stream);
    bool first = true;
    write_attr_string(stream, "id", "graph-1", &first);
    write_attr_string(stream, "class", graph_classes, &first);
    write_attr_string(stream, "charset", config->charset, &first);
    if (!first) fputs(", ", stream);
    fprintf(stream, "overlap=%s", config->overlap ? "true" : "false");
    first = false;
    write_attr_string(stream, "outputorder", config->outputorder, &first);
    write_attr_string(stream, "fontname", config->graph_fontname, &first);
    write_attr_string(stream, "sep", config->graph_sep, &first);
    if (config->graph_pack_set) {
        if (!first) fputs(", ", stream);
        fprintf(stream, "pack=%s", config->graph_pack ? "true" : "false");
        first = false;
    }
    write_attr_string(stream, "packmode", config->graph_packmode, &first);
    write_attr_string(stream, "splines", config->graph_splines, &first);
    fputs("];\n", stream);

    fputs("  node [", stream);
    first = true;
    write_attr_string(stream, "shape", config->node_shape, &first);
    write_attr_string(stream, "fontname", config->node_fontname, &first);
    fputs("];\n", stream);

    fputs("  edge [", stream);
    first = true;
    if (!first) fputs(", ", stream);
    fprintf(stream, "penwidth=%.17g", config->edge_penwidth);
    first = false;
    if (config->edge_len_set) {
        fputs(", len=", stream);
        fprintf(stream, "%.17g", config->edge_len);
    }
    write_attr_string(stream, "fontname", config->edge_fontname, &first);
    fputs("];\n", stream);

    for (size_t i = 0; i < nodes->len; i++) {
        const NodeRef *node = &nodes->items[i];
        char *label = emit_make_node_label(node->id, config);
        char element_id[64];
        char classes[96];

        snprintf(element_id, sizeof(element_id), "node-%zu", i + 1);
        snprintf(classes, sizeof(classes), "font-by-%s font-rank-%zu",
                 font_source, emit_node_font_rank(node, range, config));

        fputs("  ", stream);
        dot_string(stream, node->id);
        fputs(" [", stream);
        first = true;
        write_attr_string(stream, "id", element_id, &first);
        write_attr_string(stream, "class", classes, &first);
        write_attr_string(stream, "label", label, &first);
        if (!first)
            fputs(", ", stream);
        fprintf(stream, "fontsize=%.17g];\n",
                emit_node_font_size(node, range, config));
        free(label);
    }

    for (size_t i = 0; i < edges->len; i++) {
        const Edge *edge = &edges->items[i];
        char element_id[64];
        char classes[112];
        char *url = emit_edge_url(edge, &url_config);

        snprintf(element_id, sizeof(element_id), "edge-%zu", i + 1);
        snprintf(classes, sizeof(classes), "z-rank-%zu z-%s%s",
                 emit_edge_z_rank(edge, z_range), z_sign_name(edge->z),
                 url == NULL ? "" : " has-url");

        fputs("  ", stream);
        dot_string(stream, edge->source);
        fputs(connector, stream);
        dot_string(stream, edge->target);
        fputs(" [", stream);
        first = true;
        write_attr_string(stream, "id", element_id, &first);
        write_attr_string(stream, "class", classes, &first);
        write_attr_string(stream, "URL", url, &first);
        if (url != NULL && url_config.target != NULL &&
            url_config.target[0] != '\0')
            write_attr_string(stream, "target", url_config.target, &first);
        if (config->edge_label != EDGE_LABEL_NONE) {
            if (!first)
                fputs(", ", stream);
            fputs("label=\"", stream);
            write_edge_label(stream, edge, config);
            fputc('"', stream);
            first = false;
        }
        if (tooltip_enabled(config)) {
            char *tooltip = edge_tooltip(edge, config);
            write_attr_string(stream, "tooltip", tooltip, &first);
            free(tooltip);
        }
        fputs("];\n", stream);
        free(url);
    }
    fputs("}\n", stream);
    emit_url_config_free(&url_config);
}
