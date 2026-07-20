# Pruning Techniques in cw-tools

Last update: 2026-07-20

## Overview

Pruning is an essential step for exploratory graph visualization.
cw-tools intentionally separates different kinds of pruning so that each
can be combined independently in Unix pipelines.

## 1. Core node selection

Extract only pairs related to a specified keyword.

```sh
./cw -k '^梅/名$'
```

This limits the analysis to pairs containing the core node.

---

## 2. Core node pruning

After CW values are calculated, remove the central keyword from the
visualization.

```sh
grep -v 梅
```

This exposes the surrounding lexical structure while preserving edge
weights computed with respect to the core node.

---

## 3. Annotation pruning

Remove editorial or corpus-specific symbols.

```sh
grep -v ※
grep -v 記号
```

This improves readability without changing the statistical analysis.

---

## 4. Edge pruning

Remove weak associations before graph layout.

```sh
./emit -Z 2.5
```

Only edges satisfying the threshold are visualized.

---

## Example

```sh
grep '^1' tests/data/hachidaishu-pair.txt |
    ./pair |
    ./cw -p 2,3 -k '^梅/名$' -M 7 |
    grep -v 梅 |
    grep -v ※ |
    grep -v "記号" |
    ./emit -Z 2.5 |
    neato -Tpdf -o ume-pruned.pdf
```

This pipeline performs

1. corpus selection
2. pair generation
3. core node selection
4. core node pruning
5. annotation pruning
6. edge pruning
7. graph layout

independently.

## Philosophy

Each pruning operation is intentionally independent.

Rather than implementing numerous pruning options inside cw or emit,
cw-tools encourages users to compose simple filters using standard Unix
commands. This keeps each tool small, transparent, and reusable.
