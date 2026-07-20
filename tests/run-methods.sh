#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"

./pair tests/methods-units.txt > tests/methods-pairs.tsv
for method in 1 7 12 16; do
    ./cw -p 1 -k '^K$' -M "$method" tests/methods-pairs.tsv \
        > "tests/method-$method.tsv"
done

python3 - <<'PY'
import math
from pathlib import Path

def ab_cw(method):
    for line in Path(f"tests/method-{method}.tsv").read_text().splitlines():
        f = line.split("\t")
        if f[0] == "A" and f[1] == "B":
            return float(f[10])
    raise AssertionError(f"A--B missing for method {method}")

idf = math.log(4.0 / 3.0)
expected = {
    1: (1.0 + math.log(2.0)) * idf,
    7: (1.0 + math.log10(2.0)) * idf / (1.0 + math.log10(3.0)),
    12: (1.0 + (3 // 2)) * idf,
    16: (1.0 + math.log(4.0 / 3.0)) * idf * (1.0 + math.log(2.0)),
}
for method, value in expected.items():
    actual = ab_cw(method)
    assert math.isclose(actual, value, rel_tol=1e-13, abs_tol=1e-15), \
        (method, actual, value)
print("CW methods 1, 7, 12, 16: ok")
PY
