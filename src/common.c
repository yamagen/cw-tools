#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAIR_SEP '\x1f'

struct uid_array {
    const char **item;
    size_t len;
    size_t cap;
};

struct variant {
    char *token;
    size_t df;
    const char *last_unit;
    struct variant *next;
};

struct frequency_array {
    const char **unit;
    size_t *count;
    size_t len;
    size_t cap;
};

struct node {
    char *key;
    size_t count;
    size_t df;
    const char *last_unit;
    struct uid_array uid;
    struct variant *variants;
    size_t fq;
    int fq_state; /* 0: unknown, 1: available, -1: unavailable */
    struct frequency_array frequencies;
    struct node *next;
};

struct table {
    struct node **bucket;
    size_t size;
    size_t len;
};

static const char *progname = "cw-tools";

static void die(const char *message)
{
    fprintf(stderr, "%s: %s\n", progname, message);
    exit(EXIT_FAILURE);
}

static void die_errno(const char *path)
{
    fprintf(stderr, "%s: %s: %s\n", progname, path, strerror(errno));
    exit(EXIT_FAILURE);
}

static void *xmalloc(size_t size)
{
    void *p = malloc(size);
    if (p == NULL)
        die("out of memory");
    return p;
}

static void *xcalloc(size_t n, size_t size)
{
    void *p = calloc(n, size);
    if (p == NULL)
        die("out of memory");
    return p;
}

static void *xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);
    if (p == NULL)
        die("out of memory");
    return p;
}

static char *xstrdup(const char *s)
{
    char *p = strdup(s);
    if (p == NULL)
        die("out of memory");
    return p;
}

static size_t hash_string(const char *s)
{
    size_t h = 1469598103934665603ULL;

    while (*s != '\0') {
        h ^= (unsigned char)*s++;
        h *= 1099511628211ULL;
    }
    return h;
}

static void table_grow(struct table *table)
{
    size_t new_size = table->size == 0 ? 1024 : table->size * 2;
    struct node **new_bucket = xcalloc(new_size, sizeof(*new_bucket));

    for (size_t i = 0; i < table->size; i++) {
        struct node *node = table->bucket[i];

        while (node != NULL) {
            struct node *next = node->next;
            size_t h = hash_string(node->key) % new_size;

            node->next = new_bucket[h];
            new_bucket[h] = node;
            node = next;
        }
    }

    free(table->bucket);
    table->bucket = new_bucket;
    table->size = new_size;
}

static struct node *table_get(struct table *table, const char *key,
                              bool create)
{
    if (table->size == 0)
        table_grow(table);

    size_t h = hash_string(key) % table->size;

    for (struct node *node = table->bucket[h];
         node != NULL;
         node = node->next) {
        if (strcmp(node->key, key) == 0)
            return node;
    }

    if (!create)
        return NULL;

    struct node *node = xcalloc(1, sizeof(*node));
    node->key = xstrdup(key);
    node->next = table->bucket[h];
    table->bucket[h] = node;
    table->len++;

    if (table->len > table->size * 3 / 4)
        table_grow(table);

    return node;
}

static void table_free(struct table *table)
{
    for (size_t i = 0; i < table->size; i++) {
        struct node *node = table->bucket[i];

        while (node != NULL) {
            struct node *next = node->next;
            struct variant *variant = node->variants;
            while (variant != NULL) {
                struct variant *variant_next = variant->next;
                free(variant->token);
                free(variant);
                variant = variant_next;
            }
            free(node->uid.item);
            free(node->frequencies.unit);
            free(node->frequencies.count);
            free(node->key);
            free(node);
            node = next;
        }
    }

    free(table->bucket);
    memset(table, 0, sizeof(*table));
}

static void uid_append(struct uid_array *uid, const char *unit)
{
    if (uid->len == uid->cap) {
        size_t new_cap = uid->cap == 0 ? 8 : uid->cap * 2;
        uid->item = xrealloc(uid->item, new_cap * sizeof(*uid->item));
        uid->cap = new_cap;
    }
    uid->item[uid->len++] = unit;
}

static void frequency_add(struct node *node, const char *unit, size_t count)
{
    struct frequency_array *frequencies = &node->frequencies;

    if (frequencies->len > 0 &&
        strcmp(frequencies->unit[frequencies->len - 1], unit) == 0) {
        frequencies->count[frequencies->len - 1] += count;
    } else {
        if (frequencies->len == frequencies->cap) {
            size_t new_cap = frequencies->cap == 0 ? 8 : frequencies->cap * 2;
            frequencies->unit = xrealloc(
                frequencies->unit, new_cap * sizeof(*frequencies->unit));
            frequencies->count = xrealloc(
                frequencies->count, new_cap * sizeof(*frequencies->count));
            frequencies->cap = new_cap;
        }
        frequencies->unit[frequencies->len] = unit;
        frequencies->count[frequencies->len] = count;
        frequencies->len++;
    }
    node->fq += count;
}

static void record_term_frequency(struct node *node, const char *unit,
                                  size_t count, bool available)
{
    if (!available) {
        node->fq_state = -1;
        return;
    }
    if (node->fq_state < 0)
        return;
    node->fq_state = 1;
    frequency_add(node, unit, count);
}

static void touch(struct node *node, const char *unit, bool keep_uid,
                  bool unique_per_unit)
{
    bool same_unit =
        node->last_unit != NULL && strcmp(node->last_unit, unit) == 0;

    if (unique_per_unit && same_unit)
        return;

    node->count++;

    if (!same_unit) {
        node->df++;
        node->last_unit = unit;
    }

    if (keep_uid)
        uid_append(&node->uid, unit);
}

char *cwt_token_project(const char *token, unsigned field_mask)
{
    CwtTokenFields fields;
    size_t length = 0;
    size_t selected = 0;

    cwt_token_parse(token, &fields);
    field_mask &= CWT_TOKEN_FIELD_ALL;

    for (size_t i = 0; i < fields.count; i++) {
        if ((field_mask & CWT_TOKEN_FIELD_BIT(i + 1)) == 0)
            continue;
        if (selected > 0)
            length++;
        length += fields.field[i].len;
        selected++;
    }

    /* A one-field token remains usable under defaults such as 2,3,4. */
    if (selected == 0)
        return xstrdup(token);

    char *projected = xmalloc(length + 1);
    char *out = projected;
    selected = 0;

    for (size_t i = 0; i < fields.count; i++) {
        if ((field_mask & CWT_TOKEN_FIELD_BIT(i + 1)) == 0)
            continue;
        if (selected > 0)
            *out++ = '/';
        memcpy(out, fields.field[i].ptr, fields.field[i].len);
        out += fields.field[i].len;
        selected++;
    }
    *out = '\0';
    return projected;
}

static bool record_variant(struct node *term, const char *token,
                           const char *unit)
{
    struct variant *variant;

    for (variant = term->variants; variant != NULL; variant = variant->next) {
        if (strcmp(variant->token, token) == 0)
            break;
    }

    if (variant == NULL) {
        variant = xcalloc(1, sizeof(*variant));
        variant->token = xstrdup(token);
        variant->next = term->variants;
        term->variants = variant;
    }

    if (variant->last_unit == NULL || strcmp(variant->last_unit, unit) != 0) {
        variant->df++;
        variant->last_unit = unit;
        return true;
    }
    return false;
}

static const char *representative_token(const struct node *term)
{
    const struct variant *best = NULL;

    for (const struct variant *variant = term->variants;
         variant != NULL; variant = variant->next) {
        if (best == NULL || variant->df > best->df ||
            (variant->df == best->df &&
             strcmp(variant->token, best->token) < 0))
            best = variant;
    }

    return best != NULL ? best->token : term->key;
}

static char *make_pair_key(const char *left, const char *right)
{
    size_t left_len = strlen(left);
    size_t right_len = strlen(right);
    char *key = xmalloc(left_len + right_len + 2);

    memcpy(key, left, left_len);
    key[left_len] = PAIR_SEP;
    memcpy(key + left_len + 1, right, right_len + 1);
    return key;
}

static size_t parse_positive_count(const char *text, const char *source,
                                   size_t line_number)
{
    char *end = NULL;
    errno = 0;
    unsigned long long value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value == 0 ||
        value > (unsigned long long)SIZE_MAX) {
        fprintf(stderr, "%s: %s:%zu: invalid token frequency '%s'\n",
                progname, source, line_number, text);
        exit(EXIT_FAILURE);
    }
    return (size_t)value;
}

static void process_line(char *line, size_t line_number, const char *source,
                         struct table *units, struct table *terms,
                         struct table *pairs,
                         const CwtStatsReadOptions *options)
{
    char *saveptr = NULL;
    char *unit = strtok_r(line, " \t\r\n", &saveptr);

    if (unit == NULL || unit[0] == '#')
        return;

    char *left_input = strtok_r(NULL, " \t\r\n", &saveptr);
    char *right_input = strtok_r(NULL, " \t\r\n", &saveptr);
    char *left_count_text = strtok_r(NULL, " \t\r\n", &saveptr);
    char *right_count_text = strtok_r(NULL, " \t\r\n", &saveptr);
    char *extra = strtok_r(NULL, " \t\r\n", &saveptr);

    bool counts_available = left_count_text != NULL || right_count_text != NULL;
    if (left_input == NULL || right_input == NULL || extra != NULL ||
        (counts_available &&
         (left_count_text == NULL || right_count_text == NULL))) {
        fprintf(stderr,
                "%s: %s:%zu: expected: unit_id token1 token2 [fq1 fq2]\n",
                progname, source, line_number);
        exit(EXIT_FAILURE);
    }

    size_t left_count = 0;
    size_t right_count = 0;
    if (counts_available) {
        left_count = parse_positive_count(left_count_text, source, line_number);
        right_count = parse_positive_count(right_count_text, source, line_number);
    }

    unsigned pattern_fields = CWT_TOKEN_FIELD_ALL;
    bool unordered_pairs = false;
    bool unique_pairs_per_unit = false;
    bool drop_self_pairs = false;

    if (options != NULL) {
        pattern_fields = options->pattern_fields & CWT_TOKEN_FIELD_ALL;
        unordered_pairs = options->unordered_pairs != 0;
        unique_pairs_per_unit = options->unique_pairs_per_unit != 0;
        drop_self_pairs = options->drop_self_pairs != 0;
    }
    if (pattern_fields == 0)
        pattern_fields = CWT_TOKEN_FIELD_ALL;

    char *left_key = cwt_token_project(left_input, pattern_fields);
    char *right_key = cwt_token_project(right_input, pattern_fields);

    struct node *unit_node = table_get(units, unit, true);
    const char *unit_key = unit_node->key;

    struct node *left_term = table_get(terms, left_key, true);
    struct node *right_term = table_get(terms, right_key, true);
    touch(left_term, unit_key, false, false);
    touch(right_term, unit_key, false, false);

    if (record_variant(left_term, left_input, unit_key))
        record_term_frequency(left_term, unit_key, left_count,
                              counts_available);
    if (record_variant(right_term, right_input, unit_key))
        record_term_frequency(right_term, unit_key, right_count,
                              counts_available);

    if (unordered_pairs && strcmp(left_key, right_key) > 0) {
        char *tmp = left_key;
        left_key = right_key;
        right_key = tmp;
    }

    if (!drop_self_pairs || strcmp(left_key, right_key) != 0) {
        char *pair_key = make_pair_key(left_key, right_key);
        touch(table_get(pairs, pair_key, true), unit_key, true,
              unique_pairs_per_unit);
        free(pair_key);
    }

    free(left_key);
    free(right_key);
}

static void process_stream(FILE *stream, const char *source,
                           struct table *units, struct table *terms,
                           struct table *pairs,
                           const CwtStatsReadOptions *options)
{
    char *line = NULL;
    size_t capacity = 0;
    size_t line_number = 0;

    while (getline(&line, &capacity, stream) != -1) {
        line_number++;
        process_line(line, line_number, source, units, terms, pairs, options);
    }

    if (ferror(stream)) {
        free(line);
        die_errno(source);
    }
    free(line);
}

static int compare_node_ptr(const void *a, const void *b)
{
    const struct node *left = *(const struct node *const *)a;
    const struct node *right = *(const struct node *const *)b;
    return strcmp(left->key, right->key);
}

static void copy_tokens(CwtCorpusStats *stats, struct table *terms,
                        size_t num_units)
{
    stats->tokens = xcalloc(terms->len, sizeof(*stats->tokens));

    for (size_t i = 0; i < terms->size; i++) {
        for (struct node *node = terms->bucket[i];
             node != NULL;
             node = node->next) {
            CwtTokenStat *token = &stats->tokens[stats->token_count++];
            token->pattern = xstrdup(node->key);
            token->token = xstrdup(representative_token(node));
            token->df = node->df;
            token->idf = log((double)num_units / (double)node->df);
            token->fq_available = node->fq_state == 1;
            if (token->fq_available) {
                token->fq = node->fq;
                token->fq_unit_ids.items = xcalloc(
                    node->frequencies.len,
                    sizeof(*token->fq_unit_ids.items));
                token->fq_unit_counts = xcalloc(
                    node->frequencies.len,
                    sizeof(*token->fq_unit_counts));
                token->fq_unit_ids.len = node->frequencies.len;
                token->fq_unit_ids.cap = node->frequencies.len;
                for (size_t j = 0; j < node->frequencies.len; j++) {
                    token->fq_unit_ids.items[j] =
                        xstrdup(node->frequencies.unit[j]);
                    token->fq_unit_counts[j] = node->frequencies.count[j];
                }
            }
        }
    }
}

static void copy_pair_uids(CwtStringList *dest, const struct uid_array *src)
{
    if (src->len == 0)
        return;

    dest->items = xcalloc(src->len, sizeof(*dest->items));
    dest->len = src->len;
    dest->cap = src->len;

    for (size_t i = 0; i < src->len; i++)
        dest->items[i] = xstrdup(src->item[i]);
}

static void copy_pairs(CwtCorpusStats *stats, struct table *pairs,
                       struct table *terms, size_t num_units)
{
    if (pairs->len == 0)
        return;

    struct node **array = xmalloc(pairs->len * sizeof(*array));
    size_t k = 0;

    for (size_t i = 0; i < pairs->size; i++) {
        for (struct node *node = pairs->bucket[i];
             node != NULL;
             node = node->next) {
            array[k++] = node;
        }
    }
    qsort(array, pairs->len, sizeof(*array), compare_node_ptr);

    stats->pairs = xcalloc(pairs->len, sizeof(*stats->pairs));

    for (size_t i = 0; i < pairs->len; i++) {
        struct node *pair_node = array[i];
        char *sep = strchr(pair_node->key, PAIR_SEP);

        if (sep == NULL)
            die("internal pair-key error");

        *sep = '\0';
        const char *left = pair_node->key;
        const char *right = sep + 1;
        struct node *left_term = table_get(terms, left, false);
        struct node *right_term = table_get(terms, right, false);

        if (left_term == NULL || right_term == NULL)
            die("internal term-table error");

        CwtPairStat *pair = &stats->pairs[stats->pair_count++];
        pair->pattern1 = xstrdup(left);
        pair->pattern2 = xstrdup(right);
        pair->token1 = xstrdup(representative_token(left_term));
        pair->token2 = xstrdup(representative_token(right_term));
        pair->ctf = pair_node->count;
        pair->cdf = pair_node->df;
        pair->df1 = left_term->df;
        pair->idf1 = log((double)num_units / (double)left_term->df);
        pair->df2 = right_term->df;
        pair->idf2 = log((double)num_units / (double)right_term->df);
        copy_pair_uids(&pair->unit_ids, &pair_node->uid);

        *sep = PAIR_SEP;
    }

    free(array);
}

void cwt_stats_init(CwtCorpusStats *stats)
{
    memset(stats, 0, sizeof(*stats));
}

int cwt_stats_read_with_options(FILE *stream, CwtCorpusStats *stats,
                                const char *program_name,
                                const CwtStatsReadOptions *options)
{
    struct table units = {0};
    struct table terms = {0};
    struct table pairs = {0};

    progname = program_name != NULL ? program_name : "cw-tools";
    process_stream(stream, "-", &units, &terms, &pairs, options);

    stats->unit_count = units.len;
    if (units.len != 0) {
        copy_tokens(stats, &terms, units.len);
        copy_pairs(stats, &pairs, &terms, units.len);
    }

    table_free(&pairs);
    table_free(&terms);
    table_free(&units);
    return 0;
}

int cwt_stats_read(FILE *stream, CwtCorpusStats *stats,
                   const char *program_name)
{
    return cwt_stats_read_with_options(stream, stats, program_name, NULL);
}

void cwt_stats_free(CwtCorpusStats *stats)
{
    for (size_t i = 0; i < stats->token_count; i++) {
        free(stats->tokens[i].pattern);
        free(stats->tokens[i].token);
        for (size_t j = 0; j < stats->tokens[i].fq_unit_ids.len; j++)
            free(stats->tokens[i].fq_unit_ids.items[j]);
        free(stats->tokens[i].fq_unit_ids.items);
        free(stats->tokens[i].fq_unit_counts);
    }
    free(stats->tokens);

    for (size_t i = 0; i < stats->pair_count; i++) {
        CwtPairStat *pair = &stats->pairs[i];
        free(pair->pattern1);
        free(pair->pattern2);
        free(pair->token1);
        free(pair->token2);
        for (size_t j = 0; j < pair->unit_ids.len; j++)
            free(pair->unit_ids.items[j]);
        free(pair->unit_ids.items);
    }
    free(stats->pairs);
    cwt_stats_init(stats);
}

const CwtTokenStat *cwt_stats_find_pattern(const CwtCorpusStats *stats,
                                           const char *pattern)
{
    for (size_t i = 0; i < stats->token_count; i++) {
        if (strcmp(stats->tokens[i].pattern, pattern) == 0)
            return &stats->tokens[i];
    }
    return NULL;
}

void cwt_token_parse(const char *token, CwtTokenFields *fields)
{
    const char *start = token;
    const char *p = token;
    size_t n = 0;

    memset(fields, 0, sizeof(*fields));

    while (n + 1 < CWT_TOKEN_FIELD_MAX && *p != '\0') {
        if (*p == '/') {
            fields->field[n].ptr = start;
            fields->field[n].len = (size_t)(p - start);
            n++;
            start = p + 1;
        }
        p++;
    }

    fields->field[n].ptr = start;
    fields->field[n].len = strlen(start);
    fields->count = n + 1;
}

int cwt_token_field_equals(const char *token, size_t field_number,
                           const char *value)
{
    CwtTokenFields fields;

    if (field_number == 0 || field_number > CWT_TOKEN_FIELD_MAX)
        return 0;

    cwt_token_parse(token, &fields);
    if (field_number > fields.count)
        return 0;

    CwtStringView field = fields.field[field_number - 1];
    size_t value_len = strlen(value);
    return field.len == value_len && memcmp(field.ptr, value, value_len) == 0;
}

int cwt_token_field_write(FILE *stream, const char *token,
                          size_t field_number)
{
    CwtTokenFields fields;

    if (field_number == 0 || field_number > CWT_TOKEN_FIELD_MAX)
        return -1;

    cwt_token_parse(token, &fields);
    if (field_number > fields.count)
        return -1;

    CwtStringView field = fields.field[field_number - 1];
    return fwrite(field.ptr, 1, field.len, stream) == field.len ? 0 : -1;
}
