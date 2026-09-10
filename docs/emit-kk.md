# KK snapshot: observational use

The `KK` button in the D3 viewer is an **observational operation**, not the default drawing mode.

The ordinary viewer should first be used with its historical force layout while the observer changes the Z threshold and the C/P weights. Once a threshold of interest has been found, press `KK` to compute a Kamada–Kawai snapshot of the graph that is visible at that threshold.

This distinction is important. Recomputing Kamada–Kawai at every slider movement would be expensive and, more importantly, would mix filtration with continuous changes in geometry. The intended workflow keeps exploration fast and invokes the more expensive structural layout only when the observer asks for it.

## Recommended workflow

```text
ordinary force view
        |
        v
move Z and inspect the filtration
        |
        v
find a threshold of interest
        |
        v
press KK
        |
        v
fixed Kamada-Kawai snapshot
        |
        v
raise Z: remove weaker edges/nodes
        |
        v
lower Z: return only as far as the KK snapshot threshold
```

The KK button records the current Z value as the snapshot floor. After the snapshot has been made, the graph may be filtered more strongly by raising Z, and may then be restored by lowering Z back to that floor. Nodes or edges may also be removed interactively; KK can then be run again on the remaining visible graph.

`Reset Z` is deliberately a full viewer reset. Moving the Z slider is sufficient for ordinary threshold changes; use `Reset Z` when the observation itself should be restarted from the original viewer state.

## Why KK is not the default layout

The historical D3 force view is useful precisely because it is fast and continuous. It is appropriate for searching the parameter space: changing Z, alpha/C, beta/P, and inspecting many candidate regions.

Kamada–Kawai is slower because it uses all-pairs shortest-path relations and iteratively minimizes a global spring-energy objective. It should therefore be treated as a requested structural snapshot rather than as an animation that is recomputed continuously.

The current viewer also provides explicit `Force` and `BFS` operations. These are not fallback renderers or cosmetic alternatives to KK. They are complementary observation geometries applied to the same filtered graph.

```text
Force = recompute local force equilibrium on the visible graph
KK    = inspect all-pairs shortest-path geometry and branch structure
BFS   = inspect breadth/depth layers from the visible top node
```

No one layout is assumed to be globally superior. A structure that persists across layouts is especially useful evidence; a structure that becomes clear only under one layout can reveal what that geometry is particularly good at exposing.

## Force after filtration

The `Force` button reheats the force-directed layout using only the currently visible subgraph. This differs from merely filtering an already computed full-graph layout.

```text
Z filtration
    |
    v
visible subgraph
    |
    v
Force
    |
    v
new local equilibrium
```

At some intermediate Z thresholds this recomputation separates local subclusters more clearly than KK. In source-text inspection, some of these subclusters correspond to individual waka or to small groups of textually related waka. The force calculation itself contains no episode or poem labels; the correspondence is an observation made after tracing the visible relations back to the source text.

## BFS radial observation

The `BFS` operation chooses the highest-degree node in the currently visible graph (with deterministic tie-breaking), computes breadth-first graph distance from that node, and places successive distance layers on increasing radial bands. The retrieval key and the structural center are deliberately kept distinct: a word used to retrieve a lexical world need not be its highest-degree visible node.

The radial geometry makes a two-layer organization particularly easy to inspect. For a BFS root `q`, define

$$
L_1(q)=\{v\mid d(q,v)=1\},
$$

and

$$
L_2(q)=\{v\mid d(q,v)=2\}.
$$

In observations of the Kokinshu data, the inner and outer layers have repeatedly supported a useful textual reading:

```text
L1  topic-direct relations
L2  episode-mediated relations
```

This is an empirical observation, not a semantic rule built into BFS. BFS calculates graph distance only. The interpretation must be tested against the source texts.

The distinction first became conspicuous in observations around `鶯`. The inner layer contained words directly organizing the warbler topic, while the outer layer separated into locally coherent groups when the dominant center was interactively pruned and BFS was recomputed. Source-text inspection identified, among others, a spring-arrival group organized around a warbler coming to a branch and a plum-blossom-hat group involving `梅`, `笠`, `縫ふ`, and related words.

Comparable two-layer organization was subsequently observed with other keys, including `時鳥`, `立田`, and `吉野`. These cases are important because they show that the `鶯` result is not being treated as sufficient evidence for a general rule. The lexical membership and textual basis of each layer must still be inspected independently.

The `吉野` observation supplied a further useful case: a local cluster in the mediated region could be traced to a single choka. A sufficiently long textual unit can therefore generate enough internal relational structure to appear as a local cluster by itself. This suggests that the outer layer should not be equated narrowly with a semantic topic. It can expose **textually mediated local structure**, whose source may be one long poem, several short poems, or another recurrent textual episode.

## Prune and redraw

Interactive node/edge removal is not only a decluttering operation. Recomputing a layout after pruning can expose structure that was geometrically dominated by a highly connected center.

For BFS, the useful observational sequence is:

```text
filtered graph
    |
    v
BFS radial view
    |
    v
inspect direct and mediated layers
    |
    v
prune a dominant node or relation
    |
    v
BFS again on the remaining graph
    |
    v
inspect newly separated local clusters
    |
    v
source-text verification
```

The redraw does not discover literary episodes by itself. It re-geometrizes the remaining relational structure so that a human observer can inspect candidate units that were previously difficult to see.

## Bud–petal structure

A recurring configuration observed in filtered lexical networks is called a **bud–petal structure**.

A **bud** is a central, strongly connected lexical node. A **petal** is an attached local cluster whose principal relation is to the bud or to a short branch extending from it, while lateral connections to other petals are relatively sparse. The **bud–petal structure** is the whole configuration formed by the bud and several such petals.

Schematically:

```text
             petal
            /--+--\
           /       \
          |         |
           \       /
            \--+--/
               |
               |
petal -------- BUD -------- petal
               |
               |
            /--+--\
           /       \
          |  petal  |
           \       /
            \-----/
```

The term describes a network relation, not merely a flower-like visual appearance. Its relevant properties are:

1. a central node provides the main attachment point;
2. attached groups have relatively few lateral relations across groups;
3. individual groups often form short tree-like or locally coherent structures; and
4. the groups can be checked against the source texts rather than being interpreted from geometry alone.

For example, in observations centered on `梅`, one petal contains relations involving `笠`, `縫ふ`, `糸`, and related words. Source-text inspection recovers the waka in which willow branches are construed as thread and the warbler as sewing a hat of plum blossoms. Another branch around `香` retrieves waka organized around the scent of plum. The layout therefore provides a way to locate candidate textual episodes, but the source texts remain the criterion for interpreting what a petal represents.

This source-text check is essential. A visible petal is a **candidate structural unit** until its lexical relations are traced back to one or more actual texts. When a petal corresponds to one waka, or to several waka sharing a semantic or thematic relation, the network structure has an independently inspectable textual basis.

## Core-node pruning

After a bud–petal structure has been identified, the central bud may be
removed interactively and `KK` run again on the remaining visible graph.
This provides a second observational operation: **core-node pruning**.

The purpose is not merely to remove a visually dominant node. If several
petals were attached principally through the bud, removing that node can
expose the internal organization of the remaining branches and separate
them into locally coherent components or substructures.

```text
bud–petal snapshot
        |
        v
remove the bud
        |
        v
remove newly isolated nodes
        |
        v
run KK again
        |
        v
inspect surviving structures
        |
        v
check their source texts
```

A surviving structure is treated as an episode candidate, not as an
episode by definition. Its interpretation must still be checked against
the source texts.

This operation is especially useful for asking whether an apparent petal
has structure of its own. If removal of the central lexical node leaves a
coherent connected structure, the petal is not merely a set of independent
neighbors radiating from the bud. It contains relations among its remaining
nodes that can be inspected independently.

Core-node pruning and Z filtration should therefore be distinguished:

Z filtration removes relations according to the calculated edge threshold.
core-node pruning removes a selected lexical center in order to inspect
the organization that remains.

Used together with KK snapshots and source-text inspection, these operations
provide complementary views of the same emitted lexical network.

## Petal emergence under Z filtration

The Z slider makes the temporal order of observation particularly useful. Begin with a high threshold and lower it gradually in the ordinary force view. Strong central relations survive first; additional branches and local clusters appear as weaker edges are admitted.

At a threshold where this organization becomes interesting, press `KK`. The resulting snapshot can make the graph-distance organization easier to inspect. This process may reveal **petal emergence**: a threshold region in which previously compact central relations acquire distinguishable attached subclusters.

```text
high Z          bud
                 |
                 v
lower Z       short branches
                 |
                 v
lower Z       petal candidates
                 |
                 v
KK snapshot   inspect their separation
                 |
                 v
source text   test their interpretation
```

The important observation is therefore not simply that the final graph resembles a flower. The question is whether filtration reveals a reproducible transition from a compact core to multiple locally coherent, textually interpretable branches.

## KK geometry and interpretation

Kamada–Kawai assigns ideal geometric distances from graph-theoretic shortest-path distances. Nodes separated by more graph steps therefore tend to be placed farther apart, subject to the global energy minimization and the subsequent overlap-removal pass. The current viewer uses `overlap=false` and an `edge_length` setting of `40`, which gives enough separation to inspect small branches without dispersing the network excessively.

The geometry should not itself be interpreted as semantics. KK is used here because it can expose graph-distance organization and relatively independent branches more clearly than a continuously moving force simulation. Semantic or thematic claims should be made only after checking the source-text panel.

## Layout as an observation operator

The three layouts are best treated as different observation operators over the same filtered relational network:

```text
Z / alpha / beta    determine what remains visible
Force / KK / BFS    determine the geometry from which it is observed
source text         determines whether a literary interpretation is supported
```

This makes layout selection methodological rather than decorative. Force can make local equilibrium and subclusters conspicuous; KK can make tree-like branches and shortest-path organization conspicuous; BFS can make graph-distance layers and mediated local structures conspicuous. The observer should compare them rather than assume that one drawing is the network itself.

## Practical interpretation rule

The intended chain of evidence is now broader than KK alone:

```text
cw / emitted relations
        |
        v
Z / alpha / beta filtration
        |
        v
candidate structural transition
        |
        +----> Force: local equilibrium / subclusters
        |
        +----> KK: shortest-path branches
        |
        +----> BFS: direct and mediated distance layers
        |
        v
optional pruning and redraw
        |
        v
source-text inspection
        |
        v
textually supported episode / theme / textual unit
```

Thus no layout validates the network by producing an attractive picture. The viewer supports an observational procedure in which mathematically derived relational structures are repeatedly drawn, inspected, pruned, redrawn, and compared with the textual phenomena from which the network was calculated.
