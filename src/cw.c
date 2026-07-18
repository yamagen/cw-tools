#define _POSIX_C_SOURCE 200809L

#include "common.h"

#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <math.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROG_NAME "cw"
#define PROG_VERSION "0.4.0"

typedef struct {
    const char **items;
    size_t len;
    size_t cap;
} StringSetBuilder;

static void die_errno(const char *path)
{
    fprintf(stderr, "%s: %s: %s\n", PROG_NAME, path, strerror(errno));
    exit(EXIT_FAILURE);
}

static void *xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);
    if (p == NULL) {
        fprintf(stderr, "%s: out of memory\n", PROG_NAME);
        exit(EXIT_FAILURE);
    }
    return p;
}

static int compare_string_ptr(const void *a, const void *b)
{
    const char *left = *(const char *const *)a;
    const char *right = *(const char *const *)b;
    return strcmp(left, right);
}

static void set_builder_append(StringSetBuilder *set, const char *value)
{
    if (set->len == set->cap) {
        size_t new_cap = set->cap == 0 ? 64 : set->cap * 2;
        set->items = xrealloc(set->items, new_cap * sizeof(*set->items));
        set->cap = new_cap;
    }
    set->items[set->len++] = value;
}

static void set_builder_sort_unique(StringSetBuilder *set)
{
    if (set->len == 0)
        return;

    qsort(set->items, set->len, sizeof(*set->items), compare_string_ptr);

    size_t out = 1;
    for (size_t i = 1; i < set->len; i++) {
        if (strcmp(set->items[i], set->items[out - 1]) != 0)
            set->items[out++] = set->items[i];
    }
    set->len = out;
}

static int string_set_contains(const StringSetBuilder *set, const char *value)
{
    const char *key = value;
    return bsearch(&key, set->items, set->len, sizeof(*set->items),
                   compare_string_ptr) != NULL;
}

static int token_surface_matches_regex(const char *token,
                                       const regex_t *regex)
{
    CwtTokenFields fields;
    char *surface;
    int matched;

    cwt_token_parse(token, &fields);
    if (fields.count == 0)
        return 0;

    surface = malloc(fields.field[0].len + 1);
    if (surface == NULL) {
        fprintf(stderr, "%s: out of memory\n", PROG_NAME);
        exit(EXIT_FAILURE);
    }

    memcpy(surface, fields.field[0].ptr, fields.field[0].len);
    surface[fields.field[0].len] = '\0';
    matched = regexec(regex, surface, 0, NULL, 0) == 0;
    free(surface);
    return matched;
}

static void collect_key_units(const CwtCorpusStats *stats,
                              const regex_t *key_regex,
                              StringSetBuilder *key_units)
{
    StringSetBuilder matching_tokens = {0};

    /* Match each distinct token only once. */
    for (size_t i = 0; i < stats->token_count; i++) {
        const char *token = stats->tokens[i].token;

        if (token_surface_matches_regex(token, key_regex))
            set_builder_append(&matching_tokens, token);
    }
    set_builder_sort_unique(&matching_tokens);

    for (size_t i = 0; i < stats->pair_count; i++) {
        const CwtPairStat *pair = &stats->pairs[i];

        if (!string_set_contains(&matching_tokens, pair->token1) &&
            !string_set_contains(&matching_tokens, pair->token2))
            continue;

        for (size_t j = 0; j < pair->unit_ids.len; j++)
            set_builder_append(key_units, pair->unit_ids.items[j]);
    }

    free(matching_tokens.items);
    set_builder_sort_unique(key_units);
}

static size_t count_selected_cdf(const CwtStringList *unit_ids,
                                 const StringSetBuilder *key_units)
{
    size_t cdf = 0;
    const char *previous = NULL;

    for (size_t i = 0; i < unit_ids->len; i++) {
        const char *unit_id = unit_ids->items[i];

        if (key_units != NULL && !string_set_contains(key_units, unit_id))
            continue;

        if (previous == NULL || strcmp(previous, unit_id) != 0) {
            cdf++;
            previous = unit_id;
        }
    }

    return cdf;
}

static size_t count_selected_ctf(const CwtStringList *unit_ids,
                                 const StringSetBuilder *key_units)
{
    if (key_units == NULL)
        return unit_ids->len;

    size_t ctf = 0;
    for (size_t i = 0; i < unit_ids->len; i++) {
        if (string_set_contains(key_units, unit_ids->items[i]))
            ctf++;
    }
    return ctf;
}

static void print_selected_unit_ids(const CwtStringList *unit_ids,
                                    const StringSetBuilder *key_units)
{
    for (size_t i = 0; i < unit_ids->len; i++) {
        const char *unit_id = unit_ids->items[i];

        if (key_units != NULL && !string_set_contains(key_units, unit_id))
            continue;

        printf("\t%s", unit_id);
    }
}

typedef struct {
    size_t count;
    double mean;
    double sd;
} CwDistribution;

static double calculate_cw(const CwtPairStat *pair, size_t ctf)
{
    return (1.0 + log((double)ctf)) *
           sqrt(pair->idf1 * pair->idf2);
}

/*
 * Calculate the mean and sample standard deviation of the CW values in the
 * selected pair set.  This is the same one-pass recurrence used by the
 * earlier cw implementation:
 *
 *     sd = sqrt(M2 / (n - 1))
 */
static CwDistribution calculate_cw_distribution(
    const CwtCorpusStats *stats,
    const StringSetBuilder *key_units)
{
    CwDistribution distribution = {0};
    double m2 = 0.0;

    for (size_t i = 0; i < stats->pair_count; i++) {
        const CwtPairStat *pair = &stats->pairs[i];
        size_t ctf = count_selected_ctf(&pair->unit_ids, key_units);

        if (ctf == 0)
            continue;

        double cw = calculate_cw(pair, ctf);
        double delta = cw - distribution.mean;

        distribution.count++;
        distribution.mean += delta / (double)distribution.count;
        m2 += ((double)(distribution.count - 1) * delta * delta) /
              (double)distribution.count;
    }

    if (distribution.count > 1)
        distribution.sd = sqrt(m2 / (double)(distribution.count - 1));

    if (!isfinite(distribution.sd) || distribution.sd < 0.0)
        distribution.sd = 0.0;

    return distribution;
}

static void print_pairs(const CwtCorpusStats *stats,
                        const StringSetBuilder *key_units,
                        const CwDistribution *distribution)
{
    for (size_t i = 0; i < stats->pair_count; i++) {
        const CwtPairStat *pair = &stats->pairs[i];
        size_t ctf = count_selected_ctf(&pair->unit_ids, key_units);

        if (ctf == 0)
            continue;

        size_t cdf = count_selected_cdf(&pair->unit_ids, key_units);
        double cw = calculate_cw(pair, ctf);
        double z = 0.0;

        if (distribution->sd > 0.0)
            z = (cw - distribution->mean) / distribution->sd;

        printf("%s\t%s\t%zu\t%zu\t%zu\t%.12g\t%zu\t%.12g\t%.17g\t%.17g",
               pair->token1, pair->token2,
               ctf, cdf,
               pair->df1, pair->idf1,
               pair->df2, pair->idf2,
               cw, z);
        print_selected_unit_ids(&pair->unit_ids, key_units);
        putchar('\n');
    }
}

static void process_pairs(const CwtCorpusStats *stats,
                          const StringSetBuilder *key_units)
{
    CwDistribution distribution =
        calculate_cw_distribution(stats, key_units);

    print_pairs(stats, key_units, &distribution);
}

static void print_help(FILE *stream)
{
    fprintf(stream,
            "Usage: %s [OPTION]... [FILE]\n"
            "\n"
            "Calculate CW values directly from pair output.\n"
            "\n"
            "Input:\n"
            "  unit_id token1 token2\n"
            "\n"
            "Output:\n"
            "  token1 token2 ctf cdf df1 idf1 df2 idf2 cw z unit_id...\n"
            "\n"
            "The global N, df, and idf values are calculated from the complete\n"
            "input.  With -k, pair occurrences are then restricted to units\n"
            "containing the key, while the global df and idf values are retained.\n"
            "\n"
            "Formulas:\n"
            "  cw = (1 + log(ctf)) * sqrt(idf1 * idf2)\n"
            "  z  = (cw - mean_selected_cw) / sd_selected_cw\n"
            "\n"
            "Options:\n"
            "  -k, --key REGEX  restrict pair occurrences to units whose token\n"
            "                   surface matches a POSIX extended regular expression\n"
            "                   (examples: '梅|桜', '^春', '春$')\n"
            "  -h, --help       display this help and exit\n"
            "  -v, --version    display version information and exit\n",
            PROG_NAME);
}

int main(int argc, char **argv)
{
    static const struct option long_options[] = {
        {"key", required_argument, NULL, 'k'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {NULL, 0, NULL, 0}
    };

    const char *key = NULL;
    regex_t key_regex;
    int key_regex_compiled = 0;
    int option;

    (void)setlocale(LC_ALL, "");

    while ((option = getopt_long(argc, argv, "k:hv", long_options, NULL)) != -1) {
        switch (option) {
        case 'k':
            key = optarg;
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

    if (key != NULL) {
        int error = regcomp(&key_regex, key, REG_EXTENDED | REG_NOSUB);
        if (error != 0) {
            size_t size = regerror(error, &key_regex, NULL, 0);
            char *message = malloc(size);

            if (message == NULL) {
                fprintf(stderr, "%s: out of memory\n", PROG_NAME);
                return EXIT_FAILURE;
            }

            regerror(error, &key_regex, message, size);
            fprintf(stderr, "%s: invalid regular expression '%s': %s\n",
                    PROG_NAME, key, message);
            free(message);
            return EXIT_FAILURE;
        }
        key_regex_compiled = 1;
    }

    FILE *stream = stdin;
    const char *source = "-";

    if (optind < argc) {
        source = argv[optind];
        stream = fopen(source, "r");
        if (stream == NULL)
            die_errno(source);
    }

    CwtCorpusStats stats;
    cwt_stats_init(&stats);
    cwt_stats_read(stream, &stats, PROG_NAME);

    if (stream != stdin && fclose(stream) != 0) {
        cwt_stats_free(&stats);
        die_errno(source);
    }

    StringSetBuilder key_units = {0};
    const StringSetBuilder *selection = NULL;

    if (key != NULL) {
        collect_key_units(&stats, &key_regex, &key_units);
        selection = &key_units;
    }

    process_pairs(&stats, selection);

    free(key_units.items);
    cwt_stats_free(&stats);
    if (key_regex_compiled)
        regfree(&key_regex);
    return EXIT_SUCCESS;
}
