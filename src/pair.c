#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROGRAM_NAME "pair"
#define PROGRAM_VERSION "0.1.0"

typedef struct {
  char **items;
  size_t len;
  size_t cap;
} StringVec;

typedef struct {
  bool ordered;
  size_t window; /* 0: all pairs, 1: adjacent, N: within N positions */
} Options;

static void die(const char *message) {
  fprintf(stderr, "%s: %s\n", PROGRAM_NAME, message);
  exit(EXIT_FAILURE);
}

static void die_errno(const char *path) {
  fprintf(stderr, "%s: %s: %s\n", PROGRAM_NAME, path, strerror(errno));
  exit(EXIT_FAILURE);
}

static void *xrealloc(void *ptr, size_t size) {
  void *new_ptr = realloc(ptr, size);
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

static void vec_init(StringVec *vec) {
  vec->items = NULL;
  vec->len = 0;
  vec->cap = 0;
}

static void vec_push(StringVec *vec, char *item) {
  if (vec->len == vec->cap) {
    size_t new_cap = vec->cap == 0 ? 16 : vec->cap * 2;
    vec->items = xrealloc(vec->items, new_cap * sizeof(*vec->items));
    vec->cap = new_cap;
  }
  vec->items[vec->len++] = item;
}

static void vec_clear(StringVec *vec) {
  for (size_t i = 0; i < vec->len; i++)
    free(vec->items[i]);
  vec->len = 0;
}

static void vec_destroy(StringVec *vec) {
  vec_clear(vec);
  free(vec->items);
  vec->items = NULL;
  vec->cap = 0;
}

static bool vec_contains(const StringVec *vec, const char *s) {
  for (size_t i = 0; i < vec->len; i++) {
    if (strcmp(vec->items[i], s) == 0)
      return true;
  }
  return false;
}

static void print_help(FILE *stream) {
  fprintf(
      stream,
      "Usage: %s [OPTION]... [FILE]...\n"
      "\n"
      "Generate token pairs from unit-based input.\n"
      "\n"
      "Each input line has the form:\n"
      "  unit_id token token ...\n"
      "\n"
      "Tokens are copied unchanged. A token may contain one to four\n"
      "slash-separated fields; only the first field is required.\n"
      "\n"
      "Output:\n"
      "  unit_id<TAB>token1<TAB>token2\n"
      "\n"
      "Options:\n"
      "  -a, --adjacent       pair adjacent tokens only (same as --window 1)\n"
      "  -w, --window N       pair tokens whose positions differ by at most N\n"
      "  -o, --ordered        preserve token order\n"
      "  -u, --unordered      normalize each pair lexically (default)\n"
      "  -h, --help           display this help and exit\n"
      "  -v, --version        display version information and exit\n"
      "\n"
      "In the default unordered all-pairs mode, duplicate tokens in a unit\n"
      "are removed and each combination is emitted once. Ordered and windowed\n"
      "modes preserve token occurrences.\n",
      PROGRAM_NAME);
}

static void print_version(void) {
  printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
}

static size_t parse_positive_size(const char *s) {
  char *end = NULL;
  errno = 0;
  unsigned long value = strtoul(s, &end, 10);

  if (errno != 0 || end == s || *end != '\0' || value == 0)
    die("--window requires a positive integer");

  if (value > (size_t)-1)
    die("--window value is too large");

  return (size_t)value;
}

static void emit_pair(const char *unit_id, const char *left, const char *right,
                      bool ordered) {
  if (!ordered && strcmp(left, right) > 0) {
    const char *tmp = left;
    left = right;
    right = tmp;
  }

  printf("%s\t%s\t%s\n", unit_id, left, right);
}

static void emit_all_pairs(const char *unit_id, const StringVec *tokens,
                           bool ordered) {
  if (ordered) {
    for (size_t i = 0; i < tokens->len; i++) {
      for (size_t j = i + 1; j < tokens->len; j++)
        emit_pair(unit_id, tokens->items[i], tokens->items[j], true);
    }
    return;
  }

  StringVec unique;
  vec_init(&unique);

  for (size_t i = 0; i < tokens->len; i++) {
    if (!vec_contains(&unique, tokens->items[i]))
      vec_push(&unique, xstrdup(tokens->items[i]));
  }

  for (size_t i = 0; i < unique.len; i++) {
    for (size_t j = i + 1; j < unique.len; j++)
      emit_pair(unit_id, unique.items[i], unique.items[j], false);
  }

  vec_destroy(&unique);
}

static void emit_window_pairs(const char *unit_id, const StringVec *tokens,
                              size_t window, bool ordered) {
  for (size_t i = 0; i < tokens->len; i++) {
    size_t limit = i + window + 1;
    if (limit > tokens->len)
      limit = tokens->len;

    for (size_t j = i + 1; j < limit; j++)
      emit_pair(unit_id, tokens->items[i], tokens->items[j], ordered);
  }
}

static void process_line(char *line, size_t line_number, const char *source,
                         const Options *options) {
  const char *delim = " \t\r\n";
  char *saveptr = NULL;
  char *unit_id = strtok_r(line, delim, &saveptr);

  if (unit_id == NULL)
    return;

  StringVec tokens;
  vec_init(&tokens);

  for (char *token = strtok_r(NULL, delim, &saveptr); token != NULL;
       token = strtok_r(NULL, delim, &saveptr)) {
    vec_push(&tokens, xstrdup(token));
  }

  if (tokens.len == 0) {
    fprintf(stderr, "%s: %s:%zu: unit has no tokens\n", PROGRAM_NAME, source,
            line_number);
    vec_destroy(&tokens);
    return;
  }

  if (options->window == 0)
    emit_all_pairs(unit_id, &tokens, options->ordered);
  else
    emit_window_pairs(unit_id, &tokens, options->window, options->ordered);

  vec_destroy(&tokens);
}

static void process_stream(FILE *stream, const char *source,
                           const Options *options) {
  char *line = NULL;
  size_t capacity = 0;
  size_t line_number = 0;

  while (getline(&line, &capacity, stream) != -1) {
    line_number++;
    process_line(line, line_number, source, options);
  }

  if (ferror(stream)) {
    free(line);
    die_errno(source);
  }

  free(line);
}

int main(int argc, char **argv) {
  Options options = {.ordered = false, .window = 0};

  static const struct option long_options[] = {
      {"adjacent", no_argument, NULL, 'a'},
      {"window", required_argument, NULL, 'w'},
      {"ordered", no_argument, NULL, 'o'},
      {"unordered", no_argument, NULL, 'u'},
      {"help", no_argument, NULL, 'h'},
      {"version", no_argument, NULL, 'v'},
      {NULL, 0, NULL, 0}};

  int option;
  while ((option = getopt_long(argc, argv, "aw:ouhv", long_options, NULL)) !=
         -1) {
    switch (option) {
    case 'a':
      options.window = 1;
      break;
    case 'w':
      options.window = parse_positive_size(optarg);
      break;
    case 'o':
      options.ordered = true;
      break;
    case 'u':
      options.ordered = false;
      break;
    case 'h':
      print_help(stdout);
      return EXIT_SUCCESS;
    case 'v':
      print_version();
      return EXIT_SUCCESS;
    default:
      print_help(stderr);
      return EXIT_FAILURE;
    }
  }

  if (optind == argc) {
    process_stream(stdin, "-", &options);
    return EXIT_SUCCESS;
  }

  for (int i = optind; i < argc; i++) {
    FILE *stream = fopen(argv[i], "r");
    if (stream == NULL)
      die_errno(argv[i]);

    process_stream(stream, argv[i], &options);

    if (fclose(stream) != 0)
      die_errno(argv[i]);
  }

  return EXIT_SUCCESS;
}
