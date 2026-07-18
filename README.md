# cw-tools: Command-line exploratory data filters

Last change: 2026/07/18-12:31:44

Hilofumi Yamamoto, Ph.D.
Institute of Science Tokyo

## pair, idf, cw, cm, emit

`cw-tools` is a collection of command-line utilities for pairwise text-data analysis. It includes `pair`, `idf`, `cw`, `cm`, and `emit`, which work together as small, composable Unix-style tools for generating pairs, calculating weights and measures, and exporting results in reusable formats.

The tools read unit-based token data from standard input or files. Each input line begins with a unit identifier, followed by tokens represented by one to four slash-separated fields:

```text
unit_id field1/field2/field3/field4 ...
```

Only `field1` is mandatory. Tokens may therefore take any of the following forms:

```text
surface
surface/lemma
surface/lemma/pos
surface/lemma/pos/gloss
```

The meanings of the four fields are determined by the user. For example, Japanese morphological data may use:

```text
unit_id surface/lemma/pos/reading ...
```

A typical processing pipeline is:

```sh
pair < input.txt | idf | cw | emit
```

## Compile

```sh
make
```

## Install

```sh
make install
```

To install under a custom prefix:

```sh
make PREFIX=$HOME/.local install
```

## Clean

```sh
make clean
```

## Tests

Test data and conversion tools are provided in the `tests` directory.

The Hachidaishu part-of-speech data can be converted to the `cw-tools` input format with:

```sh
awk -f tests/hachidaishu2pair.awk \
    tests/hachidaishu-pos.txt \
    > tests/hachidaishu-pair.txt
```

## Related tools

`rbin` is an independent command-line tool for frequency distributions, descriptive statistics, and distribution diagnostics. Numerical output produced by `cw-tools` can be passed to `rbin` for further analysis.

## Related repositories

- [Hachidaishu Part-of-Speech Dataset](https://doi.org/10.5281/zenodo.13940187)
  [![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.13940187.svg)](https://doi.org/10.5281/zenodo.13940187)

## License

This software is released under the MIT License. See `LICENSE` for details.
