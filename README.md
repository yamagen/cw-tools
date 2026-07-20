# cw-tools: Transparent Unix Filters for Exploratory Text Analysis

Last updated: 2026/07/20-09:31:54.

Hilofumi Yamamoto, Ph.D.  
Institute of Science Tokyo

## Overview

`cw-tools` is a collection of small command-line programs for exploratory
text-data analysis. The tools expose each analytical stage as a readable Unix
pipeline instead of hiding the complete procedure inside one application.

```text
unit-based token data
        |
        v
      pair            complete-token relations within each unit
        |
        v
       cw              pattern projection, global/local statistics, CW, and Z
        |
        v
  grep / awk / sort    explicit researcher-defined selection and pruning
        |
        v
      emit             JSON, Graphviz DOT, Markdown, LaTeX, or HTML
        |
        +----> neato / dot / sfdp ----> SVG, PDF, PNG, ...
        |
        +----> publication tables
```

The project is designed as **digital humanities that works alongside the
researcher** (_yorisou DH_):

> The computer should not replace the researcher's decisions. It should make
> those decisions executable, inspectable, citable, and reproducible.

A researcher does not need to rewrite the C programs. The researcher does,
however, retain responsibility for defining the analytical unit, choosing the
pattern fields, selecting a CW method, recording filters, and explaining the
procedure in a paper.

## Current development snapshot

| Program |      Version | Status      | Responsibility                                                      |
| ------- | -----------: | ----------- | ------------------------------------------------------------------- |
| `pair`  |        0.2.0 | implemented | generate complete-token pairs and preserve per-unit token frequency |
| `cw`    |        0.9.0 | implemented | project patterns and calculate global/local statistics, CW, and Z   |
| `emit`  |        0.7.0 | implemented | serialize graphs and publication tables safely                      |
| `cm`    | design stage | planned     | model directed adjacent transitions including `<BOS>` and `<EOS>`   |

The version of every program used in an analysis should be recorded. Method
numbers and command-line options should also be written explicitly even when a
default exists.

## Input data

The tools read unit-based token data. Each input line begins with a unit
identifier followed by one or more tokens:

```text
unit_id token1 token2 ... tokenN
```

A token may contain one to four slash-separated fields:

```text
f1
f1/f2
f1/f2/f3
f1/f2/f3/f4
```

For example:

```text
u1 梅/梅/名/うめ 鴬/鴬/名/うぐひす 春/春/名/はる
u2 桜/桜/名/さくら 花/花/名/はな 散る/散る/動/ちる
```

The field meanings are entirely user-defined. A Japanese morphological file
might use:

```text
surface/lemma/part-of-speech/reading
```

Another project may use different information in the same four positions.
The programs know field offsets, not linguistic categories.

This distinction is fundamental:

```text
complete token
    the full observed record retained by pair and cw

pattern
    the computational identity selected by cw -p

label
    the human-facing representation selected by emit configuration
```

## Quick start

Compile the programs:

```sh
make
```

A basic whole-corpus calculation is:

```sh
./pair < input.txt | ./cw -M 1 > result.tsv
```

A key-selected M16 graph can be produced as follows:

```sh
./pair < input.txt \
  | ./cw -p 2,3 -k '^梅/名$' -M 16 \
  > ume-cw.tsv

awk -F '\t' '$12 >= 2' ume-cw.tsv \
  | ./emit -c config/emit-config.json \
  | neato -Tsvg \
  > ume.svg
```

The same measured rows can be turned into a LaTeX table:

```sh
sort -t $'\t' -k12,12gr ume-cw.tsv \
  | head -20 \
  | ./emit -c config/emit-table.config -T tex \
  > ume-top20.tex
```

The commands themselves form a compact research record:

```text
-p 2,3          fields defining computational identity
-k '^梅/名$'    key pattern defining the local set
-M 16           CW method
$12 >= 2        Z threshold
-k12,12gr       descending Z order
head -20        number of rows presented
-T tex          publication-table format
```

## `pair`: observe relations before statistical weighting

`pair` reads each unit as a token sequence and writes pair records:

```text
unit_id token1 token2 fq1 fq2
```

`fq1` and `fq2` are the occurrence counts of the complete endpoint tokens in
the source unit. They preserve information needed when several complete tokens
are later merged into one projected pattern.

The default mode emits every unordered pair of distinct complete-token types
once per unit:

```sh
./pair input.txt
```

Adjacent, windowed, and directed relations are also available:

```sh
./pair --adjacent input.txt
./pair --window 3 input.txt
./pair --adjacent --ordered input.txt
```

Unlike the default type-pair mode, adjacent and windowed modes preserve token
occurrences. The same pair may therefore be emitted more than once in one
unit.

`pair` is useful independently of CW. Its line-oriented output can be inspected
with ordinary Unix tools before any statistical identity or weighting is
imposed:

```sh
./pair input.txt | grep '梅'
./pair --adjacent --ordered input.txt | sort | uniq -c
./pair input.txt | awk -F '\t' '$4 >= 2 || $5 >= 2'
```

This stage shows what combinations are actually present in the source units,
including surface-form variants that may disappear after pattern projection.

See [`docs/man-pair.md`](docs/man-pair.md) for the complete specification.

## `cw`: define patterns and calculate weights

`cw` receives the pair stream and performs the statistical stage. Its main
responsibilities are:

1. retain the complete representative tokens;
2. project selected fields onto computational patterns;
3. calculate global token DF and IDF;
4. calculate global pair DF;
5. define a local set with `-k`, while retaining the complete input as the
   global reference corpus;
6. calculate local pair and token frequencies;
7. calculate a selected CW method;
8. standardize the selected CW distribution as Z values.

### Pattern projection

The default pattern fields are `2,3,4`:

```sh
./cw
```

Select fields explicitly for reproducibility:

```sh
./cw -p 2,3
./cw --pattern-fields 1,4
./cw -s                    # complete-token compatibility mode: 1,2,3,4
```

Suppose the input contains:

```text
咲き/咲く/動/x
咲け/咲く/動/x
```

With:

```sh
./cw -p 2,3
```

both forms have the computational identity:

```text
咲く/動
```

The original complete token remains available for inspection and display.
Pattern identity is therefore independent of visible labeling.

### Global and local reference sets

Let:

| Symbol                               | Meaning                                              |
| ------------------------------------ | ---------------------------------------------------- | --- | -------------------------------------- |
| \(C\)                                | complete input corpus                                |
| \(N=                                 | C                                                    | \)  | number of units in the complete corpus |
| \(S\)                                | selected local unit set; \(S=C\) when `-k` is absent |
| \(\operatorname{df}\_{C}(t)\)        | global unit frequency of pattern \(t\)               |
| \(\operatorname{idf}\_{C}(t)\)       | global inverse document frequency of pattern \(t\)   |
| \(\operatorname{gdf}\_{C}(t_1,t_2)\) | global unit frequency of the pair                    |
| \(\operatorname{ctf}\_{S}(t_1,t_2)\) | retained local pair frequency                        |

The key option defines the local observation set without changing the global
IDF reference:

```sh
./cw -p 2,3 -k '^梅/名$' -M 16
```

Thus a graph can represent relations inside a selected topic while each token
and pair is still weighted against the full corpus.

### CW methods

`cw` currently implements four historical and explanatory methods:

| Method | Main purpose                                                |
| -----: | ----------------------------------------------------------- |
|    `1` | compact explanation of the basic CW principle               |
|    `7` | historically adjusted waka-graph formula; current default   |
|   `12` | experimental weighting of locally rare patterns             |
|   `16` | global token weight × global pair weight × local repetition |

For publication and reproducibility, specify the method explicitly:

```sh
./cw -M 1
./cw -M 7 -k REGEX
./cw -M 12 -k REGEX
./cw -M 16 -k REGEX
```

Method 16 is:

\[
CW*{16}(t_1,t_2;S,C)
=
\left(
1+
\ln\!\left(
\frac{N}{\operatorname{gdf}*{C}(t*1,t_2)}
\right)
\right)
\sqrt{
\operatorname{idf}*{C}(t*1)
\operatorname{idf}*{C}(t*2)
}
\left(
1+\ln \operatorname{ctf}*{S}(t_1,t_2)
\right).
\]

It combines three kinds of evidence:

```text
global token weight
    whether the two patterns themselves are informative

global pair weight
    whether their combination is unusual in the complete corpus

local repetition
    whether that combination recurs in the selected local set
```

In words:

> CW is high when globally weighty patterns form a globally unusual
> combination that recurs in the selected local set.

The methods are not interchangeable rescalings. Raw CW values and thresholds
should be interpreted separately for each method. Z values are often more
convenient for comparing distributions produced by different methods.

### Output columns

`cw` writes one tab-separated row for each retained projected pair:

```text
token1 token2 ctf cdf df1 idf1 fq1 df2 idf2 fq2 cw z unit_id...
```

| Column | Name         | Meaning                                     |
| -----: | ------------ | ------------------------------------------- |
|      1 | `token1`     | representative complete token for pattern 1 |
|      2 | `token2`     | representative complete token for pattern 2 |
|      3 | `ctf`        | retained local pair frequency               |
|      4 | `cdf`        | selected-unit frequency of the pair         |
|      5 | `df1`        | global unit frequency of pattern 1          |
|      6 | `idf1`       | global IDF of pattern 1                     |
|      7 | `fq1`        | local occurrence frequency of pattern 1     |
|      8 | `df2`        | global unit frequency of pattern 2          |
|      9 | `idf2`       | global IDF of pattern 2                     |
|     10 | `fq2`        | local occurrence frequency of pattern 2     |
|     11 | `cw`         | CW under the selected method                |
|     12 | `z`          | Z within the selected CW distribution       |
|  13... | `unit_id...` | selected units containing the pair          |

With the default all-pairs output, `ctf` and `cdf` are normally numerically
equal. Adjacent or windowed `pair` output may contain repeated occurrences
inside one unit, in which case `ctf` can exceed `cdf`. The two concepts
therefore remain distinct in the interface.

See [`docs/man-cw.md`](docs/man-cw.md) for all formulas, notation, options, and
citation-ready descriptions.

## Unix filters: retain the researcher's decisions

Before `emit`, the analysis remains a line-oriented TSV stream. Conditions
that can be decided from one row should normally be written with `grep`, `awk`,
`sort`, `uniq`, `head`, or a short shell script.

```text
row pruning
    ordinary Unix filters

graph and publication formatting
    emit
```

Examples:

```sh
# CW at least 10
awk -F '\t' '$11 >= 10' result.tsv

# Z at least 2
awk -F '\t' '$12 >= 2' result.tsv

# CTF at least 2, CW at least 10, and Z at least 2
awk -F '\t' '$3 >= 2 && $11 >= 10 && $12 >= 2' result.tsv

# Exclude a category appearing at either endpoint
awk -F '\t' '$1 !~ /\/格助\// && $2 !~ /\/格助\//' result.tsv
```

More elaborate operations, such as removing small island components, may be
written as an `awk` or shell script and stored with the research materials:

```sh
./remove-small-islands.sh result.tsv > result-pruned.tsv
```

The paper can then state exactly what was removed, while the script provides an
executable supplementary record. The analytical decision is neither hidden in
a GUI nor buried inside a large custom program.

Saving the measured table before experimenting is recommended:

```sh
./pair < input.txt \
  | ./cw -p 2,3 -k '^梅/名$' -M 16 \
  > ume-cw.tsv

awk -F '\t' '$12 >= 2' ume-cw.tsv | ./emit > ume.dot
```

## `emit`: reliable graph and table formatting

`emit` is a formatter. It does not recalculate IDF, CW, Z, CTF, CDF, or token
frequency. It translates already measured and selected rows into reusable
formats:

```text
json
dot
md / markdown
tex / latex
html
```

Select the format with:

```sh
./emit -T json
./emit -T dot
./emit -T md
./emit -T tex
./emit -T html
```

### Graph output

Graph behavior is configured in `config/emit-config.json`. `emit` handles the
technical details that should not have to be rewritten for every analysis:

- safe quoting and escaping of DOT identifiers and labels;
- safe serialization of floating-point values, including scientific notation;
- directed or undirected graph syntax;
- graph, node, and edge fonts;
- Graphviz attributes such as `overlap`, `sep`, `pack`, `packmode`, and
  `splines`;
- node shapes and visible labels;
- edge labels and tooltips;
- preferred edge length;
- node font-size mapping by local frequency, IDF, or displayed degree.

Example configuration fragment:

```json
{
  "format": "dot",
  "dot": {
    "directed": false,
    "fontname": "Noto Serif CJK JP",
    "overlap": false,
    "sep": "+8",
    "pack": true,
    "packmode": "graph",
    "splines": "line"
  },
  "node": {
    "shape": "oval",
    "fontname": "Noto Serif CJK JP",
    "label_fields": [1, 4],
    "label_separator": "/",
    "font_size_by": "fq",
    "min_font_size": 7,
    "max_font_size": 32
  },
  "edge": {
    "fontname": "Noto Serif CJK JP",
    "label": "ctf",
    "length": 1.4
  }
}
```

The researcher specifies readable display policy; `emit` produces valid DOT.
Graphviz remains responsible for geometry and rendering:

```sh
./emit -c config/emit-config.json result-pruned.tsv \
  | neato -Tsvg \
  > result.svg
```

### Pattern identity and visible labels

Pattern identity has already been decided by `cw`. Visible labels are selected
independently by `emit`:

```json
"label_fields": [1, 4],
"label_separator": "/"
```

For a complete token:

```text
桜/桜/名/さくら
```

this produces:

```text
桜/さくら
```

The computation may therefore merge by lemma and class while the graph displays
surface form and reading. Computational identity and human-facing explanation
remain separate choices.

### Publication tables

`config/emit-table.config` selects columns, headings, label fields, numerical
precision, caption, and table label. One configuration can produce three
simple publication formats:

```sh
./emit -c config/emit-table.config -T md   result.tsv > result.md
./emit -c config/emit-table.config -T tex  result.tsv > result.tex
./emit -c config/emit-table.config -T html result.tsv > result.html
```

A typical table configuration is:

```json
{
  "format": "md",
  "table": {
    "columns": ["token1", "token2", "ctf", "cdf", "idf1", "idf2", "cw", "z", "unit_ids"],
    "headers": ["Pattern 1", "Pattern 2", "CTF", "CDF", "IDF 1", "IDF 2", "CW", "Z", "Unit IDs"],
    "label_fields": [1, 4],
    "label_separator": "/",
    "precision": 6,
    "unit_separator": ", ",
    "caption": "CW results",
    "label": "tab:cw-results"
  }
}
```

The output is deliberately modest and editable:

- Markdown uses a normal pipe table;
- LaTeX uses a standard `table` and `tabular` fragment;
- HTML uses a semantic table fragment with `thead` and `tbody`.

`emit` preserves input order. Ranking, truncation, and row selection remain
explicit upstream decisions:

```sh
sort -t $'\t' -k11,11gr result.tsv \
  | head -20 \
  | ./emit -c config/emit-table.config -T tex
```

See [`docs/man-emit.md`](docs/man-emit.md) for the complete configuration and
output specification.

## `cm`: planned directed transition model

`cm` is reserved for sequential relations that are not adequately represented
by an unordered co-occurrence graph.

The planned model reads the original unit-based token sequence, projects the
selected pattern fields, and inserts structural boundary symbols:

```text
A B C

<BOS> A B C <EOS>
```

It can then count directed adjacent transitions:

```text
<BOS> -> A
A     -> B
B     -> C
C     -> <EOS>
```

The boundary symbols are essential. Without `<EOS>`, a pattern occurring at a
unit end disappears from the forward-probability denominator and continuation
probabilities are inflated. Without `<BOS>`, initial position is lost.

The intended forward transition probability is:

$$
P*S(y\mid x)
=
\frac{\operatorname{af}*{S}(x,y)}
{\sum*{y'}\operatorname{af}*{S}(x,y')}.
$$

A later CM weight can combine the local directed transition probability with
global pattern IDF. Boundary symbols will be treated as structural markers,
not ordinary lexical items, because ordinary IDF would be zero when they occur
in every unit.

This section records the design direction; the `cm` interface and formula are
not yet frozen.

## Distribution analysis with `rbin`

[`rbin`](https://zenodo.org/records/21229729) is an independent command-line
tool for frequency distributions, descriptive statistics, and distribution
diagnostics.

CW values are column 11 and Z values are column 12:

```sh
awk -F '\t' '{print $11}' result.tsv | rbin -c
awk -F '\t' '{print $12}' result.tsv | rbin -c
```

This keeps measurement and distribution analysis separate while preserving the
same Unix-stream workflow.

## Reproducible research record

A reproducible `cw-tools` analysis should preserve at least:

1. the input dataset and unit definition;
2. the token field specification;
3. the versions of `pair`, `cw`, and `emit`;
4. the `pair` mode and ordering;
5. the `cw -p`, `-k`, and `-M` options;
6. the unfiltered `cw` TSV output;
7. every `grep`, `awk`, `sort`, or shell-script condition;
8. the `emit` configuration;
9. the Graphviz command and renderer version;
10. the final figure or table.

A compact project record may be a Makefile target:

```make
ume-cw.tsv: input.txt
	./pair < $< | ./cw -p 2,3 -k '^梅/名$$' -M 16 > $@

ume-pruned.tsv: ume-cw.tsv
	awk -F '\t' '$$12 >= 2' $< > $@

ume.svg: ume-pruned.tsv config/emit-config.json
	./emit -c config/emit-config.json $< | neato -Tsvg > $@

ume-top20.tex: ume-cw.tsv config/emit-table.config
	sort -t "$$(printf '\t')" -k12,12gr $< | head -20 \
	  | ./emit -c config/emit-table.config -T tex > $@
```

The resulting paper can describe the same stages in ordinary language:

> Complete-token pairs were generated within each poem. Computational
> patterns were defined by fields 2 and 3. Units containing the selected key
> pattern were analyzed using CW method 16. Edges with Z below 2 were removed.
> The retained network was serialized by `emit` and laid out with Graphviz
> `neato`.

The commands and the prose describe the same analytical choices.

## Build and install

Compile:

```sh
make
```

Install under the default prefix:

```sh
make install
```

Install under a custom prefix:

```sh
make PREFIX=$HOME/.local install
```

Clean generated objects and binaries:

```sh
make clean
```

## Tests and sample data

Test data, conversion scripts, and examples are provided under `tests`.

The Hachidaishu part-of-speech data can be converted to the unit-based input
format with:

```sh
awk -f tests/hachidaishu2pair.awk \
  tests/data/hachidaishu-pos.txt \
  > tests/data/hachidaishu-pair.txt
```

A small selected-unit workflow is then:

```sh
awk '$1 == 1' tests/data/hachidaishu-pair.txt \
  | ./pair \
  | ./cw -p 2,3 -k '^梅/名$' -M 16 \
  > kokin-ume.tsv
```

Before relying on a result, inspect the intermediate rows as well as the final
visualization:

```sh
head kokin-ume.tsv
awk -F '\t' '{print $11}' kokin-ume.tsv | rbin -c
```

## Documentation

The manuals are intended both for operation and for citation-ready method
description:

- [`docs/man-pair.md`](docs/man-pair.md) — pair generation, windowing,
  direction, and per-unit token frequencies;
- [`docs/man-cw.md`](docs/man-cw.md) — pattern projection, reference sets,
  formulas, methods, output columns, and Z values;
- [`docs/man-emit.md`](docs/man-emit.md) — Unix-stage pruning, Graphviz
  configuration, JSON/DOT output, and Markdown/LaTeX/HTML tables.

The manuals state not only what command to run, but also what each value means
and how the method can be reported in scholarly writing.

## Related resources

- [Hachidaishu Part-of-Speech Dataset](https://doi.org/10.5281/zenodo.13940187)  
  [![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.13940187.svg)](https://doi.org/10.5281/zenodo.13940187)
- [rbin: A small command-line utility for rank-based binning](https://zenodo.org/records/21229729)  
  [![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.21229729.svg)](https://doi.org/10.5281/zenodo.21229729)

## Citation

For a reproducible citation, record the released version or commit identifier,
the versions of the individual programs, and the selected CW method. Permanent
release and DOI information can be added here when the repository release is
archived.

## License

This software is released under the MIT License. See [`LICENSE`](LICENSE) for
details.
