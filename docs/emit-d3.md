# External D3 route

This document records the separated D3 route. The existing `-T d3` output
remains available as a self-contained HTML file. The external route separates
graph-specific data, reusable rendering, and visibility controls:

```text
cw rows
  -> emit -T js
  -> emit-data.js
  -> emit-d3.js + D3 v7
  -> fixed browser SVG
  -> emit-slider.js
  -> visibility changes on the fixed SVG
```

## Files and responsibilities

```text
emit-data.js               generated for one graph by emit -T js
assets/emit-d3.js          reusable cw-tools renderer
assets/emit-slider.js      reusable Z-threshold visibility controller
assets/emit-interaction.js reusable browser interaction controller
assets/emit-d3.css         reusable appearance rules
templates/emit-d3.html     assembly template
d3.v7.min.js               external D3 library
```

`emit-data.js`, `emit-d3.js`, `emit-slider.js`, `emit-interaction.js`, and
`emit-d3.css` are cw-tools files. D3 itself is an external library. The template
currently loads the pinned D3 7.9.0 browser bundle from jsDelivr. It may instead
point to a locally downloaded copy named, for example, `d3.v7.min.js`.

## Generate emit-data.js

The external route adds `js` as a command-line output format:

```sh
... | ./emit -c config/emit-config.json -T js > templates/emit-data.js
```

The result is JavaScript rather than pure JSON:

```js
"use strict";
globalThis.emitData = {
  "element_id": "graph-1",
  "directed": false,
  "nodes": [],
  "links": []
};
```

The object body is JSON-compatible. The assignment allows a local HTML file to
load the data with an ordinary `<script>` element; no `fetch()` call or local
web server is required.

`js` is currently a CLI-only format. Use `-T js`, `-Tjs`, `--format js`, or
`--format=js`. A configuration entry `"format": "js"` is not yet accepted by
the legacy configuration parser. The existing `-T d3` behavior is unchanged.

## Data contract

The graph object contains:

```text
element_id
classes
directed
font_size_by
rank_count
link_distance
nodes
links
```

Each node contains both measured values and semantic display attributes:

```text
id, label, df, idf, fq, degree
font_size                 compatibility geometry value
element_id                node-1, node-2, ...
font_size_by
font_rank                 0 ... 9
classes                   node, font-by-*, font-rank-*
```

Each link contains:

```text
source, target, ctf, cdf, cw, z, unit_ids
label
element_id                edge-1, edge-2, ...
z_rank                    0 ... 9
z_sign                    negative, zero, positive
classes                   edge, z-rank-*, z-*, optional has-url
url, url_target           optional link destination
```

The rank and class values are produced by `emit`. Neither `assets/emit-d3.js`
nor `assets/emit-slider.js` calculates a second rank system.

## Render the graph

With `templates/emit-data.js` generated, open the supplied template:

```sh
firefox templates/emit-d3.html
```

The template loads files in this order:

```html
<script src="https://cdn.jsdelivr.net/npm/d3@7.9.0/dist/d3.min.js"></script>
<script src="emit-data.js"></script>
<script src="../assets/emit-d3.js"></script>
<script src="../assets/emit-interaction.js"></script>
<script src="../assets/emit-slider.js"></script>
```

The dependency chain is:

```text
D3 library + emit-data.js
          -> emit-d3.js
          -> fixed SVG
emit-data.js + fixed SVG
          -> emit-interaction.js
          -> emit-slider.js
```

`emit-d3.js` creates the SVG elements, copies `element_id` and `classes` from
the data, runs the initial force layout, supports zoom and drag, and exposes the
result as `globalThis.emitGraph`.

When the initial force simulation ends, node positions are pinned. Slider
changes therefore do not recalculate the map. The renderer also dispatches an
`emit-layout-ready` browser event.

## Z slider

`assets/emit-slider.js` reads the existing `z` values and builds one ordinary
HTML range control. Its first implementation uses this rule:

```text
visible edge: z >= threshold
visible node: connected to at least one visible edge
```

The slider performs presentation only:

```text
no Z calculation
no rank calculation
no distribution estimation by cw or emit
no force-layout restart
no mutation of emit-data.js
```

For each input event it toggles the `is-hidden` class on existing edge and node
SVG groups. The distribution panel is a small histogram derived from the
already supplied link values. Its selected fill covers the retained right-hand
region from the current threshold to the maximum Z value.

The control reports the current threshold together with retained edge and node
counts and percentages. Edge and node counts are shown separately so that the
change in link volume and node coverage can be observed at the same Z
threshold. It also dispatches:

```js
new CustomEvent("emit-z-change", { detail })
```

and exposes:

```js
globalThis.emitSlider.setThreshold(value)
globalThis.emitSlider.reset()
```

`emit-slider.js` does not call D3. It depends on the SVG already created by
`emit-d3.js` and uses browser DOM and SVG APIs only.

### Double-click pruning

The external viewer also supports manual pruning in addition to Z-threshold
filtering. Double-clicking a node makes that node and all edges incident to it
transparent. This is an interactive presentation operation implemented by
`assets/emit-interaction.js`.

```text
Z slider       statistical pruning by threshold
single click   show source texts for the node
double click   make the node and its incident edges transparent
reload         restore the original viewer state
```

Double-click pruning does not change `emit-data.js`, recalculate Z values,
restart the force layout, or remove the node from the underlying graph data. It
therefore allows an observer to suppress visually unhelpful nodes after the
statistical Z filtering has exposed the region of interest, while keeping the
original cw/emit result unchanged.

This distinction is intentional. The Z slider provides reproducible filtering
based on the supplied statistics, whereas double-click pruning provides
observer-directed pruning during exploration. The slider `Reset` operation
resets only the Z threshold; a browser reload restores manually pruned nodes and
edges.

## CSS and links

`assets/emit-d3.css` uses the same semantic rank names as Graphviz SVG:

```text
font-rank-0 ... font-rank-9
z-rank-0 ... z-rank-9
z-negative | z-zero | z-positive
has-url
is-hidden
```

An edge URL generated from `unit_ids` wraps the visible edge line, its wider
transparent hit line, and its numeric label in one SVG anchor. The URL is
resolved relative to the HTML page, not relative to `emit-data.js`.

Firefox may cache a previous `emit-data.js` or stylesheet. Use `Ctrl+Shift+R`
after regenerating data or editing CSS.

## Local verification

Build and generate the data:

```sh
make clean
make emit
... | ./emit -c config/emit-config.json -T js > templates/emit-data.js
```

Inspect the contract:

```sh
head -c 120 templates/emit-data.js

grep -o 'font-rank-[0-9]' templates/emit-data.js | head
grep -o 'z-rank-[0-9]' templates/emit-data.js | head
```

When Node.js is installed, syntax-check the generated and reusable JavaScript:

```sh
node --check templates/emit-data.js
node --check assets/emit-d3.js
node --check assets/emit-interaction.js
node --check assets/emit-slider.js
```

Run the supplied static checks:

```sh
sh tests/run-emit-js.sh
sh tests/run-emit-slider.sh
```

Then open the graph:

```sh
firefox templates/emit-d3.html
```

The default slider position is the minimum Z value, so all edges are initially
visible. Move it to the right to retain progressively higher-Z edges while the
node positions remain fixed. Double-click individual nodes when manual pruning
is useful; reload the page to restore the unpruned viewer state.
