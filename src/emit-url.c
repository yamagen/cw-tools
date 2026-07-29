#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emit-url.h"
#include "emit-util.h"

static void dot_config_error(const char *path, const char *message)
{
    fprintf(stderr, "emit: %s: %s\n", path, message);
    exit(EXIT_FAILURE);
}

static char *read_config_text(const char *path)
{
    FILE *stream = fopen(path, "rb");
    if (stream == NULL) {
        fprintf(stderr, "emit: %s: %s\n", path, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (fseek(stream, 0, SEEK_END) != 0)
        dot_config_error(path, "cannot seek configuration file");
    long end = ftell(stream);
    if (end < 0)
        dot_config_error(path, "cannot determine configuration size");
    if (fseek(stream, 0, SEEK_SET) != 0)
        dot_config_error(path, "cannot rewind configuration file");

    size_t length = (size_t)end;
    char *text = emit_xmalloc(length + 1);
    if (length > 0 && fread(text, 1, length, stream) != length)
        dot_config_error(path, "cannot read configuration file");
    text[length] = '\0';
    if (fclose(stream) != 0)
        dot_config_error(path, "cannot close configuration file");
    return text;
}

static int hex_value(char c)
{
    if ('0' <= c && c <= '9') return c - '0';
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void append_byte(char **buffer, size_t *length, size_t *capacity,
                        unsigned char byte)
{
    if (*length + 2 > *capacity) {
        size_t new_capacity = *capacity == 0 ? 32 : *capacity * 2;
        *buffer = emit_xrealloc(*buffer, new_capacity);
        *capacity = new_capacity;
    }
    (*buffer)[(*length)++] = (char)byte;
}

static void append_codepoint(char **buffer, size_t *length, size_t *capacity,
                             unsigned codepoint)
{
    if (codepoint <= 0x7f) {
        append_byte(buffer, length, capacity, (unsigned char)codepoint);
    } else if (codepoint <= 0x7ff) {
        append_byte(buffer, length, capacity,
                    (unsigned char)(0xc0 | (codepoint >> 6)));
        append_byte(buffer, length, capacity,
                    (unsigned char)(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        append_byte(buffer, length, capacity,
                    (unsigned char)(0xe0 | (codepoint >> 12)));
        append_byte(buffer, length, capacity,
                    (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f)));
        append_byte(buffer, length, capacity,
                    (unsigned char)(0x80 | (codepoint & 0x3f)));
    } else {
        append_byte(buffer, length, capacity,
                    (unsigned char)(0xf0 | (codepoint >> 18)));
        append_byte(buffer, length, capacity,
                    (unsigned char)(0x80 | ((codepoint >> 12) & 0x3f)));
        append_byte(buffer, length, capacity,
                    (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f)));
        append_byte(buffer, length, capacity,
                    (unsigned char)(0x80 | (codepoint & 0x3f)));
    }
}

static unsigned parse_hex4(const char **cursor, const char *path)
{
    unsigned value = 0;
    for (size_t i = 0; i < 4; i++) {
        int digit = hex_value((*cursor)[i]);
        if (digit < 0)
            dot_config_error(path, "invalid Unicode escape in edge URL setting");
        value = (value << 4) | (unsigned)digit;
    }
    *cursor += 4;
    return value;
}

static char *parse_json_string(const char **cursor, const char *path)
{
    const char *p = *cursor;
    if (*p != '"')
        dot_config_error(path, "edge URL setting must be a string or null");
    p++;

    char *result = NULL;
    size_t length = 0;
    size_t capacity = 0;
    while (*p != '\0' && *p != '"') {
        unsigned char c = (unsigned char)*p++;
        if (c == '\\') {
            char escape = *p++;
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
                unsigned codepoint = parse_hex4(&p, path);
                if (0xd800 <= codepoint && codepoint <= 0xdbff) {
                    if (p[0] != '\\' || p[1] != 'u')
                        dot_config_error(path,
                                         "missing low surrogate in edge URL setting");
                    p += 2;
                    unsigned low = parse_hex4(&p, path);
                    if (low < 0xdc00 || low > 0xdfff)
                        dot_config_error(path,
                                         "invalid low surrogate in edge URL setting");
                    codepoint = 0x10000 +
                        ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                } else if (0xdc00 <= codepoint && codepoint <= 0xdfff) {
                    dot_config_error(path,
                                     "unexpected low surrogate in edge URL setting");
                }
                append_codepoint(&result, &length, &capacity, codepoint);
                continue;
            }
            default:
                dot_config_error(path, "invalid escape in edge URL setting");
            }
        }
        append_byte(&result, &length, &capacity, c);
    }
    if (*p != '"')
        dot_config_error(path, "unterminated edge URL setting");
    p++;
    append_byte(&result, &length, &capacity, '\0');
    *cursor = p;
    return result;
}

static char *find_json_object(const char *text, const char *key,
                              const char *path)
{
    size_t key_length = strlen(key);
    char *pattern = emit_xmalloc(key_length + 3);
    snprintf(pattern, key_length + 3, "\"%s\"", key);

    char *result = NULL;
    const char *search = text;
    const char *found;
    while ((found = strstr(search, pattern)) != NULL) {
        const char *p = found + key_length + 2;
        while (isspace((unsigned char)*p)) p++;
        if (*p != ':') {
            search = found + 1;
            continue;
        }
        p++;
        while (isspace((unsigned char)*p)) p++;
        if (*p != '{') {
            search = found + 1;
            continue;
        }

        const char *start = p;
        size_t depth = 0;
        bool in_string = false;
        bool escaped = false;
        do {
            char c = *p++;
            if (c == '\0')
                dot_config_error(path, "unterminated edge configuration object");
            if (in_string) {
                if (escaped) {
                    escaped = false;
                } else if (c == '\\') {
                    escaped = true;
                } else if (c == '"') {
                    in_string = false;
                }
            } else if (c == '"') {
                in_string = true;
            } else if (c == '{') {
                depth++;
            } else if (c == '}') {
                depth--;
            }
        } while (depth > 0);

        size_t length = (size_t)(p - start);
        char *replacement = emit_xmalloc(length + 1);
        memcpy(replacement, start, length);
        replacement[length] = '\0';
        free(result);
        result = replacement;
        search = p;
    }
    free(pattern);
    return result;
}

static char *find_json_string_setting(const char *text, const char *key,
                                      const char *path, bool *seen)
{
    size_t key_length = strlen(key);
    char *pattern = emit_xmalloc(key_length + 3);
    snprintf(pattern, key_length + 3, "\"%s\"", key);

    char *result = NULL;
    const char *search = text;
    const char *found;
    *seen = false;
    while ((found = strstr(search, pattern)) != NULL) {
        const char *p = found + key_length + 2;
        while (isspace((unsigned char)*p)) p++;
        if (*p != ':') {
            search = found + 1;
            continue;
        }
        p++;
        while (isspace((unsigned char)*p)) p++;

        char *replacement;
        if (strncmp(p, "null", 4) == 0) {
            replacement = NULL;
        } else {
            replacement = parse_json_string(&p, path);
        }
        free(result);
        result = replacement;
        *seen = true;
        search = p;
    }
    free(pattern);
    return result;
}

EmitUrlConfig emit_url_config_load(const char *config_path)
{
    EmitUrlConfig result = {0};
    result.parameter = emit_xstrdup("unit_id");
    result.target = emit_xstrdup("_blank");

    if (config_path == NULL)
        return result;

    char *text = read_config_text(config_path);
    char *edge_object = find_json_object(text, "edge", config_path);
    free(text);
    if (edge_object == NULL)
        return result;

    bool seen = false;
    result.base = find_json_string_setting(edge_object, "url_base",
                                           config_path, &seen);

    char *value = find_json_string_setting(edge_object, "url_parameter",
                                           config_path, &seen);
    if (seen) {
        free(result.parameter);
        result.parameter = value;
    }

    value = find_json_string_setting(edge_object, "url_target",
                                     config_path, &seen);
    if (seen) {
        free(result.target);
        result.target = value;
    }
    free(edge_object);

    if (result.base != NULL && result.base[0] != '\0' &&
        (result.parameter == NULL || result.parameter[0] == '\0'))
        dot_config_error(config_path,
                         "edge.url_parameter must be a nonempty string when edge.url_base is set");
    return result;
}

void emit_url_config_free(EmitUrlConfig *config)
{
    free(config->base);
    free(config->parameter);
    free(config->target);
    memset(config, 0, sizeof(*config));
}

static bool url_unreserved(unsigned char c)
{
    return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') ||
           ('0' <= c && c <= '9') || c == '-' || c == '.' ||
           c == '_' || c == '~';
}

static size_t encoded_length(const char *text)
{
    size_t length = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++)
        length += url_unreserved(*p) ? 1 : 3;
    return length;
}

static char *append_encoded(char *out, const char *text)
{
    static const char hex[] = "0123456789ABCDEF";
    for (const unsigned char *p = (const unsigned char *)text; *p != '\0'; p++) {
        if (url_unreserved(*p)) {
            *out++ = (char)*p;
        } else {
            *out++ = '%';
            *out++ = hex[*p >> 4];
            *out++ = hex[*p & 0x0f];
        }
    }
    return out;
}

char *emit_edge_url(const Edge *edge, const EmitUrlConfig *config)
{
    if (config->base == NULL || config->base[0] == '\0' ||
        config->parameter == NULL || edge->unit_count == 0)
        return NULL;

    size_t base_length = strlen(config->base);
    size_t parameter_length = strlen(config->parameter);
    size_t length = base_length + 1;
    for (size_t i = 0; i < edge->unit_count; i++) {
        if (i > 0)
            length++;
        length += parameter_length + 1 + encoded_length(edge->unit_ids[i]);
    }

    char *result = emit_xmalloc(length + 1);
    char *out = result;
    memcpy(out, config->base, base_length);
    out += base_length;
    if (base_length > 0 &&
        (config->base[base_length - 1] == '?' ||
         config->base[base_length - 1] == '&')) {
        /* The base already ends with a query separator. */
    } else if (strchr(config->base, '?') != NULL) {
        *out++ = '&';
    } else {
        *out++ = '?';
    }

    for (size_t i = 0; i < edge->unit_count; i++) {
        if (i > 0)
            *out++ = '&';
        memcpy(out, config->parameter, parameter_length);
        out += parameter_length;
        *out++ = '=';
        out = append_encoded(out, edge->unit_ids[i]);
    }
    *out = '\0';
    return result;
}
