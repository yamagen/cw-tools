#!/bin/sh
# run this script to generate the data for the slider-ume.html page
# cd to the root of the repository and run this script
#

cat tests/data/hachidaishu-bg-split.txt |
  ./pair |
  ./cw -p 5 --substr 16 --idf-out > tests/data/hachidaishu-bg-split-16.idf 
grep "^1" tests/data/hachidaishu-bg-split.txt |
  ./pair |
  ./cw -p 5 --substr 16 --idf-in tests/data/hachidaishu-bg-split-16.idf -M 16 -f "梅" |
  ./emit -T js -c config/emit-config.json > examples/kokin/emit-data.js

cp "$(realpath "$0")" examples/kokin/t.command

# fc -ln -1 > t.command

