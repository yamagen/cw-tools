# emit

## NAME

`emit` — format line-oriented `cw` results as JSON, Graphviz DOT, Markdown, LaTeX, or HTML

## SYNOPSIS

```sh
emit [OPTION]... [FILE]
```

## DESCRIPTION

`emit` reads the tab-separated edge table produced by [`cw`](./man-cw.md) and writes one of the following:

- a reusable JSON graph dataset;
- a Graphviz DOT description;
- a Markdown table;
- a LaTeX table; or
- an HTML table fragment.

This manual describes `emit` version 0.7.0.

`emit` is a formatter. It does not calculate or recalculate IDF, CW, Z, CTF, CDF, or token frequency. Those values belong to the measurement stage implemented by `cw`.

The normal display policy is read from `config/emit-config.json`. Command-line options are intended for temporary changes to output format or simple thresholds.

With no file operand, `emit` reads standard input. At most one input file may be specified.

## PLACE IN THE CW-TOOLS PIPELINE

The principal processing boundary is:

```text
pair -> cw -> grep / awk -> emit -> neato / dot / sfdp
            line-oriented    graph-oriented

pair -> cw -> grep / awk -> emit -T md / tex / html
            line-oriented    publication table
```

The programs have distinct responsibilities:

```text
pair
    enumerate complete-token pairs within units
    preserve unit-level token frequencies

cw
    project complete tokens onto user-defined patterns
    calculate global and local statistics
    calculate CW and Z
    write one result edge per line

grep / awk
    perform conditions decidable from one TSV row
    retain or reject rows without changing their measurements

emit
    construct the displayed node and edge set for graph output
    derive display values such as degree and font size
    select and label columns for table output
    translate configuration into safe JSON, DOT, Markdown, LaTeX, or HTML

neato / dot / sfdp
    calculate geometric layout
    render SVG, PDF, PNG, or another device format
```

A typical pipeline is:

```sh
grep '^1' tests/data/hachidaishu-pair.txt \
  | ./pair \
  | ./cw -p 2,3 -k '^梅/名$' -M 16 \
  | awk -F '\t' '$12 >= 2' \
  | ./emit \
  | neato -Tsvg > ume.svg
```

The research conditions are visible in the pipeline. Ordinary row filtering remains ordinary Unix processing; Graphviz syntax, table syntax, quoting, and target-specific escaping are handled by `emit`.

## INPUT

### Current format

The current input is the tab-separated output of `cw` 0.7.0 or later:

```text
token1 token2 ctf cdf df1 idf1 fq1 df2 idf2 fq2 cw z unit_id...
```

| Column | Name | Meaning |
| ---: | --- | --- |
| 1 | `token1` | complete representative token for the first pattern |
| 2 | `token2` | complete representative token for the second pattern |
| 3 | `ctf` | local pair occurrence count supplied by `cw` |
| 4 | `cdf` | number of selected units containing the pair |
| 5 | `df1` | global unit frequency of pattern 1 |
| 6 | `idf1` | global IDF of pattern 1 |
| 7 | `fq1` | local frequency of pattern 1, or `-` when unavailable |
| 8 | `df2` | global unit frequency of pattern 2 |
| 9 | `idf2` | global IDF of pattern 2 |
| 10 | `fq2` | local frequency of pattern 2, or `-` when unavailable |
| 11 | `cw` | precomputed co-occurrence weight |
| 12 | `z` | precomputed Z value in the selected CW distribution |
| 13... | `unit_id...` | one unit identifier for every occurrence counted by `ctf` |

### Legacy format

The earlier ten-column format without `fq1` and `fq2` is also accepted:

```text
token1 token2 ctf cdf df1 idf1 df2 idf2 cw z unit_id...
```

When legacy input is used, node frequency `fq` is unavailable. Consequently:

```json
"font_size_by": "fq"
```

causes an error. Use current `pair` and `cw` output, or select `idf` or `degree` for font sizing.

### Tabs, comments, and validation

Fields must be separated by literal tab characters. Empty lines and lines beginning with `#` are ignored.

`emit` verifies, among other things, that:

- numeric fields are syntactically valid and finite;
- `ctf` is greater than zero;
- `cdf` does not exceed `ctf`;
- token fields and unit identifiers are nonempty;
- the number of trailing unit identifiers agrees with `ctf`;
- repeated appearances of a node carry consistent `df`, `idf`, and available `fq` values.

`emit` does not recalculate the supplied measurements.

## COMPLETE TOKEN, PATTERN, AND LABEL

`cw` has already decided pattern identity before `emit` receives the table. The token strings retained in columns 1 and 2 are complete representative tokens with up to four slash-separated fields:

```text
f1/f2/f3/f4
```

The complete token is used as the DOT and JSON node identifier. The visible node label is independently constructed from `node.label_fields`.

For example:

```json
"label_fields": [1, 4],
"label_separator": "/"
```

turns:

```text
桜/桜/名/さくら
```

into the visible label:

```text
桜/さくら
```

The program does not assume that any field is a surface form, lemma, part of speech, gloss, semantic code, or another predetermined category.

## PRUNING BEFORE EMIT

### The line/graph boundary

Before `emit`, every `cw` result is one TSV row. A condition that can be decided from that row should normally be expressed with `grep`, `grep -v`, or `awk`, rather than added as a new special-purpose `emit` option.

This preserves a simple rule:

```text
row pruning
    grep / grep -v / awk

graph-structural processing
    emit
```

The row filter does not alter IDF, CW, or Z. It only decides which already measured edges are passed to the graph formatter.

### Filter by CW

CW is column 11 in the current format:

```sh
./cw ... \
  | awk -F '\t' '$11 >= 10' \
  | ./emit
```

### Filter by Z

Z is column 12:

```sh
./cw ... \
  | awk -F '\t' '$12 >= 2' \
  | ./emit
```

### Combine numerical conditions

```sh
./cw ... \
  | awk -F '\t' '$3 >= 2 && $11 >= 10 && $12 >= 2' \
  | ./emit
```

This retains rows whose CTF is at least 2, CW is at least 10, and Z is at least 2.

### Exclude a pattern from either endpoint

When the third complete-token field records a category such as `格助`, a precise field-aware filter can be written in `awk`:

```sh
./cw ... \
  | awk -F '\t' '$1 !~ /\/格助\// && $2 !~ /\/格助\//' \
  | ./emit
```

A general `grep -v` is convenient:

```sh
./cw ... | grep -v '/格助/' | ./emit
```

but it searches the complete line, including both endpoints and trailing unit identifiers. `awk` is safer when the target must be restricted to columns 1 and 2.

### Select one endpoint

```sh
./cw ... \
  | awk -F '\t' '$1 ~ /^梅\// || $2 ~ /^梅\//' \
  | ./emit
```

### Save the measured table before experimenting

```sh
./cw -p 2,3 -k '^梅/名$' -M 16 > ume-cw.tsv

awk -F '\t' '$12 >= 1' ume-cw.tsv | ./emit > ume-z1.dot
awk -F '\t' '$12 >= 2' ume-cw.tsv | ./emit > ume-z2.dot
```

The expensive measurement stage is not repeated, and every display condition remains inspectable.

### `-W` and `-Z` as conveniences

`emit` retains `-W` and `-Z` as concise forms for simple thresholds:

```sh
./emit -W 10
./emit -Z 2
```

Conceptually, these are equivalent to row filters on columns 11 and 12. They are convenient for interactive use, while `awk` is more expressive for compound research conditions.

### Graph-structural pruning

Conditions requiring knowledge of the complete displayed graph cannot be decided from one input row. Examples include:

- degree thresholds;
- retaining the component containing a designated node;
- removing small disconnected islands;
- selecting the largest connected component;
- iterative k-core pruning.

Such operations belong conceptually after graph construction and therefore in `emit` or a later graph-processing program. Version 0.7.0 calculates displayed degree for font sizing but does not yet implement structural pruning options.

## CONFIGURATION

The default configuration path is:

```text
config/emit-config.json
```

Use another file with:

```sh
emit -c configs/publication.json
```

A complete configuration is:

```json
{
  "format": "dot",
  "filters": {
    "min_cw": null,
    "min_z": null
  },
  "dot": {
    "graph_name": "G",
    "directed": false,
    "charset": "UTF-8",
    "overlap": false,
    "outputorder": "edgesfirst",
    "fontname": "Noto Serif CJK JP",
    "sep": "+8",
    "pack": true,
    "packmode": "graph",
    "splines": "line"
  },
  "node": {
    "shape": "oval",
    "fontname": "Noto Serif CJK JP",
    "label_fields": [1],
    "label_separator": "/",
    "font_size_by": "fq",
    "min_font_size": 7,
    "max_font_size": 32
  },
  "edge": {
    "fontname": "Noto Serif CJK JP",
    "label": "ctf",
    "tooltip": ["ctf", "cdf", "cw", "z", "unit_ids"],
    "penwidth": 1.0,
    "length": 1.4
  },
  "table": {
    "columns": ["token1", "token2", "ctf", "cdf", "cw", "z"],
    "headers": ["Pattern 1", "Pattern 2", "CTF", "CDF", "CW", "Z"],
    "label_fields": [1],
    "label_separator": "/",
    "precision": 6,
    "unit_separator": ", ",
    "caption": null,
    "label": null
  }
}
```

The file must be valid JSON. Comments and trailing commas are not accepted. Unknown keys are currently ignored.

### Precedence

Effective settings are determined in this order:

```text
internal defaults
        ↓
configuration file
        ↓
explicit command-line overrides
```

A command-line option changes only the named setting.

## OUTPUT FORMAT

```json
"format": "dot"
```

Accepted values are:

| Value | Output |
| --- | --- |
| `"dot"` | Graphviz DOT |
| `"json"` | reusable cw-tools graph JSON |
| `"md"` | Markdown table |
| `"tex"` | LaTeX `table` and `tabular` fragment |
| `"html"` | HTML `<table>` fragment |

The aliases `markdown` and `latex` are also accepted.

Temporarily override the setting with:

```sh
emit -T json
emit -T dot
emit -T md
emit -T tex
emit -T html
```

## TABLE OUTPUT

### `emit-table.config`

Table output is controlled by an ordinary JSON configuration file. The filename may be `config/emit-table.config`; the extension does not affect parsing.

A practical configuration is:

```json
{
  "format": "md",
  "filters": {
    "min_cw": null,
    "min_z": null
  },
  "table": {
    "columns": [
      "token1",
      "token2",
      "ctf",
      "cdf",
      "idf1",
      "idf2",
      "cw",
      "z",
      "unit_ids"
    ],
    "headers": [
      "Pattern 1",
      "Pattern 2",
      "CTF",
      "CDF",
      "IDF 1",
      "IDF 2",
      "CW",
      "Z",
      "Unit IDs"
    ],
    "label_fields": [1, 4],
    "label_separator": "/",
    "precision": 6,
    "unit_separator": ", ",
    "caption": "CW results",
    "label": "tab:cw-results"
  }
}
```

Use it as follows:

```sh
./cw ... > result.tsv

./emit -c config/emit-table.config result.tsv > result.md
./emit -c config/emit-table.config -T tex result.tsv > result.tex
./emit -c config/emit-table.config -T html result.tsv > result.html
```

The same selected rows and column policy are therefore reused across all three publication formats.

### `table.columns`

`columns` determines both inclusion and order. Accepted names are:

| Name | Value |
| --- | --- |
| `token1` | token 1 rendered through `table.label_fields` |
| `token2` | token 2 rendered through `table.label_fields` |
| `raw_token1` | complete token 1 exactly as supplied by `cw` |
| `raw_token2` | complete token 2 exactly as supplied by `cw` |
| `ctf` | local pair occurrence count |
| `cdf` | selected-unit pair frequency |
| `df1`, `df2` | global unit frequencies |
| `idf1`, `idf2` | global IDF values |
| `fq1`, `fq2` | local token frequencies, or `NA` for legacy input |
| `cw` | CW value |
| `z` | Z value |
| `unit_ids` | joined trailing unit identifiers |

Duplicate columns are rejected. The default, when no `table` object is supplied, is:

```json
["token1", "token2", "ctf", "cdf", "cw", "z", "unit_ids"]
```

### `table.headers`

`headers` is optional. When supplied, it must contain exactly one string for every selected column. When omitted, `emit` supplies short English headings.

This permits publication-specific terminology without changing the computed data:

```json
"headers": ["語1", "語2", "頻度", "CW", "Z"]
```

### Token labels

The table's `token1` and `token2` columns are constructed independently from DOT node labels:

```json
"label_fields": [1, 4],
"label_separator": "/"
```

For example:

```text
桜/桜/名/さくら
```

is written as:

```text
桜/さくら
```

Select `raw_token1` or `raw_token2` when the complete representative token must appear in the table.

### Numeric precision

```json
"precision": 6
```

`precision` is the number of significant digits used for IDF, CW, and Z. It must be an integer from 1 through 17. Integer counts are never rounded.

### Unit identifiers

```json
"unit_separator": ", "
```

joins the unit identifiers in one table cell. A table intended for the main body of a paper will often omit `unit_ids`; a supplementary table can retain them.

### Caption and label

```json
"caption": "CW results",
"label": "tab:cw-results"
```

Both values may be strings or `null`.

- Markdown writes the label as an HTML anchor and the caption as a bold line above the table.
- LaTeX writes `\caption{...}` and `\label{...}` inside a `table` environment.
- HTML writes the label as the table `id` and the caption as `<caption>`.

### Output character

The three table outputs are deliberately modest:

- Markdown is a pipe table with numeric columns right-aligned;
- LaTeX is a standard `table` plus `tabular` fragment using `l` and `r` columns and no required package;
- HTML is a semantic `<table>` fragment with `<thead>`, `<tbody>`, and `class="numeric"` on numeric cells.

`emit` escapes format-sensitive characters in token text, headings, captions, and unit identifiers. The output is intended to be directly usable as a first publication table and easy to refine with CSS or LaTeX formatting later.

### Preserve Unix-stage decisions

`emit` preserves input row order. It does not sort, rank, truncate, or select the "best" rows. Those research decisions remain explicit upstream:

```sh
sort -t $'\t' -k11,11gr result.tsv \
  | head -20 \
  | ./emit -c config/emit-table.config -T tex
```

Thus the paper can state both the numerical condition and the presentation limit without hiding either inside the formatter.

## DISPLAY FILTERS

```json
"filters": {
  "min_cw": null,
  "min_z": null
}
```

Each value is either a finite number or `null`. A number applies an inclusive minimum threshold. `null` disables the corresponding threshold.

These filters are applied before incident nodes, degree, and font-size normalization are calculated.

## GRAPHVIZ GRAPH SETTINGS

The `dot` object controls graph-level DOT syntax and attributes.

### `graph_name`

```json
"graph_name": "G"
```

The value is safely quoted and used as the DOT graph identifier.

### `directed`

```json
"directed": false
```

`false` writes an undirected `graph` with `--`. `true` writes a `digraph` with `->`. This affects serialization only; `emit` does not reinterpret upstream pair direction.

### `charset`

```json
"charset": "UTF-8"
```

Writes the Graphviz `charset` graph attribute.

### `overlap`

```json
"overlap": false
```

Writes a boolean Graphviz `overlap` attribute. The layout engine determines the exact overlap-removal behavior.

### `outputorder`

```json
"outputorder": "edgesfirst"
```

Controls Graphviz drawing order. `edgesfirst` commonly keeps node labels visually above edges.

### Graph font

```json
"fontname": "Noto Serif CJK JP"
```

Writes the graph-level `fontname` attribute. This applies to graph labels and other graph-level text. Node and edge fonts are configured independently.

The named font must be visible to the Graphviz installation. `emit` quotes the name correctly but does not install or embed the font.

### `sep`

```json
"sep": "+8"
```

Writes the Graphviz `sep` attribute. It supplies additional separation used by overlap removal. It is stored as a string so values such as `+8` remain intact.

### `pack` and `packmode`

```json
"pack": true,
"packmode": "graph"
```

These attributes control packing of disconnected components. They are useful when many small island components would otherwise occupy excessive space.

### `splines`

```json
"splines": "line"
```

Writes the Graphviz `splines` attribute as a quoted string. Common values include `line`, `polyline`, `curved`, `ortho`, and `spline`; support depends on the selected Graphviz engine.

If `fontname`, `sep`, `pack`, `packmode`, or `splines` is omitted, `emit` omits that DOT attribute and leaves the choice to Graphviz.

## NODE SETTINGS

### `shape`

```json
"shape": "oval"
```

Writes the Graphviz node `shape` attribute.

### Node font

```json
"fontname": "Noto Serif CJK JP"
```

Writes the node-level Graphviz `fontname` attribute. It controls visible node labels.

### `label_fields`

```json
"label_fields": [1, 4]
```

Selects one or more fields from the complete token. Values must be integers from 1 through 4. Duplicate entries are ignored while preserving the first occurrence.

The older singular setting remains accepted:

```json
"label_field": 1
```

### `label_separator`

```json
"label_separator": "/"
```

Joins multiple selected label fields.

### `font_size_by`

```json
"font_size_by": "fq"
```

Accepted values are:

| Value | Meaning |
| --- | --- |
| `fq` | local token frequency in the key-selected set; reproduces the intention of the earlier `cw.c` visualization |
| `idf` | global token weight |
| `degree` | number of retained displayed edges incident to the node |

The minimum and maximum values among the retained nodes are linearly mapped to:

```json
"min_font_size": 7,
"max_font_size": 32
```

If every retained node has the same source value, the midpoint of the configured font-size range is used.

Changing font-size mode changes presentation, not CW or Z.

## EDGE SETTINGS

### Edge font

```json
"fontname": "Noto Serif CJK JP"
```

Writes the edge-level Graphviz `fontname` attribute and controls visible edge labels.

### `label`

```json
"label": "ctf"
```

Accepted values are:

| Value | Visible edge label |
| --- | --- |
| `none` | no visible label |
| `ctf` | CTF |
| `cdf` | CDF |
| `cw` | CW with six significant digits |
| `z` | Z with six significant digits |

Full-precision CW and Z remain stored as custom DOT attributes.

### `tooltip`

```json
"tooltip": ["ctf", "cdf", "cw", "z", "unit_ids"]
```

Accepted entries are `ctf`, `cdf`, `cw`, `z`, and `unit_ids`. An empty array disables edge tooltips.

### `penwidth`

```json
"penwidth": 1.0
```

Sets one Graphviz `penwidth` for all edges. The value must be greater than zero.

### `length`

```json
"length": 1.4
```

Writes Graphviz edge attribute `len`. For `neato`, this is the preferred edge length in inches. It is a layout preference, not a guaranteed geometric distance.

The alias `len` is also accepted in the JSON configuration:

```json
"len": 1.4
```

If neither key is present, no `len` attribute is emitted.

## SAFE DOT SERIALIZATION

`emit` handles Graphviz syntax rather than requiring users to compose DOT manually.

It safely quotes and escapes:

- graph names;
- node identifiers and labels;
- font names;
- shapes;
- separation and spline settings;
- tooltips and unit identifiers.

Floating-point custom attributes are quoted. This avoids a DOT parsing ambiguity for values written in scientific notation with a negative exponent, such as:

```text
-2.6946750088283494e-05
```

A generated header may look like:

```dot
graph "G" {
  graph [charset="UTF-8", overlap=false, outputorder="edgesfirst",
         fontname="Noto Serif CJK JP", sep="+8", pack=true,
         packmode="graph", splines="line"];
  node [shape="oval", fontname="Noto Serif CJK JP"];
  edge [penwidth="1", fontname="Noto Serif CJK JP", len="1.4"];
}
```

The actual serializer writes each attribute statement on one line.

## JSON OUTPUT

JSON output is selected with:

```sh
emit -T json
```

The output contains:

- effective display filters;
- output and label settings;
- font-size mode and calculated node font sizes;
- effective Graphviz font and layout settings;
- node IDs, labels, fields, DF, IDF, FQ, degree, and font size;
- edge CTF, CDF, CW, Z, and unit identifiers.

JSON is intended as reusable research data. DOT is a visualization description; SVG, PDF, and PNG are rendered products.

## OPTIONS

### `-c FILE`, `--config FILE`

Read `FILE` instead of `config/emit-config.json`.

### `-T FORMAT`, `--format FORMAT`

Temporarily select `json`, `dot`, `md`, `tex`, or `html`. The aliases `markdown` and `latex` are accepted.

### `-W VALUE`, `--min-cw VALUE`

Temporarily retain only edges whose CW is at least `VALUE`.

### `-Z VALUE`, `--min-z VALUE`

Temporarily retain only edges whose Z is at least `VALUE`.

### `-h`, `--help`

Display a usage summary and exit.

### `-v`, `--version`

Display:

```text
emit 0.7.0
```

## RENDERING

`emit` writes DOT but does not invoke a Graphviz layout engine. The configuration records Graphviz attributes so the user need not repeat them as command-line options.

```sh
./cw ... | ./emit > graph.dot
neato -Tsvg graph.dot > graph.svg
```

The pipeline may be shortened:

```sh
./cw ... | ./emit | neato -Tsvg > graph.svg
```

Other devices remain the renderer's responsibility:

```sh
./emit < graph.tsv | neato -Tpdf > graph.pdf
./emit < graph.tsv | neato -Tpng > graph.png
```

Likewise, choosing `neato`, `dot`, or `sfdp` remains explicit because the layout algorithm is a substantive visualization choice. The repetitive and error-prone DOT attribute syntax belongs to `emit`.

## EXAMPLES

### M16 graph with configured formatting

```sh
grep '^1' tests/data/hachidaishu-pair.txt \
  | ./pair \
  | ./cw -p 2,3 -k '^梅/名$' -M 16 \
  | ./emit \
  | neato -Tsvg > ume.svg
```

### Exclude a category before graph construction

```sh
./cw ... \
  | awk -F '\t' '$1 !~ /\/格助\// && $2 !~ /\/格助\//' \
  | ./emit \
  | neato -Tsvg > content-patterns.svg
```

### Compare FQ, IDF, and degree font sizing

Prepare three configuration files differing only in:

```json
"font_size_by": "fq"
```

```json
"font_size_by": "idf"
```

```json
"font_size_by": "degree"
```

Then reuse one measured table:

```sh
./cw ... > measured.tsv
./emit -c config/emit-fq.json measured.tsv | neato -Tsvg > fq.svg
./emit -c config/emit-idf.json measured.tsv | neato -Tsvg > idf.svg
./emit -c config/emit-degree.json measured.tsv | neato -Tsvg > degree.svg
```

### Keep island components compact

```json
"dot": {
  "overlap": false,
  "sep": "+8",
  "pack": true,
  "packmode": "graph"
}
```

### Inspect Graphviz font resolution

`emit` writes the requested family name. To inspect what Graphviz resolves on the local machine:

```sh
fc-match "Noto Serif CJK JP"
neato -v -Tsvg graph.dot -o graph.svg 2>&1 | grep -i font
```

### Produce the same publication table in three formats

```sh
./cw -p 2,3 -k '^梅/名$' -M 16 > ume.tsv

./emit -c config/emit-table.config -T md ume.tsv > ume.md
./emit -c config/emit-table.config -T tex ume.tsv > ume.tex
./emit -c config/emit-table.config -T html ume.tsv > ume.html
```

### Prepare a short main-text table

```sh
sort -t $'\t' -k12,12gr ume.tsv \
  | head -10 \
  | ./emit -c config/emit-table.config -T tex > ume-top10.tex
```

The sort key, row limit, selected columns, labels, and numerical precision are all inspectable parts of the research record.

## EXIT STATUS

`emit` exits successfully after producing output. It exits with failure for conditions such as:

- unreadable input or configuration files;
- invalid JSON;
- malformed or inconsistent TSV input;
- invalid option arguments;
- nonpositive font sizes, pen width, or edge length;
- unavailable `fq` when `font_size_by` is `fq`.

Diagnostics are written to standard error.

## CURRENT LIMITATIONS

Version 0.7.0 intentionally leaves several responsibilities outside `emit`:

1. It does not invoke `neato`, `dot`, `sfdp`, or another renderer.
2. It does not implement graph-structural pruning such as component size or k-core selection.
3. It supports boolean `overlap`; nonboolean Graphviz overlap modes are not represented.
4. It does not map edge width, color, or style to CW, Z, or frequency.
5. Font names are passed to Graphviz but fonts are not installed or embedded.
6. Unknown configuration keys are ignored, so spelling mistakes may leave a default unchanged.
7. It trusts `cdf` after verifying only that `cdf <= ctf`.
8. Table output deliberately uses plain Markdown, standard LaTeX `tabular`, and an HTML fragment; advanced pagination, long tables, CSS, and journal-specific styling remain later formatting steps.

These are presentation and validation limitations. They do not affect measurements already calculated by `cw`.

## DESIGN PRINCIPLE

Two complementary rules define `emit`:

```text
Do not reimplement ordinary line filtering that Unix already performs well.

Do not require researchers to write or debug Graphviz or publication-table syntax by hand.
```

Accordingly:

```text
researcher-visible analytical choices
    pattern projection
    key selection
    CW method
    row-selection conditions
    visible labels
    display weight

formatter responsibilities
    DOT quoting and escaping
    scientific-number safety
    graph/node/edge font attributes
    Graphviz layout hints
    JSON and DOT serialization
```

This division keeps the analytical record understandable while making graph production reliable and repeatable.

## FILES

```text
config/emit-config.json
```

Default configuration.

```text
src/emit.c
```

Program source.

```text
docs/man-emit.md
```

This manual.

## SEE ALSO

- [`pair`](./man-pair.md)
- [`cw`](./man-cw.md)
- [`idf-cw-z`](../notes/idf-cw-z.md)
- `grep(1)`
- `awk(1)`
- Graphviz `neato`, `dot`, and `sfdp`
