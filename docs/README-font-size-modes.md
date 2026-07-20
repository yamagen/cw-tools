# cw-tools node font-size modes

This bundle adds configurable node font-size weights while preserving the
separation between calculation (`cw`) and display (`emit`).

## Versions

- `pair 0.2.0`
- `cw 0.7.0`
- `emit 0.5.0`

## Configuration

Set `node.font_size_by` in `config/emit-config.json`:

```json
"node": {
  "shape": "oval",
  "label_fields": [1],
  "label_separator": "/",
  "font_size_by": "fq",
  "min_font_size": 7,
  "max_font_size": 32
}
```

Supported values:

- `fq`: token occurrence frequency in the units selected by `cw -k`. This
  reproduces the intention of the older `cw.c` font-size calculation.
- `idf`: global inverse document frequency calculated by `cw`.
- `degree`: number of retained edges incident on the node after `emit` applies
  its `min_cw` and `min_z` filters.

For every mode, the minimum and maximum values among the emitted nodes are
linearly mapped to `min_font_size` and `max_font_size`. If all values are equal,
the midpoint is used.

## Pair frequency metadata

`pair 0.2.0` outputs two additional columns:

```text
unit_id  token1  token2  fq1  fq2
```

`fq1` and `fq2` are counts of the complete input tokens inside that unit. `cw`
uses them to recover exact local frequency even when several original tokens
are merged by `--pattern-fields`.

`cw 0.7.0` also accepts legacy three-column pair input. In that case it writes
`-` for unavailable `fq1` and `fq2`. `emit` can still use `idf` or `degree`, but
reports an error if `font_size_by` is `fq`.

## cw output

```text
token1 token2 ctf cdf df1 idf1 fq1 df2 idf2 fq2 cw z unit_id...
```

## Installation

From the repository root:

```sh
cp src/pair.c   /path/to/cw-tools/src/pair.c
cp src/common.c /path/to/cw-tools/src/common.c
cp src/common.h /path/to/cw-tools/src/common.h
cp src/cw.c     /path/to/cw-tools/src/cw.c
cp src/emit.c   /path/to/cw-tools/src/emit.c
cp config/emit-config.json /path/to/cw-tools/config/emit-config.json

cd /path/to/cw-tools
make clean
make
```

Regenerate stored pair data with `pair 0.2.0` when `font_size_by: "fq"` is
required.
