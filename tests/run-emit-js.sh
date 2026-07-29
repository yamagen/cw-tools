#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT HUP INT TERM

make -C "$root" emit
"$root/emit" -c "$root/config/emit-config.json" -T js \
  "$root/tests/input.tsv" > "$tmp"

grep -F 'globalThis.emitData=' "$tmp" >/dev/null
grep -F '"element_id":"graph-1"' "$tmp" >/dev/null
grep -F '"font_rank":' "$tmp" >/dev/null
grep -F '"z_rank":' "$tmp" >/dev/null
grep -F '"classes":["edge"' "$tmp" >/dev/null

if command -v node >/dev/null 2>&1; then
  node --check "$tmp"
  node - "$tmp" <<'NODE'
require(process.argv[2]);
const data = globalThis.emitData;
if (!data || !Array.isArray(data.nodes) || !Array.isArray(data.links)) {
  process.exit(1);
}
if (!data.nodes.every((node) => node.element_id && Array.isArray(node.classes))) {
  process.exit(1);
}
if (!data.links.every((link) => link.element_id && Array.isArray(link.classes))) {
  process.exit(1);
}
NODE
fi

printf 'emit -T js: ok\n'
