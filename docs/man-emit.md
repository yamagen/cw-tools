# emit

## NAME

`emit` — format line-oriented `cw` results as JSON, Graphviz DOT, Markdown, LaTeX, HTML, or interactive D3 HTML

## SYNOPSIS

```sh
emit [OPTION]... [FILE]
```

## DESCRIPTION

`emit` reads the tab-separated edge table produced by [`cw`](./man-cw.md) and writes one of the following:

- a reusable JSON graph dataset;
- a Graphviz DOT description;
- a Markdown table;
- a LaTeX table;
- an HTML table fragment; or
- a complete interactive HTML document using D3.js.

This manual describes `emit` version 0.8.0.

`emit` is a formatter. It does not calculate or recalculate IDF, CW, Z, CTF, CDF, or token frequency. Those values belong to the measurement stage implemented by `cw`.

The normal display policy is read from `config/emit-config.json`. Command-line options are intended for temporary changes to output format or simple thresholds.

With no file operand, `emit` reads standard input. At most one input file may be specified.

Version 0.8.0 separates the output implementations into independent source modules while retaining one executable named `emit`. This internal reorganization does not change the input format or the ordinary command-line interface.

## PLACE IN THE CW-TOOLS PIPELINE

The principal processing boundaries are:

```text
pair -> cw -> grep / awk -> emit -> neato / dot / sfdp
            line-oriented    Graphviz description

pair -> cw -> grep / awk -> emit -T md / tex / html
            line-oriented    publication table

pair -> cw -> grep / awk -> emit -T d3 -> web browser
            line-oriented    interactive graph
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
    translate configuration into safe target-specific output
    dispatch to JSON, DOT, table, or D3 output modules

neato / dot / sfdp
    calculate geometric layout for DOT output
    render SVG, PDF, PNG, or another device format

web browser
    load D3.js
    calculate and display the interactive force layout
```

A typical Graphviz pipeline is:

```sh
grep '^1' tests/data/hachidaishu-pair.txt \
  | ./pair \
  | ./cw -p 2,3 -k '^梅/名$' -M 16 \
  | awk -F '\t' '$12 >= 2' \
  | ./emit \
  | neato -Tsvg > ume.svg
```

The same measured rows can be written as an interactive graph:

```sh
grep '^1' tests/data/hachidaishu-pair.txt \
  | ./pair \
  | ./cw -p 2,3 -k '^梅/名$' -M 16 \
  | awk -F '\t' '$12 >= 2' \
  | ./emit -T d3 > ume.html

firefox ume.html
```

The research conditions remain visible in the pipeline. Ordinary row filtering remains ordinary Unix processing; Graphviz syntax, table syntax, JavaScript data serialization, quoting, and target-specific escaping are handled by `emit`.

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

causes an error for JSON, DOT, and D3 output. Use current `pair` and `cw` output, or select `idf` or `degree` for font sizing.

Table output may still include `fq1` or `fq2`; unavailable values are written as `-`.

### Tabs, comments, and validation

Fields must be separated by literal tab characters. Empty lines and lines beginning with `#` are ignored.

`emit` verifies that:

- the row can be parsed as either the current or legacy layout;
- count fields are nonnegative integers representable as `size_t`;
- IDF, CW, and Z fields are syntactically valid finite numbers;
- repeated appearances of a node carry consistent `df`, `idf`, and available `fq` values.

Version 0.8.0 does not yet check relational constraints such as `ctf > 0`, `cdf <= ctf`, or agreement between `ctf` and the number of trailing unit identifiers. It also does not reject empty token or unit-identifier strings.

`emit` does not recalculate the supplied measurements.

## COMPLETE TOKEN, PATTERN, AND LABEL

`cw` has already decided pattern identity before `emit` receives the table. The token strings retained in columns 1 and 2 are complete representative tokens with up to four slash-separated fields:

```text
f1/f2/f3/f4
```

The complete token is used as the JSON, DOT, and D3 node identifier. The visible node label is independently constructed from `node.label_fields`.

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

```text
row pruning
    grep / grep -v / awk

graph-structural processing
    emit or a later graph-processing program
```

The row filter does not alter IDF, CW, or Z. It only decides which already measured edges are passed to the formatter.

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

When the third complete-token field records a category such as `格助`, a field-aware filter can be written in `awk`:

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
awk -F '\t' '$12 >= 2' ume-cw.tsv | ./emit -T d3 > ume-z2.html
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

Version 0.8.0 calculates displayed degree for font sizing but does not implement structural pruning options.

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

### D3 configuration in version 0.8.0

Version 0.8.0 does **not** define a separate `d3` configuration object, and the distribution does not require a file named `emit-d3.json`.

The ordinary configuration file can select D3 output:

```json
"format": "d3"
```

or the output can be selected temporarily:

```sh
./emit -T d3
```

A separate file may be created merely as a convenient saved profile:

```sh
cp config/emit-config.json config/emit-d3.json
```

Change its format to:

```json
"format": "d3"
```

and use it with:

```sh
./emit -c config/emit-d3.json result.tsv > graph.html
```

Such a file contains the same shared settings as `emit-config.json`; it does not unlock additional D3-specific settings. Adding an unimplemented object such as:

```json
"d3": {
  "charge": -180
}
```

has no effect in version 0.8.0 because unknown configuration keys are ignored.

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
| `"d3"` | complete interactive D3 HTML document |

The aliases `markdown` and `latex` are also accepted.

Temporarily override the setting with:

```sh
emit -T json
emit -T dot
emit -T md
emit -T tex
emit -T html
emit -T d3
```

## TABLE OUTPUT

### Table configuration

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
| `fq1`, `fq2` | local token frequencies, or `-` for legacy input |
| `cw` | CW value |
| `z` | Z value |
| `unit_ids` | joined trailing unit identifiers |

Repeated column names are ignored after their first appearance. The default, when no `table` object is supplied, is:

```json
["token1", "token2", "ctf", "cdf", "cw", "z", "unit_ids"]
```

### `table.headers`

`headers` is optional. When supplied, it must not contain more entries than the selected columns. Omitted headings are supplied from the corresponding default short English names.

This permits publication-specific terminology without changing the computed data:

```json
"headers": ["語1", "語2", "頻度", "CW", "Z"]
```

### Token labels

The table's `token1` and `token2` columns are constructed independently from graph node labels:

```json
"label_fields": [1, 4],
"label_separator": "/"
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

- LaTeX writes `\caption{...}` and `\label{...}` inside a `table` environment.
- HTML writes `caption` as `<caption>`.
- Markdown ignores both values in version 0.8.0.
- HTML ignores `label` in version 0.8.0.

### Output character

The three table outputs are deliberately modest:

- Markdown is a pipe table with numeric columns right-aligned;
- LaTeX is a standard `table` plus `tabular` fragment using `l` and `r` columns and no required package;
- HTML is a semantic `<table>` fragment with `<thead>`, `<tbody>`, and `class="numeric"` on numeric cells.

`emit` escapes format-sensitive characters in token text, headings, captions, and unit identifiers.

### Preserve Unix-stage decisions

`emit` preserves input row order. It does not sort, rank, truncate, or select the "best" rows. Those research decisions remain explicit upstream:

```sh
sort -t $'\t' -k11,11gr result.tsv \
  | head -20 \
  | ./emit -c config/emit-table.config -T tex
```

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

The `dot` object controls graph-level DOT syntax and attributes. Most settings in this object affect only DOT output. The `directed` value is also used by D3 output.

### `graph_name`

```json
"graph_name": "G"
```

The value is safely quoted and used as the DOT graph identifier.

### `directed`

```json
"directed": false
```

For DOT, `false` writes an undirected `graph` with `--`; `true` writes a `digraph` with `->`.

For D3, `true` draws arrow markers at link targets. This affects serialization and display only; `emit` does not reinterpret upstream pair direction.

### `charset`

```json
"charset": "UTF-8"
```

Writes the Graphviz `charset` graph attribute. D3 HTML is always written with UTF-8 metadata.

### `overlap`

```json
"overlap": false
```

Writes a boolean Graphviz `overlap` attribute. It does not control the D3 force simulation.

### `outputorder`

```json
"outputorder": "edgesfirst"
```

Controls Graphviz drawing order. It does not affect D3 output.

### Graph font

```json
"fontname": "Noto Serif CJK JP"
```

Writes the graph-level Graphviz `fontname` attribute. D3 version 0.8.0 uses its built-in CSS font stack and does not use this setting.

### `sep`

```json
"sep": "+8"
```

Writes the Graphviz `sep` attribute. It does not affect D3 output.

### `pack` and `packmode`

```json
"pack": true,
"packmode": "graph"
```

These attributes control Graphviz packing of disconnected components. They do not affect D3 output.

### `splines`

```json
"splines": "line"
```

Writes the Graphviz `splines` attribute. D3 version 0.8.0 draws straight SVG lines.

## NODE SETTINGS

### `shape`

```json
"shape": "oval"
```

Writes the Graphviz node `shape` attribute. D3 version 0.8.0 draws circular nodes and does not use this setting.

### Node font

```json
"fontname": "Noto Serif CJK JP"
```

Writes the Graphviz node `fontname` attribute. D3 version 0.8.0 does not use this setting.

### `label_fields`

```json
"label_fields": [1, 4]
```

Selects one or more fields from the complete token. Values must be integers from 1 through 4. Duplicate entries are ignored while preserving the first occurrence.

The older singular setting remains accepted:

```json
"label_field": 1
```

The resulting label is used in JSON, DOT, and D3 output.

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
| `fq` | local token frequency in the key-selected set |
| `idf` | global token weight |
| `degree` | number of retained displayed edges incident to the node |

The minimum and maximum values among the retained nodes are linearly mapped to:

```json
"min_font_size": 7,
"max_font_size": 32
```

The calculated font size is used by JSON, DOT, and D3 output. D3 also derives its circle radius and collision spacing from this value.

If every retained node has the same source value, the midpoint of the configured font-size range is used.

Changing font-size mode changes presentation, not CW or Z.

## EDGE SETTINGS

### Edge font

```json
"fontname": "Noto Serif CJK JP"
```

Writes the Graphviz edge `fontname` attribute. D3 version 0.8.0 does not draw visible edge labels and does not use this setting.

### `label`

```json
"label": "ctf"
```

Accepted values are:

| Value | Visible DOT edge label |
| --- | --- |
| `none` | no visible label |
| `ctf` | CTF |
| `cdf` | CDF |
| `cw` | CW with six significant digits |
| `z` | Z with six significant digits |

When CW or Z is selected as the visible DOT label, it is written with six significant digits. Full-precision values remain available in the DOT tooltip when the corresponding tooltip fields are enabled.

D3 version 0.8.0 does not draw edge text labels. Edge statistics are available in the link tooltip.

### `tooltip`

```json
"tooltip": ["ctf", "cdf", "cw", "z", "unit_ids"]
```

For DOT, accepted entries are `ctf`, `cdf`, `cw`, `z`, and `unit_ids`. An empty array disables DOT edge tooltips.

D3 version 0.8.0 always includes source, target, CTF, CDF, CW, Z, and unit identifiers in the link tooltip; the `edge.tooltip` array does not yet customize it.

### `penwidth`

```json
"penwidth": 1.0
```

Sets one Graphviz `penwidth` for all edges. D3 version 0.8.0 does not use this value; it maps absolute Z to an internal link-width range.

### `length`

```json
"length": 1.4
```

For DOT, this writes the Graphviz edge attribute `len`. For `neato`, it is a preferred edge length in inches.

The alias `len` is also accepted:

```json
"len": 1.4
```

For D3, the configured value is reused as a proportional force-link distance hint. It is not interpreted as a browser measurement in inches.

If neither key is present, DOT omits `len` and D3 uses its internal default link distance.

## SAFE SERIALIZATION

`emit` handles target syntax rather than requiring users to compose DOT, JSON, HTML, or JavaScript data manually.

DOT output safely quotes and escapes:

- graph names;
- node identifiers and labels;
- font names;
- shapes;
- separation and spline settings;
- tooltips and unit identifiers.

DOT string attributes are quoted and escaped. Numeric values such as font size, pen width, and preferred edge length are written as numeric DOT attributes.

JSON and D3 output escape control characters, quotation marks, backslashes, and other syntax-sensitive text. D3 data serialization also writes `<` as a Unicode escape so token text cannot prematurely open or close HTML markup inside the embedded script.

Table output applies Markdown-, LaTeX-, or HTML-specific escaping.

## JSON OUTPUT

JSON output is selected with:

```sh
emit -T json
```

The output contains:

- the `cw-tools/graph` format identifier and schema version;
- the configuration path and selected output format;
- node label fields, label separator, and font-size mode;
- the directed/undirected flag;
- node IDs, labels, token fields, DF, IDF, FQ, degree, and calculated font size;
- edge source, target, CTF, CDF, endpoint DF/IDF/FQ values, CW, Z, and unit identifiers.

Version 0.8.0 JSON output does not serialize the complete effective configuration or the active filter thresholds.

JSON is intended as reusable research data. DOT is a visualization description; D3 is an interactive browser document; SVG, PDF, and PNG are rendered products.

## D3 OUTPUT

### Select D3 output

```sh
emit -T d3 result.tsv > graph.html
firefox graph.html
```

D3 output is a complete HTML document rather than an HTML table fragment.

### Interactive behavior

The generated page provides:

- a force-directed network layout calculated in the browser;
- wheel or gesture zooming;
- panning by dragging the background;
- node dragging;
- node tooltips containing label, complete identifier, DF, IDF, FQ, and displayed degree;
- edge tooltips containing source, target, CTF, CDF, CW, Z, and unit identifiers;
- target arrows when `dot.directed` is `true`;
- node label sizes calculated from the ordinary node font-size settings;
- edge width mapped from absolute Z.

The graph data, CSS, and interaction code are embedded in the generated document.

### External D3 dependency

Version 0.8.0 loads D3 version 7 from:

```text
https://cdn.jsdelivr.net/npm/d3@7
```

Therefore the page normally requires network access when opened. The data itself remains in the generated HTML file, but the interactive graph cannot start unless the D3 library is available from the CDN or the browser cache.

### Settings reused by D3

| Configuration setting | D3 effect in version 0.8.0 |
| --- | --- |
| `filters.min_cw`, `filters.min_z` | selects emitted links before graph construction |
| `dot.directed` | enables target arrows |
| `node.label_fields` | constructs visible node text |
| `node.label_separator` | joins selected label fields |
| `node.font_size_by` | selects FQ, IDF, or degree for label size |
| `node.min_font_size`, `node.max_font_size` | defines the calculated label-size range |
| `edge.length` or `edge.len` | supplies a proportional force-link distance hint |

### Settings not yet reused by D3

The following remain DOT- or table-specific in version 0.8.0:

- `dot.graph_name`, `charset`, `overlap`, `outputorder`, `fontname`, `sep`, `pack`, `packmode`, and `splines`;
- `node.shape` and `node.fontname`;
- `edge.fontname`, `edge.label`, `edge.tooltip`, and `edge.penwidth`;
- all `table` settings.

D3-specific force, collision, zoom, color, font, and edge-width settings are fixed internally in version 0.8.0. They can be evaluated through actual use before a later version exposes a stable `d3` configuration object.

## OPTIONS

### `-c FILE`, `--config FILE`

Read `FILE` instead of `config/emit-config.json`.

### `-T FORMAT`, `--format FORMAT`

Temporarily select `json`, `dot`, `md`, `tex`, `html`, or `d3`. The aliases `markdown` and `latex` are accepted.

### `-W VALUE`, `--min-cw VALUE`

Temporarily retain only edges whose CW is at least `VALUE`.

### `-Z VALUE`, `--min-z VALUE`

Temporarily retain only edges whose Z is at least `VALUE`.

### `-h`, `--help`

Display a usage summary and exit.

### `-v`, `--version`

Display:

```text
emit 0.8.0
```

## RENDERING

### Graphviz rendering

`emit` writes DOT but does not invoke a Graphviz layout engine:

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

Choosing `neato`, `dot`, or `sfdp` remains explicit because the layout algorithm is a substantive visualization choice.

### D3 rendering

D3 output is opened directly in a web browser:

```sh
./emit -T d3 graph.tsv > graph.html
firefox graph.html
```

No Graphviz process is involved. The browser calculates the force layout at page load.

## EXAMPLES

### M16 graph with configured Graphviz formatting

```sh
grep '^1' tests/data/hachidaishu-pair.txt \
  | ./pair \
  | ./cw -p 2,3 -k '^梅/名$' -M 16 \
  | ./emit \
  | neato -Tsvg > ume.svg
```

### Interactive D3 graph from the same data

```sh
grep '^1' tests/data/hachidaishu-pair.txt \
  | ./pair \
  | ./cw -p 2,3 -k '^梅/名$' -M 16 \
  | ./emit -T d3 > ume.html

firefox ume.html
```

### Exclude a category before graph construction

```sh
./cw ... \
  | awk -F '\t' '$1 !~ /\/格助\// && $2 !~ /\/格助\//' \
  | ./emit -T d3 > content-patterns.html
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

./emit -c config/emit-fq.json -T d3 measured.tsv > fq.html
./emit -c config/emit-idf.json -T d3 measured.tsv > idf.html
./emit -c config/emit-degree.json -T d3 measured.tsv > degree.html
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

## EXIT STATUS

`emit` exits successfully after producing output. It exits with failure for conditions such as:

- unreadable input or configuration files;
- invalid JSON;
- malformed or inconsistent TSV input;
- invalid option arguments;
- nonpositive font sizes, pen width, or edge length;
- unavailable `fq` when `font_size_by` is `fq` for JSON, DOT, or D3 output;
- more table headers than selected table columns;
- output write errors.

Diagnostics are written to standard error.

## CURRENT LIMITATIONS

Version 0.8.0 intentionally leaves several responsibilities outside `emit`:

1. DOT output does not invoke `neato`, `dot`, `sfdp`, or another renderer.
2. It does not implement graph-structural pruning such as component-size or k-core selection.
3. DOT supports boolean `overlap`; nonboolean Graphviz overlap modes are not represented.
4. DOT does not map edge width, color, or style to CW, Z, or frequency.
5. Font names are passed to Graphviz but fonts are not installed or embedded.
6. Unknown configuration keys are ignored, so spelling mistakes may leave a default unchanged.
7. It does not yet validate `ctf > 0`, `cdf <= ctf`, nonempty identifiers, or agreement between `ctf` and the number of unit identifiers.
8. Table output deliberately uses plain Markdown, standard LaTeX `tabular`, and an HTML fragment; advanced pagination, long tables, CSS, and journal-specific styling remain later formatting steps.
9. D3 output requires D3 version 7 from an external CDN unless the browser already has it cached.
10. D3-specific simulation, color, font, zoom, tooltip selection, and edge-width settings are not configurable in version 0.8.0.
11. D3 output does not yet reuse Graphviz node shape, font, edge-label, tooltip-selection, or pen-width settings.
12. D3 output is an interactive browser view, not a stable publication layout comparable to a rendered Graphviz PDF or SVG.

These are presentation and validation limitations. They do not affect measurements already calculated by `cw`.

## IMPLEMENTATION LAYOUT

Version 0.8.0 builds one `emit` executable from the following modules:

```text
src/emit.c
    command-line parsing
    JSON configuration parsing
    TSV input and validation
    filtering, node construction, degree calculation
    output-format dispatch

src/emit-types.h
    shared graph and configuration types

src/emit-util.c
src/emit-util.h
    allocation helpers
    token-field selection
    node-label construction
    node-weight and font-size calculation

src/emit-json.c
src/emit-json.h
    reusable graph JSON output

src/emit-dot.c
src/emit-dot.h
    Graphviz DOT output

src/emit-tables.c
src/emit-tables.h
    Markdown, LaTeX, and HTML table output

src/emit-d3.c
src/emit-d3.h
    interactive D3 HTML output
```

The Makefile compiles these modules separately and links them into one command:

```text
emit
```

The module boundary is an implementation detail. Users do not invoke `emit-dot`, `emit-json`, `emit-tables`, or `emit-d3` as separate commands.

## DESIGN PRINCIPLE

Two complementary rules define `emit`:

```text
Do not reimplement ordinary line filtering that Unix already performs well.

Do not require researchers to write or debug Graphviz, publication-table,
or interactive graph serialization syntax by hand.
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
    static or interactive output choice

formatter responsibilities
    target-specific quoting and escaping
    scientific-number safety
    graph/node/edge display attributes
    Graphviz layout hints
    JSON, DOT, table, and D3 serialization
```

This division keeps the analytical record understandable while making graph and table production reliable and repeatable.

## FILES

```text
config/emit-config.json
```

Default configuration.

```text
src/emit.c
src/emit-types.h
src/emit-util.c
src/emit-util.h
src/emit-json.c
src/emit-json.h
src/emit-dot.c
src/emit-dot.h
src/emit-tables.c
src/emit-tables.h
src/emit-d3.c
src/emit-d3.h
```

Program sources.

```text
docs/man-emit.md
```

This manual.

No `config/emit-d3.json` file is required by version 0.8.0.

## SEE ALSO

- [`pair`](./man-pair.md)
- [`cw`](./man-cw.md)
- [`idf-cw-z`](../notes/idf-cw-z.md)
- `grep(1)`
- `awk(1)`
- Graphviz `neato`, `dot`, and `sfdp`
- D3.js version 7
