# pair

## NAME

`pair` — generate token-pair records with per-unit token frequencies

## SYNOPSIS

```sh
pair [OPTION]... [FILE]...
```

## DESCRIPTION

`pair` reads unit-based token data and writes token-pair records for use by
[`cw`](./man-cw.md) and other downstream filters.

This manual describes `pair` version 0.2.0.

Each non-comment input line begins with a unit identifier followed by one or
more whitespace-separated tokens:

```text
unit_id token1 token2 ... tokenN
```

A token may contain one to four slash-separated fields:

```text
f1[/f2[/f3[/f4]]]
```

`pair` treats the complete token as an opaque string.  It does not assign fixed
meanings such as *surface*, *lemma*, *part of speech*, or *gloss* to the field
positions, and it does not project or normalize fields.  Field projection is a
separate decision made later by `cw -p` or `cw --pattern-fields`.

For every emitted pair, `pair` also reports the occurrence frequency of each
complete token inside the source unit.  These frequencies allow `cw` to recover
exact local node frequency after several complete tokens have been projected
onto one computational pattern.

With no file operand, `pair` reads standard input.  When several files are
specified, they are processed in command-line order.

## INPUT

The input format is:

```text
unit_id token1 [token2 ... tokenN]
```

Fields are separated by spaces or tabs.  A blank line is ignored.  A line whose
first nonempty field begins with `#` is also ignored.

Example:

```text
u1 梅/梅/名/うめ 鴬/鴬/名/うぐひす 春/春/名/はる 春/春/名/はる
u2 梅/梅/名/うめ 雪/雪/名/ゆき
```

Unit identifiers should uniquely identify the analytical units in the complete
dataset.  This matters downstream because `cw` uses them to calculate document
frequencies and to select local unit sets.

A unit having no token produces a diagnostic and no output.  A unit that cannot
produce a pair—for example, a one-token unit in the default mode—also produces
no pair record and is therefore not visible to downstream pair-based programs.

## OUTPUT

Each pair is written as one tab-separated record:

```text
unit_id token1 token2 fq1 fq2
```

| Column | Name | Meaning |
| ---: | --- | --- |
| 1 | `unit_id` | identifier of the source unit |
| 2 | `token1` | first complete input token |
| 3 | `token2` | second complete input token |
| 4 | `fq1` | number of occurrences of `token1` in the complete source unit |
| 5 | `fq2` | number of occurrences of `token2` in the complete source unit |

`fq1` and `fq2` are **unit-wide frequencies of the complete input tokens**.
They are not pair frequencies, window frequencies, document frequencies, or
frequencies of fields selected later by `cw`.

In unordered mode, lexical reordering of the two endpoints also reorders their
frequency columns, so `fq1` always belongs to `token1` and `fq2` always belongs
to `token2`.

## DEFAULT MODE

The default is unordered all-pairs mode:

```text
window   = all positions
ordering = unordered
```

Within each unit:

1. repeated identical complete tokens are counted;
2. the repeated tokens are collapsed for pair generation;
3. every unordered pair of distinct complete token types is emitted once;
4. the original unit-wide counts are written as `fq1` and `fq2`.

Input:

```text
u1 a b a c b b
```

Output:

```text
u1	a	b	2	3
u1	a	c	2	1
u1	b	c	3	1
```

The pair endpoints are arranged in lexical order.  Self-pairs are not emitted
in the default mode because pairs are formed from distinct complete token
types.

## TOKEN FREQUENCY AND FIELD PROJECTION

The frequency columns preserve information that would otherwise be lost when
`pair` collapses repeated tokens for pair generation.

Suppose one unit contains:

```text
u1 咲き/咲く/V/x 咲け/咲く/V/x 咲き/咲く/V/x 春/春/N/x
```

`pair` keeps the complete tokens separate and writes their counts:

```text
u1	咲き/咲く/V/x	咲け/咲く/V/x	2	1
u1	咲き/咲く/V/x	春/春/N/x	2	1
u1	咲け/咲く/V/x	春/春/N/x	1	1
```

A later command may define computational identity by fields 2 and 3:

```sh
pair units.txt | cw -p 2,3
```

Both inflected forms then project to:

```text
咲く/V
```

Because `pair` supplied the original per-unit frequencies, `cw` can combine the
projected variants and recover:

```text
fq(咲く/V) = 2 + 1 = 3
```

Thus the division of responsibility is:

```text
pair
    preserve complete tokens
    generate pair occurrences
    attach exact per-unit complete-token frequencies

cw
    choose pattern fields
    merge projected variants
    calculate global and local statistics
```

## OPTIONS

### `-a`, `--adjacent`

Generate pairs only from adjacent token positions.

This is equivalent to:

```sh
pair --window 1
```

Token occurrences are preserved.  Consequently, the same pair may be emitted
more than once in one unit.

Example:

```text
u1 a b a
```

```sh
pair --adjacent
```

Output:

```text
u1	a	b	2	1
u1	a	b	2	1
```

Both adjacent occurrences normalize to the same unordered pair.

### `-w N`, `--window N`

Generate a pair when the distance between the two token positions is at most
`N`.

`N` must be a positive integer.  `--window 1` is adjacent mode.

For example:

```sh
pair --window 2 units.txt
```

pairs each token occurrence with the next one or two occurrences in the same
unit.

Windowed modes preserve token occurrences, so they may emit repeated pair
records and, in unordered mode, self-pairs formed from two equal tokens at
different positions.

The reported `fq1` and `fq2` remain the total frequencies of the complete tokens
in the whole unit, not counts restricted to the window.

### `-o`, `--ordered`

Preserve the left-to-right order of token occurrences.

Input:

```text
u1 a b a
```

Command:

```sh
pair --adjacent --ordered
```

Output:

```text
u1	a	b	2	1
u1	b	a	1	2
```

In ordered all-pairs mode, every pair of positions `i < j` is emitted.  Token
occurrences are therefore preserved, and repeated pair records and self-pairs
are possible.

### `-u`, `--unordered`

Arrange each pair lexically.  This is the default.

Unordered mode makes `a b` and `b a` the same endpoint ordering.  It does not by
itself remove duplicate pair records in windowed modes; it only normalizes the
two endpoints of each emitted occurrence.

### `-h`, `--help`

Display a summary of command-line usage and exit.

### `-v`, `--version`

Display version information and exit.

## COUNTING MODES

The principal modes differ as follows:

| Mode | Pair source | Repeated token positions | Repeated pair records | Self-pairs possible |
| --- | --- | --- | --- | --- |
| default unordered all-pairs | distinct complete token types | collapsed for pairing | no | no |
| ordered all-pairs | all position pairs `i < j` | preserved | yes | yes |
| unordered window | positions within `N` | preserved | yes | yes |
| ordered window | positions within `N` | preserved | yes | yes |

The default mode is the normal input mode for the current `cw` waka-graph
pipeline.  Windowed and ordered modes support other definitions of eligible
co-occurrence and should be recorded explicitly as part of the research method.

A further downstream qualification is important: current `cw` 0.9.0 projects
tokens, normalizes pair endpoints as unordered, drops projected self-pairs, and
retains each projected pair at most once per unit.  Thus `pair --ordered` can be
used by other consumers to preserve direction, but direction is not preserved by
the current `cw`.  A window still changes which pairs are eligible to reach
`cw`, while repeated occurrences of the same projected pair inside one unit do
not increase its `cw` pair count.

## EXAMPLES

Generate default unordered pairs from standard input:

```sh
pair < units.txt
```

Generate pairs from the Kokinshu section of the Hachidaishu test data:

```sh
grep '^1' tests/data/hachidaishu-pair.txt | pair
```

Generate adjacent directed pairs:

```sh
pair --adjacent --ordered units.txt
```

Generate unordered pairs within a window of three positions:

```sh
pair --window 3 units.txt
```

Read several files in sequence:

```sh
pair file1.txt file2.txt
```

Inspect the five output fields:

```sh
pair units.txt |
  awk -F '\t' '{
    printf "unit=%s  %s(fq=%s) -- %s(fq=%s)\n",
           $1, $2, $4, $3, $5
  }'
```

Use projected patterns and method 16 downstream:

```sh
grep '^1' tests/data/hachidaishu-pair.txt |
  pair |
  cw -p 2,3 -k '^梅/名$' -M 16 |
  emit -Z 2 |
  neato -Tsvg -o ume-m16-z2.svg
```

## REPRODUCIBLE RESEARCH RECORD

A reproducible pair-generation record should state at least:

1. the version of `pair`;
2. the command defining the input units;
3. whether pairs were ordered or unordered;
4. whether all positions, adjacent positions, or a fixed window were used;
5. the token-field design used in the source data.

For example:

```sh
pair --version

grep '^1' tests/data/hachidaishu-pair.txt |
  pair --unordered |
  cw -p 2,3 -k '^梅/名$' -M 16
```

This records that `pair` generated all unordered pairs of distinct complete
token types, while `cw` later defined pattern identity using fields 2 and 3.

## DIAGNOSTICS

A unit with no token produces a diagnostic of the form:

```text
pair: input.txt:12: unit has no tokens
```

and processing continues.

An invalid window value produces:

```text
pair: --window requires a positive integer
```

Unreadable files, read failures, invalid options, and memory-allocation failures
produce diagnostics on standard error and a nonzero exit status.

## EXIT STATUS

`pair` returns:

```text
0        successful completion
nonzero  invalid options, invalid window size, file errors,
         allocation failure, or another processing error
```

A warning about a tokenless unit does not by itself make the final exit status
nonzero.

## IMPLEMENTATION NOTES

`pair` processes one input line at a time.  Its working memory is proportional
to the number of token occurrences and distinct complete token types in the
largest single unit, not to the size of the complete corpus.

Per-unit token frequencies are calculated on complete token strings before any
field projection.  The program intentionally does not interpret slash-separated
fields; this keeps pair generation independent of the analytical identity later
chosen by `cw`.

## PIPELINE

`pair` is the first stage of the current `cw-tools` pipeline:

```text
unit-based token data
        |
        v
      pair
        |  unit_id token1 token2 fq1 fq2
        v
       cw
        |  projected patterns, global/local statistics, CW, Z
        v
      emit
        |  filtering and serialization
        v
   DOT / JSON / SVG
```

A typical command is:

```sh
pair < units.txt | cw -p 2,3 -M 16 | emit
```

## SEE ALSO

[`cw`](./man-cw.md), [`emit`](./man-emit.md), `awk(1)`, `sort(1)`, `neato(1)`

## AUTHOR

Hilofumi Yamamoto

## LICENSE

MIT License
