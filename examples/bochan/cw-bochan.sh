#!/usr/bin/env bash

set -euo pipefail

script_dir=$(
    cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
    pwd
)

repo_dir=$(
    cd -- "$script_dir/../.."
    pwd
)

input="$script_dir/aozora-bochan.txt"
output="$script_dir/cw-bochan.svg"

mecab "$input" \
    | awk -f "$repo_dir/tests/tools/mecab-ipadic2cw.awk" \
    | "$repo_dir/pair" \
    | "$repo_dir/cw" -p 2,3 -M 12 -k '坊っちゃん' \
    | awk -F '\t' '$1 !~ /\/記号\// && $2 !~ /\/記号\//' \
    | "$repo_dir/emit" \
        -c "$repo_dir/config/emit-config.json" \
        -T dot \
        -Z 1.6 \
    | neato -Tsvg -o "$output"

printf 'Created: %s\n' "$output"
