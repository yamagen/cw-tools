# External D3 route

This document records the first separated D3 route. The existing `-T d3`
output remains available as a self-contained HTML file. The new route separates
graph-specific data from reusable browser code:

```text
cw rows
  -> emit -T js
  -> emit-data.js
  -> emit-d3.js + D3 v7
  -> browser SVG
```

## Files and responsibilities

```text
emit-data.js          generated for one graph by emit -T js
assets/emit-d3.js     reusable cw-tools renderer
assets/emit-d3.css    reusable appearance rules
templates/emit-d3.html assembly template
d3.v7.min.js          external D3 library
```

`emit-data.js`, `emit-d3.js`, and `emit-d3.css` are cw-tools files. D3 itself is
an external library. The template currently loads the pinned D3 7.9.0 browser
bundle from jsDelivr. It may instead point to a locally downloaded copy named,
for example, `d3.v7.min.js`.

## Generate emit-data.js

The first implementation adds `js` as a command-line output format:

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

In this first separation stage, `js` is a CLI-only format. Use `-T js`,
`-Tjs`, `--format js`, or `--format=js`. A configuration entry
`"format": "js"` is not yet accepted by the legacy configuration parser.
The existing `-T d3` behavior is unchanged.

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

The rank and class values are produced by `emit`. `assets/emit-d3.js` does not
calculate a second rank system.

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
```

The order expresses the dependencies directly:

```text
D3 library
  -> emit-d3.js
emit-data.js
  -> emit-d3.js
```

`emit-d3.js` creates the SVG elements, copies `element_id` and `classes` from
the data, runs the initial force layout, supports zoom and drag, and exposes the
result as `globalThis.emitGraph`.

When the initial force simulation ends, node positions are pinned. Later slider
work can therefore hide and show graph layers without recalculating the map.
The renderer also dispatches an `emit-layout-ready` browser event.

## CSS and links

`assets/emit-d3.css` uses the same semantic rank names as Graphviz SVG:

```text
font-rank-0 ... font-rank-9
z-rank-0 ... z-rank-9
z-negative | z-zero | z-positive
has-url
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
```

Then open the graph:

```sh
firefox templates/emit-d3.html
```

## Next stage

`emit-slider.js` will be added after this data and renderer contract is
confirmed with real cw output. It will use ordinary browser controls and alter
visibility on the fixed graph. It will not define another layout or rank
calculation.
