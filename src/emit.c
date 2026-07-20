#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emit-d3.h"
#include "emit-dot.h"
#include "emit-json.h"
#include "emit-tables.h"
#include "emit-types.h"
#include "emit-util.h"

#define PROG_NAME "emit"
#define PROG_VERSION "0.8.0"
#define DEFAULT_CONFIG "config/emit-config.json"

typedef struct {
    const char *path;
    const char *text;
    size_t length;
    size_t pos;
    size_t line;
} JsonParser;

typedef struct {
    const char *config_path;
    bool format_set;
    OutputFormat format;
    bool min_cw_set;
    double min_cw;
    bool min_z_set;
    double min_z;
} CliOverrides;

static void die(const char *message)
{
    fprintf(stderr, "%s: %s\n", PROG_NAME, message);
    exit(EXIT_FAILURE);
}

static void die_errno(const char *path)
{
    fprintf(stderr, "%s: %s: %s\n", PROG_NAME, path, strerror(errno));
    exit(EXIT_FAILURE);
}

static void die_line(const char *source, size_t line_number,
                     const char *message)
{
    fprintf(stderr, "%s: %s:%zu: %s\n",
            PROG_NAME, source, line_number, message);
    exit(EXIT_FAILURE);
}

static void json_die(const JsonParser *parser, const char *message)
{
    fprintf(stderr, "%s: %s:%zu: %s\n",
            PROG_NAME, parser->path, parser->line, message);
    exit(EXIT_FAILURE);
}

static void replace_string(char **destination, char *replacement)
{
    free(*destination);
    *destination = replacement;
}

static void config_init(Config *config)
{
    memset(config, 0, sizeof(*config));
    config->format = FORMAT_DOT;
    config->graph_name = emit_xstrdup("G");
    config->charset = emit_xstrdup("UTF-8");
    config->overlap = false;
    config->outputorder = emit_xstrdup("edgesfirst");
    config->node_shape = emit_xstrdup("plaintext");
    config->node_label_fields[0] = 1;
    config->node_label_field_count = 1;
    config->node_label_separator = emit_xstrdup("/");
    config->node_font_size_by = FONT_SIZE_FQ;
    config->node_min_font_size = 7.0;
    config->node_max_font_size = 32.0;
    config->edge_label = EDGE_LABEL_CTF;
    config->tooltip_ctf = true;
    config->tooltip_cdf = true;
    config->tooltip_cw = true;
    config->tooltip_z = true;
    config->tooltip_unit_ids = true;
    config->edge_penwidth = 1.0;
    config->table_columns[0] = TABLE_TOKEN1;
    config->table_columns[1] = TABLE_TOKEN2;
    config->table_columns[2] = TABLE_CTF;
    config->table_columns[3] = TABLE_CDF;
    config->table_columns[4] = TABLE_CW;
    config->table_columns[5] = TABLE_Z;
    config->table_columns[6] = TABLE_UNIT_IDS;
    config->table_column_count = 7;
    config->table_label_fields[0] = 1;
    config->table_label_field_count = 1;
    config->table_label_separator = emit_xstrdup("/");
    config->table_precision = 6;
    config->table_unit_separator = emit_xstrdup(", ");
}

static void config_free(Config *config)
{
    free(config->graph_name);
    free(config->charset);
    free(config->outputorder);
    free(config->graph_fontname);
    free(config->graph_sep);
    free(config->graph_packmode);
    free(config->graph_splines);
    free(config->node_shape);
    free(config->node_fontname);
    free(config->node_label_separator);
    free(config->edge_fontname);
    for (size_t i = 0; i < config->table_header_count; i++)
        free(config->table_headers[i]);
    free(config->table_label_separator);
    free(config->table_unit_separator);
    free(config->table_caption);
    free(config->table_label);
    free(config->config_path);
}

static OutputFormat parse_format_name(const char *text, const char *where)
{
    if (strcmp(text, "json") == 0)
        return FORMAT_JSON;
    if (strcmp(text, "dot") == 0)
        return FORMAT_DOT;
    if (strcmp(text, "md") == 0 || strcmp(text, "markdown") == 0)
        return FORMAT_MD;
    if (strcmp(text, "tex") == 0 || strcmp(text, "latex") == 0)
        return FORMAT_TEX;
    if (strcmp(text, "html") == 0)
        return FORMAT_HTML;
    if (strcmp(text, "d3") == 0)
        return FORMAT_D3;

    fprintf(stderr,
            "%s: %s: format must be json, dot, md, tex, html, or d3: %s\n",
            PROG_NAME, where, text);
    exit(EXIT_FAILURE);
}

static FontSizeBy parse_font_size_by_name(const char *text,
                                          const char *where)
{
    if (strcmp(text, "fq") == 0)
        return FONT_SIZE_FQ;
    if (strcmp(text, "idf") == 0)
        return FONT_SIZE_IDF;
    if (strcmp(text, "degree") == 0)
        return FONT_SIZE_DEGREE;
    fprintf(stderr,
            "%s: %s: font_size_by must be fq, idf, or degree: %s\n",
            PROG_NAME, where, text);
    exit(EXIT_FAILURE);
}

static EdgeLabel parse_edge_label_name(const char *text,
                                       const JsonParser *parser)
{
    if (strcmp(text, "none") == 0) return EDGE_LABEL_NONE;
    if (strcmp(text, "ctf") == 0) return EDGE_LABEL_CTF;
    if (strcmp(text, "cdf") == 0) return EDGE_LABEL_CDF;
    if (strcmp(text, "cw") == 0) return EDGE_LABEL_CW;
    if (strcmp(text, "z") == 0) return EDGE_LABEL_Z;
    json_die(parser, "edge.label must be one of: none, ctf, cdf, cw, z");
    return EDGE_LABEL_NONE;
}

static TableColumn parse_table_column_name(const char *text,
                                           const JsonParser *parser)
{
    if (strcmp(text, "token1") == 0) return TABLE_TOKEN1;
    if (strcmp(text, "token2") == 0) return TABLE_TOKEN2;
    if (strcmp(text, "raw_token1") == 0) return TABLE_RAW_TOKEN1;
    if (strcmp(text, "raw_token2") == 0) return TABLE_RAW_TOKEN2;
    if (strcmp(text, "ctf") == 0) return TABLE_CTF;
    if (strcmp(text, "cdf") == 0) return TABLE_CDF;
    if (strcmp(text, "df1") == 0) return TABLE_DF1;
    if (strcmp(text, "idf1") == 0) return TABLE_IDF1;
    if (strcmp(text, "fq1") == 0) return TABLE_FQ1;
    if (strcmp(text, "df2") == 0) return TABLE_DF2;
    if (strcmp(text, "idf2") == 0) return TABLE_IDF2;
    if (strcmp(text, "fq2") == 0) return TABLE_FQ2;
    if (strcmp(text, "cw") == 0) return TABLE_CW;
    if (strcmp(text, "z") == 0) return TABLE_Z;
    if (strcmp(text, "unit_ids") == 0) return TABLE_UNIT_IDS;
    json_die(parser, "unknown table column");
    return TABLE_TOKEN1;
}

static double parse_option_double(const char *text, const char *option_name)
{
    char *end = NULL;
    errno = 0;
    double value = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !isfinite(value)) {
        fprintf(stderr, "%s: %s requires a finite number: %s\n",
                PROG_NAME, option_name, text);
        exit(EXIT_FAILURE);
    }
    return value;
}

static char *read_file(const char *path, size_t *length_out)
{
    FILE *stream = fopen(path, "rb");
    if (stream == NULL)
        die_errno(path);
    if (fseek(stream, 0, SEEK_END) != 0)
        die_errno(path);
    long end = ftell(stream);
    if (end < 0)
        die_errno(path);
    if (fseek(stream, 0, SEEK_SET) != 0)
        die_errno(path);

    size_t length = (size_t)end;
    char *text = emit_xmalloc(length + 1);
    if (length > 0 && fread(text, 1, length, stream) != length) {
        free(text);
        die_errno(path);
    }
    text[length] = '\0';
    if (fclose(stream) != 0) {
        free(text);
        die_errno(path);
    }
    *length_out = length;
    return text;
}

static void jp_skip_ws(JsonParser *parser)
{
    while (parser->pos < parser->length) {
        unsigned char c = (unsigned char)parser->text[parser->pos];
        if (!isspace(c))
            break;
        if (c == '\n')
            parser->line++;
        parser->pos++;
    }
}

static char jp_peek(JsonParser *parser)
{
    jp_skip_ws(parser);
    if (parser->pos >= parser->length)
        return '\0';
    return parser->text[parser->pos];
}

static void jp_expect(JsonParser *parser, char expected)
{
    jp_skip_ws(parser);
    if (parser->pos >= parser->length ||
        parser->text[parser->pos] != expected) {
        char message[96];
        snprintf(message, sizeof(message), "expected '%c'", expected);
        json_die(parser, message);
    }
    parser->pos++;
}

static int hex_value(char c)
{
    if ('0' <= c && c <= '9') return c - '0';
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void append_utf8(char **buffer, size_t *length, size_t *capacity,
                        unsigned codepoint)
{
    unsigned char bytes[4];
    size_t count;
    if (codepoint <= 0x7f) {
        bytes[0] = (unsigned char)codepoint; count = 1;
    } else if (codepoint <= 0x7ff) {
        bytes[0] = (unsigned char)(0xc0 | (codepoint >> 6));
        bytes[1] = (unsigned char)(0x80 | (codepoint & 0x3f)); count = 2;
    } else if (codepoint <= 0xffff) {
        bytes[0] = (unsigned char)(0xe0 | (codepoint >> 12));
        bytes[1] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f));
        bytes[2] = (unsigned char)(0x80 | (codepoint & 0x3f)); count = 3;
    } else {
        bytes[0] = (unsigned char)(0xf0 | (codepoint >> 18));
        bytes[1] = (unsigned char)(0x80 | ((codepoint >> 12) & 0x3f));
        bytes[2] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f));
        bytes[3] = (unsigned char)(0x80 | (codepoint & 0x3f)); count = 4;
    }
    if (*length + count + 1 > *capacity) {
        size_t new_capacity = *capacity == 0 ? 32 : *capacity * 2;
        while (new_capacity < *length + count + 1)
            new_capacity *= 2;
        *buffer = emit_xrealloc(*buffer, new_capacity);
        *capacity = new_capacity;
    }
    for (size_t i = 0; i < count; i++)
        (*buffer)[(*length)++] = (char)bytes[i];
}

static unsigned jp_parse_hex4(JsonParser *parser)
{
    unsigned value = 0;
    for (int i = 0; i < 4; i++) {
        if (parser->pos >= parser->length)
            json_die(parser, "incomplete Unicode escape");
        int digit = hex_value(parser->text[parser->pos++]);
        if (digit < 0)
            json_die(parser, "invalid Unicode escape");
        value = (value << 4) | (unsigned)digit;
    }
    return value;
}

static char *jp_parse_string(JsonParser *parser)
{
    jp_skip_ws(parser);
    if (parser->pos >= parser->length || parser->text[parser->pos] != '"')
        json_die(parser, "expected string");
    parser->pos++;

    char *result = NULL;
    size_t length = 0;
    size_t capacity = 0;
    while (parser->pos < parser->length) {
        unsigned char c = (unsigned char)parser->text[parser->pos++];
        if (c == '"') {
            if (length + 1 > capacity) {
                capacity = length + 1;
                result = emit_xrealloc(result, capacity);
            }
            result[length] = '\0';
            return result;
        }
        if (c < 0x20)
            json_die(parser, "control character in string");
        if (c == '\\') {
            if (parser->pos >= parser->length)
                json_die(parser, "incomplete escape sequence");
            char escape = parser->text[parser->pos++];
            switch (escape) {
            case '"': c = '"'; break;
            case '\\': c = '\\'; break;
            case '/': c = '/'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case 'n': c = '\n'; break;
            case 'r': c = '\r'; break;
            case 't': c = '\t'; break;
            case 'u': {
                unsigned codepoint = jp_parse_hex4(parser);
                if (0xd800 <= codepoint && codepoint <= 0xdbff) {
                    if (parser->pos + 2 > parser->length ||
                        parser->text[parser->pos] != '\\' ||
                        parser->text[parser->pos + 1] != 'u')
                        json_die(parser, "missing low surrogate");
                    parser->pos += 2;
                    unsigned low = jp_parse_hex4(parser);
                    if (low < 0xdc00 || low > 0xdfff)
                        json_die(parser, "invalid low surrogate");
                    codepoint = 0x10000 +
                        ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                } else if (0xdc00 <= codepoint && codepoint <= 0xdfff) {
                    json_die(parser, "unexpected low surrogate");
                }
                append_utf8(&result, &length, &capacity, codepoint);
                continue;
            }
            default: json_die(parser, "invalid escape sequence");
            }
        }
        if (length + 2 > capacity) {
            size_t new_capacity = capacity == 0 ? 32 : capacity * 2;
            result = emit_xrealloc(result, new_capacity);
            capacity = new_capacity;
        }
        result[length++] = (char)c;
    }
    free(result);
    json_die(parser, "unterminated string");
    return NULL;
}

static bool jp_consume_literal(JsonParser *parser, const char *literal)
{
    jp_skip_ws(parser);
    size_t length = strlen(literal);
    if (parser->length - parser->pos < length ||
        strncmp(parser->text + parser->pos, literal, length) != 0)
        return false;
    parser->pos += length;
    return true;
}

static double jp_parse_number(JsonParser *parser)
{
    jp_skip_ws(parser);
    const char *start = parser->text + parser->pos;
    char *end = NULL;
    errno = 0;
    double value = strtod(start, &end);
    if (errno != 0 || end == start || !isfinite(value))
        json_die(parser, "expected finite number");
    parser->pos += (size_t)(end - start);
    return value;
}

static bool jp_parse_bool(JsonParser *parser)
{
    if (jp_consume_literal(parser, "true")) return true;
    if (jp_consume_literal(parser, "false")) return false;
    json_die(parser, "expected true or false");
    return false;
}

static void jp_skip_value(JsonParser *parser);

static void jp_skip_array(JsonParser *parser)
{
    jp_expect(parser, '[');
    if (jp_peek(parser) == ']') { parser->pos++; return; }
    for (;;) {
        jp_skip_value(parser);
        if (jp_peek(parser) == ']') { parser->pos++; return; }
        jp_expect(parser, ',');
    }
}

static void jp_skip_object(JsonParser *parser)
{
    jp_expect(parser, '{');
    if (jp_peek(parser) == '}') { parser->pos++; return; }
    for (;;) {
        char *key = jp_parse_string(parser);
        free(key);
        jp_expect(parser, ':');
        jp_skip_value(parser);
        if (jp_peek(parser) == '}') { parser->pos++; return; }
        jp_expect(parser, ',');
    }
}

static void jp_skip_value(JsonParser *parser)
{
    char c = jp_peek(parser);
    if (c == '{') jp_skip_object(parser);
    else if (c == '[') jp_skip_array(parser);
    else if (c == '"') { char *s = jp_parse_string(parser); free(s); }
    else if (c == '-' || isdigit((unsigned char)c)) (void)jp_parse_number(parser);
    else if (jp_consume_literal(parser, "true") ||
             jp_consume_literal(parser, "false") ||
             jp_consume_literal(parser, "null")) return;
    else json_die(parser, "invalid JSON value");
}

static char *jp_parse_nullable_string(JsonParser *parser)
{
    if (jp_consume_literal(parser, "null"))
        return NULL;
    return jp_parse_string(parser);
}

static void parse_nullable_filter(JsonParser *parser,
                                  bool *present, double *value)
{
    if (jp_consume_literal(parser, "null")) {
        *present = false;
        *value = 0.0;
    } else {
        *value = jp_parse_number(parser);
        *present = true;
    }
}

static void parse_filters(JsonParser *parser, Config *config)
{
    jp_expect(parser, '{');
    if (jp_peek(parser) == '}') { parser->pos++; return; }
    for (;;) {
        char *key = jp_parse_string(parser);
        jp_expect(parser, ':');
        if (strcmp(key, "min_cw") == 0)
            parse_nullable_filter(parser, &config->filters.have_min_cw,
                                  &config->filters.min_cw);
        else if (strcmp(key, "min_z") == 0)
            parse_nullable_filter(parser, &config->filters.have_min_z,
                                  &config->filters.min_z);
        else
            jp_skip_value(parser);
        free(key);
        if (jp_peek(parser) == '}') { parser->pos++; return; }
        jp_expect(parser, ',');
    }
}

static void parse_dot(JsonParser *parser, Config *config)
{
    jp_expect(parser, '{');
    if (jp_peek(parser) == '}') { parser->pos++; return; }
    for (;;) {
        char *key = jp_parse_string(parser);
        jp_expect(parser, ':');
        if (strcmp(key, "graph_name") == 0)
            replace_string(&config->graph_name, jp_parse_string(parser));
        else if (strcmp(key, "directed") == 0)
            config->directed = jp_parse_bool(parser);
        else if (strcmp(key, "charset") == 0)
            replace_string(&config->charset, jp_parse_string(parser));
        else if (strcmp(key, "overlap") == 0)
            config->overlap = jp_parse_bool(parser);
        else if (strcmp(key, "outputorder") == 0)
            replace_string(&config->outputorder, jp_parse_string(parser));
        else if (strcmp(key, "fontname") == 0)
            replace_string(&config->graph_fontname, jp_parse_string(parser));
        else if (strcmp(key, "sep") == 0)
            replace_string(&config->graph_sep, jp_parse_string(parser));
        else if (strcmp(key, "pack") == 0) {
            config->graph_pack = jp_parse_bool(parser);
            config->graph_pack_set = true;
        } else if (strcmp(key, "packmode") == 0)
            replace_string(&config->graph_packmode, jp_parse_string(parser));
        else if (strcmp(key, "splines") == 0)
            replace_string(&config->graph_splines, jp_parse_string(parser));
        else jp_skip_value(parser);
        free(key);
        if (jp_peek(parser) == '}') { parser->pos++; return; }
        jp_expect(parser, ',');
    }
}

static void parse_field_array(JsonParser *parser, size_t fields[4],
                              size_t *count_out, const char *name)
{
    size_t count = 0;
    bool seen[5] = {false, false, false, false, false};
    jp_expect(parser, '[');
    if (jp_peek(parser) == ']') {
        char message[128];
        snprintf(message, sizeof(message), "%s must not be empty", name);
        json_die(parser, message);
    }
    for (;;) {
        double value = jp_parse_number(parser);
        if (value < 1.0 || value > 4.0 || value != (double)(size_t)value) {
            char message[160];
            snprintf(message, sizeof(message),
                     "%s entries must be integers from 1 to 4", name);
            json_die(parser, message);
        }
        size_t field = (size_t)value;
        if (!seen[field]) {
            fields[count++] = field;
            seen[field] = true;
        }
        if (jp_peek(parser) == ']') { parser->pos++; break; }
        jp_expect(parser, ',');
    }
    *count_out = count;
}

static void parse_node(JsonParser *parser, Config *config)
{
    jp_expect(parser, '{');
    if (jp_peek(parser) == '}') { parser->pos++; return; }
    for (;;) {
        char *key = jp_parse_string(parser);
        jp_expect(parser, ':');
        if (strcmp(key, "shape") == 0)
            replace_string(&config->node_shape, jp_parse_string(parser));
        else if (strcmp(key, "fontname") == 0)
            replace_string(&config->node_fontname, jp_parse_string(parser));
        else if (strcmp(key, "label_fields") == 0)
            parse_field_array(parser, config->node_label_fields,
                              &config->node_label_field_count,
                              "node.label_fields");
        else if (strcmp(key, "label_field") == 0) {
            double value = jp_parse_number(parser);
            if (value < 1.0 || value > 4.0 ||
                value != (double)(size_t)value)
                json_die(parser,
                         "node.label_field must be an integer from 1 to 4");
            config->node_label_fields[0] = (size_t)value;
            config->node_label_field_count = 1;
        } else if (strcmp(key, "label_separator") == 0)
            replace_string(&config->node_label_separator,
                           jp_parse_string(parser));
        else if (strcmp(key, "font_size_by") == 0) {
            char *value = jp_parse_string(parser);
            config->node_font_size_by =
                parse_font_size_by_name(value, "node.font_size_by");
            free(value);
        } else if (strcmp(key, "min_font_size") == 0) {
            config->node_min_font_size = jp_parse_number(parser);
            if (config->node_min_font_size <= 0.0)
                json_die(parser, "node.min_font_size must be greater than zero");
        } else if (strcmp(key, "max_font_size") == 0) {
            config->node_max_font_size = jp_parse_number(parser);
            if (config->node_max_font_size <= 0.0)
                json_die(parser, "node.max_font_size must be greater than zero");
        } else jp_skip_value(parser);
        free(key);
        if (jp_peek(parser) == '}') { parser->pos++; return; }
        jp_expect(parser, ',');
    }
}

static void set_tooltip_field(Config *config, const char *field,
                              JsonParser *parser)
{
    if (strcmp(field, "ctf") == 0) config->tooltip_ctf = true;
    else if (strcmp(field, "cdf") == 0) config->tooltip_cdf = true;
    else if (strcmp(field, "cw") == 0) config->tooltip_cw = true;
    else if (strcmp(field, "z") == 0) config->tooltip_z = true;
    else if (strcmp(field, "unit_ids") == 0) config->tooltip_unit_ids = true;
    else json_die(parser,
                  "edge.tooltip entries must be ctf, cdf, cw, z, or unit_ids");
}

static void parse_tooltip(JsonParser *parser, Config *config)
{
    config->tooltip_ctf = false;
    config->tooltip_cdf = false;
    config->tooltip_cw = false;
    config->tooltip_z = false;
    config->tooltip_unit_ids = false;
    jp_expect(parser, '[');
    if (jp_peek(parser) == ']') { parser->pos++; return; }
    for (;;) {
        char *field = jp_parse_string(parser);
        set_tooltip_field(config, field, parser);
        free(field);
        if (jp_peek(parser) == ']') { parser->pos++; return; }
        jp_expect(parser, ',');
    }
}

static void parse_edge(JsonParser *parser, Config *config)
{
    jp_expect(parser, '{');
    if (jp_peek(parser) == '}') { parser->pos++; return; }
    for (;;) {
        char *key = jp_parse_string(parser);
        jp_expect(parser, ':');
        if (strcmp(key, "label") == 0) {
            char *label = jp_parse_string(parser);
            config->edge_label = parse_edge_label_name(label, parser);
            free(label);
        } else if (strcmp(key, "tooltip") == 0)
            parse_tooltip(parser, config);
        else if (strcmp(key, "penwidth") == 0) {
            config->edge_penwidth = jp_parse_number(parser);
            if (config->edge_penwidth <= 0.0)
                json_die(parser, "edge.penwidth must be greater than zero");
        } else if (strcmp(key, "length") == 0 || strcmp(key, "len") == 0) {
            config->edge_len = jp_parse_number(parser);
            if (config->edge_len <= 0.0)
                json_die(parser, "edge.length must be greater than zero");
            config->edge_len_set = true;
        } else if (strcmp(key, "fontname") == 0)
            replace_string(&config->edge_fontname, jp_parse_string(parser));
        else jp_skip_value(parser);
        free(key);
        if (jp_peek(parser) == '}') { parser->pos++; return; }
        jp_expect(parser, ',');
    }
}

static void parse_table_columns(JsonParser *parser, Config *config)
{
    size_t count = 0;
    bool seen[TABLE_COLUMN_LIMIT];
    memset(seen, 0, sizeof(seen));
    jp_expect(parser, '[');
    if (jp_peek(parser) == ']')
        json_die(parser, "table.columns must not be empty");
    for (;;) {
        char *name = jp_parse_string(parser);
        TableColumn column = parse_table_column_name(name, parser);
        free(name);
        if (!seen[column]) {
            config->table_columns[count++] = column;
            seen[column] = true;
        }
        if (jp_peek(parser) == ']') { parser->pos++; break; }
        jp_expect(parser, ',');
    }
    config->table_column_count = count;
}

static void parse_table_headers(JsonParser *parser, Config *config)
{
    for (size_t i = 0; i < config->table_header_count; i++) {
        free(config->table_headers[i]);
        config->table_headers[i] = NULL;
    }
    config->table_header_count = 0;
    jp_expect(parser, '[');
    if (jp_peek(parser) == ']') { parser->pos++; return; }
    for (;;) {
        if (config->table_header_count >= TABLE_COLUMN_LIMIT)
            json_die(parser, "too many table.headers entries");
        config->table_headers[config->table_header_count++] =
            jp_parse_string(parser);
        if (jp_peek(parser) == ']') { parser->pos++; return; }
        jp_expect(parser, ',');
    }
}

static void parse_table(JsonParser *parser, Config *config)
{
    jp_expect(parser, '{');
    if (jp_peek(parser) == '}') { parser->pos++; return; }
    for (;;) {
        char *key = jp_parse_string(parser);
        jp_expect(parser, ':');
        if (strcmp(key, "columns") == 0)
            parse_table_columns(parser, config);
        else if (strcmp(key, "headers") == 0)
            parse_table_headers(parser, config);
        else if (strcmp(key, "label_fields") == 0)
            parse_field_array(parser, config->table_label_fields,
                              &config->table_label_field_count,
                              "table.label_fields");
        else if (strcmp(key, "label_separator") == 0)
            replace_string(&config->table_label_separator,
                           jp_parse_string(parser));
        else if (strcmp(key, "precision") == 0) {
            double value = jp_parse_number(parser);
            if (value < 1.0 || value > 17.0 ||
                value != (double)(size_t)value)
                json_die(parser,
                         "table.precision must be an integer from 1 to 17");
            config->table_precision = (size_t)value;
        } else if (strcmp(key, "unit_separator") == 0)
            replace_string(&config->table_unit_separator,
                           jp_parse_string(parser));
        else if (strcmp(key, "caption") == 0)
            replace_string(&config->table_caption,
                           jp_parse_nullable_string(parser));
        else if (strcmp(key, "label") == 0)
            replace_string(&config->table_label,
                           jp_parse_nullable_string(parser));
        else jp_skip_value(parser);
        free(key);
        if (jp_peek(parser) == '}') { parser->pos++; return; }
        jp_expect(parser, ',');
    }
}

static void parse_config(JsonParser *parser, Config *config)
{
    jp_expect(parser, '{');
    if (jp_peek(parser) == '}') { parser->pos++; return; }
    for (;;) {
        char *key = jp_parse_string(parser);
        jp_expect(parser, ':');
        if (strcmp(key, "format") == 0) {
            char *value = jp_parse_string(parser);
            config->format = parse_format_name(value, "config format");
            free(value);
        } else if (strcmp(key, "filters") == 0)
            parse_filters(parser, config);
        else if (strcmp(key, "dot") == 0)
            parse_dot(parser, config);
        else if (strcmp(key, "node") == 0)
            parse_node(parser, config);
        else if (strcmp(key, "edge") == 0)
            parse_edge(parser, config);
        else if (strcmp(key, "table") == 0)
            parse_table(parser, config);
        else jp_skip_value(parser);
        free(key);
        if (jp_peek(parser) == '}') { parser->pos++; return; }
        jp_expect(parser, ',');
    }
}

static void load_config(const char *path, Config *config)
{
    size_t length;
    char *text = read_file(path, &length);
    JsonParser parser = {
        .path = path, .text = text, .length = length, .pos = 0, .line = 1
    };
    parse_config(&parser, config);
    jp_skip_ws(&parser);
    if (parser.pos != parser.length)
        json_die(&parser, "trailing content after configuration object");
    free(text);
    replace_string(&config->config_path, emit_xstrdup(path));
}

static void apply_overrides(Config *config, const CliOverrides *overrides)
{
    if (overrides->format_set)
        config->format = overrides->format;
    if (overrides->min_cw_set) {
        config->filters.have_min_cw = true;
        config->filters.min_cw = overrides->min_cw;
    }
    if (overrides->min_z_set) {
        config->filters.have_min_z = true;
        config->filters.min_z = overrides->min_z;
    }
}

static void edge_vec_push(EdgeVec *vec, Edge edge)
{
    if (vec->len == vec->cap) {
        size_t new_cap = vec->cap == 0 ? 256 : vec->cap * 2;
        vec->items = emit_xrealloc(vec->items,
                                  new_cap * sizeof(*vec->items));
        vec->cap = new_cap;
    }
    vec->items[vec->len++] = edge;
}

static void node_vec_push(NodeVec *vec, NodeRef node)
{
    if (vec->len == vec->cap) {
        size_t new_cap = vec->cap == 0 ? 512 : vec->cap * 2;
        vec->items = emit_xrealloc(vec->items,
                                  new_cap * sizeof(*vec->items));
        vec->cap = new_cap;
    }
    vec->items[vec->len++] = node;
}

static void edge_vec_free(EdgeVec *vec)
{
    for (size_t i = 0; i < vec->len; i++) {
        Edge *edge = &vec->items[i];
        free(edge->source);
        free(edge->target);
        for (size_t j = 0; j < edge->unit_count; j++)
            free(edge->unit_ids[j]);
        free(edge->unit_ids);
    }
    free(vec->items);
    memset(vec, 0, sizeof(*vec));
}

static size_t split_tabs(char *line, char ***fields_out)
{
    size_t count = 1;
    for (const char *p = line; *p != '\0'; p++)
        if (*p == '\t') count++;
    char **fields = emit_xmalloc(count * sizeof(*fields));
    size_t index = 0;
    fields[index++] = line;
    for (char *p = line; *p != '\0'; p++) {
        if (*p == '\t') {
            *p = '\0';
            fields[index++] = p + 1;
        }
    }
    *fields_out = fields;
    return count;
}

static bool try_size(const char *text, size_t *value)
{
    if (text[0] == '\0' || text[0] == '-')
        return false;
    char *end = NULL;
    errno = 0;
    unsigned long long parsed = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        parsed > (unsigned long long)SIZE_MAX)
        return false;
    *value = (size_t)parsed;
    return true;
}

static bool try_optional_size(const char *text, size_t *value,
                              bool *available)
{
    if (strcmp(text, "-") == 0) {
        *value = 0;
        *available = false;
        return true;
    }
    *available = true;
    return try_size(text, value);
}

static bool try_double(const char *text, double *value)
{
    char *end = NULL;
    errno = 0;
    double parsed = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !isfinite(parsed))
        return false;
    *value = parsed;
    return true;
}

static void invalid_row(const char *source, size_t line_number)
{
    die_line(source, line_number,
             "expected cw output fields: token1 token2 ctf cdf df1 idf1 "
             "fq1 df2 idf2 fq2 cw z unit_id... (legacy rows without fq are also accepted)");
}

static bool edge_selected(const Edge *edge, const Filters *filters)
{
    if (filters->have_min_cw && edge->cw < filters->min_cw)
        return false;
    if (filters->have_min_z && edge->z < filters->min_z)
        return false;
    return true;
}

static Edge parse_edge_fields(char **fields, size_t count,
                              const char *source, size_t line_number)
{
    Edge edge;
    memset(&edge, 0, sizeof(edge));
    if (count < 10)
        invalid_row(source, line_number);

    edge.source = emit_xstrdup(fields[0]);
    edge.target = emit_xstrdup(fields[1]);
    if (!try_size(fields[2], &edge.ctf) ||
        !try_size(fields[3], &edge.cdf) ||
        !try_size(fields[4], &edge.df1) ||
        !try_double(fields[5], &edge.idf1))
        invalid_row(source, line_number);

    bool current = false;
    if (count >= 12) {
        size_t fq1, df2, fq2;
        bool fq1_available, fq2_available;
        double idf2, cw, z;
        current =
            try_optional_size(fields[6], &fq1, &fq1_available) &&
            try_size(fields[7], &df2) &&
            try_double(fields[8], &idf2) &&
            try_optional_size(fields[9], &fq2, &fq2_available) &&
            try_double(fields[10], &cw) &&
            try_double(fields[11], &z);
        if (current) {
            edge.fq1 = fq1;
            edge.fq1_available = fq1_available;
            edge.df2 = df2;
            edge.idf2 = idf2;
            edge.fq2 = fq2;
            edge.fq2_available = fq2_available;
            edge.cw = cw;
            edge.z = z;
            edge.unit_count = count - 12;
            if (edge.unit_count > 0) {
                edge.unit_ids = emit_xmalloc(edge.unit_count * sizeof(char *));
                for (size_t i = 0; i < edge.unit_count; i++)
                    edge.unit_ids[i] = emit_xstrdup(fields[i + 12]);
            }
        }
    }

    if (!current) {
        if (!try_size(fields[6], &edge.df2) ||
            !try_double(fields[7], &edge.idf2) ||
            !try_double(fields[8], &edge.cw) ||
            !try_double(fields[9], &edge.z))
            invalid_row(source, line_number);
        edge.fq1_available = false;
        edge.fq2_available = false;
        edge.unit_count = count - 10;
        if (edge.unit_count > 0) {
            edge.unit_ids = emit_xmalloc(edge.unit_count * sizeof(char *));
            for (size_t i = 0; i < edge.unit_count; i++)
                edge.unit_ids[i] = emit_xstrdup(fields[i + 10]);
        }
    }
    return edge;
}

static void free_edge(Edge *edge)
{
    free(edge->source);
    free(edge->target);
    for (size_t i = 0; i < edge->unit_count; i++)
        free(edge->unit_ids[i]);
    free(edge->unit_ids);
    memset(edge, 0, sizeof(*edge));
}

static void read_edges(FILE *stream, const char *source,
                       const Filters *filters,
                       EdgeVec *edges, NodeVec *nodes)
{
    char *line = NULL;
    size_t capacity = 0;
    size_t line_number = 0;
    ssize_t length;
    while ((length = getline(&line, &capacity, stream)) != -1) {
        line_number++;
        while (length > 0 &&
               (line[length - 1] == '\n' || line[length - 1] == '\r'))
            line[--length] = '\0';
        if (length == 0 || line[0] == '#')
            continue;
        char **fields;
        size_t count = split_tabs(line, &fields);
        Edge edge = parse_edge_fields(fields, count, source, line_number);
        free(fields);
        if (!edge_selected(&edge, filters)) {
            free_edge(&edge);
            continue;
        }
        NodeRef source_node = {
            .id = edge.source, .df = edge.df1, .idf = edge.idf1,
            .fq = edge.fq1, .fq_available = edge.fq1_available,
            .degree = 0
        };
        NodeRef target_node = {
            .id = edge.target, .df = edge.df2, .idf = edge.idf2,
            .fq = edge.fq2, .fq_available = edge.fq2_available,
            .degree = 0
        };
        edge_vec_push(edges, edge);
        node_vec_push(nodes, source_node);
        node_vec_push(nodes, target_node);
    }
    free(line);
    if (ferror(stream))
        die_errno(source);
}

static int compare_node_ref(const void *left, const void *right)
{
    const NodeRef *a = left;
    const NodeRef *b = right;
    return strcmp(a->id, b->id);
}

static void node_vec_sort_unique(NodeVec *vec, const char *source)
{
    if (vec->len == 0)
        return;
    qsort(vec->items, vec->len, sizeof(*vec->items), compare_node_ref);
    size_t out = 1;
    for (size_t i = 1; i < vec->len; i++) {
        NodeRef *previous = &vec->items[out - 1];
        NodeRef *current = &vec->items[i];
        if (strcmp(previous->id, current->id) == 0) {
            if (previous->df != current->df || previous->idf != current->idf ||
                previous->fq_available != current->fq_available ||
                (previous->fq_available && previous->fq != current->fq)) {
                fprintf(stderr,
                        "%s: %s: inconsistent df/idf/fq values for token '%s'\n",
                        PROG_NAME, source, current->id);
                exit(EXIT_FAILURE);
            }
            continue;
        }
        vec->items[out++] = *current;
    }
    vec->len = out;
}

static NodeRef *node_vec_find(NodeVec *vec, const char *id)
{
    NodeRef key = {.id = id};
    return bsearch(&key, vec->items, vec->len,
                   sizeof(*vec->items), compare_node_ref);
}

static void calculate_node_degrees(NodeVec *nodes, const EdgeVec *edges)
{
    for (size_t i = 0; i < nodes->len; i++)
        nodes->items[i].degree = 0;
    for (size_t i = 0; i < edges->len; i++) {
        NodeRef *source = node_vec_find(nodes, edges->items[i].source);
        NodeRef *target = node_vec_find(nodes, edges->items[i].target);
        if (source == NULL || target == NULL)
            die("internal node-degree error");
        source->degree++;
        target->degree++;
    }
}

static void print_help(FILE *stream)
{
    fprintf(stream,
            "Usage: %s [OPTIONS] [FILE]\n"
            "Emit JSON, Graphviz DOT, Markdown, LaTeX, HTML, or D3 HTML "
            "from cw output.\n\n"
            "Input columns:\n"
            "  token1 token2 ctf cdf df1 idf1 fq1 df2 idf2 fq2 cw z unit_id...\n"
            "Legacy input without fq1/fq2 is also accepted.\n\n"
            "Configuration:\n"
            "  -c, --config FILE     read FILE instead of %s\n\n"
            "Temporary overrides:\n"
            "  -T, --format FORMAT   output: json, dot, md, tex, html, or d3\n"
            "  -W, --min-cw VALUE    retain edges whose CW is at least VALUE\n"
            "  -Z, --min-z VALUE     retain edges whose Z is at least VALUE\n\n"
            "Other options:\n"
            "  -h, --help            display this help and exit\n"
            "  -v, --version         display version information and exit\n",
            PROG_NAME, DEFAULT_CONFIG);
}

int main(int argc, char **argv)
{
    static const struct option long_options[] = {
        {"config", required_argument, NULL, 'c'},
        {"format", required_argument, NULL, 'T'},
        {"min-cw", required_argument, NULL, 'W'},
        {"min-z", required_argument, NULL, 'Z'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {NULL, 0, NULL, 0}
    };

    CliOverrides overrides = {0};
    overrides.config_path = DEFAULT_CONFIG;
    (void)setlocale(LC_ALL, "");

    int option;
    while ((option = getopt_long(argc, argv, "c:T:W:Z:hv",
                                 long_options, NULL)) != -1) {
        switch (option) {
        case 'c': overrides.config_path = optarg; break;
        case 'T':
            overrides.format = parse_format_name(optarg, "--format");
            overrides.format_set = true;
            break;
        case 'W':
            overrides.min_cw = parse_option_double(optarg, "--min-cw");
            overrides.min_cw_set = true;
            break;
        case 'Z':
            overrides.min_z = parse_option_double(optarg, "--min-z");
            overrides.min_z_set = true;
            break;
        case 'h': print_help(stdout); return EXIT_SUCCESS;
        case 'v': printf("%s %s\n", PROG_NAME, PROG_VERSION); return EXIT_SUCCESS;
        default: print_help(stderr); return EXIT_FAILURE;
        }
    }

    if (argc - optind > 1) {
        fprintf(stderr, "%s: at most one input file may be specified\n",
                PROG_NAME);
        return EXIT_FAILURE;
    }

    Config config;
    config_init(&config);
    load_config(overrides.config_path, &config);
    apply_overrides(&config, &overrides);
    if (config.node_max_font_size < config.node_min_font_size)
        die("node.max_font_size must be greater than or equal to node.min_font_size");
    if (config.table_header_count > config.table_column_count)
        die("table.headers has more entries than table.columns");

    FILE *stream = stdin;
    const char *source = "-";
    if (optind < argc) {
        source = argv[optind];
        stream = fopen(source, "r");
        if (stream == NULL) {
            config_free(&config);
            die_errno(source);
        }
    }

    EdgeVec edges = {0};
    NodeVec nodes = {0};
    read_edges(stream, source, &config.filters, &edges, &nodes);
    if (stream != stdin && fclose(stream) != 0) {
        edge_vec_free(&edges);
        free(nodes.items);
        config_free(&config);
        die_errno(source);
    }

    node_vec_sort_unique(&nodes, source);
    calculate_node_degrees(&nodes, &edges);
    if (config.format == FORMAT_JSON || config.format == FORMAT_DOT ||
        config.format == FORMAT_D3)
        emit_validate_font_size_data(&nodes, &config, source);

    switch (config.format) {
    case FORMAT_JSON:
        emit_json_write(stdout, &edges, &nodes, &config);
        break;
    case FORMAT_DOT:
        emit_dot_write(stdout, &edges, &nodes, &config);
        break;
    case FORMAT_MD:
    case FORMAT_TEX:
    case FORMAT_HTML:
        emit_tables_write(stdout, &edges, &config);
        break;
    case FORMAT_D3:
        emit_d3_write(stdout, &edges, &nodes, &config);
        break;
    }

    if (ferror(stdout)) {
        edge_vec_free(&edges);
        free(nodes.items);
        config_free(&config);
        die_errno("standard output");
    }

    edge_vec_free(&edges);
    free(nodes.items);
    config_free(&config);
    return EXIT_SUCCESS;
}
