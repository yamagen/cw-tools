# cw

## NAME

`cw` — calculate co-occurrence weights and selected-set Z values from token pairs using global inverse document frequencies

## SYNOPSIS

```sh
cw [OPTION]... [FILE]
```

## DESCRIPTION

`cw` reads pair records produced by [`pair`](./man-pair.md), calculates token and pair statistics over the complete input, and writes an enriched pair table containing CW values, Z values, and the unit identifiers in which each pair occurs.

This manual describes `cw` version 0.4.0.

Each input record contains a unit identifier and two complete tokens:

```text
unit_id token1 token2
```

The fields may be separated by spaces or tabs. A token may itself contain slash-separated fields:

```text
surface/lemma/field3/field4
```

`cw` treats the complete token string as the identity of the token when calculating document frequencies and CW values.

The complete input determines the global corpus:

- the number of units $N$;
- the document frequency and IDF of each token;
- the occurrence and document frequencies of each pair;
- the unit identifiers in which each pair occurs.

When `-k` or `--key` is specified, `cw` interprets its argument as a POSIX extended regular expression and matches it against the first slash-separated field, the surface form, of each token. It then selects all units containing at least one matching token and recalculates pair frequencies within those units. The token document frequencies and IDF values remain those calculated from the complete input.

Thus, key selection affects pair frequency but does not change the global IDF reference corpus.

After calculating every CW value in the selected pair set, `cw` calculates that set's mean and sample standard deviation and converts each CW value to a Z value. With `-k`, the Z reference distribution is therefore the pair set obtained from the selected units. Without `-k`, it is the pair set obtained from the complete input.

With no file operand, `cw` reads from standard input. At most one input file may be specified.

## INPUT

The expected input format is:

```text
unit_id token1 token2
```

For example:

```text
10001	年/年/名/とし	春/春/名/はる
10001	年/年/名/とし	来/来/カ変/く
10002	春/春/名/はる	水/水/名/みづ
```

Blank lines and lines whose first nonempty field begins with `#` are ignored.

Every other input line must contain exactly three whitespace-separated fields.

### Unit ordering

All records belonging to the same unit must be contiguous. The normal output of `pair` satisfies this requirement.

Unit identifiers must identify units uniquely within the complete input dataset. A repeated identifier is treated as the same unit.

The following ordering is valid:

```text
u1 a b
u1 a c
u1 b c
u2 a b
u2 a d
u2 b d
```

The following ordering must not be used:

```text
u1 a b
u2 a b
u1 a c
```

because the records for `u1` are not contiguous.

## OUTPUT

Each distinct selected pair is written once. The output contains ten fixed tab-separated fields followed by one or more unit identifiers:

```text
token1 token2 ctf cdf df1 idf1 df2 idf2 cw z unit_id...
```

| Field | Meaning |
| --- | --- |
| `token1` | first token |
| `token2` | second token |
| `ctf` | total occurrences of the pair in the selected units |
| `cdf` | number of selected units containing the pair |
| `df1` | number of global input units containing `token1` |
| `idf1` | global inverse document frequency of `token1` |
| `df2` | number of global input units containing `token2` |
| `idf2` | global inverse document frequency of `token2` |
| `cw` | co-occurrence weight of the pair |
| `z` | Z value of `cw` within the selected pair set |
| `unit_id...` | selected unit identifiers in which the pair occurs |

The fixed numeric columns most often passed to downstream tools are:

```text
field 9   cw
field 10  z
```

One `unit_id` is written for every pair occurrence. Duplicate unit identifiers are retained when the same pair occurs more than once in one unit.

Consequently:

$$
\operatorname{ctf}(t_1,t_2)
=
\text{number of trailing unit identifiers}
$$

and:

$$
\operatorname{cdf}(t_1,t_2)
=
\text{number of distinct trailing unit identifiers}
$$

Pairs are sorted lexically by `token1` and then by `token2`.

When `-k` is specified, pairs having no occurrence in a selected unit are omitted. CW and Z values are written with sufficient precision for downstream numerical processing. `cw` does not prune the selected pair table by CW or Z threshold.

## GLOBAL CORPUS AND SELECTED UNITS

Let $C$ be the complete set of units supplied to `cw`.

The total number of global units is:

$$
N_C = |C|
$$

The global document frequency of token $t$ is:

$$
\operatorname{df}_C(t)
=
\left|
\{u \in C : t \text{ occurs in }u\}
\right|
$$

The global inverse document frequency is:

$$
\operatorname{idf}_C(t)
=
\log\left(
\frac{N_C}{\operatorname{df}_C(t)}
\right)
$$

`cw` uses the natural logarithm.

No smoothing or additive constant is applied. A token occurring in every global unit therefore has:

$$
\operatorname{idf}_C(t)
=
\log\left(
\frac{N_C}{N_C}
\right)
=0
$$

A token occurring in only one of $N_C$ units has:

$$
\operatorname{idf}_C(t)=\log N_C
$$

### Without `-k`

When no key is specified, all global units are selected:

$$
S=C
$$

Pair frequencies are therefore calculated over the complete input.

### With `-k`

Let $r$ be the regular expression supplied with `-k`. Let $\operatorname{surface}(t)$ denote the first slash-separated field of token $t$.

The selected unit set is:

$$
S_r
=
\left\{
u \in C :
\text{some token }t\text{ in }u
\text{ has }\operatorname{surface}(t)\text{ matching }r
\right\}
$$

The pair occurrence frequency is then:

$$
\operatorname{ctf}_{S_r}(t_1,t_2)
=
\text{number of records for }(t_1,t_2)
\text{ in units belonging to }S_r
$$

The pair document frequency is:

$$
\operatorname{cdf}_{S_r}(t_1,t_2)
=
\left|
\{u \in S_r : (t_1,t_2)\text{ occurs in }u\}
\right|
$$

The token IDF values are not recalculated over $S_r$. They remain the global values:

$$
\operatorname{idf}_C(t)
$$

calculated from the complete input corpus.

## CW AND Z DEFINITIONS

For a selected unit set $S$, the CW value of pair $(t_1,t_2)$ is:

$$
\operatorname{cw}_{C,S}(t_1,t_2)
=
\left(
1+\log \operatorname{ctf}_{S}(t_1,t_2)
\right)
\sqrt{
\operatorname{idf}_{C}(t_1)
\operatorname{idf}_{C}(t_2)
}
$$

Without `-k`:

$$
S=C
$$

With `-k REGEX`:

$$
S=S_{\mathrm{REGEX}}
$$

Thus:

- $N$, `df1`, `idf1`, `df2`, and `idf2` are calculated from the complete input corpus;
- `ctf`, `cdf`, `cw`, `z`, and the trailing `unit_id` fields are calculated from or refer to the selected units and their resulting pair set.

### Z value

Let $P_S$ be the set of distinct pairs having at least one occurrence in the selected unit set $S$:

$$
P_S
=
\left\{
(t_1,t_2) : \operatorname{ctf}_S(t_1,t_2)>0
\right\}
$$

and let:

$$
n_S=|P_S|
$$

The mean CW value in the selected pair set is:

$$
\overline{\operatorname{cw}}_S
=
\frac{1}{n_S}
\sum_{p\in P_S}\operatorname{cw}_{C,S}(p)
$$

For $n_S>1$, `cw` uses the sample standard deviation:

$$
s_S
=
\sqrt{
\frac{
\sum_{p\in P_S}
\left(
\operatorname{cw}_{C,S}(p)
-
\overline{\operatorname{cw}}_S
\right)^2
}{n_S-1}
}
$$

The Z value of pair $p$ is:

$$
z_{C,S}(p)
=
\frac{
\operatorname{cw}_{C,S}(p)
-
\overline{\operatorname{cw}}_S
}{s_S}
$$

When the selected pair set contains fewer than two pairs, or when its standard deviation is zero, `cw` writes `0` for every Z value because no relative dispersion can be calculated.

Apart from output rounding, a nondegenerate Z column has mean 0 and sample standard deviation 1. Standardization changes the origin and scale of the distribution but not its skewness or kurtosis. It does not make a non-normal distribution normal.

The usual interpretation is:

```text
z =  0   at the selected-set CW mean
z =  1   one selected-set sample SD above the mean
z = -1   one selected-set sample SD below the mean
```

Z calculation is automatic. No command-line option is required to enable it.

## WHY IDF AND Z USE DIFFERENT REFERENCE SETS

IDF and Z answer different questions and therefore require different reference sets.

### IDF: the weight of a token

IDF asks how rare or common a token is in the broader corpus. It must therefore be calculated from a sufficiently wide text collection. If poems containing `桜` were extracted before IDF calculation, `桜` would occur in nearly every selected poem and its IDF would approach zero. That would describe the consequence of the selection, not the general weight of the token.

For this reason, `cw` first calculates token IDF from the complete input corpus $C$.

### Z: the relative position of a pair within one selected distribution

Z asks how far a pair's CW value lies above or below the mean of the pair distribution currently being analysed. Poems selected by `桜` have their own CW distribution; poems selected by `梅` have another. Their means and standard deviations need not be equal.

This is analogous to standardizing scores from two different examinations. Examination A and examination B may have different mean scores and dispersions. Each score is therefore standardized against the mean and standard deviation of its own examination, not against a single pooled mean.

Accordingly:

```text
IDF reference    complete input corpus
CW frequency     selected units
Z reference      CW values of the selected pair set
```

The broad corpus gives each token its general weight. The selected pair set gives each pair its relative prominence within the current topic or condition.

## KEY SELECTION

### `-k REGEX`, `--key REGEX`

Select all units containing at least one token whose surface field matches `REGEX`.

`REGEX` is a POSIX extended regular expression. It is matched against the complete surface field, but POSIX regular-expression matching is unanchored unless anchors are written explicitly. Therefore, a plain string also acts as a substring search.

For example:

```sh
cw -k '桜'
```

matches surfaces such as:

```text
桜
桜花
山桜
```

To require an exact surface match, use both anchors:

```sh
cw -k '^桜$'
```

This matches `桜` but not `桜花` or `山桜`.

Other examples are:

```sh
cw -k '梅|桜'
cw -k '^春'
cw -k '春$'
cw -k '^(梅|桜)$'
```

They mean:

| Regular expression | Selected surface forms |
| --- | --- |
| `梅|桜` | forms containing `梅` or `桜` |
| `^春` | forms beginning with `春` |
| `春$` | forms ending with `春` |
| `^(梅|桜)$` | exactly `梅` or exactly `桜` |

Quote regular expressions in the shell, especially when they contain characters such as `|`, `*`, `?`, `[`, `]`, `(`, `)`, `^`, or `$`.

### Unit selection, not pair filtering

`-k` selects units. It does not restrict output to pairs having the matching token as an endpoint.

For example:

```sh
pair < input.txt |
    cw -k '桜'
```

selects every unit containing a surface matching `桜`, then outputs every pair occurring in those selected units.

A later command such as:

```sh
pair < input.txt |
    cw -k '桜' |
    grep '桜'
```

further restricts the displayed records to lines containing `桜`. That `grep` affects display only; it is not part of the unit-selection calculation performed by `cw`.

### Key not found

When no surface in the global input matches the regular expression, `cw` produces no pair records and exits successfully.

### Invalid regular expressions

An invalid regular expression causes a diagnostic and a nonzero exit status. For example:

```sh
cw -k '['
```

produces a message of the form:

```text
cw: invalid regular expression '[': Invalid regular expression
```

## CORPUS SELECTION

The input supplied to `cw` defines the global IDF corpus.

For example, to use the complete Kokinshu section as the global corpus and select poems containing a surface matching `鴬`:

```sh
grep '^1' tests/data/hachidaishu-pair.txt |
    pair |
    cw -k '鴬'
```

Here:

- $N$ is the number of Kokinshu poems represented in the input;
- token document frequencies and IDF values are calculated over the complete Kokinshu input;
- pair frequencies are calculated only over Kokinshu poems containing a matching surface.

The key must not be used to filter the source data before `cw` when global IDF is intended.

Incorrect:

```sh
grep '^1' tests/data/hachidaishu-pair.txt |
    grep '鴬' |
    pair |
    cw
```

This makes the poems containing `鴬` themselves the global corpus. The IDF of the selected token may then become zero because it occurs in every input unit.

Correct:

```sh
grep '^1' tests/data/hachidaishu-pair.txt |
    pair |
    cw -k '鴬'
```

This calculates global IDF first and applies the regular expression only to unit selection.

Several collections may be combined upstream. For example, when the input contains poems from both the Kokinshu and Gosenshu, $N$ and IDF are calculated over the combined set of poems.

## PAIR FREQUENCIES

In the default mode of `pair`, each distinct unordered pair is emitted at most once per unit. In that case:

$$
\operatorname{ctf}_{S}(t_1,t_2)
=
\operatorname{cdf}_{S}(t_1,t_2)
$$

Adjacent or windowed modes may emit the same pair more than once in one unit. In that case:

- `ctf` counts every selected pair record;
- `cdf` counts each selected unit only once;
- duplicate trailing `unit_id` values are retained.

For example:

```text
a b 4 3 ... u1 u2 u2 u3
```

means:

```text
ctf = 4
cdf = 3
```

because the pair occurs twice in `u2`.

## TOKEN FREQUENCY

`cw` calculates token document frequency, not the original token frequency in the source text.

After `pair` expands a unit into pairs, a token may occur as a pair endpoint several times. Counting endpoint occurrences would not reproduce its frequency in the original text.

For this reason:

- `df1` and `df2` count units containing each token;
- `idf1` and `idf2` are derived from those document frequencies;
- no endpoint count is labelled as source-text token frequency.

## OPTIONS

### `-k REGEX`, `--key REGEX`

Restrict pair occurrences to units containing a token whose surface field matches the POSIX extended regular expression `REGEX`, while retaining the global document frequencies and IDF values calculated from the complete input. CW mean, sample standard deviation, and Z values are then calculated from the resulting selected pair set.

### `-h`, `--help`

Display a summary of command-line usage and exit.

### `-v`, `--version`

Display version information and exit.

## EXAMPLES

Calculate CW and Z values from standard input:

```sh
pair < input.txt |
    cw
```

Process the Kokinshu section of the Hachidaishu data:

```sh
grep '^1' tests/data/hachidaishu-pair.txt |
    pair |
    cw
```

Use the complete Kokinshu section for global IDF and select poems containing `桜`, `桜花`, `山桜`, or another surface containing `桜`:

```sh
grep '^1' tests/data/hachidaishu-pair.txt |
    pair |
    cw -k '桜'
```

Select only poems containing the exact surface `桜`:

```sh
grep '^1' tests/data/hachidaishu-pair.txt |
    pair |
    cw -k '^桜$'
```

Select poems containing a surface with either `梅` or `桜`:

```sh
grep '^1' tests/data/hachidaishu-pair.txt |
    pair |
    cw -k '梅|桜'
```

Select surfaces beginning with `春`:

```sh
grep '^1' tests/data/hachidaishu-pair.txt |
    pair |
    cw -k '^春'
```

Read pair records from a file:

```sh
cw pairs.txt
```

Inspect the enriched pair table:

```sh
pair < input.txt |
    cw |
    less
```

Extract CW values for distribution analysis:

```sh
pair < input.txt |
    cw |
    awk -F '\t' '{ print $9 }' |
    rbin -s
```

Extract Z values for distribution analysis:

```sh
pair < input.txt |
    cw |
    awk -F '\t' '{ print $10 }' |
    rbin -s
```

For a nondegenerate pair set, the Z column should have a mean close to 0 and a sample standard deviation close to 1. Because its mean is zero by construction, the coefficient of variation is not meaningful for the Z column.

Calculate the CW distribution for units containing a surface matching `時鳥`:

```sh
grep '^1' tests/data/hachidaishu-pair.txt |
    pair |
    cw -k '時鳥' |
    awk -F '\t' '{ print $9 }' |
    rbin -s
```

Inspect the corresponding within-selection Z distribution:

```sh
grep '^1' tests/data/hachidaishu-pair.txt |
    pair |
    cw -k '時鳥' |
    awk -F '\t' '{ print $10 }' |
    rbin -s
```

Pass the complete calculated table to `emit`:

```sh
pair < input.txt |
    cw |
    emit
```

For example, `emit` may use the Z column to display only edges at or above a chosen threshold:

```sh
pair < input.txt |
    cw -k '^桜$' |
    emit -Z 1
```

Thresholding and pruning belong to the output stage. `cw` itself writes every selected pair and does not discard records according to CW or Z thresholds.

## EXAMPLE CALCULATION

Suppose the input contains three units:

```text
u1 a b
u1 a c
u1 b c
u2 a b
u2 a d
u2 b d
u3 b c
```

Then:

```text
N     = 3
df(a) = 2
df(b) = 3
df(c) = 2
df(d) = 1
```

Therefore:

$$
\operatorname{idf}(a)=\log(3/2)
$$

$$
\operatorname{idf}(b)=\log(3/3)=0
$$

$$
\operatorname{idf}(c)=\log(3/2)
$$

$$
\operatorname{idf}(d)=\log(3)
$$

The pair `(a,c)` occurs once:

```text
ctf(a,c) = 1
cdf(a,c) = 1
```

Its CW value is:

$$
\begin{aligned}
\operatorname{cw}(a,c)
&=
\left(1+\log 1\right)
\sqrt{\log(3/2)\log(3/2)}
\\
&=\log(3/2)
\end{aligned}
$$

Across all five distinct pairs in this example, the CW mean and sample standard deviation are approximately:

```text
mean = 0.214576945806
sd   = 0.308072465472
```

The Z value of `(a,c)` is therefore approximately:

$$
z(a,c)
=
\frac{0.405465108108-0.214576945806}{0.308072465472}
=0.619620977841
$$

The output record is therefore of the form:

```text
a	c	1	1	2	0.405465108108	2	0.405465108108	0.40546510810816438	0.6196209778406879	u1
```

The pair `(a,b)` occurs twice:

```text
ctf(a,b) = 2
cdf(a,b) = 2
```

However:

$$
\operatorname{idf}(b)=0
$$

so:

$$
\operatorname{cw}(a,b)=0
$$

### Keyed calculation

With:

```sh
cw -k '^d$'
```

only `u2` is selected because the surface `d` occurs only in `u2`.

The global values remain:

```text
N     = 3
df(a) = 2
df(d) = 1
```

For pair `(a,d)` within the selected units:

```text
ctf = 1
cdf = 1
```

and:

$$
\operatorname{cw}(a,d)
=
\sqrt{\log(3/2)\log(3)}
$$

The selected pair set contains `(a,b)`, `(a,d)`, and `(b,d)`. Their CW values are approximately `0`, `0.667419620924`, and `0`, so the selected-set mean and sample standard deviation are approximately:

```text
mean = 0.222473206975
sd   = 0.385334897803
```

The Z value of `(a,d)` is therefore approximately `1.15470053838`. The trailing unit list contains only:

```text
u2
```

## IMPLEMENTATION

The input stream is read once.

During the scan, the shared statistics implementation maintains:

- the set of global unit identifiers;
- token document frequencies;
- pair occurrence and document frequencies;
- one unit identifier for every pair occurrence.

After the complete input has been read, global IDF values are calculated.

When `-k` is specified, `cw` compiles the argument once with the POSIX regular-expression interface using extended syntax. It matches the expression against the surface field of each distinct token, constructs the selected unit set, and filters each pair's occurrence-unit list against that set.

After selection, `cw` makes one in-memory pass over the selected pairs to calculate CW mean and sample standard deviation using a numerically stable one-pass recurrence. It then makes a second in-memory pass to calculate each pair's CW and Z values and write the output. The input stream itself is still read only once.

`cw` calls `setlocale()` and performs regular-expression matching according to the active locale. UTF-8 text therefore requires an appropriate UTF-8 locale. No Windows wide-character library is used.

No temporary file, second input scan, or rewind is required.

Memory use depends principally on:

- the number of distinct unit identifiers;
- the number of distinct tokens;
- the number of distinct pairs;
- the total number of pair occurrences, because one unit identifier is retained for every occurrence.

The statistics and token-field functions shared with `cm` are implemented in `common.c` and declared in `common.h`.

## DIAGNOSTICS

A malformed input line causes an error such as:

```text
cw: -:12: expected: unit_id token1 token2
```

Specifying more than one input file causes an error such as:

```text
cw: at most one input file may be specified
```

An invalid regular expression causes an error such as:

```text
cw: invalid regular expression '[': Invalid regular expression
```

An unreadable file, a read failure, or a memory allocation failure also causes a diagnostic on standard error and a nonzero exit status.

## EXIT STATUS

`0`
: Successful completion.

nonzero
: An error occurred.

## PIPELINE

`cw` follows `pair` in the `cw-tools` pipeline:

```text
unit-based token data
        |
        v
      pair
        |
        v
       cw
        |
        +-- global N, df, and idf
        +-- selected ctf and cdf
        +-- CW value
        +-- selected-set CW mean and sample SD
        +-- Z value
        +-- occurrence unit identifiers
        |
        v
      emit
```

A typical command is:

```sh
pair < input.txt |
    cw |
    emit
```

A key-based command is:

```sh
pair < input.txt |
    cw -k '梅|桜' |
    emit
```

The division of responsibility is:

```text
pair   generate pairs
cw     calculate IDF, selected frequencies, CW, and Z
rbin   report numerical distributions and statistics
emit   select and render edges and nodes
```

Thus, statistical reporting is delegated to `rbin`, while pruning and visualization are delegated to `emit`. `cw` remains the calculation stage and preserves the complete selected pair table.

`cm` is a parallel consumer of the pair stream:

```text
                 +--> cw --> emit
pair output -----|
                 +--> cm --> emit
```

## SEE ALSO

[`pair`](./man-pair.md), `cm`, `emit`, `rbin`

## AUTHOR

Hilofumi Yamamoto  
Institute of Science Tokyo

## LICENSE

MIT License
