#!/usr/bin/env python3
from __future__ import annotations

import argparse
from collections import OrderedDict
from pathlib import Path


def parse_args():
    ap = argparse.ArgumentParser(
        description=(
            "Generate cw-tools input from all-v02-21daishu.db. "
            "For the selected anthologies, use A00/B00 records (the tokenization layer), "
            "and emit surface/lemma/class/reading/BG-code."
        )
    )
    ap.add_argument("db", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("--first-anthology", type=int, default=1)
    ap.add_argument("--last-anthology", type=int, default=8)
    return ap.parse_args()


def main():
    args = parse_args()
    poems: OrderedDict[tuple[int, int], list[str]] = OrderedDict()
    skipped_not_found = 0
    skipped_other_layers = 0

    with args.db.open(encoding="utf-8") as fh:
        for lineno, raw in enumerate(fh, 1):
            line = raw.rstrip("\n")
            if not line or line.startswith("Not found:"):
                if line.startswith("Not found:"):
                    skipped_not_found += 1
                continue

            fields = line.split()
            if len(fields) < 9:
                raise ValueError(f"line {lineno}: expected at least 9 whitespace fields: {line!r}")

            loc, layer, bg, cls, surface, lemma, lemma_reading, norm_surface, reading = fields[:9]
            try:
                anthology_s, poem_s, token_s = loc.split(":")
                anthology = int(anthology_s)
                poem = int(poem_s)
                int(token_s)
            except ValueError as e:
                raise ValueError(f"line {lineno}: bad location {loc!r}") from e

            if not (args.first_anthology <= anthology <= args.last_anthology):
                continue

            # A00 = ordinary token; B00 = unsegmented compound token.
            # C/D/E records are alternative segmentation layers and are not emitted here.
            if layer not in {"A00", "B00"}:
                skipped_other_layers += 1
                continue

            # Keep the source DB's surface and surface reading.  The third field is the
            # source 2-digit class code; it is deliberately not converted to the old
            # hachidaishu.txt's detailed POS labels.
            token = "/".join((surface, lemma, cls, reading, bg))
            poems.setdefault((anthology, poem), []).append(token)

    with args.out.open("w", encoding="utf-8") as out:
        for (anthology, poem), tokens in poems.items():
            # Preserve the traditional cw-tools song-id shape: 10001, 20001, ...
            song_id = f"{anthology}{poem:04d}"
            out.write(song_id)
            if tokens:
                out.write(" " + " ".join(tokens))
            out.write("\n")

    total_tokens = sum(map(len, poems.values()))
    print(f"songs={len(poems)} tokens={total_tokens} out={args.out}")
    print(f"skipped_not_found={skipped_not_found} skipped_other_layers={skipped_other_layers}")


if __name__ == "__main__":
    main()
