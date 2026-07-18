# pair

## NAME

`pair` — generate token pairs from unit-based data

## SYNOPSIS

```sh
pair [OPTION]... [FILE]...
```

## DESCRIPTION

`pair` reads unit-based token data and generates pairs of tokens found in each unit.

Each input line begins with a unit identifier, followed by one or more whitespace-separated tokens:

```text
unit_id token1 token2 ... tokenN
```

A token may be a simple string or may contain slash-separated fields:

```text
surface/lemma/field3/field4
```

For Japanese morphological data, the four fields may be used as:

```text
surface/lemma/pos/reading
```

The meanings of the fields are determined by the user. `pair` does not interpret or split the token fields; it copies each complete token unchanged.

With no file operand, `pair` reads from standard input.

## OUTPUT

Each generated pair is written as one tab-separated line:

```text
unit_id	token1	token2
```

By default, the two tokens are arranged in lexical order so that the pair is undirected.

## DEFAULT MODE

With no window option, `pair` generates every possible pair of distinct token types within each unit.

Repeated identical tokens in the same unit are treated as one token type. Each undirected pair is therefore emitted once per unit.

Input:

```text
u1 a b a c
```

Output:

```text
u1	a	b
u1	a	c
u1	b	c
```

## OPTIONS

### `-a`, `--adjacent`

Generate pairs only from adjacent token positions.

This is equivalent to:

```sh
pair --window 1
```

Token occurrences are preserved in windowed modes.

### `-w N`, `--window N`

Generate pairs whose token positions differ by no more than `N`.

`N` must be a positive integer.

For example:

```sh
pair --window 2
```

pairs each token with the next one or two tokens in the same unit.

### `-o`, `--ordered`

Preserve the order in which the tokens occur.

Input:

```text
u1 a b a
```

Command:

```sh
pair --adjacent --ordered
```

Output:

```text
u1	a	b
u1	b	a
```

In all-pairs ordered mode, token occurrences are preserved.

### `-u`, `--unordered`

Arrange the two tokens in lexical order.

This is the default.

In a windowed mode, repeated occurrences can generate repeated pairs. Two identical tokens at different positions can generate a self-pair.

### `-h`, `--help`

Display a summary of command-line usage and exit.

### `-v`, `--version`

Display version information and exit.

## EXAMPLES

Generate all undirected token-type pairs from standard input:

```sh
pair < input.txt
```

Generate all pairs from the Kokinshu section of the Hachidaishu test data:

```sh
grep '^1' tests/data/hachidaishu-pair.txt | pair
```

Generate adjacent directed pairs:

```sh
pair --adjacent --ordered input.txt
```

Generate undirected pairs within a window of three positions:

```sh
pair --window 3 input.txt
```

Read several files in sequence:

```sh
pair file1.txt file2.txt
```

## DIAGNOSTICS

A unit containing no token produces a warning on standard error and no output.

Malformed command-line options, an invalid window size, an unreadable file, or a memory allocation failure cause `pair` to exit with a nonzero status.

## EXIT STATUS

`0`
: Successful completion.

nonzero
: An error occurred.

## INPUT DESIGN

`pair` treats the complete token string as the identity of the token. Thus these are distinct tokens:

```text
き/来/カ変-用/き
来/来/動詞/く
```

Selection or transformation of surface forms, lemmas, parts of speech, readings, glosses, or other fields should be performed before or after `pair` by another filter.

## PIPELINE

`pair` is intended to be used as the first stage of the `cw-tools` pipeline:

```sh
pair < input.txt | idf | cw | emit
```

`pair` generates pair occurrences. Subsequent commands count, weight, measure, and format those pairs.

## SEE ALSO

`idf`, `cw`, `cm`, `emit`, `rbin`

## AUTHOR

Hilofumi Yamamoto  
Institute of Science Tokyo

## LICENSE

MIT License
