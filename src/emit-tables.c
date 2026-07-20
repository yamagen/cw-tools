#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emit-tables.h"
#include "emit-util.h"

const char *emit_table_default_header(TableColumn column)
{
    switch (column) {
    case TABLE_TOKEN1: return "token1";
    case TABLE_TOKEN2: return "token2";
    case TABLE_RAW_TOKEN1: return "raw_token1";
    case TABLE_RAW_TOKEN2: return "raw_token2";
    case TABLE_CTF: return "ctf";
    case TABLE_CDF: return "cdf";
    case TABLE_DF1: return "df1";
    case TABLE_IDF1: return "idf1";
    case TABLE_FQ1: return "fq1";
    case TABLE_DF2: return "df2";
    case TABLE_IDF2: return "idf2";
    case TABLE_FQ2: return "fq2";
    case TABLE_CW: return "cw";
    case TABLE_Z: return "z";
    case TABLE_UNIT_IDS: return "unit_ids";
    case TABLE_COLUMN_LIMIT: break;
    }
    return "";
}

bool emit_table_column_is_numeric(TableColumn column)
{
    switch (column) {
    case TABLE_CTF:
    case TABLE_CDF:
    case TABLE_DF1:
    case TABLE_IDF1:
    case TABLE_FQ1:
    case TABLE_DF2:
    case TABLE_IDF2:
    case TABLE_FQ2:
    case TABLE_CW:
    case TABLE_Z:
        return true;
    default:
        return false;
    }
}

static char *format_size(size_t value)
{
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%zu", value);
    return emit_xstrdup(buffer);
}

static char *format_double(double value, size_t precision)
{
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%.*g", (int)precision, value);
    return emit_xstrdup(buffer);
}

static char *format_optional_size(size_t value, bool available)
{
    return available ? format_size(value) : emit_xstrdup("-");
}

static char *format_units(const Edge *edge, const Config *config)
{
    size_t sep_len = strlen(config->table_unit_separator);
    size_t length = 0;
    for (size_t i = 0; i < edge->unit_count; i++) {
        length += strlen(edge->unit_ids[i]);
        if (i > 0)
            length += sep_len;
    }
    char *result = emit_xmalloc(length + 1);
    char *out = result;
    for (size_t i = 0; i < edge->unit_count; i++) {
        if (i > 0) {
            memcpy(out, config->table_unit_separator, sep_len);
            out += sep_len;
        }
        size_t n = strlen(edge->unit_ids[i]);
        memcpy(out, edge->unit_ids[i], n);
        out += n;
    }
    *out = '\0';
    return result;
}

static char *cell_text(const Edge *edge, TableColumn column,
                       const Config *config)
{
    switch (column) {
    case TABLE_TOKEN1: return emit_make_table_label(edge->source, config);
    case TABLE_TOKEN2: return emit_make_table_label(edge->target, config);
    case TABLE_RAW_TOKEN1: return emit_xstrdup(edge->source);
    case TABLE_RAW_TOKEN2: return emit_xstrdup(edge->target);
    case TABLE_CTF: return format_size(edge->ctf);
    case TABLE_CDF: return format_size(edge->cdf);
    case TABLE_DF1: return format_size(edge->df1);
    case TABLE_IDF1: return format_double(edge->idf1, config->table_precision);
    case TABLE_FQ1: return format_optional_size(edge->fq1, edge->fq1_available);
    case TABLE_DF2: return format_size(edge->df2);
    case TABLE_IDF2: return format_double(edge->idf2, config->table_precision);
    case TABLE_FQ2: return format_optional_size(edge->fq2, edge->fq2_available);
    case TABLE_CW: return format_double(edge->cw, config->table_precision);
    case TABLE_Z: return format_double(edge->z, config->table_precision);
    case TABLE_UNIT_IDS: return format_units(edge, config);
    case TABLE_COLUMN_LIMIT: break;
    }
    return emit_xstrdup("");
}

static const char *header_text(const Config *config, size_t index)
{
    if (index < config->table_header_count)
        return config->table_headers[index];
    return emit_table_default_header(config->table_columns[index]);
}

static void markdown_escape(FILE *stream, const char *text)
{
    for (const char *p = text; *p != '\0'; p++) {
        if (*p == '|' || *p == '\\')
            fputc('\\', stream);
        if (*p == '\n' || *p == '\r')
            fputc(' ', stream);
        else
            fputc(*p, stream);
    }
}

static void html_escape(FILE *stream, const char *text)
{
    for (const char *p = text; *p != '\0'; p++) {
        switch (*p) {
        case '&': fputs("&amp;", stream); break;
        case '<': fputs("&lt;", stream); break;
        case '>': fputs("&gt;", stream); break;
        case '"': fputs("&quot;", stream); break;
        case '\'': fputs("&#39;", stream); break;
        default: fputc(*p, stream); break;
        }
    }
}

static void tex_escape(FILE *stream, const char *text)
{
    for (const char *p = text; *p != '\0'; p++) {
        switch (*p) {
        case '\\': fputs("\\textbackslash{}", stream); break;
        case '{': fputs("\\{", stream); break;
        case '}': fputs("\\}", stream); break;
        case '$': fputs("\\$", stream); break;
        case '&': fputs("\\&", stream); break;
        case '#': fputs("\\#", stream); break;
        case '_': fputs("\\_", stream); break;
        case '%': fputs("\\%", stream); break;
        case '^': fputs("\\textasciicircum{}", stream); break;
        case '~': fputs("\\textasciitilde{}", stream); break;
        case '\n':
        case '\r': fputc(' ', stream); break;
        default: fputc(*p, stream); break;
        }
    }
}

static void write_markdown(FILE *stream, const EdgeVec *edges,
                           const Config *config)
{
    fputc('|', stream);
    for (size_t i = 0; i < config->table_column_count; i++) {
        fputc(' ', stream);
        markdown_escape(stream, header_text(config, i));
        fputs(" |", stream);
    }
    fputc('\n', stream);

    fputc('|', stream);
    for (size_t i = 0; i < config->table_column_count; i++) {
        if (emit_table_column_is_numeric(config->table_columns[i]))
            fputs(" ---: |", stream);
        else
            fputs(" :--- |", stream);
    }
    fputc('\n', stream);

    for (size_t row = 0; row < edges->len; row++) {
        fputc('|', stream);
        for (size_t col = 0; col < config->table_column_count; col++) {
            char *text = cell_text(&edges->items[row],
                                   config->table_columns[col], config);
            fputc(' ', stream);
            markdown_escape(stream, text);
            fputs(" |", stream);
            free(text);
        }
        fputc('\n', stream);
    }
}

static void write_html(FILE *stream, const EdgeVec *edges,
                       const Config *config)
{
    fputs("<table>\n", stream);
    if (config->table_caption != NULL) {
        fputs("  <caption>", stream);
        html_escape(stream, config->table_caption);
        fputs("</caption>\n", stream);
    }
    fputs("  <thead><tr>", stream);
    for (size_t i = 0; i < config->table_column_count; i++) {
        fputs("<th", stream);
        if (emit_table_column_is_numeric(config->table_columns[i]))
            fputs(" class=\"numeric\"", stream);
        fputc('>', stream);
        html_escape(stream, header_text(config, i));
        fputs("</th>", stream);
    }
    fputs("</tr></thead>\n  <tbody>\n", stream);
    for (size_t row = 0; row < edges->len; row++) {
        fputs("    <tr>", stream);
        for (size_t col = 0; col < config->table_column_count; col++) {
            char *text = cell_text(&edges->items[row],
                                   config->table_columns[col], config);
            fputs("<td", stream);
            if (emit_table_column_is_numeric(config->table_columns[col]))
                fputs(" class=\"numeric\"", stream);
            fputc('>', stream);
            html_escape(stream, text);
            fputs("</td>", stream);
            free(text);
        }
        fputs("</tr>\n", stream);
    }
    fputs("  </tbody>\n</table>\n", stream);
}

static void write_tex(FILE *stream, const EdgeVec *edges,
                      const Config *config)
{
    if (config->table_caption != NULL || config->table_label != NULL) {
        fputs("\\begin{table}[htbp]\n\\centering\n", stream);
    }
    fputs("\\begin{tabular}{", stream);
    for (size_t i = 0; i < config->table_column_count; i++)
        fputc(emit_table_column_is_numeric(config->table_columns[i]) ? 'r' : 'l', stream);
    fputs("}\n\\hline\n", stream);
    for (size_t i = 0; i < config->table_column_count; i++) {
        if (i > 0)
            fputs(" & ", stream);
        tex_escape(stream, header_text(config, i));
    }
    fputs(" \\\\\n\\hline\n", stream);

    for (size_t row = 0; row < edges->len; row++) {
        for (size_t col = 0; col < config->table_column_count; col++) {
            if (col > 0)
                fputs(" & ", stream);
            char *text = cell_text(&edges->items[row],
                                   config->table_columns[col], config);
            tex_escape(stream, text);
            free(text);
        }
        fputs(" \\\\\n", stream);
    }
    fputs("\\hline\n\\end{tabular}\n", stream);
    if (config->table_caption != NULL) {
        fputs("\\caption{", stream);
        tex_escape(stream, config->table_caption);
        fputs("}\n", stream);
    }
    if (config->table_label != NULL) {
        fputs("\\label{", stream);
        tex_escape(stream, config->table_label);
        fputs("}\n", stream);
    }
    if (config->table_caption != NULL || config->table_label != NULL)
        fputs("\\end{table}\n", stream);
}

void emit_tables_write(FILE *stream, const EdgeVec *edges,
                       const Config *config)
{
    switch (config->format) {
    case FORMAT_MD: write_markdown(stream, edges, config); break;
    case FORMAT_TEX: write_tex(stream, edges, config); break;
    case FORMAT_HTML: write_html(stream, edges, config); break;
    default: break;
    }
}
