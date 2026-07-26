#!/bin/sh

# Generate a word-pair graph around the lemma "punch" in the
# 2,000-token LitBank sample of The Adventures of Tom Sawyer.
#
# Usage: example/tom-sawyer-punch.sh [OUTPUT.svg]

set -eu

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
repo_dir=$(CDPATH= cd "$script_dir/.." && pwd)

input=$repo_dir/tests/data/tom-sawyer-litbank-2000.cw.txt
output=${1:-$repo_dir/t.svg}

if [ ! -r "$input" ]; then
    printf 'cannot read input: %s\n' "$input" >&2
    exit 1
fi

"$repo_dir/pair" < "$input" |
    "$repo_dir/cw" -k punch -p 2 |
    emit -Z 1.8 |
    neato -Tsvg -o "$output"

printf 'wrote %s\n' "$output"
