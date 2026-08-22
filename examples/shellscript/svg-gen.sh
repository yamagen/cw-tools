#!/bin/sh
# run this script to generate the data for a standalone svg file
# cd to the root of the repository and run this script
# graphviz is required to generate the svg file
# ume.svg is generated in the current directory
#
cat tests/data/hachidaishu-bg-split.txt |
  ./pair |
  ./cw -p 5 --substr 16 --idf-out > tests/data/hachidaishu-bg-split-16.idf 
grep "^1" tests/data/hachidaishu-bg-split.txt |
  ./pair |
  ./cw -p 5 --substr 16 --idf-in tests/data/hachidaishu-bg-split-16.idf -M 16 -f "梅" |
  ./emit -Tdot -c config/emit-config.json |
  neato -Tsvg -o ume.svg

cp "$0" ume.command
