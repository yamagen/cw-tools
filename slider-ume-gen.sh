#!/bin/sh

grep "^1" tests/data/hachidaishu.txt |
grep -v 記号 |sed -e 's/むめ/うめ/g' |
./pair |
./cw -p 3,4 --idf-in tests/data/hachidaishu.idf -M 12 -k "うめ" |
./emit -T js -c config/emit-config.json > templates/emit-data.js

fc -ln -1 > t.command

