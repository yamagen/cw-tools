#ifndef EMIT_URL_H
#define EMIT_URL_H

#include "emit-types.h"

typedef struct {
    char *base;
    char *parameter;
    char *target;
} EmitUrlConfig;

EmitUrlConfig emit_url_config_load(const char *config_path);
void emit_url_config_free(EmitUrlConfig *config);
char *emit_edge_url(const Edge *edge, const EmitUrlConfig *config);

#endif
