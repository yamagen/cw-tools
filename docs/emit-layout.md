# D3 layout observation

The browser viewer uses the D3 force layout as its ordinary interactive geometry and provides a BFS radial operation for inspecting graph-distance layers.

`emit-d3.js` remains responsible for rendering, zoom, dragging, source-text interaction, Z filtering, and the observation panels. The default force layout assigns the initial node coordinates and can be recomputed on the currently visible graph with the `Force` button.

## Force layout

The viewer uses the force layout by default:

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

The `Force` button reheats the force-directed layout using only the currently visible subgraph. This is useful after changing Z, alpha, or beta because the filtered graph can settle into a new local equilibrium instead of retaining geometry inherited from the full graph.

## BFS radial observation

The `BFS` button operates on the currently visible graph. It chooses the highest-degree visible node (with deterministic tie-breaking), computes breadth-first graph distance from that node, and places successive distance layers on increasing radial bands.

This geometry is intended for observing direct and mediated structure around the visible top node. The retrieval key and the BFS root are deliberately kept distinct: a word used to retrieve a lexical world need not be its highest-degree visible node.

BFS is an observation geometry, not a semantic classifier. Any interpretation of a visible ring, branch, or local cluster should be checked against the source texts.

## Z, alpha, and beta

Z, alpha, and beta determine which weighted relations remain visible. Layout operations determine how that currently visible relational structure is drawn.

```text
Z / alpha / beta
        |
        v
visible relational graph
        |
        +--> Force: local force equilibrium / subclusters
        |
        +--> BFS: direct and mediated distance layers
        |
        v
source-text inspection
```

Changing the weights or threshold and redrawing are therefore separate observational operations. The viewer does not treat a visually attractive layout as evidence by itself; visible structures are candidates to be traced back to the source data.

## Design principle

The separation is intentional:

```text
cw / emitted relations
        |
        v
Z / alpha / beta filtration
        |
        v
Force / BFS observation geometry
        |
        v
source-text inspection
```

This keeps the mathematical weighting, network topology, drawing geometry, and textual interpretation distinct while allowing the observer to move repeatedly among them.


## Statistical reproducibility boundary

Force and BFS are observation geometries. They are not part of the statistical definition of a cw result. In the dynamic cw-suite viewer, the `Request` button exposes the shell pipeline through `cw` that generated the current statistical rows. That command can be copied into a terminal and rerun from a clone of the repository.

```text
corpus -> pair -> cw
                    statistical reproducibility boundary
                         |
                         +--> emit -> D3 / Force / BFS
                         +--> Graphviz
                         +--> p5.js
                         +--> spreadsheet
                         +--> researcher-defined processing
```

This separation means that a researcher does not need to reproduce the D3 layout in order to reproduce or reuse the underlying measurements. Layout, filtering for visual inspection, and source-text navigation remain downstream observation choices.
