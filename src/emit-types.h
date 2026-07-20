#ifndef EMIT_TYPES_H
#define EMIT_TYPES_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    FORMAT_JSON,
    FORMAT_DOT,
    FORMAT_MD,
    FORMAT_TEX,
    FORMAT_HTML,
    FORMAT_D3
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

#endif
