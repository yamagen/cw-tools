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

#define PROG_NAME "emit"
#define PROG_VERSION "0.7.0"
#define DEFAULT_CONFIG "config/emit-config.json"

typedef enum {
    FORMAT_JSON,
    FORMAT_DOT,
    FORMAT_MD,
    FORMAT_TEX,
    FORMAT_HTML
} OutputFormat;

typedef enum {
    TABLE_TOKEN1,
    TABLE_TOKEN2,
    TABLE_RAW_TOKEN1,
    TABLE_RAW_TOKEN2,
    TABLE_CTF,
    TABLE_CDF,
    TABLE_DF1,
    TABLE_IDF1,
    TABLE_FQ1,
    TABLE_DF2,
    TABLE_IDF2,
    TABLE_FQ2,
    TABLE_CW,
    TABLE_Z,
    TABLE_UNIT_IDS,
    TABLE_COLUMN_LIMIT
} TableColumn;

typedef enum {
    EDGE_LABEL_NONE,
    EDGE_LABEL_CTF,
    EDGE_LABEL_CDF,
    EDGE_LABEL_CW,
    EDGE_LABEL_Z
} EdgeLabel;

typedef enum {
    FONT_SIZE_FQ,
    FONT_SIZE_IDF,
    FONT_SIZE_DEGREE
} FontSizeBy;

typedef struct {
    char *source;
    char *target;
    size_t ctf;
    size_t cdf;
    size_t df1;
    double idf1;
    size_t fq1;
    bool fq1_available;
    size_t df2;
    double idf2;
    size_t fq2;
    bool fq2_available;
    double cw;
    double z;
    char **unit_ids;
    size_t unit_count;
} Edge;

typedef struct {
    Edge *items;
    size_t len;
    size_t cap;
} EdgeVec;

typedef struct {
    const char *id;
    size_t df;
    double idf;
    size_t fq;
    bool fq_available;
    size_t degree;
} NodeRef;

typedef struct {
    NodeRef *items;
    size_t len;
    size_t cap;
} NodeVec;

typedef struct {
    bool have_min_cw;
    double min_cw;
    bool have_min_z;
    double min_z;
} Filters;

typedef struct {
    OutputFormat format;
    Filters filters;
    char *graph_name;
    bool directed;
    char *charset;
    bool overlap;
    char *outputorder;
    char *graph_fontname;
    char *graph_sep;
    bool graph_pack_set;
    bool graph_pack;
    char *graph_packmode;
    char *graph_splines;
    char *node_shape;
    char *node_fontname;
    size_t node_label_fields[4];
    size_t node_label_field_count;
    char *node_label_separator;
    FontSizeBy node_font_size_by;
    double node_min_font_size;
    double node_max_font_size;
    EdgeLabel edge_label;
    bool tooltip_ctf;
    bool tooltip_cdf;
    bool tooltip_cw;
    bool tooltip_z;
    bool tooltip_unit_ids;
    double edge_penwidth;
    bool edge_len_set;
    double edge_len;
    char *edge_fontname;
    TableColumn table_columns[TABLE_COLUMN_LIMIT];
    size_t table_column_count;
    char *table_headers[TABLE_COLUMN_LIMIT];
    size_t table_header_count;
    size_t table_label_fields[4];
    size_t table_label_field_count;
    char *table_label_separator;
    size_t table_precision;
    char *table_unit_separator;
    char *table_caption;
    char *table_label;
    char *config_path;
} Config;

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

static void *xmalloc(size_t size)
{
    void *ptr = malloc(size == 0 ? 1 : size);
    if (ptr == NULL)
        die("out of memory");
    return ptr;
}

static void *xrealloc(void *ptr, size_t size)
{
    void *new_ptr = realloc(ptr, size == 0 ? 1 : size);
    if (new_ptr == NULL)
        die("out of memory");
    return new_ptr;
}

static char *xstrdup(const char *s)
{
    char *copy = strdup(s);
    if (copy == NULL)
        die("out of memory");
    return copy;
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
    config->graph_name = xstrdup("G");
    config->charset = xstrdup("UTF-8");
    config->overlap = false;
    config->outputorder = xstrdup("edgesfirst");
    config->node_shape = xstrdup("plaintext");
    config->node_label_fields[0] = 1;
    config->node_label_field_count = 1;
    config->node_label_separator = xstrdup("/");
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
    config->table_label_separator = xstrdup("/");
    config->table_precision = 6;
    config->table_unit_separator = xstrdup(", ");
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

static void edge_vec_push(EdgeVec *vec, Edge edge)
{
    if (vec->len == vec->cap) {
        size_t new_cap = vec->cap == 0 ? 256 : vec->cap * 2;
        vec->items = xrealloc(vec->items, new_cap * sizeof(*vec->items));
        vec->cap = new_cap;
    }
    vec->items[vec->len++] = edge;
}

static void node_vec_push(NodeVec *vec, NodeRef node)
{
    if (vec->len == vec->cap) {
        size_t new_cap = vec->cap == 0 ? 512 : vec->cap * 2;
        vec->items = xrealloc(vec->items, new_cap * sizeof(*vec->items));
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
    vec->items = NULL;
    vec->len = 0;
    vec->cap = 0;
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

static size_t split_tabs(char *line, char ***fields_out)
{
    size_t count = 1;
    for (const char *p = line; *p != '\0'; p++) {
        if (*p == '\t')
            count++;
    }

    char **fields = xmalloc(count * sizeof(*fields));
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

static size_t parse_size(const char *text, const char *source,
                         size_t line_number, const char *field_name)
{
    char *end = NULL;

    if (text[0] == '\0' || text[0] == '-') {
        char message[160];
        snprintf(message, sizeof(message),
                 "invalid %s value '%s'", field_name, text);
        die_line(source, line_number, message);
    }

    errno = 0;
    unsigned long long value = strtoull(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        value > (unsigned long long)SIZE_MAX) {
        char message[160];
        snprintf(message, sizeof(message),
                 "invalid %s value '%s'", field_name, text);
        die_line(source, line_number, message);
    }

    return (size_t)value;
}

static size_t parse_optional_size(const char *text, const char *source,
                                  size_t line_number, const char *field_name,
                                  bool *available)
{
    if (strcmp(text, "-") == 0) {
        *available = false;
        return 0;
    }
    *available = true;
    return parse_size(text, source, line_number, field_name);
}

static double parse_double(const char *text, const char *source,
                           size_t line_number, const char *field_name)
{
    char *end = NULL;
    errno = 0;
    double value = strtod(text, &end);

    if (errno != 0 || end == text || *end != '\0' || !isfinite(value)) {
        char message[160];
        snprintf(message, sizeof(message),
                 "invalid %s value '%s'", field_name, text);
        die_line(source, line_number, message);
    }

    return value;
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

    fprintf(stderr,
            "%s: %s: format must be json, dot, md, tex, or html: %s\n",
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

static const char *font_size_by_name(FontSizeBy value)
{
    switch (value) {
    case FONT_SIZE_FQ: return "fq";
    case FONT_SIZE_IDF: return "idf";
    case FONT_SIZE_DEGREE: return "degree";
    }
    return "fq";
}

static EdgeLabel parse_edge_label_name(const char *text,
                                       const JsonParser *parser)
{
    if (strcmp(text, "none") == 0)
        return EDGE_LABEL_NONE;
    if (strcmp(text, "ctf") == 0)
        return EDGE_LABEL_CTF;
    if (strcmp(text, "cdf") == 0)
        return EDGE_LABEL_CDF;
    if (strcmp(text, "cw") == 0)
        return EDGE_LABEL_CW;
    if (strcmp(text, "z") == 0)
        return EDGE_LABEL_Z;
    json_die(parser, "edge.label must be one of: none, ctf, cdf, cw, z");
    return EDGE_LABEL_NONE;
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
    char *text = xmalloc(length + 1);
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
    if ('0' <= c && c <= '9')
        return c - '0';
    if ('a' <= c && c <= 'f')
        return c - 'a' + 10;
    if ('A' <= c && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static void append_utf8(char **buffer, size_t *length, size_t *capacity,
                        unsigned codepoint)
{
    unsigned char bytes[4];
    size_t count;

    if (codepoint <= 0x7f) {
        bytes[0] = (unsigned char)codepoint;
        count = 1;
    } else if (codepoint <= 0x7ff) {
        bytes[0] = (unsigned char)(0xc0 | (codepoint >> 6));
        bytes[1] = (unsigned char)(0x80 | (codepoint & 0x3f));
        count = 2;
    } else if (codepoint <= 0xffff) {
        bytes[0] = (unsigned char)(0xe0 | (codepoint >> 12));
        bytes[1] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f));
        bytes[2] = (unsigned char)(0x80 | (codepoint & 0x3f));
        count = 3;
    } else {
        bytes[0] = (unsigned char)(0xf0 | (codepoint >> 18));
        bytes[1] = (unsigned char)(0x80 | ((codepoint >> 12) & 0x3f));
        bytes[2] = (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f));
        bytes[3] = (unsigned char)(0x80 | (codepoint & 0x3f));
        count = 4;
    }

    if (*length + count + 1 > *capacity) {
        size_t new_capacity = *capacity == 0 ? 32 : *capacity * 2;
        while (new_capacity < *length + count + 1)
            new_capacity *= 2;
        *buffer = xrealloc(*buffer, new_capacity);
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
                result = xrealloc(result, capacity);
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
            default:
                json_die(parser, "invalid escape sequence");
            }
        }

        if (length + 2 > capacity) {
            size_t new_capacity = capacity == 0 ? 32 : capacity * 2;
            result = xrealloc(result, new_capacity);
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
    if (jp_consume_literal(parser, "true"))
        return true;
    if (jp_consume_literal(parser, "false"))
        return false;
    json_die(parser, "expected true or false");
    return false;
}

static void jp_skip_value(JsonParser *parser);

static void jp_skip_array(JsonParser *parser)
{
    jp_expect(parser, '[');
    if (jp_peek(parser) == ']') {
        parser->pos++;
        return;
    }
    for (;;) {
        jp_skip_value(parser);
        char c = jp_peek(parser);
        if (c == ']') {
            parser->pos++;
            return;
        }
        jp_expect(parser, ',');
    }
}

static void jp_skip_object(JsonParser *parser)
{
    jp_expect(parser, '{');
    if (jp_peek(parser) == '}') {
        parser->pos++;
        return;
    }
    for (;;) {
        char *key = jp_parse_string(parser);
        free(key);
        jp_expect(parser, ':');
        jp_skip_value(parser);
        char c = jp_peek(parser);
        if (c == '}') {
            parser->pos++;
            return;
        }
        jp_expect(parser, ',');
    }
}

static void jp_skip_value(JsonParser *parser)
{
    char c = jp_peek(parser);
    if (c == '{')
        jp_skip_object(parser);
    else if (c == '[')
        jp_skip_array(parser);
    else if (c == '"') {
        char *value = jp_parse_string(parser);
        free(value);
    } else if (c == '-' || isdigit((unsigned char)c))
        (void)jp_parse_number(parser);
    else if (jp_consume_literal(parser, "true") ||
             jp_consume_literal(parser, "false") ||
             jp_consume_literal(parser, "null"))
        return;
    else
        json_die(parser, "invalid JSON value");
}

static void parse_nullable_filter(JsonParser *parser,
                                  bool *present, double *value)
{
    if (jp_consume_literal(parser, "null")) {
        *present = false;
        *value = 0.0;
        return;
    }
    *value = jp_parse_number(parser);
    *present = true;
}

static void parse_filters(JsonParser *parser, Config *config)
{
    jp_expect(parser, '{');
    if (jp_peek(parser) == '}') {
        parser->pos++;
        return;
    }

    for (;;) {
        char *key = jp_parse_string(parser);
        jp_expect(parser, ':');
        if (strcmp(key, "min_cw") == 0)
            parse_nullable_filter(parser,
                                  &config->filters.have_min_cw,
                                  &config->filters.min_cw);
        else if (strcmp(key, "min_z") == 0)
            parse_nullable_filter(parser,
                                  &config->filters.have_min_z,
                                  &config->filters.min_z);
        else
            jp_skip_value(parser);
        free(key);

        char c = jp_peek(parser);
        if (c == '}') {
            parser->pos++;
            return;
        }
        jp_expect(parser, ',');
    }
}

static void parse_dot(JsonParser *parser, Config *config)
{
    jp_expect(parser, '{');
    if (jp_peek(parser) == '}') {
        parser->pos++;
        return;
    }

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
        else
            jp_skip_value(parser);
        free(key);

        char c = jp_peek(parser);
        if (c == '}') {
            parser->pos++;
            return;
        }
        jp_expect(parser, ',');
    }
}

static void parse_label_fields(JsonParser *parser, Config *config)
{
    size_t count = 0;
    bool seen[5] = {false, false, false, false, false};

    jp_expect(parser, '[');
    if (jp_peek(parser) == ']')
        json_die(parser, "node.label_fields must not be empty");

    for (;;) {
        double value = jp_parse_number(parser);
        if (value < 1.0 || value > 4.0 ||
            value != (double)(size_t)value)
            json_die(parser,
                     "node.label_fields entries must be integers from 1 to 4");

        size_t field = (size_t)value;
        if (!seen[field]) {
            config->node_label_fields[count++] = field;
            seen[field] = true;
        }

        char c = jp_peek(parser);
        if (c == ']') {
            parser->pos++;
            break;
        }
        jp_expect(parser, ',');
    }

    if (count == 0)
        json_die(parser, "node.label_fields must not be empty");
    config->node_label_field_count = count;
}

static void parse_node(JsonParser *parser, Config *config)
{
    jp_expect(parser, '{');
    if (jp_peek(parser) == '}') {
        parser->pos++;
        return;
    }

    for (;;) {
        char *key = jp_parse_string(parser);
        jp_expect(parser, ':');
        if (strcmp(key, "shape") == 0)
            replace_string(&config->node_shape, jp_parse_string(parser));
        else if (strcmp(key, "fontname") == 0)
            replace_string(&config->node_fontname, jp_parse_string(parser));
        else if (strcmp(key, "label_fields") == 0)
            parse_label_fields(parser, config);
        else if (strcmp(key, "label_field") == 0) {
            /* Backward-compatible singular form. */
            double value = jp_parse_number(parser);
            if (value < 1.0 || value > 4.0 ||
                value != (double)(size_t)value)
                json_die(parser, "node.label_field must be an integer from 1 to 4");
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
                json_die(parser,
                         "node.min_font_size must be greater than zero");
        } else if (strcmp(key, "max_font_size") == 0) {
            config->node_max_font_size = jp_parse_number(parser);
            if (config->node_max_font_size <= 0.0)
                json_die(parser,
                         "node.max_font_size must be greater than zero");
        } else
            jp_skip_value(parser);
        free(key);

        char c = jp_peek(parser);
        if (c == '}') {
            parser->pos++;
            return;
        }
        jp_expect(parser, ',');
    }
}

static void set_tooltip_field(Config *config, const char *field,
                              JsonParser *parser)
{
    if (strcmp(field, "ctf") == 0)
        config->tooltip_ctf = true;
    else if (strcmp(field, "cdf") == 0)
        config->tooltip_cdf = true;
    else if (strcmp(field, "cw") == 0)
        config->tooltip_cw = true;
    else if (strcmp(field, "z") == 0)
        config->tooltip_z = true;
    else if (strcmp(field, "unit_ids") == 0)
        config->tooltip_unit_ids = true;
    else
        json_die(parser,
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
    if (jp_peek(parser) == ']') {
        parser->pos++;
        return;
    }
    for (;;) {
        char *field = jp_parse_string(parser);
        set_tooltip_field(config, field, parser);
        free(field);
        char c = jp_peek(parser);
        if (c == ']') {
            parser->pos++;
            return;
        }
        jp_expect(parser, ',');
    }
}

static void parse_edge(JsonParser *parser, Config *config)
{
    jp_expect(parser, '{');
    if (jp_peek(parser) == '}') {
        parser->pos++;
        return;
    }

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
        else
            jp_skip_value(parser);
        free(key);

        char c = jp_peek(parser);
        if (c == '}') {
            parser->pos++;
            return;
        }
        jp_expect(parser, ',');
    }
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
    json_die(parser,
             "table.columns entries must be token1, token2, raw_token1, "
             "raw_token2, ctf, cdf, df1, idf1, fq1, df2, idf2, fq2, "
             "cw, z, or unit_ids");
    return TABLE_TOKEN1;
}

static const char *table_column_default_header(TableColumn column)
{
    switch (column) {
    case TABLE_TOKEN1: return "Token 1";
    case TABLE_TOKEN2: return "Token 2";
    case TABLE_RAW_TOKEN1: return "Raw token 1";
    case TABLE_RAW_TOKEN2: return "Raw token 2";
    case TABLE_CTF: return "CTF";
    case TABLE_CDF: return "CDF";
    case TABLE_DF1: return "DF1";
    case TABLE_IDF1: return "IDF1";
    case TABLE_FQ1: return "FQ1";
    case TABLE_DF2: return "DF2";
    case TABLE_IDF2: return "IDF2";
    case TABLE_FQ2: return "FQ2";
    case TABLE_CW: return "CW";
    case TABLE_Z: return "Z";
    case TABLE_UNIT_IDS: return "Unit IDs";
    case TABLE_COLUMN_LIMIT: break;
    }
    return "";
}

static bool table_column_is_numeric(TableColumn column)
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
    case TABLE_TOKEN1:
    case TABLE_TOKEN2:
    case TABLE_RAW_TOKEN1:
    case TABLE_RAW_TOKEN2:
    case TABLE_UNIT_IDS:
    case TABLE_COLUMN_LIMIT:
        return false;
    }
    return false;
}

static void parse_table_columns(JsonParser *parser, Config *config)
{
    bool seen[TABLE_COLUMN_LIMIT] = {false};
    size_t count = 0;

    jp_expect(parser, '[');
    if (jp_peek(parser) == ']')
        json_die(parser, "table.columns must not be empty");

    for (;;) {
        char *name = jp_parse_string(parser);
        TableColumn column = parse_table_column_name(name, parser);
        free(name);
        if (seen[column])
            json_die(parser, "table.columns must not contain duplicates");
        seen[column] = true;
        config->table_columns[count++] = column;

        char c = jp_peek(parser);
        if (c == ']') {
            parser->pos++;
            break;
        }
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
    if (jp_peek(parser) == ']')
        json_die(parser, "table.headers must not be empty");

    for (;;) {
        if (config->table_header_count >= TABLE_COLUMN_LIMIT)
            json_die(parser, "too many table.headers entries");
        config->table_headers[config->table_header_count++] =
            jp_parse_string(parser);

        char c = jp_peek(parser);
        if (c == ']') {
            parser->pos++;
            break;
        }
        jp_expect(parser, ',');
    }
}

static void parse_table_label_fields(JsonParser *parser, Config *config)
{
    size_t count = 0;
    bool seen[5] = {false, false, false, false, false};

    jp_expect(parser, '[');
    if (jp_peek(parser) == ']')
        json_die(parser, "table.label_fields must not be empty");

    for (;;) {
        double value = jp_parse_number(parser);
        if (value < 1.0 || value > 4.0 ||
            value != (double)(size_t)value)
            json_die(parser,
                     "table.label_fields entries must be integers from 1 to 4");
        size_t field = (size_t)value;
        if (!seen[field]) {
            config->table_label_fields[count++] = field;
            seen[field] = true;
        }

        char c = jp_peek(parser);
        if (c == ']') {
            parser->pos++;
            break;
        }
        jp_expect(parser, ',');
    }
    config->table_label_field_count = count;
}

static char *jp_parse_nullable_string(JsonParser *parser)
{
    if (jp_consume_literal(parser, "null"))
        return NULL;
    return jp_parse_string(parser);
}

static void parse_table(JsonParser *parser, Config *config)
{
    jp_expect(parser, '{');
    if (jp_peek(parser) == '}') {
        parser->pos++;
        return;
    }

    for (;;) {
        char *key = jp_parse_string(parser);
        jp_expect(parser, ':');
        if (strcmp(key, "columns") == 0)
            parse_table_columns(parser, config);
        else if (strcmp(key, "headers") == 0)
            parse_table_headers(parser, config);
        else if (strcmp(key, "label_fields") == 0)
            parse_table_label_fields(parser, config);
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
        else
            jp_skip_value(parser);
        free(key);

        char c = jp_peek(parser);
        if (c == '}') {
            parser->pos++;
            return;
        }
        jp_expect(parser, ',');
    }
}

static void load_config(const char *path, Config *config)
{
    size_t length = 0;
    char *text = read_file(path, &length);
    JsonParser parser = {path, text, length, 0, 1};

    jp_expect(&parser, '{');
    if (jp_peek(&parser) != '}') {
        for (;;) {
            char *key = jp_parse_string(&parser);
            jp_expect(&parser, ':');
            if (strcmp(key, "format") == 0) {
                char *format = jp_parse_string(&parser);
                config->format = parse_format_name(format, path);
                free(format);
            } else if (strcmp(key, "filters") == 0)
                parse_filters(&parser, config);
            else if (strcmp(key, "dot") == 0)
                parse_dot(&parser, config);
            else if (strcmp(key, "node") == 0)
                parse_node(&parser, config);
            else if (strcmp(key, "edge") == 0)
                parse_edge(&parser, config);
            else if (strcmp(key, "table") == 0)
                parse_table(&parser, config);
            else
                jp_skip_value(&parser);
            free(key);

            char c = jp_peek(&parser);
            if (c == '}')
                break;
            jp_expect(&parser, ',');
        }
    }
    jp_expect(&parser, '}');
    if (jp_peek(&parser) != '\0')
        json_die(&parser, "unexpected content after root object");

    if (config->table_header_count != 0 &&
        config->table_header_count != config->table_column_count)
        json_die(&parser,
                 "table.headers must have the same number of entries as table.columns");

    free(config->config_path);
    config->config_path = xstrdup(path);
    free(text);
}

static bool edge_is_selected(const Edge *edge, const Filters *filters)
{
    if (filters->have_min_cw && edge->cw < filters->min_cw)
        return false;
    if (filters->have_min_z && edge->z < filters->min_z)
        return false;
    return true;
}

static void read_edges(FILE *stream, const char *source,
                       const Filters *filters,
                       EdgeVec *edges, NodeVec *nodes)
{
    char *line = NULL;
    size_t line_capacity = 0;
    size_t line_number = 0;
    ssize_t length;

    while ((length = getline(&line, &line_capacity, stream)) != -1) {
        line_number++;

        while (length > 0 &&
               (line[length - 1] == '\n' || line[length - 1] == '\r'))
            line[--length] = '\0';

        if (length == 0 || line[0] == '#')
            continue;

        char **fields = NULL;
        size_t field_count = split_tabs(line, &fields);

        if (field_count < 10) {
            free(fields);
            die_line(source, line_number,
                     "expected at least 10 tab-separated fields");
        }

        Edge edge = {0};
        edge.source = xstrdup(fields[0]);
        edge.target = xstrdup(fields[1]);
        edge.ctf = parse_size(fields[2], source, line_number, "ctf");
        edge.cdf = parse_size(fields[3], source, line_number, "cdf");

        bool has_fq = false;
        size_t unit_start = 0;
        if (field_count == 12 + edge.ctf) {
            has_fq = true;
            unit_start = 12;
        } else if (field_count == 10 + edge.ctf) {
            unit_start = 10;
        } else {
            char message[256];
            snprintf(message, sizeof(message),
                     "expected either %zu fields (with fq) or %zu fields "
                     "(legacy without fq), but found %zu",
                     12 + edge.ctf, 10 + edge.ctf, field_count);
            free(fields);
            free(edge.source);
            free(edge.target);
            die_line(source, line_number, message);
        }

        edge.df1 = parse_size(fields[4], source, line_number, "df1");
        edge.idf1 = parse_double(fields[5], source, line_number, "idf1");
        if (has_fq) {
            edge.fq1 = parse_optional_size(fields[6], source, line_number,
                                           "fq1", &edge.fq1_available);
            edge.df2 = parse_size(fields[7], source, line_number, "df2");
            edge.idf2 = parse_double(fields[8], source, line_number, "idf2");
            edge.fq2 = parse_optional_size(fields[9], source, line_number,
                                           "fq2", &edge.fq2_available);
            edge.cw = parse_double(fields[10], source, line_number, "cw");
            edge.z = parse_double(fields[11], source, line_number, "z");
        } else {
            edge.df2 = parse_size(fields[6], source, line_number, "df2");
            edge.idf2 = parse_double(fields[7], source, line_number, "idf2");
            edge.cw = parse_double(fields[8], source, line_number, "cw");
            edge.z = parse_double(fields[9], source, line_number, "z");
        }
        edge.unit_count = field_count - unit_start;

        if (edge.source[0] == '\0' || edge.target[0] == '\0') {
            free(fields);
            free(edge.source);
            free(edge.target);
            die_line(source, line_number, "empty token field");
        }
        if (edge.ctf == 0) {
            free(fields);
            free(edge.source);
            free(edge.target);
            die_line(source, line_number, "ctf must be greater than zero");
        }
        if (edge.cdf > edge.ctf) {
            free(fields);
            free(edge.source);
            free(edge.target);
            die_line(source, line_number, "cdf is greater than ctf");
        }
        if (edge.unit_count != edge.ctf) {
            char message[192];
            snprintf(message, sizeof(message),
                     "ctf is %zu but %zu unit_id fields were supplied",
                     edge.ctf, edge.unit_count);
            free(fields);
            free(edge.source);
            free(edge.target);
            die_line(source, line_number, message);
        }

        if (edge.unit_count > 0) {
            edge.unit_ids = xmalloc(edge.unit_count * sizeof(*edge.unit_ids));
            for (size_t i = 0; i < edge.unit_count; i++) {
                if (fields[unit_start + i][0] == '\0') {
                    for (size_t j = 0; j < i; j++)
                        free(edge.unit_ids[j]);
                    free(edge.unit_ids);
                    free(fields);
                    free(edge.source);
                    free(edge.target);
                    die_line(source, line_number, "empty unit_id field");
                }
                edge.unit_ids[i] = xstrdup(fields[unit_start + i]);
            }
        }

        free(fields);

        if (!edge_is_selected(&edge, filters)) {
            free(edge.source);
            free(edge.target);
            for (size_t i = 0; i < edge.unit_count; i++)
                free(edge.unit_ids[i]);
            free(edge.unit_ids);
            continue;
        }

        edge_vec_push(edges, edge);
        node_vec_push(nodes, (NodeRef){
            .id = edge.source,
            .df = edge.df1,
            .idf = edge.idf1,
            .fq = edge.fq1,
            .fq_available = edge.fq1_available,
            .degree = 0
        });
        node_vec_push(nodes, (NodeRef){
            .id = edge.target,
            .df = edge.df2,
            .idf = edge.idf2,
            .fq = edge.fq2,
            .fq_available = edge.fq2_available,
            .degree = 0
        });
    }

    if (ferror(stream)) {
        free(line);
        die_errno(source);
    }

    free(line);
}

static void json_write_string_n(FILE *stream, const char *text, size_t length)
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
            } else
                fputc(c, stream);
            break;
        }
    }
    fputc('"', stream);
}

static void json_write_string(FILE *stream, const char *text)
{
    json_write_string_n(stream, text, strlen(text));
}

static void json_write_token_fields(FILE *stream, const char *token)
{
    fputc('[', stream);
    const char *start = token;
    size_t field_number = 0;
    for (const char *p = token;; p++) {
        if (*p == '/' || *p == '\0') {
            if (field_number > 0)
                fputs(", ", stream);
            json_write_string_n(stream, start, (size_t)(p - start));
            field_number++;
            if (*p == '\0')
                break;
            start = p + 1;
        }
    }
    fputc(']', stream);
}

static void json_write_optional_number(FILE *stream, bool present, double value)
{
    if (present)
        fprintf(stream, "%.17g", value);
    else
        fputs("null", stream);
}

static const char *format_name(OutputFormat format)
{
    switch (format) {
    case FORMAT_JSON: return "json";
    case FORMAT_DOT: return "dot";
    case FORMAT_MD: return "md";
    case FORMAT_TEX: return "tex";
    case FORMAT_HTML: return "html";
    }
    return "dot";
}

static char *make_node_label(const char *token, const Config *config);

typedef struct {
    double min;
    double max;
} NodeWeightRange;

static double node_weight(const NodeRef *node, const Config *config)
{
    switch (config->node_font_size_by) {
    case FONT_SIZE_FQ: return (double)node->fq;
    case FONT_SIZE_IDF: return node->idf;
    case FONT_SIZE_DEGREE: return (double)node->degree;
    }
    return 0.0;
}

static void validate_font_size_data(const NodeVec *nodes,
                                    const Config *config,
                                    const char *source)
{
    if (config->node_font_size_by != FONT_SIZE_FQ)
        return;

    for (size_t i = 0; i < nodes->len; i++) {
        if (!nodes->items[i].fq_available) {
            fprintf(stderr,
                    "%s: %s: fq is unavailable for token '%s'; "
                    "use pair 0.2.0 or choose font_size_by idf/degree\n",
                    PROG_NAME, source, nodes->items[i].id);
            exit(EXIT_FAILURE);
        }
    }
}

static NodeWeightRange node_weight_range(const NodeVec *nodes,
                                         const Config *config)
{
    NodeWeightRange range = {0.0, 0.0};
    if (nodes->len == 0)
        return range;

    range.min = node_weight(&nodes->items[0], config);
    range.max = range.min;
    for (size_t i = 1; i < nodes->len; i++) {
        double weight = node_weight(&nodes->items[i], config);
        if (weight < range.min)
            range.min = weight;
        if (weight > range.max)
            range.max = weight;
    }
    return range;
}

static double node_font_size(const NodeRef *node,
                             NodeWeightRange range,
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

static void write_json(FILE *stream, const EdgeVec *edges,
                       const NodeVec *nodes, const Config *config)
{
    NodeWeightRange weight_range = node_weight_range(nodes, config);
    fputs("{\n", stream);
    fputs("  \"format\": \"cw-tools/graph\",\n", stream);
    fputs("  \"version\": 1,\n", stream);
    fputs("  \"emit\": {\"config\": ", stream);
    json_write_string(stream, config->config_path);
    fputs(", \"output_format\": ", stream);
    json_write_string(stream, format_name(config->format));
    fputs(", \"label_fields\": [", stream);
    for (size_t i = 0; i < config->node_label_field_count; i++) {
        if (i > 0)
            fputs(", ", stream);
        fprintf(stream, "%zu", config->node_label_fields[i]);
    }
    fputs("], \"label_separator\": ", stream);
    json_write_string(stream, config->node_label_separator);
    fprintf(stream,
            ", \"font_size_by\": \"%s\", \"min_font_size\": %.17g, \"max_font_size\": %.17g",
            font_size_by_name(config->node_font_size_by),
            config->node_min_font_size, config->node_max_font_size);
    fputs(", \"graphviz\": {\"graph\": {\"fontname\": ", stream);
    if (config->graph_fontname != NULL)
        json_write_string(stream, config->graph_fontname);
    else
        fputs("null", stream);
    fputs(", \"sep\": ", stream);
    if (config->graph_sep != NULL)
        json_write_string(stream, config->graph_sep);
    else
        fputs("null", stream);
    fputs(", \"pack\": ", stream);
    if (config->graph_pack_set)
        fputs(config->graph_pack ? "true" : "false", stream);
    else
        fputs("null", stream);
    fputs(", \"packmode\": ", stream);
    if (config->graph_packmode != NULL)
        json_write_string(stream, config->graph_packmode);
    else
        fputs("null", stream);
    fputs(", \"splines\": ", stream);
    if (config->graph_splines != NULL)
        json_write_string(stream, config->graph_splines);
    else
        fputs("null", stream);
    fputs("}, \"node\": {\"fontname\": ", stream);
    if (config->node_fontname != NULL)
        json_write_string(stream, config->node_fontname);
    else
        fputs("null", stream);
    fputs("}, \"edge\": {\"fontname\": ", stream);
    if (config->edge_fontname != NULL)
        json_write_string(stream, config->edge_fontname);
    else
        fputs("null", stream);
    fputs(", \"length\": ", stream);
    if (config->edge_len_set)
        fprintf(stream, "%.17g", config->edge_len);
    else
        fputs("null", stream);
    fputs("}}", stream);
    fputs("},\n", stream);
    fputs("  \"filters\": {\n", stream);
    fputs("    \"min_cw\": ", stream);
    json_write_optional_number(stream,
                               config->filters.have_min_cw,
                               config->filters.min_cw);
    fputs(",\n", stream);
    fputs("    \"min_z\": ", stream);
    json_write_optional_number(stream,
                               config->filters.have_min_z,
                               config->filters.min_z);
    fputs("\n  },\n", stream);
    fprintf(stream,
            "  \"counts\": {\"nodes\": %zu, \"edges\": %zu},\n",
            nodes->len, edges->len);

    fputs("  \"nodes\": [\n", stream);
    for (size_t i = 0; i < nodes->len; i++) {
        const NodeRef *node = &nodes->items[i];
        char *label = make_node_label(node->id, config);
        fputs("    {\"id\": ", stream);
        json_write_string(stream, node->id);
        fputs(", \"label\": ", stream);
        json_write_string(stream, label);
        fputs(", \"fields\": ", stream);
        json_write_token_fields(stream, node->id);
        fprintf(stream, ", \"df\": %zu, \"idf\": %.12g, \"fq\": ",
                node->df, node->idf);
        if (node->fq_available)
            fprintf(stream, "%zu", node->fq);
        else
            fputs("null", stream);
        fprintf(stream,
                ", \"degree\": %zu, \"font_size\": %.17g}",
                node->degree,
                node_font_size(node, weight_range, config));
        free(label);
        fputs(i + 1 == nodes->len ? "\n" : ",\n", stream);
    }
    fputs("  ],\n", stream);

    fputs("  \"edges\": [\n", stream);
    for (size_t i = 0; i < edges->len; i++) {
        const Edge *edge = &edges->items[i];
        fputs("    {\"source\": ", stream);
        json_write_string(stream, edge->source);
        fputs(", \"target\": ", stream);
        json_write_string(stream, edge->target);
        fprintf(stream,
                ", \"ctf\": %zu, \"cdf\": %zu, \"cw\": %.17g, \"z\": %.17g, \"unit_ids\": [",
                edge->ctf, edge->cdf, edge->cw, edge->z);
        for (size_t j = 0; j < edge->unit_count; j++) {
            if (j > 0)
                fputs(", ", stream);
            json_write_string(stream, edge->unit_ids[j]);
        }
        fputs("]}", stream);
        fputs(i + 1 == edges->len ? "\n" : ",\n", stream);
    }
    fputs("  ]\n", stream);
    fputs("}\n", stream);
}

static void dot_write_escaped_n(FILE *stream, const char *text, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)text[i];
        switch (c) {
        case '"': fputs("\\\"", stream); break;
        case '\\': fputs("\\\\", stream); break;
        case '\n': fputs("\\n", stream); break;
        case '\r': break;
        case '\t': fputc(' ', stream); break;
        default:
            if (c >= 0x20)
                fputc(c, stream);
            break;
        }
    }
}

static void dot_write_string_n(FILE *stream, const char *text, size_t length)
{
    fputc('"', stream);
    dot_write_escaped_n(stream, text, length);
    fputc('"', stream);
}

static void dot_write_string(FILE *stream, const char *text)
{
    dot_write_string_n(stream, text, strlen(text));
}

typedef struct {
    const char *start;
    size_t length;
} TokenFieldView;

static size_t token_split_fields(const char *token, TokenFieldView fields[4])
{
    const char *start = token;
    size_t count = 0;

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

static char *make_node_label(const char *token, const Config *config)
{
    TokenFieldView fields[4] = {{0}};
    size_t field_count = token_split_fields(token, fields);
    size_t length = 0;
    size_t selected = 0;
    size_t separator_length = strlen(config->node_label_separator);

    for (size_t i = 0; i < config->node_label_field_count; i++) {
        size_t requested = config->node_label_fields[i];
        if (requested == 0 || requested > field_count ||
            fields[requested - 1].length == 0)
            continue;
        if (selected > 0)
            length += separator_length;
        length += fields[requested - 1].length;
        selected++;
    }

    if (selected == 0) {
        length = field_count > 0 ? fields[0].length : strlen(token);
        char *fallback = xmalloc(length + 1);
        memcpy(fallback, field_count > 0 ? fields[0].start : token, length);
        fallback[length] = '\0';
        return fallback;
    }

    char *label = xmalloc(length + 1);
    char *out = label;
    selected = 0;
    for (size_t i = 0; i < config->node_label_field_count; i++) {
        size_t requested = config->node_label_fields[i];
        if (requested == 0 || requested > field_count ||
            fields[requested - 1].length == 0)
            continue;
        if (selected > 0) {
            memcpy(out, config->node_label_separator, separator_length);
            out += separator_length;
        }
        memcpy(out, fields[requested - 1].start,
               fields[requested - 1].length);
        out += fields[requested - 1].length;
        selected++;
    }
    *out = '\0';
    return label;
}

static void dot_write_unit_ids(FILE *stream, const Edge *edge)
{
    fputc('"', stream);
    for (size_t i = 0; i < edge->unit_count; i++) {
        if (i > 0)
            fputc(',', stream);
        dot_write_escaped_n(stream,
                            edge->unit_ids[i], strlen(edge->unit_ids[i]));
    }
    fputc('"', stream);
}

static bool tooltip_enabled(const Config *config)
{
    return config->tooltip_ctf || config->tooltip_cdf ||
           config->tooltip_cw || config->tooltip_z ||
           config->tooltip_unit_ids;
}

static void dot_write_tooltip(FILE *stream, const Edge *edge,
                              const Config *config)
{
    fputc('"', stream);
    bool first = true;
#define TOOLTIP_SEP() do { if (!first) fputs("; ", stream); first = false; } while (0)
    if (config->tooltip_ctf) {
        TOOLTIP_SEP();
        fprintf(stream, "ctf=%zu", edge->ctf);
    }
    if (config->tooltip_cdf) {
        TOOLTIP_SEP();
        fprintf(stream, "cdf=%zu", edge->cdf);
    }
    if (config->tooltip_cw) {
        TOOLTIP_SEP();
        fprintf(stream, "cw=%.17g", edge->cw);
    }
    if (config->tooltip_z) {
        TOOLTIP_SEP();
        fprintf(stream, "z=%.17g", edge->z);
    }
    if (config->tooltip_unit_ids) {
        TOOLTIP_SEP();
        fputs("unit_ids=", stream);
        for (size_t i = 0; i < edge->unit_count; i++) {
            if (i > 0)
                fputc(',', stream);
            dot_write_escaped_n(stream,
                                edge->unit_ids[i], strlen(edge->unit_ids[i]));
        }
    }
#undef TOOLTIP_SEP
    fputc('"', stream);
}

static void dot_write_edge_label(FILE *stream, const Edge *edge,
                                 EdgeLabel label)
{
    fputc('"', stream);
    switch (label) {
    case EDGE_LABEL_CTF: fprintf(stream, "%zu", edge->ctf); break;
    case EDGE_LABEL_CDF: fprintf(stream, "%zu", edge->cdf); break;
    case EDGE_LABEL_CW: fprintf(stream, "%.6g", edge->cw); break;
    case EDGE_LABEL_Z: fprintf(stream, "%.6g", edge->z); break;
    case EDGE_LABEL_NONE: break;
    }
    fputc('"', stream);
}

static void write_dot(FILE *stream, const EdgeVec *edges,
                      const NodeVec *nodes, const Config *config)
{
    NodeWeightRange weight_range = node_weight_range(nodes, config);
    fputs(config->directed ? "digraph " : "graph ", stream);
    dot_write_string(stream, config->graph_name);
    fputs(" {\n", stream);

    fputs("  graph [charset=", stream);
    dot_write_string(stream, config->charset);
    fprintf(stream, ", overlap=%s, outputorder=",
            config->overlap ? "true" : "false");
    dot_write_string(stream, config->outputorder);
    if (config->graph_fontname != NULL) {
        fputs(", fontname=", stream);
        dot_write_string(stream, config->graph_fontname);
    }
    if (config->graph_sep != NULL) {
        fputs(", sep=", stream);
        dot_write_string(stream, config->graph_sep);
    }
    if (config->graph_pack_set)
        fprintf(stream, ", pack=%s", config->graph_pack ? "true" : "false");
    if (config->graph_packmode != NULL) {
        fputs(", packmode=", stream);
        dot_write_string(stream, config->graph_packmode);
    }
    if (config->graph_splines != NULL) {
        fputs(", splines=", stream);
        dot_write_string(stream, config->graph_splines);
    }
    fputs("];\n", stream);

    fputs("  node [shape=", stream);
    dot_write_string(stream, config->node_shape);
    if (config->node_fontname != NULL) {
        fputs(", fontname=", stream);
        dot_write_string(stream, config->node_fontname);
    }
    fputs("];\n", stream);
    fprintf(stream, "  edge [penwidth=\"%.17g\"",
            config->edge_penwidth);
    if (config->edge_fontname != NULL) {
        fputs(", fontname=", stream);
        dot_write_string(stream, config->edge_fontname);
    }
    if (config->edge_len_set)
        fprintf(stream, ", len=\"%.12g\"", config->edge_len);
    fputs("];\n\n", stream);

    for (size_t i = 0; i < nodes->len; i++) {
        const NodeRef *node = &nodes->items[i];
        char *label = make_node_label(node->id, config);

        fputs("  ", stream);
        dot_write_string(stream, node->id);
        fputs(" [label=", stream);
        dot_write_string(stream, label);
        fprintf(stream,
                ", fontsize=\"%.17g\", df=%zu, idf=\"%.12g\"",
                node_font_size(node, weight_range, config),
                node->df, node->idf);
        if (node->fq_available)
            fprintf(stream, ", fq=%zu", node->fq);
        fprintf(stream, ", degree=%zu, tooltip=", node->degree);
        fputc('"', stream);
        fprintf(stream, "df=%zu; idf=%.12g; fq=", node->df, node->idf);
        if (node->fq_available)
            fprintf(stream, "%zu", node->fq);
        else
            fputs("NA", stream);
        fprintf(stream, "; degree=%zu", node->degree);
        fputs("\"];\n", stream);
        free(label);
    }

    if (nodes->len > 0 && edges->len > 0)
        fputc('\n', stream);

    const char *connector = config->directed ? " -> " : " -- ";
    for (size_t i = 0; i < edges->len; i++) {
        const Edge *edge = &edges->items[i];
        fputs("  ", stream);
        dot_write_string(stream, edge->source);
        fputs(connector, stream);
        dot_write_string(stream, edge->target);
        fputs(" [", stream);

        bool need_comma = false;
        if (config->edge_label != EDGE_LABEL_NONE) {
            fputs("label=", stream);
            dot_write_edge_label(stream, edge, config->edge_label);
            need_comma = true;
        }
        if (need_comma)
            fputs(", ", stream);
        fprintf(stream,
                "ctf=%zu, cdf=%zu, cw=\"%.17g\", z=\"%.17g\", unit_ids=",
                edge->ctf, edge->cdf, edge->cw, edge->z);
        dot_write_unit_ids(stream, edge);
        if (tooltip_enabled(config)) {
            fputs(", tooltip=", stream);
            dot_write_tooltip(stream, edge, config);
        }
        fputs("];\n", stream);
    }

    fputs("}\n", stream);
}

static char *make_selected_token_label(const char *token,
                                       const size_t *selected_fields,
                                       size_t selected_field_count,
                                       const char *separator)
{
    TokenFieldView fields[4] = {{0}};
    size_t field_count = token_split_fields(token, fields);
    size_t length = 0;
    size_t selected = 0;
    size_t separator_length = strlen(separator);

    for (size_t i = 0; i < selected_field_count; i++) {
        size_t requested = selected_fields[i];
        if (requested == 0 || requested > field_count ||
            fields[requested - 1].length == 0)
            continue;
        if (selected > 0)
            length += separator_length;
        length += fields[requested - 1].length;
        selected++;
    }

    if (selected == 0)
        return xstrdup(token);

    char *label = xmalloc(length + 1);
    char *out = label;
    selected = 0;
    for (size_t i = 0; i < selected_field_count; i++) {
        size_t requested = selected_fields[i];
        if (requested == 0 || requested > field_count ||
            fields[requested - 1].length == 0)
            continue;
        if (selected > 0) {
            memcpy(out, separator, separator_length);
            out += separator_length;
        }
        memcpy(out, fields[requested - 1].start,
               fields[requested - 1].length);
        out += fields[requested - 1].length;
        selected++;
    }
    *out = '\0';
    return label;
}

static char *format_size_cell(size_t value)
{
    int length = snprintf(NULL, 0, "%zu", value);
    if (length < 0)
        die("failed to format table integer");
    char *text = xmalloc((size_t)length + 1);
    (void)snprintf(text, (size_t)length + 1, "%zu", value);
    return text;
}

static char *format_double_cell(double value, size_t precision)
{
    int length = snprintf(NULL, 0, "%.*g", (int)precision, value);
    if (length < 0)
        die("failed to format table number");
    char *text = xmalloc((size_t)length + 1);
    (void)snprintf(text, (size_t)length + 1,
                   "%.*g", (int)precision, value);
    return text;
}

static char *format_unit_ids_cell(const Edge *edge, const Config *config)
{
    size_t separator_length = strlen(config->table_unit_separator);
    size_t length = 0;
    for (size_t i = 0; i < edge->unit_count; i++) {
        if (i > 0)
            length += separator_length;
        length += strlen(edge->unit_ids[i]);
    }

    char *text = xmalloc(length + 1);
    char *out = text;
    for (size_t i = 0; i < edge->unit_count; i++) {
        if (i > 0) {
            memcpy(out, config->table_unit_separator, separator_length);
            out += separator_length;
        }
        size_t item_length = strlen(edge->unit_ids[i]);
        memcpy(out, edge->unit_ids[i], item_length);
        out += item_length;
    }
    *out = '\0';
    return text;
}

static char *table_cell_text(const Edge *edge, TableColumn column,
                             const Config *config)
{
    switch (column) {
    case TABLE_TOKEN1:
        return make_selected_token_label(edge->source,
                                         config->table_label_fields,
                                         config->table_label_field_count,
                                         config->table_label_separator);
    case TABLE_TOKEN2:
        return make_selected_token_label(edge->target,
                                         config->table_label_fields,
                                         config->table_label_field_count,
                                         config->table_label_separator);
    case TABLE_RAW_TOKEN1: return xstrdup(edge->source);
    case TABLE_RAW_TOKEN2: return xstrdup(edge->target);
    case TABLE_CTF: return format_size_cell(edge->ctf);
    case TABLE_CDF: return format_size_cell(edge->cdf);
    case TABLE_DF1: return format_size_cell(edge->df1);
    case TABLE_IDF1:
        return format_double_cell(edge->idf1, config->table_precision);
    case TABLE_FQ1:
        return edge->fq1_available ? format_size_cell(edge->fq1) : xstrdup("NA");
    case TABLE_DF2: return format_size_cell(edge->df2);
    case TABLE_IDF2:
        return format_double_cell(edge->idf2, config->table_precision);
    case TABLE_FQ2:
        return edge->fq2_available ? format_size_cell(edge->fq2) : xstrdup("NA");
    case TABLE_CW:
        return format_double_cell(edge->cw, config->table_precision);
    case TABLE_Z:
        return format_double_cell(edge->z, config->table_precision);
    case TABLE_UNIT_IDS:
        return format_unit_ids_cell(edge, config);
    case TABLE_COLUMN_LIMIT:
        break;
    }
    return xstrdup("");
}

static const char *table_header(const Config *config, size_t index)
{
    if (config->table_header_count > 0)
        return config->table_headers[index];
    return table_column_default_header(config->table_columns[index]);
}

static void markdown_write_escaped(FILE *stream, const char *text)
{
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
        switch (*p) {
        case '\\': fputs("\\\\", stream); break;
        case '|': fputs("\\|", stream); break;
        case '&': fputs("&amp;", stream); break;
        case '<': fputs("&lt;", stream); break;
        case '>': fputs("&gt;", stream); break;
        case '\n': fputs("<br>", stream); break;
        case '\r': break;
        default: fputc(*p, stream); break;
        }
    }
}

static void html_write_escaped(FILE *stream, const char *text)
{
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
        switch (*p) {
        case '&': fputs("&amp;", stream); break;
        case '<': fputs("&lt;", stream); break;
        case '>': fputs("&gt;", stream); break;
        case '"': fputs("&quot;", stream); break;
        case '\'': fputs("&#39;", stream); break;
        case '\n': fputs("<br>\n", stream); break;
        case '\r': break;
        default: fputc(*p, stream); break;
        }
    }
}

static void tex_write_escaped(FILE *stream, const char *text)
{
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
        switch (*p) {
        case '\\': fputs("\\textbackslash{}", stream); break;
        case '{': fputs("\\{", stream); break;
        case '}': fputs("\\}", stream); break;
        case '$': fputs("\\$", stream); break;
        case '&': fputs("\\&", stream); break;
        case '#': fputs("\\#", stream); break;
        case '_': fputs("\\_", stream); break;
        case '%': fputs("\\%", stream); break;
        case '~': fputs("\\textasciitilde{}", stream); break;
        case '^': fputs("\\textasciicircum{}", stream); break;
        case '\n': fputc(' ', stream); break;
        case '\r': break;
        default: fputc(*p, stream); break;
        }
    }
}

static void write_markdown_table(FILE *stream, const EdgeVec *edges,
                                 const Config *config)
{
    if (config->table_label != NULL) {
        fputs("<a id=\"", stream);
        html_write_escaped(stream, config->table_label);
        fputs("\"></a>\n\n", stream);
    }
    if (config->table_caption != NULL) {
        fputs("**", stream);
        markdown_write_escaped(stream, config->table_caption);
        fputs("**\n\n", stream);
    }

    fputc('|', stream);
    for (size_t i = 0; i < config->table_column_count; i++) {
        fputc(' ', stream);
        markdown_write_escaped(stream, table_header(config, i));
        fputs(" |", stream);
    }
    fputc('\n', stream);

    fputc('|', stream);
    for (size_t i = 0; i < config->table_column_count; i++) {
        if (table_column_is_numeric(config->table_columns[i]))
            fputs(" ---: |", stream);
        else
            fputs(" --- |", stream);
    }
    fputc('\n', stream);

    for (size_t row = 0; row < edges->len; row++) {
        fputc('|', stream);
        for (size_t i = 0; i < config->table_column_count; i++) {
            char *text = table_cell_text(&edges->items[row],
                                         config->table_columns[i], config);
            fputc(' ', stream);
            markdown_write_escaped(stream, text);
            fputs(" |", stream);
            free(text);
        }
        fputc('\n', stream);
    }
}

static void write_html_table(FILE *stream, const EdgeVec *edges,
                             const Config *config)
{
    fputs("<table", stream);
    if (config->table_label != NULL) {
        fputs(" id=\"", stream);
        html_write_escaped(stream, config->table_label);
        fputc('"', stream);
    }
    fputs(">\n", stream);
    if (config->table_caption != NULL) {
        fputs("  <caption>", stream);
        html_write_escaped(stream, config->table_caption);
        fputs("</caption>\n", stream);
    }
    fputs("  <thead>\n    <tr>\n", stream);
    for (size_t i = 0; i < config->table_column_count; i++) {
        fputs("      <th scope=\"col\"", stream);
        if (table_column_is_numeric(config->table_columns[i]))
            fputs(" class=\"numeric\"", stream);
        fputc('>', stream);
        html_write_escaped(stream, table_header(config, i));
        fputs("</th>\n", stream);
    }
    fputs("    </tr>\n  </thead>\n  <tbody>\n", stream);

    for (size_t row = 0; row < edges->len; row++) {
        fputs("    <tr>\n", stream);
        for (size_t i = 0; i < config->table_column_count; i++) {
            char *text = table_cell_text(&edges->items[row],
                                         config->table_columns[i], config);
            fputs("      <td", stream);
            if (table_column_is_numeric(config->table_columns[i]))
                fputs(" class=\"numeric\"", stream);
            fputc('>', stream);
            html_write_escaped(stream, text);
            fputs("</td>\n", stream);
            free(text);
        }
        fputs("    </tr>\n", stream);
    }
    fputs("  </tbody>\n</table>\n", stream);
}

static void write_tex_table(FILE *stream, const EdgeVec *edges,
                            const Config *config)
{
    fputs("\\begin{table}[htbp]\n\\centering\n", stream);
    if (config->table_caption != NULL) {
        fputs("\\caption{", stream);
        tex_write_escaped(stream, config->table_caption);
        fputs("}\n", stream);
    }
    if (config->table_label != NULL) {
        fputs("\\label{", stream);
        tex_write_escaped(stream, config->table_label);
        fputs("}\n", stream);
    }
    fputs("\\begin{tabular}{", stream);
    for (size_t i = 0; i < config->table_column_count; i++)
        fputc(table_column_is_numeric(config->table_columns[i]) ? 'r' : 'l',
              stream);
    fputs("}\n\\hline\n", stream);

    for (size_t i = 0; i < config->table_column_count; i++) {
        if (i > 0)
            fputs(" & ", stream);
        tex_write_escaped(stream, table_header(config, i));
    }
    fputs(" \\\\\n\\hline\n", stream);

    for (size_t row = 0; row < edges->len; row++) {
        for (size_t i = 0; i < config->table_column_count; i++) {
            char *text = table_cell_text(&edges->items[row],
                                         config->table_columns[i], config);
            if (i > 0)
                fputs(" & ", stream);
            tex_write_escaped(stream, text);
            free(text);
        }
        fputs(" \\\\\n", stream);
    }
    fputs("\\hline\n\\end{tabular}\n\\end{table}\n", stream);
}

static void write_table(FILE *stream, const EdgeVec *edges,
                        const Config *config)
{
    switch (config->format) {
    case FORMAT_MD:
        write_markdown_table(stream, edges, config);
        return;
    case FORMAT_TEX:
        write_tex_table(stream, edges, config);
        return;
    case FORMAT_HTML:
        write_html_table(stream, edges, config);
        return;
    case FORMAT_JSON:
    case FORMAT_DOT:
        break;
    }
    die("internal table-format error");
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

static void print_help(FILE *stream)
{
    fprintf(stream,
            "Usage: %s [OPTION]... [FILE]\n"
            "\n"
            "Emit JSON, Graphviz DOT, Markdown, LaTeX, or HTML from cw output.\n"
            "Normal behavior is read from %s; command-line options override\n"
            "only the values explicitly supplied.\n"
            "\n"
            "Input (current):\n"
            "  token1 token2 ctf cdf df1 idf1 fq1 df2 idf2 fq2 cw z unit_id...\n"
            "Legacy input without fq1/fq2 is also accepted.\n"
            "\n"
            "Configuration:\n"
            "  -c, --config FILE     read FILE instead of %s\n"
            "\n"
            "Temporary overrides:\n"
            "  -T, --format FORMAT   output format: json, dot, md, tex, or html\n"
            "  -W, --min-cw VALUE    retain edges whose CW is at least VALUE\n"
            "  -Z, --min-z VALUE     retain edges whose Z is at least VALUE\n"
            "\n"
            "Other options:\n"
            "  -h, --help             display this help and exit\n"
            "  -v, --version          display version information and exit\n"
            "\n"
            "The node.label_fields array selects one or more fields from the\n"
            "complete token retained by cw; label_separator joins them.\n"
            "node.font_size_by selects fq, idf, or degree.  Values among\n"
            "the emitted nodes are linearly mapped to node.min_font_size and\n"
            "node.max_font_size.  degree is calculated after edge filtering.\n"
            "Graphviz graph, node, and edge fonts and layout hints such as\n"
            "sep, pack, packmode, splines, and edge length are read from the\n"
            "configuration and safely serialized as DOT attributes.\n"
            "The table object selects columns, headings, label fields, numeric\n"
            "precision, caption, label, and unit-id separator for md, tex,\n"
            "and html table output.\n"
            "Thresholds affect only emitted edges and incident nodes.\n"
            "IDF, CW, and Z are never recalculated by emit.\n",
            PROG_NAME, DEFAULT_CONFIG, DEFAULT_CONFIG);
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

    int option;
    (void)setlocale(LC_ALL, "");

    while ((option = getopt_long(argc, argv, "c:T:W:Z:hv",
                                 long_options, NULL)) != -1) {
        switch (option) {
        case 'c':
            overrides.config_path = optarg;
            break;
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
        case 'h':
            print_help(stdout);
            return EXIT_SUCCESS;
        case 'v':
            printf("%s %s\n", PROG_NAME, PROG_VERSION);
            return EXIT_SUCCESS;
        default:
            print_help(stderr);
            return EXIT_FAILURE;
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
    if (config.node_max_font_size < config.node_min_font_size)
        die("node.max_font_size must be greater than or equal to node.min_font_size");
    apply_overrides(&config, &overrides);

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
    if (config.format == FORMAT_JSON || config.format == FORMAT_DOT)
        validate_font_size_data(&nodes, &config, source);

    switch (config.format) {
    case FORMAT_JSON:
        write_json(stdout, &edges, &nodes, &config);
        break;
    case FORMAT_DOT:
        write_dot(stdout, &edges, &nodes, &config);
        break;
    case FORMAT_MD:
    case FORMAT_TEX:
    case FORMAT_HTML:
        write_table(stdout, &edges, &config);
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
