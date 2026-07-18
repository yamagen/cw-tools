#ifndef CW_TOOLS_COMMON_H
#define CW_TOOLS_COMMON_H

#include <stddef.h>
#include <stdio.h>

#define CWT_TOKEN_FIELD_MAX 4

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
    char *token;
    size_t df;
    double idf;
} CwtTokenStat;

typedef struct {
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
    size_t unit_count;
    CwtTokenStat *tokens;
    size_t token_count;
    CwtPairStat *pairs;
    size_t pair_count;
} CwtCorpusStats;

void cwt_stats_init(CwtCorpusStats *stats);
int cwt_stats_read(FILE *stream, CwtCorpusStats *stats, const char *progname);
void cwt_stats_free(CwtCorpusStats *stats);
const CwtTokenStat *cwt_stats_find_token(const CwtCorpusStats *stats, const char *token);
void cwt_token_parse(const char *token, CwtTokenFields *fields);
int cwt_token_field_equals(const char *token, size_t field_number, const char *value);
int cwt_token_field_write(FILE *stream, const char *token, size_t field_number);

#endif
