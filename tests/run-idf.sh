#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"

tmp=${TMPDIR:-/tmp}/cw-idf-test.$$
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp"

./pair tests/methods-units.txt > "$tmp/pairs.tsv"
./cw -p 1 --idf-out "$tmp/pairs.tsv" > "$tmp/reference.idf"

./cw -p 1 -k '^K$' -M 7 "$tmp/pairs.tsv" > "$tmp/local.tsv"
./cw -p 1 -k '^K$' -M 7 --idf-in "$tmp/reference.idf" \
    "$tmp/pairs.tsv" > "$tmp/external.tsv"
cmp "$tmp/local.tsv" "$tmp/external.tsv"

printf 'broken\n' > "$tmp/broken.idf"
if ./cw -p 1 -k '^K$' -M 7 --idf-in "$tmp/broken.idf" \
    "$tmp/pairs.tsv" > /dev/null 2> "$tmp/broken.err"; then
    echo "malformed IDF file was accepted" >&2
    exit 1
fi
grep -F 'regenerate it with cw --idf-out' "$tmp/broken.err" > /dev/null

sed '$d' "$tmp/reference.idf" > "$tmp/missing.idf"
if ./cw -p 1 -k '^K$' -M 7 --idf-in "$tmp/missing.idf" \
    "$tmp/pairs.tsv" > /dev/null 2> "$tmp/missing.err"; then
    echo "incomplete IDF file was accepted" >&2
    exit 1
fi
grep -F 'missing IDF for pattern' "$tmp/missing.err" > /dev/null

printf 'CW IDF input/output: ok\n'
