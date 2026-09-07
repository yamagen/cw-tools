# D3 layout selection

The browser viewer separates layout computation from D3 rendering.

Select the layout engine in `emit-d3.config.json`:

```json
{
  "layout": "kamada-kawai"
}
```

or retain the historical D3 force layout:

```json
{
  "layout": "force"
}
```

`emit-d3.js` remains responsible for rendering, zoom, dragging, source-text interaction, Z filtering, and the observation panels. The selected layout engine only assigns node coordinates.

## Kamada–Kawai

The Kamada–Kawai implementation is in:

```text
assets/emit-layout-kamada-kawai.js
```

The browser implementation is now deliberately modeled on the observable behavior of Graphviz `neato` with `mode=KK` rather than on D3 force tuning.

Its main stages are:

```text
shortest-path distance matrix
        |
        v
Kamada-Kawai ideal distances
(normalized by graph diameter)
        |
        v
Newton optimization
        |
        v
Graphviz-style overlap=false post-process
(proximity / Delaunay, Prism-like)
        |
        v
component packing
        |
        v
final overlap check + centering
        |
        v
D3 rendering
```

The distance model is unweighted graph-theoretic shortest path, corresponding to the default `model=shortpath` idea in `neato`. Ideal Euclidean lengths are scaled by the component graph diameter rather than using an unbounded `edge_length * distance` scale.

Initialization is deterministic pseudo-random placement. This is closer to the default `neato` starting condition than the earlier circular initialization and remains repeatable from one browser load to the next.

The minimal configuration remains:

```json
{
  "layout": "kamada-kawai"
}
```

### `overlap=false`

```json
{
  "layout": "kamada-kawai",
  "layout_options": {
    "overlap": false
  }
}
```

`overlap=false` is the browser default.

Modern Graphviz uses its Prism proximity-graph overlap-removal algorithm for `overlap=false` when Prism is available; older Graphviz versions and explicit `overlap=voronoi` use a Voronoi method. The browser implementation therefore uses a **Prism-like proximity pass**: an initial gentle scale-up is followed by repeated collision resolution along Delaunay-neighbor relations. A conservative final collision pass removes residual label/node overlaps while attempting to keep the Kamada–Kawai geometry dominant.

This is not Graphviz source code and is not claimed to reproduce Graphviz pixel for pixel. It reproduces the relevant design principles in browser JavaScript: shortest-path KK geometry first, proximity-based overlap removal second, D3 rendering last.

Optional parameters include:

```json
{
  "layout": "kamada-kawai",
  "layout_options": {
    "seed": 1,
    "spring_strength": 1,
    "inner_iterations": 80,
    "max_auto_iterations": 3000,
    "component_padding": 60,
    "yield_every": 8,
    "overlap": false,
    "overlap_padding": 4,
    "overlap_iterations": 120,
    "overlap_scaling": 4,
    "overlap_damping": 0.75
  }
}
```

`edge_length`, `epsilon`, and `iterations` may also be supplied explicitly. If omitted, the browser uses diameter-normalized ideal edge length and graph-size-dependent stopping defaults inspired by `neato`'s KK behavior, with a browser-safe upper bound on automatic iterations.

The `Reheat` button reruns the selected layout. Under Kamada–Kawai it therefore performs a deterministic relayout rather than restarting a D3 force simulation.

## Why this matters for the Z slider

The layout is computed from the full emitted relational graph; moving the Z slider changes visibility rather than recomputing force geometry. A stable shortest-path layout can therefore reveal successive graph-distance shells and attached subclusters as the threshold is lowered.

For a hub such as `梅`, the intended observational behavior is:

```text
high Z       center / strongest relations
   |
   v
lower Z      first ring of direct relations
   |
   v
still lower  attached subclusters become visible
```

This makes it possible to distinguish a genuine structural transition in the filtered network from mere motion caused by a force simulation.

## Force layout

The historical viewer remains available with:

```json
{
  "layout": "force"
}
```

Optional force parameters currently include:

```json
{
  "layout": "force",
  "layout_options": {
    "charge": -50,
    "x_strength": 0.02,
    "y_strength": 0.04
  }
}
```

## Design principle

The separation is intentional:

```text
layout engine
    |
    v
node x/y coordinates
    |
    v
D3 renderer
    |
    +--> zoom / drag / source text
    +--> Z slider
    +--> alpha / beta observation
    +--> retention / POS / C support
```

Thus the graph can be observed with different geometrical layouts without changing the cw calculation or the emitted relational data.
