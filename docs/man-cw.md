# cw

## NAME

`cw` — calculate co-occurrence weights from projected token patterns

## SYNOPSIS

```sh
cw [OPTION]... [FILE]
```

## DESCRIPTION

`cw` reads token-pair records produced by [`pair`](./man-pair.md), defines the
identity of each token by a user-selected combination of fields, calculates
global and local statistics, and writes one enriched record for every selected
pattern pair.

This manual describes `cw` version 0.9.0.

Formulas are shown twice: first in plain text for terminal and source-code
comparison, and then in LaTeX notation for citation and reuse in scholarly
writing.

A token may contain one to four slash-separated fields:

```text
f1[/f2[/f3[/f4]]]
```

The program does not assign fixed linguistic meanings such as _surface_,
_lemma_, _part of speech_, or _gloss_ to these fields. Their meanings belong to
the data design. A researcher may use the four positions for lexical,
grammatical, semantic, bibliographic, machine-generated, or other distinctions.

The central operation of `cw` is **field projection**. The fields selected by
`-p` or `--pattern-fields` form the pattern used for:

- hash registration;
- token identity;
- pair identity;
- token document frequency and IDF;
- local token frequency;
- the regular-expression search performed by `-k`.

For example, with:

```text
f1/f2/f3/f4
```

and:

```sh
cw -p 2,3
```

`cw` uses:

```text
f2/f3
```

as the computational pattern. The complete original token is nevertheless
retained and passed to downstream programs such as [`emit`](./man-emit.md).
Thus, computational identity and displayed label remain separate decisions.

The complete input is the **global corpus**. If `-k` is supplied, `cw` selects
the units containing a matching pattern and treats those units as the **local
set**. Token IDF and global pair DF continue to be calculated from the complete
input; local pair frequency and local token frequency are calculated from the
selected units.

Four CW methods are available:

- method 1: the compact explanatory formula;
- method 7: the historically adjusted waka-graph formula and current default;
- method 12: an experimental rare-pattern formula;
- method 16: the global-pair formula, marked “best” in the earlier source.

After calculating CW for every pair in the selected set, `cw` converts the CW
values to Z values using the mean and sample standard deviation of that same
selected CW distribution.

With no file operand, `cw` reads standard input. At most one input file may be
specified.

## INPUT

The preferred input format, produced by `pair` 0.2.0 or later, is:

```text
unit_id token1 token2 fq1 fq2
```

For example:

```text
u1	梅/梅/名/うめ	鴬/鴬/名/うぐひす	1	1
u1	梅/梅/名/うめ	春/春/名/はる	1	2
u2	梅/梅/名/うめ	鴬/鴬/名/うぐひす	1	1
```

The fields are separated by spaces or tabs.

`fq1` and `fq2` are the occurrence counts of the complete input tokens inside
that unit. They allow `cw` to recover exact local node frequency even when
several original tokens collapse into one projected pattern.

The older three-column form is also accepted:

```text
unit_id token1 token2
```

With three-column input, token frequency is unavailable. Methods 1 and 16 can
still be used, but methods 7 and 12 fail because their formulas require the
frequency of the key pattern. Output fields `fq1` and `fq2` are written as `-`.

Blank lines and lines whose first nonempty field begins with `#` are ignored.
Every other line must contain exactly three or five whitespace-separated
fields.

### Unit ordering

All records belonging to the same unit must be contiguous. The normal output
of `pair` satisfies this requirement.

Valid ordering:

```text
u1 a b 1 1
u1 a c 1 1
u1 b c 1 1
u2 a b 1 1
u2 a d 1 1
u2 b d 1 1
```

Invalid ordering:

```text
u1 a b 1 1
u2 a b 1 1
u1 a c 1 1
```

A repeated unit identifier is treated as the same unit only when its records
remain contiguous.

## PATTERN PROJECTION

### Default projection

The default pattern fields are:

```text
2,3,4
```

Thus:

```text
surface/lemma/class/code
```

is registered as:

```text
lemma/class/code
```

The names in this example are illustrative only. `cw` knows the positions, not
their meanings.

A token having only one field remains usable under the default projection: if
none of the requested fields exists, the complete token is used as its pattern.

### Selecting fields

Equivalent forms include:

```sh
cw -p 2,3
cw -p 2f.3f
cw --pattern-fields 2
cw --pattern-fields 1,4
```

Field selection is a set of positions from 1 to 4. The projected pattern
preserves the original field order. For example, both `3,2` and `2,3` produce
fields 2 then 3.

The following separators are accepted in a field list:

```text
,  .  /  :  +  -  _
```

Whitespace is also accepted.

### Surface compatibility mode

```sh
cw -s
cw --surface
```

is an alias for:

```sh
cw -p 1,2,3,4
```

It restores identity based on the complete token.

### Merging variants

Suppose the input contains:

```text
咲き/咲く/動詞/A
咲け/咲く/動詞/A
```

With:

```sh
cw -p 2,3
```

both become the pattern:

```text
咲く/動詞
```

Their document frequencies and local frequencies are combined. If several
complete tokens share one pattern, `cw` retains one representative complete
token for output:

1. the variant occurring in the greatest number of units is selected;
2. lexical order breaks a tie.

The representative affects downstream display, not pattern identity or the
calculated statistics.

### Pair normalization

`cw` treats pairs as unordered. The projected patterns are placed in lexical
order before hash registration.

If two different original tokens collapse into the same projected pattern, the
resulting self-pair is discarded.

Within one unit, the same projected pair is counted only once. This preserves
the historical waka-graph interpretation in which pair frequency is the number
of units containing a pair, rather than the number of repeated pair tokens
inside one unit.

## GLOBAL AND LOCAL REFERENCE SETS

Let `C` be the complete set of input units and:

```text
N = |C|
```

$$
N = |C|
$$

### Global token statistics

For a projected pattern `t`, global document frequency is:

```text
df_C(t) = number of units in C containing t
```

```math
\mathrm{df}_{C}(t) = | \{u \in C : t \in u\} |
```

Global inverse document frequency is:

```text
idf_C(t) = ln(N / df_C(t))
```

```math
\mathrm{idf}_{C}(t)
=
\ln\!\left(\frac{N}{\mathrm{df}_{C}(t)}\right)
```

`cw` uses the natural logarithm and floating-point division. No additive
smoothing is applied.

A pattern occurring in every unit has IDF 0. A pattern occurring in one unit
has IDF `ln(N)`.

### Global pair DF

For a projected pair `(t1,t2)`, global pair document frequency is:

```text
gdf_C(t1,t2) = number of units in C containing the pair
```

$$
\mathrm{gdf}_{C}(t_1,t_2)
=
\left|\{u \in C : (t_1,t_2) \in u\}\right|
$$

Method 16 uses this value to measure how unusual the combination itself is in
the global corpus.

When `-k` is active, the output column `cdf` is the **local** pair DF, not this
global pair DF. The global pair DF used by method 16 is retained internally.

### Key-selected local set

Let `r` be the POSIX extended regular expression supplied with `-k`. `cw`
matches `r` against the projected pattern, not against a fixed field and not
against the displayed representative token.

The selected unit set is:

```text
S_r = units containing at least one pattern matching r
```

$$
S_r
=
\{u \in C : \exists t \in u,\; t \models r\}
$$

Every pair occurring in a selected unit is included. `-k` therefore selects
units; it does not merely retain edges directly touching the key.

Example:

```sh
cw -p 2,3 -k '^梅/名$'
```

uses fields 2 and 3 as pattern identity, finds units containing the exact
pattern `梅/名`, and calculates local pair statistics for all pairs in those
units.

If the regular expression matches several patterns, the selected set is the
union of units containing any of them.

### Local pair frequency

For selected set `S`, `ctf` is the number of retained occurrences of a projected
pair in `S`.

Because `cw` collapses repeated occurrences of the same projected pair inside a
unit, current `ctf` is effectively:

```text
local pair DF = number of selected units containing the pair
```

The separately reported `cdf` has the same value under the current counting
rules. Both names are retained to preserve the conceptual distinction and the
stable tabular interface.

### Local token frequency

`fq(t)` is the total number of occurrences of projected pattern `t` in the
selected units. It is reconstructed from the per-unit token counts supplied by
`pair`.

When several complete tokens collapse into one pattern, their occurrence counts
are added.

### Key frequency

`key_fq` is the sum of local token frequencies for all projected patterns
matching `-k`.

For an exact key expression such as:

```sh
-k '^梅/名$'
```

`key_fq` is the local occurrence frequency of that one pattern. Methods 7 and
12 use `key_fq` and therefore require `-k` and five-column pair input.

## MATHEMATICAL NOTATION

The citation-ready formulas use the following symbols:

| Symbol                       | Meaning                                           |
| ---------------------------- | ------------------------------------------------- |
| $C$                          | complete input corpus                             |
| $N=\lvert C\rvert$           | number of units in the complete corpus            |
| $S$                          | current local unit set; $S=C$ when `-k` is absent |
| $S_r$                        | units selected by key regular expression $r$      |
| $\mathrm{df}\_{C}(t)$        | global unit frequency of pattern $t$              |
| $\mathrm{idf}\_{C}(t)$       | global inverse document frequency of pattern $t$  |
| $\mathrm{gdf}\_{C}(t_1,t_2)$ | global unit frequency of pair $(t_1,t_2)$         |
| $\mathrm{ctf}\_{S}(t_1,t_2)$ | retained local pair frequency in $S$              |
| $\mathrm{fq}\_{S_r}(r)$      | local frequency of patterns matching key $r$      |

Under the current per-unit pair-counting rule,
$\mathrm{ctf}\_{S}(t_1,t_2)$ is numerically equal to the local pair DF,
although the historical name `ctf` is retained in the output interface.

## CW METHODS

Select a method with:

```sh
cw -M NUMBER
```

The default is method 7. For reproducible research commands, specifying the
method explicitly is recommended even when using the default.

Let:

```text
w_token = sqrt(idf1 * idf2)
```

$$
w_{\mathrm{token}}(t_1,t_2)
=
\sqrt{\mathrm{idf}_{C}(t_1)\mathrm{idf}_{C}(t_2)}
$$

This geometric mean combines the global weights of the two projected patterns.

### Method 1: basic CW

```text
CW_1 = (1 + ln(ctf)) * sqrt(idf1 * idf2)
```

$$
CW_{1}(t_1,t_2)
=
\left(1+\ln \mathrm{ctf}_{S}(t_1,t_2)\right)
\sqrt{\mathrm{idf}_{C}(t_1)\mathrm{idf}_{C}(t_2)}
$$

Usage:

```sh
cw -M 1
cw -M 1 -k REGEX
```

Method 1 is the compact explanatory formula. It combines:

- local repetition of the pair;
- global weight of the two component patterns.

It can be used with or without `-k`.

### Method 7: waka-graph adjusted CW

```text
       (1 + log10(ctf)) * sqrt(idf1 * idf2)
CW_7 = --------------------------------------
                    1 + log10(key_fq)
```

$$
CW_{7}(t_1,t_2\mid r)
=
\frac{
\left(1+\log_{10}\mathrm{ctf}_{S_r}(t_1,t_2)\right)
\sqrt{\mathrm{idf}_{C}(t_1)\mathrm{idf}_{C}(t_2)}
}{
1+\log_{10}\mathrm{fq}_{S_r}(r)
}
$$

Usage:

```sh
cw -M 7 -k REGEX
```

Method 7 is the default and restores the default formula of the earlier
waka-graph implementation.

Compared with method 1:

- `log10(ctf)` makes the effect of frequently repeated pairs gentler than
  `ln(ctf)`;
- division by `1 + log10(key_fq)` adjusts the scale of graphs made from keys
  having different local frequencies.

The denominator is common to every pair selected by one key expression. It
therefore changes the scale between key graphs but not the ranking of pairs
within one key graph.

Method 7 requires `-k` and exact token-frequency input from `pair` 0.2.0 or
later.

### Method 12: rare-pattern experimental CW

```text
CW_12 = (1 + key_fq / ctf) * sqrt(idf1 * idf2)
```

To express the historical integer quotient explicitly:

$$
CW_{12}(t_1,t_2\mid r)
=
\left(
1+
\left\lfloor
\frac{\mathrm{fq}_{S_r}(r)}
     {\mathrm{ctf}_{S_r}(t_1,t_2)}
\right\rfloor
\right)
\sqrt{\mathrm{idf}_{C}(t_1)\mathrm{idf}_{C}(t_2)}
$$

Usage:

```sh
cw -M 12 -k REGEX
```

Method 12 restores the historical experimental formula that strongly rewards
pairs occurring rarely inside the key-selected set.

To reproduce the earlier C implementation, `key_fq / ctf` is intentionally
calculated as an integer quotient before conversion to floating point.

Method 12 requires `-k` and exact token-frequency input.

### Method 16: global-pair CW

```text
CW_16 = (1 + ln(N / global_pair_df))
        * sqrt(idf1 * idf2)
        * (1 + ln(local_ctf))
```

$$
CW_{16}(t_1,t_2\mid S)
=
\left(
1+
\ln\!\left(
\frac{N}{\mathrm{gdf}_{C}(t_1,t_2)}
\right)
\right)
\sqrt{\mathrm{idf}_{C}(t_1)\mathrm{idf}_{C}(t_2)}
\left(
1+\ln \mathrm{ctf}_{S}(t_1,t_2)
\right)
$$

Usage:

```sh
cw -M 16
cw -M 16 -k REGEX
```

Method 16 restores the structure of the historical formula marked “best” in
the earlier source. It combines three distinct kinds of evidence:

1. **global token weight** — whether the two patterns themselves are
   informative;
2. **global pair weight** — whether their combination is unusual in the full
   corpus;
3. **local repetition** — whether that combination recurs in the selected
   topic set.

In words:

> CW is high when globally weighty patterns form a globally unusual
> combination that recurs in the selected local set.

The current implementation calculates `N / global_pair_df` with floating-point
division. It does not reproduce the accidental integer truncation of the old C
expression.

Method 16 can be used without `-k`. In that case the complete input is both the
global corpus and the local set.

### Choosing a method

A practical interpretation is:

| Method | Main purpose                                                |
| ------ | ----------------------------------------------------------- |
| `1`    | explain the basic CW principle                              |
| `7`    | draw historically adjusted waka graphs; current default     |
| `12`   | experiment with locally rare patterns                       |
| `16`   | combine global token, global pair, and local topic evidence |

Methods are not interchangeable rescalings. They can change both numerical
scale and pair ranking. CW thresholds such as `emit -W` should therefore be
chosen separately for each method. Z thresholds are often more convenient
when comparing methods with different CW scales.

## Z VALUE

After CW has been calculated for all selected pairs, `cw` calculates the mean
and sample standard deviation of that selected CW distribution.

For pair CW value `x`:

```text
z = (x - mean_selected_cw) / sd_selected_cw
```

$$
z_i
=
\frac{CW_i-\overline{CW}_{S}}{s_{CW,S}}
$$

The sample standard deviation uses denominator `n - 1`.

The selected distribution is:

- all output pairs when no `-k` is used;
- all pairs occurring in key-selected units when `-k` is used.

Thus IDF and Z use different reference sets:

- IDF measures weight against the global input corpus;
- Z measures relative position inside the current output distribution.

If fewer than two pairs are selected, or if the selected CW values have zero
standard deviation, Z is written as `0`.

## OUTPUT

Each distinct selected projected pair is written once. The output contains 12
fixed tab-separated fields followed by the selected unit identifiers:

```text
token1 token2 ctf cdf df1 idf1 fq1 df2 idf2 fq2 cw z unit_id...
```

| Column | Name         | Meaning                                                        |
| -----: | ------------ | -------------------------------------------------------------- |
|      1 | `token1`     | representative complete token for pattern 1                    |
|      2 | `token2`     | representative complete token for pattern 2                    |
|      3 | `ctf`        | retained local pair frequency; currently one per selected unit |
|      4 | `cdf`        | number of selected units containing the pair                   |
|      5 | `df1`        | global unit frequency of pattern 1                             |
|      6 | `idf1`       | global IDF of pattern 1                                        |
|      7 | `fq1`        | local occurrence frequency of pattern 1, or `-`                |
|      8 | `df2`        | global unit frequency of pattern 2                             |
|      9 | `idf2`       | global IDF of pattern 2                                        |
|     10 | `fq2`        | local occurrence frequency of pattern 2, or `-`                |
|     11 | `cw`         | CW value under the selected method                             |
|     12 | `z`          | Z value within the selected CW distribution                    |
|  13... | `unit_id...` | selected units containing the pair                             |

The CW and Z columns are therefore:

```text
$11 = cw
$12 = z
```

This differs from earlier output formats that did not contain `fq1` and `fq2`.
Scripts written for older versions must update their field offsets.

For example, display the highest CW pairs with:

```sh
sort -t $'\t' -k11,11gr
```

or:

```sh
awk -F '\t' '{ print $11, $1, $2 }' | sort -gr
```

The trailing unit identifiers are sorted and are unique under the current
per-unit pair-counting rule.

Output pairs are sorted lexically by their projected pattern keys. Because
columns 1 and 2 contain representative complete tokens, their visible order may
not by itself reveal the projected keys used for sorting.

`cw` does not prune by CW or Z. Filtering and graphical serialization belong
to `emit`.

## OPTIONS

### `-p LIST`, `--pattern-fields LIST`

Use the selected token fields to construct the computational pattern.

Default:

```text
2,3,4
```

Examples:

```sh
-p 2
-p 2,3
-p 1,4
-p 2f.3f.4f
```

### `-k REGEX`, `--key REGEX`

Select all units containing at least one projected pattern matching the POSIX
extended regular expression `REGEX`.

The expression is applied to the entire projected pattern. Anchor it when an
exact match is required:

```sh
-k '^梅/名$'
```

Selection is by unit. All pairs in selected units are calculated.

### `-M NUMBER`, `--method NUMBER`

Select CW method `1`, `7`, `12`, or `16`.

Default:

```text
7
```

Methods 7 and 12 require `-k`.

### `-s`, `--surface`

Use all available fields as the pattern. Equivalent to:

```sh
-p 1,2,3,4
```

### `-h`, `--help`

Display command help and exit.

### `-v`, `--version`

Display version information and exit.

## EXAMPLES

### Basic whole-corpus calculation

Because the default method 7 requires a key, select method 1 or 16 explicitly
for an unselected corpus:

```sh
./pair units.txt | ./cw -M 1
```

```sh
./pair units.txt | ./cw -M 16
```

### Waka graph with the historical default formula

```sh
awk '$1 == 1' tests/data/hachidaishu-pair.txt |
  ./pair |
  ./cw -p 2,3 -k '^梅/名$' -M 7
```

Although `-M 7` is the default, writing it explicitly records the research
condition.

### Global-pair calculation for the same key

```sh
awk '$1 == 1' tests/data/hachidaishu-pair.txt |
  ./pair |
  ./cw -p 2,3 -k '^梅/名$' -M 16
```

### Visualize high-Z M16 pairs

```sh
awk '$1 == 1' tests/data/hachidaishu-pair.txt |
  ./pair |
  ./cw -p 2,3 -k '^梅/名$' -M 16 |
  ./emit -Z 2 |
  neato -Tsvg -o ume-m16.svg
```

### Use one field as identity

```sh
./pair units.txt |
  ./cw -p 2 -k '^plum$' -M 16
```

Here field 2 alone defines hash identity, pair identity, IDF, key matching, and
local frequency.

### Compare methods

```sh
for method in 1 7 12 16; do
  ./pair units.txt |
    ./cw -p 2,3 -k '^梅/名$' -M "$method" \
    > "ume-M${method}.tsv"
done
```

Compare Z rather than raw CW when the methods have substantially different
scales.

### Inspect the fixed columns

```sh
./pair units.txt |
  ./cw -p 2,3 -k '^梅/名$' -M 16 |
  awk -F '\t' '{
    printf "%s -- %s  ctf=%s  idf=(%s,%s)  fq=(%s,%s)  cw=%s  z=%s\n",
           $1, $2, $3, $6, $9, $7, $10, $11, $12
  }'
```

## REPRODUCIBLE RESEARCH RECORD

A reproducible CW analysis should record at least:

1. the command that defines the global input corpus;
2. the version of `pair` and `cw`;
3. the pattern-field projection;
4. the key regular expression and therefore the local set;
5. the CW method;
6. any downstream `emit` thresholds and display configuration.

For example:

```sh
pair --version
cw --version

awk '$1 == 1' tests/data/hachidaishu-pair.txt |
  ./pair |
  ./cw -p 2,3 -k '^梅/名$' -M 16 |
  ./emit -Z 2 -c config/emit-config.json |
  neato -Tsvg -o ume-m16-z2.svg
```

The command then states, without changing program code:

- which corpus supplied global token and pair statistics;
- which fields defined pattern identity;
- which pattern selected the local units;
- which mathematical model produced CW;
- which display conditions produced the graph.

## DIAGNOSTICS

Typical diagnostics include:

```text
cw: method 7 requires -k/--key because key_fq is part of the formula
```

Use `-k`, or select method 1 or 16.

```text
cw: method 7 requires token frequencies; regenerate the pair data with pair 0.2.0 or later
```

Regenerate the input with a current `pair`, or use method 1 or 16 with the older
three-column format.

```text
cw: invalid regular expression '...': ...
```

Correct the POSIX extended regular expression supplied to `-k`.

```text
cw: --method requires 1, 7, 12, or 16
```

Choose one of the implemented methods.

If the key expression matches no pattern, `cw` writes no output and exits
successfully.

## EXIT STATUS

`cw` returns:

```text
0  successful completion
nonzero  invalid options, malformed input, missing required frequency data,
         file errors, allocation failure, or another processing error
```

## IMPLEMENTATION NOTES

`cw` stores only observed patterns and observed pairs in hash tables. It does
not allocate a dense token-by-token matrix.

Global token statistics, global pair statistics, representative variants, and
unit identifiers are collected while reading the input. Key selection then
reuses those statistics to calculate local frequencies without changing the
global reference corpus.

CW distribution mean and sample standard deviation are calculated with a
one-pass numerically stable recurrence.

The original complete token fields are preserved so that `emit` can construct
labels independently of the fields used by `cw` for pattern identity.

## SEE ALSO

[`pair`](./man-pair.md), [`emit`](./man-emit.md), `sort(1)`, `awk(1)`,
`neato(1)`

## AUTHOR

Hilofumi Yamamoto

## LICENSE

MIT License
