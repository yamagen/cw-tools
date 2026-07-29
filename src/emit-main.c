#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emit-d3.h"

int emit_legacy_main(int argc, char **argv);

static void die_allocation(void)
{
    fputs("emit: memory allocation failed\n", stderr);
    exit(EXIT_FAILURE);
}

static bool is_format_option(const char *argument)
{
    return strcmp(argument, "-T") == 0 || strcmp(argument, "--format") == 0;
}

int main(int argc, char **argv)
{
    char **forward = malloc((size_t)(argc + 1) * sizeof(*forward));
    if (forward == NULL)
        die_allocation();

    for (int i = 0; i < argc; i++)
        forward[i] = argv[i];
    forward[argc] = NULL;

    bool data_only = false;
    bool help_requested = false;
    for (int i = 1; i < argc; i++) {
        const char *argument = forward[i];

        if (strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0)
            help_requested = true;

        if (is_format_option(argument) && i + 1 < argc) {
            data_only = strcmp(forward[i + 1], "js") == 0;
            if (data_only)
                forward[i + 1] = (char *)"d3";
            i++;
            continue;
        }
        if (strncmp(argument, "--format=", 9) == 0) {
            data_only = strcmp(argument + 9, "js") == 0;
            if (data_only)
                forward[i] = (char *)"--format=d3";
            continue;
        }
        if (strncmp(argument, "-T", 2) == 0 && argument[2] != '\0') {
            data_only = strcmp(argument + 2, "js") == 0;
            if (data_only)
                forward[i] = (char *)"-Td3";
        }
    }

    emit_d3_set_data_only(data_only);
    int status = emit_legacy_main(argc, forward);
    if (help_requested && status == EXIT_SUCCESS) {
        fputs("\nExternal D3 data:\n"
              "  -T js, --format js   output emit-data.js for assets/emit-d3.js\n",
              stdout);
    }
    free(forward);
    return status;
}
