# emit SVG route

This document records the Graphviz-to-SVG implementation and verification
steps.  It is updated one confirmed stage at a time.

For the responsibility boundaries, see
[emit-architecture.md](./emit-architecture.md).  For the shared attribute
contract, see [emit-semantic-attributes.md](./emit-semantic-attributes.md).

## Stage 1: semantic IDs and classes

Implemented in `src/emit-dot.c` and shared helpers in `src/emit-util.c`.

The first stage does not yet attach a stylesheet.  Its purpose is to verify that
semantic identifiers and classes survive this route:

```text
cw rows -> emit -T dot -> Graphviz -> SVG
```

### Graph

`emit` writes:

```dot
id="graph-1"
class="font-by-fq undirected"
```

The actual font source is one of:

```text
font-by-fq
font-by-idf
font-by-degree
```

The direction class is one of:

```text
directed
undirected
```

Graphviz automatically adds the base class `graph` to the generated SVG group.
Therefore `emit` does not repeat `graph` inside the DOT `class` attribute.

### Nodes

Each emitted node receives an order-based safe element ID:

```text
node-1
node-2
...
```

It also receives:

```text
font-by-fq | font-by-idf | font-by-degree
font-rank-0 ... font-rank-9
```

Example DOT attributes:

```dot
id="node-1",
class="font-by-fq font-rank-7"
```

Graphviz automatically adds the base class `node` to the SVG group.

The existing concrete Graphviz `fontsize` attribute remains unchanged for
layout compatibility.  `font-rank-N` is additional semantic information; it
does not yet control the layout.

### Edges

Each emitted edge receives:

```text
edge-1
edge-2
...
```

and the classes:

```text
z-rank-0 ... z-rank-9
z-negative | z-zero | z-positive
```

Example DOT attributes:

```dot
id="edge-1",
class="z-rank-6 z-positive"
```

Graphviz automatically adds the base class `edge` to the SVG group.

`z-rank-N` is derived from `abs(z)` among the edges retained in the displayed
graph.  The sign is recorded separately so that a later stylesheet can assign
line width from magnitude and color or dash pattern from sign.

## Rank calculation

The shared rank scale is one decimal digit:

```text
0 1 2 3 4 5 6 7 8 9
```

For a value `x` in the displayed range:

```text
normalized = (x - min) / (max - min)
rank       = floor(normalized * 10)
```

The maximum is clamped to 9.  When every displayed value is equal, rank 5 is
used.

Node font rank uses the configured source (`fq`, `idf`, or displayed degree).
Edge Z rank uses `abs(z)`.

## Local verification

Fetch and enter the implementation branch:

```sh
git fetch origin
git switch --track origin/agent/emit-architecture
```

If the local branch already exists:

```sh
git switch agent/emit-architecture
git pull --ff-only
```

Build `emit`:

```sh
make clean
make emit
```

Use the normal cw-tools pipeline and write SVG through Graphviz.  For example:

```sh
... | ./emit -c config/emit-config.json -Tdot -Z 1.6 \
  | neato -Tsvg -o t.svg
inkview t.svg
```

The existing visual result should remain unchanged at this stage.

Inspect the generated groups:

```sh
grep -o 'id="[^"]*"' t.svg | head
grep -o 'class="[^"]*"' t.svg | head
```

Expected group classes include forms such as:

```xml
class="graph font&#45;by&#45;fq undirected"
class="node font&#45;by&#45;fq font&#45;rank&#45;7"
class="edge z&#45;rank&#45;6 z&#45;positive"
```

Graphviz may serialize a hyphen as the XML character reference `&#45;`.
An XML or browser parser resolves it back to `-`, so CSS selectors such as
`.font-rank-7` still address the class normally.

## Next stage

After the generated SVG has been confirmed locally:

1. add `assets/svg.css`;
2. decide how the stylesheet path enters DOT (`stylesheet` graph attribute);
3. preserve the current Graphviz layout while testing class-based appearance;
4. document the copy or installation path for `svg.css`.

No JavaScript is required for this stage.
