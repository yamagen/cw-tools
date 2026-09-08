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

In short:

```text
force = explore
KK    = inspect
```

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

## Practical interpretation rule

The intended chain of evidence is:

```text
cw / emitted relations
        |
        v
Z filtration
        |
        v
candidate structural transition
        |
        v
KK snapshot
        |
        v
bud-petal candidate
        |
        v
source-text inspection
        |
        v
textually supported episode / theme
```

Thus KK does not validate the network by producing an attractive layout. It supports an observational procedure in which a mathematically derived relational structure is compared with the textual phenomena from which the network was calculated.
