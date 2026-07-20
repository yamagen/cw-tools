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
#define PROG_VERSION "0.9.0"

typedef enum {
    CW_METHOD_BASIC = 1,
    CW_METHOD_WAKA_GRAPH = 7,
    CW_METHOD_RARE_PATTERN = 12,
    CW_METHOD_GLOBAL_PAIR = 16
} CwMethod;

#define DEFAULT_CW_METHOD CW_METHOD_WAKA_GRAPH
#define CW_LOG_BASE 10.0

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

static int pattern_matches_regex(const char *pattern, const regex_t *regex)
{
    return regexec(regex, pattern, 0, NULL, 0) == 0;
}

static void collect_key_units(const CwtCorpusStats *stats,
                              const regex_t *key_regex,
                              StringSetBuilder *key_units)
{
    StringSetBuilder matching_patterns = {0};

    /* Match each distinct hash pattern only once. */
    for (size_t i = 0; i < stats->token_count; i++) {
        const char *pattern = stats->tokens[i].pattern;

        if (pattern_matches_regex(pattern, key_regex))
            set_builder_append(&matching_patterns, pattern);
    }
    set_builder_sort_unique(&matching_patterns);

    for (size_t i = 0; i < stats->pair_count; i++) {
        const CwtPairStat *pair = &stats->pairs[i];

        if (!string_set_contains(&matching_patterns, pair->pattern1) &&
            !string_set_contains(&matching_patterns, pair->pattern2))
            continue;

        for (size_t j = 0; j < pair->unit_ids.len; j++)
            set_builder_append(key_units, pair->unit_ids.items[j]);
    }

    free(matching_patterns.items);
    set_builder_sort_unique(key_units);
}

static unsigned parse_field_selection(const char *text, const char *option_name)
{
    unsigned mask = 0;
    const unsigned char *p = (const unsigned char *)text;

    while (*p != '\0') {
        if (*p >= '1' && *p <= '4') {
            unsigned field = (unsigned)(*p - '0');
            mask |= CWT_TOKEN_FIELD_BIT(field);
            p++;
            if (*p == 'f' || *p == 'F')
                p++;
            continue;
        }

        if (*p == ',' || *p == '.' || *p == '/' || *p == ':' ||
            *p == '+' || *p == '-' || *p == '_' || *p == ' ' ||
            *p == '\t') {
            p++;
            continue;
        }

        fprintf(stderr,
                "%s: %s: invalid field selection '%s' "
                "(use fields 1 to 4, for example 2,3 or 2f.3f)\n",
                PROG_NAME, option_name, text);
        exit(EXIT_FAILURE);
    }

    if (mask == 0) {
        fprintf(stderr, "%s: %s: at least one field is required\n",
                PROG_NAME, option_name);
        exit(EXIT_FAILURE);
    }

    return mask;
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
    const char *pattern;
    size_t fq;
    int available;
} SelectedTokenFrequency;

typedef struct {
    SelectedTokenFrequency *items;
    size_t len;
} SelectedFrequencyIndex;

static int compare_selected_frequency(const void *left, const void *right)
{
    const SelectedTokenFrequency *a = left;
    const SelectedTokenFrequency *b = right;
    return strcmp(a->pattern, b->pattern);
}

static size_t selected_token_frequency(const CwtTokenStat *token,
                                       const StringSetBuilder *key_units)
{
    if (key_units == NULL)
        return token->fq;

    size_t fq = 0;
    for (size_t i = 0; i < token->fq_unit_ids.len; i++) {
        if (string_set_contains(key_units, token->fq_unit_ids.items[i]))
            fq += token->fq_unit_counts[i];
    }
    return fq;
}

static SelectedFrequencyIndex build_selected_frequency_index(
    const CwtCorpusStats *stats,
    const StringSetBuilder *key_units)
{
    SelectedFrequencyIndex index = {0};
    if (stats->token_count == 0)
        return index;

    index.items = xrealloc(NULL,
                          stats->token_count * sizeof(*index.items));
    index.len = stats->token_count;
    for (size_t i = 0; i < stats->token_count; i++) {
        index.items[i].pattern = stats->tokens[i].pattern;
        index.items[i].available = stats->tokens[i].fq_available;
        index.items[i].fq = stats->tokens[i].fq_available
            ? selected_token_frequency(&stats->tokens[i], key_units)
            : 0;
    }
    qsort(index.items, index.len, sizeof(*index.items),
          compare_selected_frequency);
    return index;
}

static const SelectedTokenFrequency *find_selected_frequency(
    const SelectedFrequencyIndex *index, const char *pattern)
{
    SelectedTokenFrequency key = {.pattern = pattern};
    return bsearch(&key, index->items, index->len,
                   sizeof(*index->items), compare_selected_frequency);
}

typedef struct {
    size_t count;
    double mean;
    double sd;
} CwDistribution;

static double calculate_cw(CwMethod method, const CwtPairStat *pair,
                           size_t ctf, size_t key_fq,
                           size_t global_unit_count)
{
    double token_weight = sqrt(pair->idf1 * pair->idf2);

    switch (method) {
    case CW_METHOD_BASIC:
        return (1.0 + log((double)ctf)) * token_weight;

    case CW_METHOD_WAKA_GRAPH:
        /*
         * Historical method 7 (the default in the earlier cw.c):
         *
         *   (1 + log_10(pair_fq)) * sqrt(idf1 * idf2)
         *   ----------------------------------------------------
         *              1 + log_10(key_fq)
         */
        return (1.0 + log((double)ctf) / log(CW_LOG_BASE)) *
               token_weight /
               (1.0 + log((double)key_fq) / log(CW_LOG_BASE));

    case CW_METHOD_RARE_PATTERN:
        /*
         * Historical method 12.  Preserve the original integer N/cidf
         * quotient exactly: key_fq and pair frequency were both integers.
         */
        return (1.0 + (double)(key_fq / ctf)) * token_weight;

    case CW_METHOD_GLOBAL_PAIR:
        /*
         * Historical method 16 (marked "best" in the earlier cw.c):
         *
         *   (1 + ln(N / global_pair_df))
         *       * sqrt(idf1 * idf2)
         *       * (1 + ln(local_pair_ctf))
         *
         * pair->cdf is the number of global input units containing the
         * pair.  Use floating-point division here rather than reproducing
         * the accidental integer division of the historical C expression.
         */
        return (1.0 + log((double)global_unit_count /
                          (double)pair->cdf)) *
               token_weight *
               (1.0 + log((double)ctf));
    }

    return NAN;
}

static size_t calculate_key_frequency(const CwtCorpusStats *stats,
                                      const regex_t *key_regex,
                                      const StringSetBuilder *key_units,
                                      int *available)
{
    size_t key_fq = 0;
    int matched = 0;

    *available = 1;
    for (size_t i = 0; i < stats->token_count; i++) {
        const CwtTokenStat *token = &stats->tokens[i];

        if (!pattern_matches_regex(token->pattern, key_regex))
            continue;

        matched = 1;
        if (!token->fq_available) {
            *available = 0;
            continue;
        }
        key_fq += selected_token_frequency(token, key_units);
    }

    if (!matched)
        return 0;
    return key_fq;
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
    const StringSetBuilder *key_units,
    CwMethod method, size_t key_fq)
{
    CwDistribution distribution = {0};
    double m2 = 0.0;

    for (size_t i = 0; i < stats->pair_count; i++) {
        const CwtPairStat *pair = &stats->pairs[i];
        size_t ctf = count_selected_ctf(&pair->unit_ids, key_units);

        if (ctf == 0)
            continue;

        double cw = calculate_cw(method, pair, ctf, key_fq,
                                 stats->unit_count);
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

static void print_frequency_field(const SelectedTokenFrequency *frequency)
{
    if (frequency != NULL && frequency->available)
        printf("%zu", frequency->fq);
    else
        putchar('-');
}

static void print_pairs(const CwtCorpusStats *stats,
                        const StringSetBuilder *key_units,
                        const CwDistribution *distribution,
                        const SelectedFrequencyIndex *frequencies,
                        CwMethod method, size_t key_fq)
{
    for (size_t i = 0; i < stats->pair_count; i++) {
        const CwtPairStat *pair = &stats->pairs[i];
        size_t ctf = count_selected_ctf(&pair->unit_ids, key_units);

        if (ctf == 0)
            continue;

        size_t cdf = count_selected_cdf(&pair->unit_ids, key_units);
        double cw = calculate_cw(method, pair, ctf, key_fq,
                                 stats->unit_count);
        double z = 0.0;
        const SelectedTokenFrequency *fq1 =
            find_selected_frequency(frequencies, pair->pattern1);
        const SelectedTokenFrequency *fq2 =
            find_selected_frequency(frequencies, pair->pattern2);

        if (distribution->sd > 0.0)
            z = (cw - distribution->mean) / distribution->sd;

        printf("%s\t%s\t%zu\t%zu\t%zu\t%.12g\t",
               pair->token1, pair->token2,
               ctf, cdf, pair->df1, pair->idf1);
        print_frequency_field(fq1);
        printf("\t%zu\t%.12g\t", pair->df2, pair->idf2);
        print_frequency_field(fq2);
        printf("\t%.17g\t%.17g", cw, z);
        print_selected_unit_ids(&pair->unit_ids, key_units);
        putchar('\n');
    }
}

static void process_pairs(const CwtCorpusStats *stats,
                          const StringSetBuilder *key_units,
                          CwMethod method, size_t key_fq)
{
    CwDistribution distribution =
        calculate_cw_distribution(stats, key_units, method, key_fq);
    SelectedFrequencyIndex frequencies =
        build_selected_frequency_index(stats, key_units);

    print_pairs(stats, key_units, &distribution, &frequencies,
                method, key_fq);
    free(frequencies.items);
}

static void print_help(FILE *stream)
{
    fprintf(stream,
            "Usage: %s [OPTION]... [FILE]\n"
            "\n"
            "Calculate CW and Z values directly from pair output.\n"
            "\n"
            "Input:\n"
            "  unit_id token1 token2 [fq1 fq2]\n"
            "\n"
            "Each token may contain one to four slash-separated fields:\n"
            "  f1[/f2[/f3[/f4]]]\n"
            "\n"
            "Output:\n"
            "  token1 token2 ctf cdf df1 idf1 fq1 df2 idf2 fq2 cw z unit_id...\n"
            "\n"
            "Pattern fields determine token identity, hash registration, pair\n"
            "identity, df/idf, local fq, and the string searched by -k.  The complete\n"
            "original token fields are retained for emit.  If several original\n"
            "tokens share one pattern, the form occurring in the most units is\n"
            "used as the representative; lexical order breaks ties.\n"
            "\n"
            "The default pattern is fields 2,3,4.  A one-field token falls back\n"
            "to field 1.  pair 0.2.0 supplies exact per-unit token counts;\n"
            "older three-column pair input remains readable, but fq is output as '-'.\n"
            "\n"
            "CW methods (-M 7 is the default):\n"
            "  -M 1   basic: (1 + ln(ctf)) * sqrt(idf1 * idf2)\n"
            "  -M 7   waka graph: (1 + log10(ctf)) * sqrt(idf1 * idf2)\n"
            "           divided by (1 + log10(key_fq))\n"
            "  -M 12  rare pattern: (1 + key_fq / ctf) * sqrt(idf1 * idf2)\n"
            "           (key_fq / ctf preserves the historical integer quotient)\n"
            "  -M 16  global pair: (1 + ln(N / global_pair_df))\n"
            "           * sqrt(idf1 * idf2) * (1 + ln(local_ctf))\n"
            "           (uses floating-point N / global_pair_df)\n"
            "  z      = (cw - mean_selected_cw) / sd_selected_cw\n"
            "\n"
            "Options:\n"
            "  -p, --pattern-fields LIST\n"
            "                   fields used for the hash pattern\n"
            "                   (examples: 2,3  2f.3f  2)\n"
            "  -k, --key REGEX  restrict pair occurrences to units containing a\n"
            "                   pattern matching a POSIX extended regex\n"
            "  -M, --method N   CW method: 1, 7, 12, or 16 (default: 7)\n"
            "  -s, --surface    compatibility alias for -p 1,2,3,4\n"
            "  -h, --help       display this help and exit\n"
            "  -v, --version    display version information and exit\n",
            PROG_NAME);
}

int main(int argc, char **argv)
{
    static const struct option long_options[] = {
        {"pattern-fields", required_argument, NULL, 'p'},
        {"key", required_argument, NULL, 'k'},
        {"method", required_argument, NULL, 'M'},
        {"surface", no_argument, NULL, 's'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {NULL, 0, NULL, 0}
    };

    const char *key = NULL;
    CwMethod method = DEFAULT_CW_METHOD;
    unsigned pattern_fields =
        CWT_TOKEN_FIELD_BIT(2) | CWT_TOKEN_FIELD_BIT(3) |
        CWT_TOKEN_FIELD_BIT(4);
    regex_t key_regex;
    int key_regex_compiled = 0;
    int option;

    (void)setlocale(LC_ALL, "");

    while ((option = getopt_long(argc, argv, "p:k:M:shv", long_options, NULL)) != -1) {
        switch (option) {
        case 'p':
            pattern_fields = parse_field_selection(optarg, "--pattern-fields");
            break;
        case 'k':
            key = optarg;
            break;
        case 'M': {
            char *end = NULL;
            errno = 0;
            long value = strtol(optarg, &end, 10);
            if (errno != 0 || end == optarg || *end != '\0' ||
                (value != CW_METHOD_BASIC &&
                 value != CW_METHOD_WAKA_GRAPH &&
                 value != CW_METHOD_RARE_PATTERN &&
                 value != CW_METHOD_GLOBAL_PAIR)) {
                fprintf(stderr,
                        "%s: --method requires 1, 7, 12, or 16 (got '%s')\n",
                        PROG_NAME, optarg);
                return EXIT_FAILURE;
            }
            method = (CwMethod)value;
            break;
        }
        case 's':
            pattern_fields = CWT_TOKEN_FIELD_ALL;
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

    if ((method == CW_METHOD_WAKA_GRAPH ||
         method == CW_METHOD_RARE_PATTERN) && key == NULL) {
        fprintf(stderr,
                "%s: method %d requires -k/--key because key_fq is part "
                "of the formula (use -M 1 or -M 16 for an unselected corpus)\n",
                PROG_NAME, (int)method);
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

    const CwtStatsReadOptions read_options = {
        .pattern_fields = pattern_fields,
        .unordered_pairs = 1,
        .unique_pairs_per_unit = 1,
        .drop_self_pairs = 1
    };

    cwt_stats_read_with_options(stream, &stats, PROG_NAME,
                                &read_options);

    if (stream != stdin && fclose(stream) != 0) {
        cwt_stats_free(&stats);
        die_errno(source);
    }

    StringSetBuilder key_units = {0};
    const StringSetBuilder *selection = NULL;
    size_t key_fq = 0;

    if (key != NULL) {
        int key_fq_available = 1;

        collect_key_units(&stats, &key_regex, &key_units);
        selection = &key_units;

        if (key_units.len == 0)
            goto cleanup;

        key_fq = calculate_key_frequency(&stats, &key_regex, selection,
                                         &key_fq_available);
        if ((method == CW_METHOD_WAKA_GRAPH ||
             method == CW_METHOD_RARE_PATTERN) && !key_fq_available) {
            fprintf(stderr,
                    "%s: method %d requires token frequencies; regenerate "
                    "the pair data with pair 0.2.0 or later\n",
                    PROG_NAME, (int)method);
            free(key_units.items);
            cwt_stats_free(&stats);
            if (key_regex_compiled)
                regfree(&key_regex);
            return EXIT_FAILURE;
        }
        if ((method == CW_METHOD_WAKA_GRAPH ||
             method == CW_METHOD_RARE_PATTERN) && key_fq == 0) {
            fprintf(stderr, "%s: selected key frequency is zero\n", PROG_NAME);
            free(key_units.items);
            cwt_stats_free(&stats);
            if (key_regex_compiled)
                regfree(&key_regex);
            return EXIT_FAILURE;
        }
    }

    process_pairs(&stats, selection, method, key_fq);

cleanup:

    free(key_units.items);
    cwt_stats_free(&stats);
    if (key_regex_compiled)
        regfree(&key_regex);
    return EXIT_SUCCESS;
}
