#!/bin/sh

grep "^1" tests/data/hachidaishu.txt |
./pair |
./cw -p 3,4 --idf-in tests/data/hachidaishu.idf -M 12 -k "梅" |
./emit -Tdot -c config/emit-config.json |
neato -Tsvg -o t.svg

fc -ln -1 > t.command
