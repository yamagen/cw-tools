<!--
cw-tools/examples/cw-bochan.md
-->

## Source text

This example uses Natsume Soseki's _Botchan_, obtained from Aozora Bunko.

- Text: [`aozora-bochan.txt`](aozora-bochan.txt)
- Bibliographic and source information:
  [`bochan-source-information.txt`](bochan-source-information.txt)
- Aozora Bunko book card: No. 752

## Prerequisites

- MeCab, a Japanese morphological analyzer, with the IPADIC dictionary
- `awk`, a pattern-scanning and text-processing language
- `nkf`, Network Kanji Filter, for character-encoding conversion
- Graphviz, for generating graphs

If the Aozora Bunko HTML file contains ruby annotations for Japanese
character readings, use `aozora-html2txt.awk` to extract the plain text
while removing the ruby readings.

```sh
cd examples

nkf -w -Lu aozora-bochan.html \
  | awk -f aozora-html2txt.awk \
  > aozora-bochan.txt
```

## Source information

The Japanese bibliographic information supplied by Aozora Bunko and its
English translation are preserved in
[`bochan-source-information.txt`](bochan-source-information.txt).
