# emit output architecture

This document records the responsibilities and boundaries of the `emit` output
system before further implementation.  It is intended to be updated together
with the code, one confirmed step at a time.

For command-line behavior and configuration, see [man-emit.md](./man-emit.md).

## Core principle

`cw` measures; `emit` interprets measurements for output; CSS and JavaScript
control presentation and interaction.

```text
pair -> cw -> optional row or graph organization -> emit -> target renderer
```

The boundaries are:

```text
cw
    calculate ctf, cdf, df, idf, fq, cw, z, and unit_id values
    write one line-oriented edge record

optional organizer
    combine or reorganize several independent cw result sets
    calculate relations that exist only between result sets
    example: overlap_count in a compound model

emit
    read cw or organized edge records
    construct the displayed node and edge set
    derive display semantics such as degree and rank
    emit target-safe identifiers, classes, data fields, links, and labels
    dispatch to the selected output module

CSS
    map semantic classes and ranks to concrete appearance
    control font sizes, colors, strokes, opacity, spacing, and hover appearance

JavaScript
    draw interactive graphs
    implement sliders, selection, filtering, dragging, zooming, and click behavior

template
    assemble data, CSS, JavaScript, and document structure
```

`emit` should not recalculate the statistical measurements owned by `cw`.
CSS should not be required to infer statistical meaning from raw values.

## Current C modules

The executable remains one program named `emit`, with target-specific C modules.

| File | Current responsibility | Status |
| --- | --- | --- |
| `src/emit.c` | configuration, input parsing, node and edge construction, dispatch | implemented |
| `src/emit-types.h` | shared edge, node, configuration, and output types | implemented |
| `src/emit-util.c` | shared allocation, labels, ranks or display-value helpers | implemented |
| `src/emit-json.c` | reusable graph JSON | implemented |
| `src/emit-dot.c` | Graphviz DOT | implemented |
| `src/emit-d3.c` | complete interactive D3 HTML document | implemented as a self-contained first version |
| `src/emit-tables.c` | Markdown, LaTeX, and HTML tables | implemented |

The current modularization is therefore complete at the C output-module level.
The next work is to separate reusable browser assets and add semantic attributes
for CSS and JavaScript access.

## Output routes

### Graphviz route

```text
cw rows
  -> emit -T dot
  -> dot / neato / sfdp
  -> SVG, PDF, PNG, ...
```

`emit-dot.c` owns DOT syntax and escaping.  Graphviz owns layout and final SVG
construction.

Planned stylesheet:

```text
assets/svg.css
```

`svg.css` owns the concrete appearance of Graphviz-generated SVG elements.  To
support it, DOT output should provide stable `id`, `class`, and link attributes.
Examples of semantic classes are:

```text
node
edge
font-rank-1 ... font-rank-N
z-rank-1 ... z-rank-N
degree-rank-1 ... degree-rank-N
keyword
compound
overlap-1 ... overlap-N
has-url
```

No `emit-dot.js` is planned.  Graphviz already interprets DOT.  A later
`emit-svg.js` should be added only if static SVG plus CSS is insufficient for a
specific interaction.

### D3 route

Current version:

```text
cw rows
  -> emit -T d3
  -> one complete HTML document containing data, CSS, and JavaScript
```

Planned reusable structure:

```text
emit output data
  + templates/emit-d3.html
  + assets/d3.css
  + assets/emit-d3.js
  + assets/emit-slider.js
```

Responsibilities:

| Asset | Responsibility |
| --- | --- |
| `assets/d3.css` | concrete appearance of D3 SVG and controls |
| `assets/emit-d3.js` | graph construction, force layout, zoom, drag, tooltip, links |
| `assets/emit-slider.js` | Z, overlap, or other interactive controls |
| `templates/emit-d3.html` | document structure and asset loading |

The first refactoring step should preserve the current `emit -T d3` behavior
while moving reusable code out of C string literals.

### Table route

```text
cw rows
  -> emit -T md
  -> Markdown pipe table

cw rows
  -> emit -T tex
  -> LaTeX table / tabular fragment

cw rows
  -> emit -T html
  -> semantic HTML table fragment
```

All three formats are correctly grouped in `src/emit-tables.c`; separate C
modules named `emit-table-md.c`, `emit-table-tex.c`, or `emit-table-html.c` are
not currently needed.

Possible shared browser asset:

```text
assets/table.css
```

It applies only to HTML table output.  Markdown and LaTeX styling remains the
responsibility of their downstream renderer or document.

## Semantic values and presentation values

A measurement such as `fq`, `idf`, `cw`, or `z` is supplied by `cw`.

A semantic display category such as `font-rank-4` is derived by `emit`.

A concrete presentation such as `font-size: 18px` belongs in CSS.

```text
cw value
  -> emit rank or class
  -> CSS appearance
```

For compatibility, an output may temporarily contain both a concrete Graphviz
attribute and a semantic class.  The long-term aim is that users can alter the
appearance without rebuilding `cw` or changing C source code.

## Links and URLs

CSS controls the appearance of a link but not its destination.

`emit` must produce the URL or data needed for a link:

```text
DOT: URL, href, tooltip, target
D3/JSON: url or data fields consumed by JavaScript
```

JavaScript implements dynamic click behavior where required.

## Compound models

A compound relation is not a measurement internal to one `cw` run.
It arises when several independent PPD outputs are combined.

```text
cw output A --\
              -> compound organizer -> emit
cw output B --/
```

The organizer may initially be an AWK program.  It owns operations such as:

```text
normalize t1 and t2
identify the same edge across result sets
calculate overlap_count
merge or preserve unit_id values
```

`emit` only reads the organized attributes and exposes them as DOT, JSON, D3,
or table fields and semantic classes.

## Current status and implementation order

| Item | Status | Next action |
| --- | --- | --- |
| C output modules | complete | retain boundaries |
| Markdown / LaTeX / HTML table output | complete | document and test |
| self-contained D3 HTML | complete first version | separate reusable assets |
| `d3.css` | not yet externalized | create after class contract is fixed |
| `svg.css` | not yet present | create after DOT class contract is fixed |
| `emit-d3.js` | currently embedded in `emit-d3.c` | extract without changing behavior |
| `emit-slider.js` | not yet present | add after graph-data contract is fixed |
| `emit-dot.js` | not required | do not add without a concrete need |
| `emit-svg.js` | not currently required | add only for SVG interaction beyond CSS |
| compound organizer | not yet present | define input and output contract first |
| semantic rank / class output | partial | specify and implement incrementally |

Recommended order:

1. Freeze this responsibility document.
2. Define semantic node and edge attributes shared by DOT, JSON, and D3.
3. Add those attributes to `emit-types.h` and output modules.
4. Add `svg.css` and verify Graphviz SVG class access.
5. Extract `emit-d3.js` and `d3.css` from the current self-contained D3 output.
6. Define and add `emit-slider.js`.
7. Define the compound-organizer row contract and implement the AWK filter.
8. Extend documentation and tests with every completed step.

## Rule for future additions

Before adding an option or file, identify which layer owns the operation:

```text
measurement          cw
between-model set operation  organizer
semantic output      emit
static appearance    CSS
interaction          JavaScript
assembly             template
layout               Graphviz or D3
```

A new feature should be placed in the lowest layer that can implement it without
absorbing responsibilities from another layer.
