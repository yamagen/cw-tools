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

It uses unweighted all-pairs shortest-path distances within each connected component, converts graph-theoretic distance to ideal spring length, minimizes the Kamada–Kawai energy with Newton updates, packs disconnected components, and writes the resulting `x` / `y` coordinates back to the D3 nodes.

The minimal configuration is deliberately simple:

```json
{
  "layout": "kamada-kawai"
}
```

Optional parameters may be supplied with `layout_options`:

```json
{
  "layout": "kamada-kawai",
  "layout_options": {
    "edge_length": 48,
    "spring_strength": 1,
    "epsilon": 0.01,
    "iterations": 300,
    "inner_iterations": 80,
    "component_padding": 60,
    "yield_every": 8
  }
}
```

These defaults are intended as practical browser defaults, not as a claim of pixel-for-pixel equivalence with Graphviz. The implementation follows the Kamada–Kawai energy model; Graphviz may differ in initialization, disconnected-component handling, scaling, and implementation details.

The `Reheat` button reruns the selected layout. Under Kamada–Kawai it therefore performs a deterministic relayout rather than restarting a D3 force simulation.

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
