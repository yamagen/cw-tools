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
#define PROG_VERSION "0.2.0"
#define DEFAULT_CONFIG "config/emit-config.json"

typedef enum { FORMAT_JSON, FORMAT_DOT } OutputFormat;

typedef enum {
  EDGE_LABEL_NONE,
  EDGE_LABEL_CTF,
  EDGE_LABEL_CDF,
  EDGE_LABEL_CW,
  EDGE_LABEL_Z
} EdgeLabel;

typedef struct {
  char *source;
  char *target;
  size_t ctf;
  size_t cdf;
  size_t df1;
  double idf1;
  size_t df2;
  double idf2;
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
  char *node_shape;
  size_t node_label_field;
  EdgeLabel edge_label;
  bool tooltip_ctf;
  bool tooltip_cdf;
  bool tooltip_cw;
  bool tooltip_z;
  bool tooltip_unit_ids;
  double edge_penwidth;
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

static void die(const char *message) {
  fprintf(stderr, "%s: %s\n", PROG_NAME, message);
  exit(EXIT_FAILURE);
}

static void die_errno(const char *path) {
  fprintf(stderr, "%s: %s: %s\n", PROG_NAME, path, strerror(errno));
  exit(EXIT_FAILURE);
}

static void die_line(const char *source, size_t line_number,
                     const char *message) {
  fprintf(stderr, "%s: %s:%zu: %s\n", PROG_NAME, source, line_number, message);
  exit(EXIT_FAILURE);
}

static void json_die(const JsonParser *parser, const char *message) {
  fprintf(stderr, "%s: %s:%zu: %s\n", PROG_NAME, parser->path, parser->line,
          message);
  exit(EXIT_FAILURE);
}

static void *xmalloc(size_t size) {
  void *ptr = malloc(size == 0 ? 1 : size);
  if (ptr == NULL)
    die("out of memory");
  return ptr;
}

static void *xrealloc(void *ptr, size_t size) {
  void *new_ptr = realloc(ptr, size == 0 ? 1 : size);
  if (new_ptr == NULL)
    die("out of memory");
  return new_ptr;
}

static char *xstrdup(const char *s) {
  char *copy = strdup(s);
  if (copy == NULL)
    die("out of memory");
  return copy;
}

static void replace_string(char **destination, char *replacement) {
  free(*destination);
  *destination = replacement;
}

static void config_init(Config *config) {
  memset(config, 0, sizeof(*config));
  config->format = FORMAT_DOT;
  config->graph_name = xstrdup("G");
  config->charset = xstrdup("UTF-8");
  config->overlap = false;
  config->outputorder = xstrdup("edgesfirst");
  config->node_shape = xstrdup("plaintext");
  config->node_label_field = 1;
  config->edge_label = EDGE_LABEL_CTF;
  config->tooltip_ctf = true;
  config->tooltip_cdf = true;
  config->tooltip_cw = true;
  config->tooltip_z = true;
  config->tooltip_unit_ids = true;
  config->edge_penwidth = 1.0;
}

static void config_free(Config *config) {
  free(config->graph_name);
  free(config->charset);
  free(config->outputorder);
  free(config->node_shape);
  free(config->config_path);
}

static void edge_vec_push(EdgeVec *vec, Edge edge) {
  if (vec->len == vec->cap) {
    size_t new_cap = vec->cap == 0 ? 256 : vec->cap * 2;
    vec->items = xrealloc(vec->items, new_cap * sizeof(*vec->items));
    vec->cap = new_cap;
  }
  vec->items[vec->len++] = edge;
}

static void node_vec_push(NodeVec *vec, NodeRef node) {
  if (vec->len == vec->cap) {
    size_t new_cap = vec->cap == 0 ? 512 : vec->cap * 2;
    vec->items = xrealloc(vec->items, new_cap * sizeof(*vec->items));
    vec->cap = new_cap;
  }
  vec->items[vec->len++] = node;
}

static void edge_vec_free(EdgeVec *vec) {
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

static int compare_node_ref(const void *left, const void *right) {
  const NodeRef *a = left;
  const NodeRef *b = right;
  return strcmp(a->id, b->id);
}

static void node_vec_sort_unique(NodeVec *vec, const char *source) {
  if (vec->len == 0)
    return;

  qsort(vec->items, vec->len, sizeof(*vec->items), compare_node_ref);

  size_t out = 1;
  for (size_t i = 1; i < vec->len; i++) {
    NodeRef *previous = &vec->items[out - 1];
    NodeRef *current = &vec->items[i];

    if (strcmp(previous->id, current->id) == 0) {
      if (previous->df != current->df || previous->idf != current->idf) {
        fprintf(stderr, "%s: %s: inconsistent df/idf values for token '%s'\n",
                PROG_NAME, source, current->id);
        exit(EXIT_FAILURE);
      }
      continue;
    }

    vec->items[out++] = *current;
  }
  vec->len = out;
}

static size_t split_tabs(char *line, char ***fields_out) {
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
                         size_t line_number, const char *field_name) {
  char *end = NULL;

  if (text[0] == '\0' || text[0] == '-') {
    char message[160];
    snprintf(message, sizeof(message), "invalid %s value '%s'", field_name,
             text);
    die_line(source, line_number, message);
  }

  errno = 0;
  unsigned long long value = strtoull(text, &end, 10);

  if (errno != 0 || end == text || *end != '\0' ||
      value > (unsigned long long)SIZE_MAX) {
    char message[160];
    snprintf(message, sizeof(message), "invalid %s value '%s'", field_name,
             text);
    die_line(source, line_number, message);
  }

  return (size_t)value;
}

static double parse_double(const char *text, const char *source,
                           size_t line_number, const char *field_name) {
  char *end = NULL;
  errno = 0;
  double value = strtod(text, &end);

  if (errno != 0 || end == text || *end != '\0' || !isfinite(value)) {
    char message[160];
    snprintf(message, sizeof(message), "invalid %s value '%s'", field_name,
             text);
    die_line(source, line_number, message);
  }

  return value;
}

static double parse_option_double(const char *text, const char *option_name) {
  char *end = NULL;
  errno = 0;
  double value = strtod(text, &end);

  if (errno != 0 || end == text || *end != '\0' || !isfinite(value)) {
    fprintf(stderr, "%s: %s requires a finite number: %s\n", PROG_NAME,
            option_name, text);
    exit(EXIT_FAILURE);
  }

  return value;
}

static OutputFormat parse_format_name(const char *text, const char *where) {
  if (strcmp(text, "json") == 0)
    return FORMAT_JSON;
  if (strcmp(text, "dot") == 0)
    return FORMAT_DOT;

  fprintf(stderr, "%s: %s: format must be 'json' or 'dot': %s\n", PROG_NAME,
          where, text);
  exit(EXIT_FAILURE);
}

static EdgeLabel parse_edge_label_name(const char *text,
                                       const JsonParser *parser) {
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

static char *read_file(const char *path, size_t *length_out) {
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

static void jp_skip_ws(JsonParser *parser) {
  while (parser->pos < parser->length) {
    unsigned char c = (unsigned char)parser->text[parser->pos];
    if (!isspace(c))
      break;
    if (c == '\n')
      parser->line++;
    parser->pos++;
  }
}

static char jp_peek(JsonParser *parser) {
  jp_skip_ws(parser);
  if (parser->pos >= parser->length)
    return '\0';
  return parser->text[parser->pos];
}

static void jp_expect(JsonParser *parser, char expected) {
  jp_skip_ws(parser);
  if (parser->pos >= parser->length || parser->text[parser->pos] != expected) {
    char message[96];
    snprintf(message, sizeof(message), "expected '%c'", expected);
    json_die(parser, message);
  }
  parser->pos++;
}

static int hex_value(char c) {
  if ('0' <= c && c <= '9')
    return c - '0';
  if ('a' <= c && c <= 'f')
    return c - 'a' + 10;
  if ('A' <= c && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

static void append_utf8(char **buffer, size_t *length, size_t *capacity,
                        unsigned codepoint) {
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

static unsigned jp_parse_hex4(JsonParser *parser) {
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

static char *jp_parse_string(JsonParser *parser) {
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
      case '"':
        c = '"';
        break;
      case '\\':
        c = '\\';
        break;
      case '/':
        c = '/';
        break;
      case 'b':
        c = '\b';
        break;
      case 'f':
        c = '\f';
        break;
      case 'n':
        c = '\n';
        break;
      case 'r':
        c = '\r';
        break;
      case 't':
        c = '\t';
        break;
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
          codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
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

static bool jp_consume_literal(JsonParser *parser, const char *literal) {
  jp_skip_ws(parser);
  size_t length = strlen(literal);
  if (parser->length - parser->pos < length ||
      strncmp(parser->text + parser->pos, literal, length) != 0)
    return false;
  parser->pos += length;
  return true;
}

static double jp_parse_number(JsonParser *parser) {
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

static bool jp_parse_bool(JsonParser *parser) {
  if (jp_consume_literal(parser, "true"))
    return true;
  if (jp_consume_literal(parser, "false"))
    return false;
  json_die(parser, "expected true or false");
  return false;
}

static void jp_skip_value(JsonParser *parser);

static void jp_skip_array(JsonParser *parser) {
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

static void jp_skip_object(JsonParser *parser) {
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

static void jp_skip_value(JsonParser *parser) {
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

static void parse_nullable_filter(JsonParser *parser, bool *present,
                                  double *value) {
  if (jp_consume_literal(parser, "null")) {
    *present = false;
    *value = 0.0;
    return;
  }
  *value = jp_parse_number(parser);
  *present = true;
}

static void parse_filters(JsonParser *parser, Config *config) {
  jp_expect(parser, '{');
  if (jp_peek(parser) == '}') {
    parser->pos++;
    return;
  }

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

    char c = jp_peek(parser);
    if (c == '}') {
      parser->pos++;
      return;
    }
    jp_expect(parser, ',');
  }
}

static void parse_dot(JsonParser *parser, Config *config) {
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

static void parse_node(JsonParser *parser, Config *config) {
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
    else if (strcmp(key, "label_field") == 0) {
      double value = jp_parse_number(parser);
      if (value < 1.0 || value > 4.0 || value != (double)(size_t)value)
        json_die(parser, "node.label_field must be an integer from 1 to 4");
      config->node_label_field = (size_t)value;
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
                              JsonParser *parser) {
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

static void parse_tooltip(JsonParser *parser, Config *config) {
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

static void parse_edge(JsonParser *parser, Config *config) {
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

static void load_config(const char *path, Config *config) {
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

  free(config->config_path);
  config->config_path = xstrdup(path);
  free(text);
}

static bool edge_is_selected(const Edge *edge, const Filters *filters) {
  if (filters->have_min_cw && edge->cw < filters->min_cw)
    return false;
  if (filters->have_min_z && edge->z < filters->min_z)
    return false;
  return true;
}

static void read_edges(FILE *stream, const char *source, const Filters *filters,
                       EdgeVec *edges, NodeVec *nodes) {
  char *line = NULL;
  size_t line_capacity = 0;
  size_t line_number = 0;
  ssize_t length;

  while ((length = getline(&line, &line_capacity, stream)) != -1) {
    line_number++;

    while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r'))
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
    edge.df1 = parse_size(fields[4], source, line_number, "df1");
    edge.idf1 = parse_double(fields[5], source, line_number, "idf1");
    edge.df2 = parse_size(fields[6], source, line_number, "df2");
    edge.idf2 = parse_double(fields[7], source, line_number, "idf2");
    edge.cw = parse_double(fields[8], source, line_number, "cw");
    edge.z = parse_double(fields[9], source, line_number, "z");
    edge.unit_count = field_count - 10;

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
               "ctf is %zu but %zu unit_id fields were supplied", edge.ctf,
               edge.unit_count);
      free(fields);
      free(edge.source);
      free(edge.target);
      die_line(source, line_number, message);
    }

    if (edge.unit_count > 0) {
      edge.unit_ids = xmalloc(edge.unit_count * sizeof(*edge.unit_ids));
      for (size_t i = 0; i < edge.unit_count; i++) {
        if (fields[10 + i][0] == '\0') {
          for (size_t j = 0; j < i; j++)
            free(edge.unit_ids[j]);
          free(edge.unit_ids);
          free(fields);
          free(edge.source);
          free(edge.target);
          die_line(source, line_number, "empty unit_id field");
        }
        edge.unit_ids[i] = xstrdup(fields[10 + i]);
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
    node_vec_push(nodes, (NodeRef){edge.source, edge.df1, edge.idf1});
    node_vec_push(nodes, (NodeRef){edge.target, edge.df2, edge.idf2});
  }

  if (ferror(stream)) {
    free(line);
    die_errno(source);
  }

  free(line);
}

static void json_write_string_n(FILE *stream, const char *text, size_t length) {
  static const char hex[] = "0123456789abcdef";

  fputc('"', stream);
  for (size_t i = 0; i < length; i++) {
    unsigned char c = (unsigned char)text[i];
    switch (c) {
    case '"':
      fputs("\\\"", stream);
      break;
    case '\\':
      fputs("\\\\", stream);
      break;
    case '\b':
      fputs("\\b", stream);
      break;
    case '\f':
      fputs("\\f", stream);
      break;
    case '\n':
      fputs("\\n", stream);
      break;
    case '\r':
      fputs("\\r", stream);
      break;
    case '\t':
      fputs("\\t", stream);
      break;
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

static void json_write_string(FILE *stream, const char *text) {
  json_write_string_n(stream, text, strlen(text));
}

static void json_write_token_fields(FILE *stream, const char *token) {
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

static void json_write_optional_number(FILE *stream, bool present,
                                       double value) {
  if (present)
    fprintf(stream, "%.17g", value);
  else
    fputs("null", stream);
}

static const char *format_name(OutputFormat format) {
  return format == FORMAT_DOT ? "dot" : "json";
}

static void write_json(FILE *stream, const EdgeVec *edges, const NodeVec *nodes,
                       const Config *config) {
  fputs("{\n", stream);
  fputs("  \"format\": \"cw-tools/graph\",\n", stream);
  fputs("  \"version\": 1,\n", stream);
  fputs("  \"emit\": {\"config\": ", stream);
  json_write_string(stream, config->config_path);
  fputs(", \"output_format\": ", stream);
  json_write_string(stream, format_name(config->format));
  fputs("},\n", stream);
  fputs("  \"filters\": {\n", stream);
  fputs("    \"min_cw\": ", stream);
  json_write_optional_number(stream, config->filters.have_min_cw,
                             config->filters.min_cw);
  fputs(",\n", stream);
  fputs("    \"min_z\": ", stream);
  json_write_optional_number(stream, config->filters.have_min_z,
                             config->filters.min_z);
  fputs("\n  },\n", stream);
  fprintf(stream, "  \"counts\": {\"nodes\": %zu, \"edges\": %zu},\n",
          nodes->len, edges->len);

  fputs("  \"nodes\": [\n", stream);
  for (size_t i = 0; i < nodes->len; i++) {
    const NodeRef *node = &nodes->items[i];
    fputs("    {\"id\": ", stream);
    json_write_string(stream, node->id);
    fputs(", \"fields\": ", stream);
    json_write_token_fields(stream, node->id);
    fprintf(stream, ", \"df\": %zu, \"idf\": %.12g}", node->df, node->idf);
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
            ", \"ctf\": %zu, \"cdf\": %zu, \"cw\": %.17g, \"z\": %.17g, "
            "\"unit_ids\": [",
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

static void dot_write_escaped_n(FILE *stream, const char *text, size_t length) {
  for (size_t i = 0; i < length; i++) {
    unsigned char c = (unsigned char)text[i];
    switch (c) {
    case '"':
      fputs("\\\"", stream);
      break;
    case '\\':
      fputs("\\\\", stream);
      break;
    case '\n':
      fputs("\\n", stream);
      break;
    case '\r':
      break;
    case '\t':
      fputc(' ', stream);
      break;
    default:
      if (c >= 0x20)
        fputc(c, stream);
      break;
    }
  }
}

static void dot_write_string_n(FILE *stream, const char *text, size_t length) {
  fputc('"', stream);
  dot_write_escaped_n(stream, text, length);
  fputc('"', stream);
}

static void dot_write_string(FILE *stream, const char *text) {
  dot_write_string_n(stream, text, strlen(text));
}

static void token_field_slice(const char *token, size_t requested,
                              const char **start_out, size_t *length_out) {
  const char *field_start = token;
  size_t field = 1;
  const char *first_end = strchr(token, '/');
  if (first_end == NULL)
    first_end = token + strlen(token);

  for (const char *p = token;; p++) {
    if (*p == '/' || *p == '\0') {
      if (field == requested && p > field_start) {
        *start_out = field_start;
        *length_out = (size_t)(p - field_start);
        return;
      }
      if (*p == '\0')
        break;
      field++;
      field_start = p + 1;
    }
  }

  *start_out = token;
  *length_out = (size_t)(first_end - token);
}

static void dot_write_unit_ids(FILE *stream, const Edge *edge) {
  fputc('"', stream);
  for (size_t i = 0; i < edge->unit_count; i++) {
    if (i > 0)
      fputc(',', stream);
    dot_write_escaped_n(stream, edge->unit_ids[i], strlen(edge->unit_ids[i]));
  }
  fputc('"', stream);
}

static bool tooltip_enabled(const Config *config) {
  return config->tooltip_ctf || config->tooltip_cdf || config->tooltip_cw ||
         config->tooltip_z || config->tooltip_unit_ids;
}

static void dot_write_tooltip(FILE *stream, const Edge *edge,
                              const Config *config) {
  fputc('"', stream);
  bool first = true;
#define TOOLTIP_SEP()                                                          \
  do {                                                                         \
    if (!first)                                                                \
      fputs("; ", stream);                                                     \
    first = false;                                                             \
  } while (0)
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
      dot_write_escaped_n(stream, edge->unit_ids[i], strlen(edge->unit_ids[i]));
    }
  }
#undef TOOLTIP_SEP
  fputc('"', stream);
}

static void dot_write_edge_label(FILE *stream, const Edge *edge,
                                 EdgeLabel label) {
  fputc('"', stream);
  switch (label) {
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
  case EDGE_LABEL_NONE:
    break;
  }
  fputc('"', stream);
}

static void write_dot(FILE *stream, const EdgeVec *edges, const NodeVec *nodes,
                      const Config *config) {
  fputs(config->directed ? "digraph " : "graph ", stream);
  dot_write_string(stream, config->graph_name);
  fputs(" {\n", stream);

  fputs("  graph [charset=", stream);
  dot_write_string(stream, config->charset);
  fprintf(stream,
          ", overlap=%s, outputorder=", config->overlap ? "true" : "false");
  dot_write_string(stream, config->outputorder);
  fputs("];\n", stream);

  fputs("  node [shape=", stream);
  dot_write_string(stream, config->node_shape);
  fputs("];\n", stream);
  fprintf(stream, "  edge [penwidth=%.17g];\n\n", config->edge_penwidth);

  for (size_t i = 0; i < nodes->len; i++) {
    const NodeRef *node = &nodes->items[i];
    const char *label_start = NULL;
    size_t label_length = 0;
    token_field_slice(node->id, config->node_label_field, &label_start,
                      &label_length);

    fputs("  ", stream);
    dot_write_string(stream, node->id);
    fputs(" [label=", stream);
    dot_write_string_n(stream, label_start, label_length);
    fprintf(stream, ", df=%zu, idf=%.12g, tooltip=", node->df, node->idf);
    fputc('"', stream);
    fprintf(stream, "df=%zu; idf=%.12g", node->df, node->idf);
    fputs("\"];\n", stream);
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
    fprintf(stream, "ctf=%zu, cdf=%zu, cw=%.17g, z=%.17g, unit_ids=", edge->ctf,
            edge->cdf, edge->cw, edge->z);
    dot_write_unit_ids(stream, edge);
    if (tooltip_enabled(config)) {
      fputs(", tooltip=", stream);
      dot_write_tooltip(stream, edge, config);
    }
    fputs("];\n", stream);
  }

  fputs("}\n", stream);
}

static void apply_overrides(Config *config, const CliOverrides *overrides) {
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

static void print_help(FILE *stream) {
  fprintf(stream,
          "Usage: %s [OPTION]... [FILE]\n"
          "\n"
          "Emit JSON or Graphviz DOT from cw output.\n"
          "Normal behavior is read from %s; command-line options override\n"
          "only the values explicitly supplied.\n"
          "\n"
          "Input:\n"
          "  token1 token2 ctf cdf df1 idf1 df2 idf2 cw z unit_id...\n"
          "\n"
          "Configuration:\n"
          "  -c, --config FILE     read FILE instead of %s\n"
          "\n"
          "Temporary overrides:\n"
          "  -T, --format FORMAT   output format: json or dot\n"
          "  -W, --min-cw VALUE    retain edges whose CW is at least VALUE\n"
          "  -Z, --min-z VALUE     retain edges whose Z is at least VALUE\n"
          "\n"
          "Other options:\n"
          "  -h, --help             display this help and exit\n"
          "  -v, --version          display version information and exit\n"
          "\n"
          "Thresholds affect only emitted edges and incident nodes.\n"
          "IDF, CW, and Z are never recalculated by emit.\n",
          PROG_NAME, DEFAULT_CONFIG, DEFAULT_CONFIG);
}

int main(int argc, char **argv) {
  static const struct option long_options[] = {
      {"config", required_argument, NULL, 'c'},
      {"format", required_argument, NULL, 'T'},
      {"min-cw", required_argument, NULL, 'W'},
      {"min-z", required_argument, NULL, 'Z'},
      {"help", no_argument, NULL, 'h'},
      {"version", no_argument, NULL, 'v'},
      {NULL, 0, NULL, 0}};

  CliOverrides overrides = {0};
  overrides.config_path = DEFAULT_CONFIG;

  int option;
  (void)setlocale(LC_ALL, "");

  while ((option = getopt_long(argc, argv, "c:T:W:Z:hv", long_options, NULL)) !=
         -1) {
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
    fprintf(stderr, "%s: at most one input file may be specified\n", PROG_NAME);
    return EXIT_FAILURE;
  }

  Config config;
  config_init(&config);
  load_config(overrides.config_path, &config);
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
  if (config.format == FORMAT_JSON)
    write_json(stdout, &edges, &nodes, &config);
  else
    write_dot(stdout, &edges, &nodes, &config);

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
