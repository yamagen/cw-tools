# emit semantic attributes

This document defines the semantic attributes that `emit` may expose to DOT,
JSON, D3, SVG stylesheets, and browser JavaScript.

It deliberately separates measurements, semantic categories, and concrete
presentation.

```text
cw measurement -> emit semantic attribute -> CSS / JavaScript behavior
```

For the larger component boundaries, see
[emit-architecture.md](./emit-architecture.md).

## Naming rules

Semantic names use lowercase ASCII words separated by hyphens.

```text
font-rank-5
z-positive
compound
overlap-2
has-url
```

The same semantic name is represented as:

```text
DOT / SVG class     font-rank-5
JSON / D3 class     "font-rank-5"
JSON property       font_rank: 5
```

JSON property names use `snake_case`.  CSS class names use `kebab-case`.

A class must describe meaning, not a concrete appearance.  Therefore:

```text
font-rank-5     correct
large-red-node  incorrect
```

The stylesheet decides whether `font-rank-5` is large, bold, red, or otherwise
styled.

## Identity and output element IDs

The complete token string remains the research identity of a node.  Edge
identity remains its source and target token strings.

Raw token strings are not suitable as HTML or SVG element IDs.  `emit` should
therefore provide a separate output-safe element identifier:

```text
node-1
node-2
...

edge-1
edge-2
...
```

The number follows emitted order.  The raw token or endpoint values remain
available separately.

Target mapping:

| Meaning | DOT / SVG | JSON / D3 |
| --- | --- | --- |
| raw node identity | Graphviz node name | `id` |
| safe element identity | `id="node-N"` | `element_id: "node-N"` |
| visible text | `label` | `label` |
| semantic classes | `class="..."` | `classes: [...]` |

The safe element ID is for CSS, JavaScript, and document links.  It must not
replace the complete token as the data identity.

## Rank scale

The first implementation uses nine ordered ranks:

```text
1 2 3 4 5 6 7 8 9
```

Rank 1 is the lowest displayed value, rank 9 is the highest, and rank 5 is the
middle rank.  Nine ranks provide a true midpoint and enough levels for CSS
without exposing a separate class for every numeric value.

For a displayed value `x` with displayed minimum `min` and maximum `max`:

```text
normalized = (x - min) / (max - min)
rank       = 1 + floor(normalized * 9)
```

The maximum is clamped to rank 9.  When all displayed values are equal, every
item receives rank 5.

The raw value remains in the output.  A rank does not replace or round the
measurement.

## Node attributes

### Attributes available from current cw output

Each emitted node has:

| Property | Meaning |
| --- | --- |
| `id` | complete representative token |
| `element_id` | safe output ID such as `node-1` |
| `label` | configured visible node label |
| `fields` | complete token split into fields where supported |
| `df` | global document frequency |
| `idf` | global inverse document frequency |
| `fq` | selected token frequency, or unavailable for legacy input |
| `degree` | degree in the displayed graph |
| `font_size_by` | source selected by configuration: `fq`, `idf`, or `degree` |
| `font_rank` | rank 1 through 9 derived from that selected source |
| `classes` | semantic classes listed below |

Base node classes are:

```text
node
font-by-fq | font-by-idf | font-by-degree
font-rank-1 ... font-rank-9
```

Example:

```text
node font-by-fq font-rank-7
```

The current concrete `font_size` value may remain during the compatibility
period.  New stylesheets should prefer `font-rank-N`.

### Conditional node classes

These classes require explicit information not currently present in an
ordinary `cw` row:

| Class | Condition |
| --- | --- |
| `keyword` | an upstream source or configuration explicitly identifies the key |
| `has-url` | `emit` has received or constructed a nonempty URL |
| `compound` | an organizer explicitly marks the node as belonging to a compound model, if node-level marking is needed |

`emit` must not infer `keyword` merely from visual prominence or token
frequency.  Current `cw` output does not carry the original `-k` expression as
row metadata, so this class is reserved until an explicit source is defined.

## Edge attributes

### Attributes available from current cw output

Each ordinary edge has:

| Property | Meaning |
| --- | --- |
| `element_id` | safe output ID such as `edge-1` |
| `source` | complete source token |
| `target` | complete target token |
| `ctf` | selected pair occurrence count |
| `cdf` | selected-unit pair frequency |
| `cw` | CW value |
| `z` | Z value |
| `z_rank` | rank 1 through 9 derived from `abs(z)` in the displayed graph |
| `unit_ids` | trailing unit identifiers |
| `classes` | semantic classes listed below |

Base edge classes are:

```text
edge
z-rank-1 ... z-rank-9
z-negative | z-zero | z-positive
```

The sign class and magnitude rank are separate.  A stylesheet may therefore
choose line width from `z-rank-N` and color or dash pattern from the sign class.

### Conditional edge attributes

These require explicit organizer or configuration input:

| Property or class | Meaning |
| --- | --- |
| `compound` | edge belongs to an organized compound result |
| `overlap_count` | number of independent result sets containing the edge |
| `overlap-N` | CSS class corresponding to `overlap_count=N` |
| `has-url` | edge has a nonempty URL |
| `url` | link destination supplied or constructed by `emit` |

`overlap_count` is not calculated from one ordinary `cw` run.  It belongs to a
compound organizer and is only exposed by `emit` after it appears in organized
input.

## Graph-level attributes

The graph output should expose:

| Property | Meaning |
| --- | --- |
| `directed` | directed or undirected graph |
| `font_size_by` | configured node ranking source |
| `rank_count` | `9` for the first semantic-rank implementation |
| `classes` | graph-level semantic classes |

Initial graph-level classes are:

```text
graph
font-by-fq | font-by-idf | font-by-degree
directed | undirected
```

## Target mapping

### DOT

A node may be written conceptually as:

```dot
"花/名" [
    id="node-1",
    class="node font-by-fq font-rank-7",
    label="花"
];
```

An edge may be written conceptually as:

```dot
"花/名" -- "春/名" [
    id="edge-1",
    class="edge z-rank-6 z-positive"
];
```

Graphviz produces the final SVG structure.  `assets/svg.css` addresses the
classes that survive into SVG.

### JSON

A node may contain:

```json
{
  "id": "花/名",
  "element_id": "node-1",
  "font_size_by": "fq",
  "font_rank": 7,
  "classes": ["node", "font-by-fq", "font-rank-7"]
}
```

An edge may contain:

```json
{
  "element_id": "edge-1",
  "source": "花/名",
  "target": "春/名",
  "z": 1.8,
  "z_rank": 6,
  "classes": ["edge", "z-rank-6", "z-positive"]
}
```

### D3

D3 data uses the same semantic properties and class names as reusable JSON.
`emit-d3.js` applies `element_id` and joins `classes` when constructing SVG
elements.  It must not independently invent a second rank system.

## CSS responsibility

CSS maps classes to concrete appearance:

```css
.font-rank-7 text {
    font-size: 18px;
}

.z-rank-6 path,
.z-rank-6 line {
    stroke-width: 3px;
}

.z-negative path,
.z-negative line {
    stroke-dasharray: 4 2;
}

.has-url {
    cursor: pointer;
}
```

These numbers and visual choices are examples only.  They do not belong in the
semantic contract.

## Compatibility rule

Adding semantic properties must not initially remove existing output fields or
change current layout behavior.

During transition, output may contain both:

```text
font_size      current concrete compatibility value
font_rank      new semantic value
```

After external CSS and JavaScript assets are established and documented, the
continued need for concrete presentation fields can be reviewed separately.

## Implementation sequence

1. Add shared rank helpers to `emit-util.c` and `emit-util.h`.
2. Add safe element IDs, ranks, and classes to reusable JSON.
3. Add the same IDs and classes to DOT without removing current `fontsize`.
4. Make self-contained D3 output consume the same helpers and attributes.
5. Verify Graphviz SVG preservation of `id` and `class`.
6. Add `assets/svg.css` only after that verification.
7. Extract `assets/emit-d3.js` and `assets/d3.css` without changing semantics.

Each step should update this document and the relevant manual or tests.
