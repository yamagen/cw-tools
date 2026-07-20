# Pattern and label field projection

This bundle contains:

- `src/cw.c` — cw 0.6.0
- `src/common.c`
- `src/common.h`
- `src/emit.c` — emit 0.3.0
- `config/emit-config.json`

## Pattern fields

`cw` uses `-p` / `--pattern-fields` to select the input token fields that
form the hash pattern.

```sh
cw -p 2,3
cw --pattern-fields 2f.3f
cw -p 2
```

The default is fields 2,3,4. `-s` remains as an alias for fields 1,2,3,4.
The regular expression supplied with `-k` is applied to the projected
pattern string.

The complete original token, with up to four fields, is retained in the cw
output. When multiple original tokens share one pattern, the token occurring
in the greatest number of units is used as the representative. Ties are
resolved in lexical order.

## Label fields

`emit` reads label projection from `config/emit-config.json`:

```json
"node": {
  "shape": "plaintext",
  "label_fields": [1, 4],
  "label_separator": "/"
}
```

`label_fields` affects only the visible node label. It does not change IDF,
CW, Z, pair identity, or edge selection.

## Installation

From the repository root:

```sh
cp src/cw.c     /path/to/cw-tools/src/cw.c
cp src/common.c /path/to/cw-tools/src/common.c
cp src/common.h /path/to/cw-tools/src/common.h
cp src/emit.c   /path/to/cw-tools/src/emit.c
cp config/emit-config.json /path/to/cw-tools/config/emit-config.json
```

Then run:

```sh
make clean
make
./cw --version
./emit --version
```

Expected:

```text
cw 0.6.0
emit 0.3.0
```
