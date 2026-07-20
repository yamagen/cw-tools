#ifndef CW_TOOLS_COMMON_H
#define CW_TOOLS_COMMON_H

#include <stddef.h>
#include <stdio.h>

#define CWT_TOKEN_FIELD_MAX 4
#define CWT_TOKEN_FIELD_BIT(field_number) (1u << ((field_number) - 1u))
#define CWT_TOKEN_FIELD_ALL ((1u << CWT_TOKEN_FIELD_MAX) - 1u)

typedef struct {
    char **items;
    size_t len;
    size_t cap;
} CwtStringList;

typedef struct {
    const char *ptr;
    size_t len;
} CwtStringView;

typedef struct {
    CwtStringView field[CWT_TOKEN_FIELD_MAX];
    size_t count;
} CwtTokenFields;

typedef struct {
    char *pattern;
    char *token;
    size_t df;
    double idf;
    size_t fq;
    int fq_available;
    CwtStringList fq_unit_ids;
    size_t *fq_unit_counts;
} CwtTokenStat;

typedef struct {
    char *pattern1;
    char *pattern2;
    char *token1;
    char *token2;
    size_t ctf;
    size_t cdf;
    size_t df1;
    double idf1;
    size_t df2;
    double idf2;
    CwtStringList unit_ids;
} CwtPairStat;

typedef struct {
    unsigned pattern_fields;
    int unordered_pairs;
    int unique_pairs_per_unit;
    int drop_self_pairs;
} CwtStatsReadOptions;

typedef struct {
    size_t unit_count;
    CwtTokenStat *tokens;
    size_t token_count;
    CwtPairStat *pairs;
    size_t pair_count;
} CwtCorpusStats;

void cwt_stats_init(CwtCorpusStats *stats);
int cwt_stats_read(FILE *stream, CwtCorpusStats *stats, const char *progname);
int cwt_stats_read_with_options(FILE *stream, CwtCorpusStats *stats,
                                const char *progname,
                                const CwtStatsReadOptions *options);
void cwt_stats_free(CwtCorpusStats *stats);
const CwtTokenStat *cwt_stats_find_pattern(const CwtCorpusStats *stats,
                                           const char *pattern);
void cwt_token_parse(const char *token, CwtTokenFields *fields);
char *cwt_token_project(const char *token, unsigned field_mask);
int cwt_token_field_equals(const char *token, size_t field_number,
                           const char *value);
int cwt_token_field_write(FILE *stream, const char *token,
                          size_t field_number);

#endif
