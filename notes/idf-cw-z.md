# IDF, CW, and Z: three different measurements

Last change: 2026/07/18-22:12:33.

Hilofumi Yamamoto, Ph.D.  
Institute of Science Tokyo

## Introduction

This note explains the roles of IDF, CW, and Z in `cw-tools`. The important point is not simply that one value is calculated globally and another locally. Each value answers a different question and therefore requires a different reference set.

## 1. IDF: the weight of a word

IDF represents how common or rare a word is in a sufficiently broad text collection.

For a token $t$,

$$
\operatorname{idf}(t)=\log\frac{N}{\operatorname{df}(t)}
$$

where:

- $N$ is the number of units in the reference collection;
- $\operatorname{df}(t)$ is the number of units containing $t$.

IDF should be calculated from the broad collection selected for analysis, such as the whole _Kokinshū_ or a combined collection of several anthologies. It should not be recalculated only from the units extracted by a keyword.

For example, after extracting poems containing `桜`, the word `桜` is no longer rare inside that extracted set. This does not mean that `桜` has little weight as a word. It only means that poems were selected precisely because they contain `桜`. Calculating IDF from the extracted set would therefore destroy the intended meaning of IDF.

A familiar analogy is body weight. To judge whether an elementary-school child's weight is heavy or light, one needs an appropriate social reference population, such as children of the same age. One does not construct a special group consisting only of children selected for being heavy and then use that group as the standard.

Thus IDF is an external reference value: it gives the weight of the word itself within the broader textual society.

## 2. CW: the score of a pair in the extracted units

CW evaluates a token pair in the units selected for the current analysis.

For a pair $(t_1,t_2)$, the current default formula is:

$$
\operatorname{cw}(t_1,t_2)
=
\left(1+\log \operatorname{ctf}(t_1,t_2)\right)
\sqrt{\operatorname{idf}(t_1)\operatorname{idf}(t_2)}
$$

where $\operatorname{ctf}(t_1,t_2)$ is the number of occurrences of the pair in the selected units.

CW therefore combines two kinds of information:

1. the general weight of each word, supplied by IDF from the broad reference collection;
2. the observed strength of the pair in the units extracted for the present topic.

For `cw -k '^桜$'`, the IDF values still come from the broad input collection, but `ctf` is counted only in units containing the exact surface form `桜`. For `cw -k '桜'`, units containing forms such as `桜花` or `山桜` may also be selected, so the resulting pair set and pair frequencies differ.

CW is therefore the raw score produced by the current measurement.

## 3. $Z$: the relative position of a pair within the current topic

Different topics produce different CW distributions. The average CW value for `桜` need not equal the average for `梅`, `鴬`, or `時鳥`.

Z standardizes each CW value within the CW distribution produced by the current extraction:

$$
z_i=\frac{cw_i-\overline{cw}}{s}
$$

where:

- $\overline{cw}$ is the mean of the CW values in the selected pair set;
- $s$ is their sample standard deviation.

This is analogous to comparing scores from different examinations. A score of 70 on examination A and a score of 70 on examination B do not necessarily indicate the same relative performance, because the two examinations may have different means and dispersions. Standardization places the mean of each examination at 0 and expresses each score by its distance from that examination's mean.

The same reasoning applies here:

- pairs extracted for `桜` are standardized using the mean and standard deviation of the `桜` CW distribution;
- pairs extracted for `梅` are standardized using the mean and standard deviation of the `梅` CW distribution.

Thus $Z$ is not another word weight. It is the relative position of a pair inside the distribution generated for the current topic.

If fewer than two pairs are available, or if all CW values are identical and the standard deviation is zero, `cw` assigns `z = 0`.

## 4. Why the reference sets differ

The distinction can be summarized as follows:

| Value | Question                                                   | Reference set                        |
| ----- | ---------------------------------------------------------- | ------------------------------------ |
| IDF   | How common or rare is this word?                           | A broad text collection              |
| CW    | How strong is this pair in the currently selected units?   | Units selected for the current topic |
| Z     | How exceptional is this CW value within the current topic? | The selected topic's CW distribution |

Calling these values merely "global" and "local" is not enough. The essential point is that they measure different things.

- IDF requires a broad external standard because it measures the general weight of a word.
- CW uses the current extraction because it measures the observed score of a pair in that material.
- Z uses the resulting CW distribution because it measures the pair's position within that particular measurement.

## 5. Distributional consequences

Z transformation changes the origin and scale of a CW distribution but does not change its shape.

After standardization:

- the mean is approximately 0;
- the sample standard deviation is approximately 1;
- skewness is unchanged;
- kurtosis is unchanged;
- the ordering of pairs is unchanged.

Therefore, Z transformation does not make a non-normal CW distribution normal. If the CW values show the Pear/Pair Pack Distribution form, that form remains visible after Z transformation.

The coefficient of variation is not meaningful for Z values because their mean is zero or extremely close to zero.

## 6. Division of responsibilities in `cw-tools`

The tools are separated by function:

```text
pair
    generate token pairs within units

cw
    calculate global IDF
    select units by key
    calculate ctf and cdf
    calculate CW
    calculate Z within the selected CW distribution
    output all pair records without pruning

rbin
    calculate descriptive statistics
    produce histograms
    report skewness, kurtosis, and normality statistics

emit
    choose what to display
    prune by Z, CW, degree, or other display conditions
    emit DOT, SVG, JSON, Markdown, or other visualization formats
```

Pruning is not part of measurement. It is a display decision. Therefore `cw` retains every calculated pair, while `emit` decides which edges to show.

## 7. Typical pipelines

Inspect the raw CW distribution:

```sh
grep '^1' tests/data/hachidaishu-pair.txt \
  | ./pair \
  | ./cw -k '^桜$' \
  | awk -F '\t' '{print $9}' \
  | rbin -s
```

Inspect the standardized Z distribution:

```sh
grep '^1' tests/data/hachidaishu-pair.txt \
  | ./pair \
  | ./cw -k '^桜$' \
  | awk -F '\t' '{print $10}' \
  | rbin -s
```

Display only edges whose Z value is at least 1:

```sh
grep '^1' tests/data/hachidaishu-pair.txt \
  | ./pair \
  | ./cw -k '^桜$' \
  | ./emit -Z 1
```

## 8. Compact interpretation

```text
IDF = the word's weight under a broad external standard
CW  = the pair's raw score in the current extraction
Z   = the pair's relative position within that extraction
```

These three values form one continuous measurement process, but they should not be collapsed into one undifferentiated notion of weight.
