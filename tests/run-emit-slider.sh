#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
slider="$root/assets/emit-slider.js"
template="$root/templates/emit-d3.html"
css="$root/assets/emit-d3.css"

if command -v node >/dev/null 2>&1; then
  node --check "$slider"
fi

grep -F 'const data = globalThis.emitData;' "$slider" >/dev/null
grep -F 'const graph = globalThis.emitGraph;' "$slider" >/dev/null
grep -F 'Number(link.z) >= threshold' "$slider" >/dev/null
grep -F 'classList.toggle("is-hidden", !visible)' "$slider" >/dev/null
grep -F 'new CustomEvent("emit-z-change"' "$slider" >/dev/null

grep -F '<script src="emit-data.js"></script>' "$template" >/dev/null
grep -F '<script src="../assets/emit-d3.js"></script>' "$template" >/dev/null
grep -F '<script src="../assets/emit-slider.js"></script>' "$template" >/dev/null

data_line=$(grep -n 'src="emit-data.js"' "$template" | cut -d: -f1)
d3_line=$(grep -n 'src="../assets/emit-d3.js"' "$template" | cut -d: -f1)
slider_line=$(grep -n 'src="../assets/emit-slider.js"' "$template" | cut -d: -f1)
[ "$data_line" -lt "$d3_line" ]
[ "$d3_line" -lt "$slider_line" ]

grep -F '.is-hidden { display: none; }' "$css" >/dev/null
grep -F '.emit-distribution-selected' "$css" >/dev/null

printf 'emit-slider.js: ok\n'
