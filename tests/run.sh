#!/bin/sh
set -eu
CC=${CC:-cc}
$CC -O2 -std=c11 -Wall -Wextra -Wpedantic -o emit ../src/emit.c -lm
./emit -c ../config/emit-config.json input.tsv > output.dot
grep -F 'fontname="Noto Serif CJK JP"' output.dot >/dev/null
grep -F 'sep="+8"' output.dot >/dev/null
grep -F 'pack=true' output.dot >/dev/null
grep -F 'packmode="graph"' output.dot >/dev/null
grep -F 'splines="line"' output.dot >/dev/null
grep -F 'len="1.4"' output.dot >/dev/null
if command -v neato >/dev/null 2>&1; then
  neato -Tsvg output.dot -o output.svg
fi
./emit -c ../config/emit-config.json -T json input.tsv > output.json
python -m json.tool output.json >/dev/null
printf 'emit 0.6.0: ok\n'
