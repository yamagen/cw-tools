# emit SVG route

This document records the Graphviz-to-SVG implementation and verification
steps. It is updated one confirmed stage at a time.

For the responsibility boundaries, see
[emit-architecture.md](./emit-architecture.md). For the shared attribute
contract, see [emit-semantic-attributes.md](./emit-semantic-attributes.md).

## Stage 1: semantic IDs and classes

Implemented in `src/emit-dot.c` and shared helpers in `src/emit-util.c`.

The first stage does not attach a stylesheet. Its purpose is to verify that
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
layout compatibility. `font-rank-N` is additional semantic information; it
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
graph. The sign is recorded separately so that a stylesheet can assign line
width from magnitude and color or dash pattern from sign.

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

The maximum is clamped to 9. When every displayed value is equal, rank 5 is
used.

Node font rank uses the configured source (`fq`, `idf`, or displayed degree).
Edge Z rank uses `abs(z)`.

## Stage 1 local verification

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

Use the normal cw-tools pipeline and write SVG through Graphviz. For example:

```sh
... | ./emit -c config/emit-config.json -Tdot -Z 1.6 \
  | neato -Tsvg -o t.svg
inkview t.svg
```

The existing visual result should remain unchanged at this stage. This was
confirmed locally after the semantic attributes were added.

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

## Stage 2: external SVG stylesheet

The initial stylesheet is:

```text
assets/svg.css
```

This stage tests the stylesheet independently before adding another `emit`
configuration field. Graphviz accepts the graph attribute `stylesheet`; its
command-line equivalent is `-Gstylesheet=PATH`.

Generate the SVG with the stylesheet attached:

```sh
... | ./emit -c config/emit-config.json -Tdot -Z 1.6 \
  | neato -Gstylesheet=assets/svg.css -Tsvg -o t.svg
firefox t.svg
```

The generated SVG should contain a processing instruction similar to:

```xml
<?xml-stylesheet href="assets/svg.css" type="text/css"?>
```

Confirm it with:

```sh
head -n 5 t.svg
grep 'xml-stylesheet' t.svg
```

The stylesheet path is interpreted relative to the generated SVG file when a
viewer loads it. Therefore the command above assumes this arrangement:

```text
t.svg
assets/svg.css
```

If the SVG is written to another directory, either copy the stylesheet beside
that output using a suitable relative path, or pass a stylesheet path that is
correct from the SVG file's location.

### SVG attributes and CSS

Graphviz continues to write concrete presentation attributes into SVG, for
example:

```xml
<path fill="none" stroke="black" stroke-width="0.2" ... />
```

`emit` does not read `assets/svg.css`, and Graphviz does not replace that
attribute with the stylesheet value. Instead, Graphviz writes the external CSS
reference and the viewer applies CSS when the SVG is displayed.

For example, an edge may be grouped as:

```xml
<g id="edge&#45;2" class="edge z&#45;rank&#45;4 z&#45;positive">
  <path fill="none" stroke="black" stroke-width="0.2" ... />
</g>
```

and the stylesheet may contain:

```css
.edge.z-rank-4 path {
    stroke-width: 0.70;
    opacity: 0.63;
}
```

A browser resolves `&#45;` to `-` and uses the semantic class selector. The
original `stroke-width="0.2"` remains visible in the SVG source even when the
computed displayed width comes from CSS.

### Initial stylesheet behavior

The first stylesheet intentionally does not change node font size. Graphviz has
already calculated its geometry using the DOT `fontsize`; changing the font
size afterward can make labels exceed their calculated space.

Instead, the stylesheet uses:

```text
font-rank-N -> text opacity and font weight
z-rank-N    -> edge width and opacity
z-negative  -> dashed edge
z-zero      -> dotted edge
```

This makes stylesheet application visibly testable while preserving the
existing node geometry.

### Viewer behavior confirmed locally

The external stylesheet was confirmed in Firefox by temporarily changing one
edge rule to a conspicuous red line with a large width.

Firefox may retain an older local stylesheet in its cache. After changing
`assets/svg.css`, use a forced reload such as:

```text
Ctrl+Shift+R
```

or close and reopen the SVG tab.

In the locally tested environment, `inkview` displayed the SVG's concrete
presentation attributes but did not reflect the external stylesheet referenced
by `<?xml-stylesheet ...?>`. Therefore:

```text
inkview   useful for the ordinary Graphviz SVG and geometry
Firefox   use for testing the external semantic stylesheet
```

This is a viewer distinction, not a failure of the class or rank output.

### Control comparison

The two commands provide a direct comparison:

```sh
# Existing appearance
... | ./emit -c config/emit-config.json -Tdot -Z 1.6 \
  | neato -Tsvg -o t-plain.svg

# Semantic stylesheet
... | ./emit -c config/emit-config.json -Tdot -Z 1.6 \
  | neato -Gstylesheet=assets/svg.css -Tsvg -o t-css.svg

firefox t-plain.svg
firefox t-css.svg
```

Only Graphviz's stylesheet attachment differs between the two pipelines.

## Next stage

After Stage 2 is confirmed locally:

1. decide whether the stylesheet path belongs in `dot.stylesheet`, a command-line
   option, or remains a Graphviz-side concern;
2. adjust the initial CSS rank mapping from the observed graph if necessary;
3. document copying or installing `svg.css` outside the repository;
4. consider font-size control separately because it affects Graphviz layout.

No JavaScript is required for the SVG route at this stage.
