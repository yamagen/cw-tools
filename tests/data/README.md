# Test data

This directory contains source data and generated data used by the test and example scripts.

## Source data

### `all-v02-21daishu.db`

Source database for the Hachidaishu data.

Related tools:

- `../tools/db2cw.awk`
- `../tools/db2cw-split.awk`
- `../tools/db2cw.py`
- `../tools/db2cw-split.py`
- `../tools/db2texts.awk`

Generated files include:

- `hachidaishu-bg.txt`
- `hachidaishu-bg-split.txt`
- `emit-texts.json`

### `tom-sawyer-gutenberg.txt`

Source text of _The Adventures of Tom Sawyer_ from Project Gutenberg.

Used for examples and conversion tests.

### `tom-sawyer-litbank-2000.cw.txt`

CW-formatted Tom Sawyer data based on the LitBank sample.

Used for comparison and test processing.

## Generated Hachidaishu data

### `hachidaishu-bg.txt`

Generated from `all-v02-21daishu.db` using:

```sh
awk -f tests/tools/db2cw.awk \
  tests/data/all-v02-21daishu.db \
  > tests/data/hachidaishu-bg.txt
```

### `hachidaishu-bg-split.txt`

Generated using:

```sh
awk -f tests/tools/db2cw-split.awk \
  tests/data/all-v02-21daishu.db \
  > tests/data/hachidaishu-bg-split.txt
```

### `emit-texts.json`

Generated using:

```sh
awk -f tests/tools/db2texts.awk \
  tests/data/all-v02-21daishu.db \
  > tests/data/emit-texts.json
```

This file provides source texts corresponding to `unit_ids`
used by the interactive D3 visualization.

## IDF files

Files matching:

```text
hachidaishu-bg*.idf
```

are generated from the Hachidaishu CW data by `cw --idf-out`.

For example:

```sh
cat tests/data/hachidaishu-bg-split.txt |
  ./pair |
  ./cw -p 5 --substr 16 --idf-out \
  > tests/data/hachidaishu-bg-split-16.idf
```

These files are reused with `cw --idf-in` to keep the same IDF
reference across subsets such as Kokinshu.
