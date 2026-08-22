#!/usr/bin/env python3
from __future__ import annotations

import argparse
from collections import OrderedDict
from pathlib import Path


def parse_args():
    ap = argparse.ArgumentParser(
        description=(
            "Generate split cw-tools input from all-v02-21daishu.db. "
            "At each token location, prefer the segmented C layer over B, "
            "and the segmented E layer over D; otherwise use A00. "
            "Emit surface/lemma/class/reading/BG-code."
        )
    )
    ap.add_argument("db", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("--first-anthology", type=int, default=1)
    ap.add_argument("--last-anthology", type=int, default=8)
    return ap.parse_args()


def layer_index(layer: str) -> int:
    return int(layer[1:])


def main():
    args = parse_args()
    locations: OrderedDict[tuple[int, int, int], list[tuple[str, str, str, str, str, str]]] = OrderedDict()
    skipped_not_found = 0

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
                token = int(token_s)
            except ValueError as e:
                raise ValueError(f"line {lineno}: bad location {loc!r}") from e

            if not (args.first_anthology <= anthology <= args.last_anthology):
                continue

            locations.setdefault((anthology, poem, token), []).append(
                (layer, bg, cls, surface, lemma, reading)
            )

    poems: OrderedDict[tuple[int, int], list[str]] = OrderedDict()
    n_a = n_c = n_e = 0

    for (anthology, poem, token_no), records in locations.items():
        # Cxx is the segmented counterpart of a B-layer compound.
        c_records = sorted(
            (r for r in records if r[0].startswith("C")),
            key=lambda r: layer_index(r[0]),
        )
        # Exx is the segmented counterpart of a D-layer unit (e.g. proper name).
        e_records = sorted(
            (r for r in records if r[0].startswith("E")),
            key=lambda r: layer_index(r[0]),
        )

        if e_records:
            chosen = e_records
            n_e += 1
        elif c_records:
            chosen = c_records
            n_c += 1
        else:
            chosen = [r for r in records if r[0] == "A00"]
            if len(chosen) != 1:
                raise ValueError(
                    f"{anthology:02d}:{poem:06d}:{token_no:04d}: "
                    f"no unique split-layer record; layers={[r[0] for r in records]}"
                )
            n_a += 1

        out_tokens = poems.setdefault((anthology, poem), [])
        for layer, bg, cls, surface, lemma, reading in chosen:
            out_tokens.append("/".join((surface, lemma, cls, reading, bg)))

    with args.out.open("w", encoding="utf-8") as out:
        for (anthology, poem), tokens in poems.items():
            song_id = f"{anthology}{poem:04d}"
            out.write(song_id)
            if tokens:
                out.write(" " + " ".join(tokens))
            out.write("\n")

    total_tokens = sum(map(len, poems.values()))
    print(f"songs={len(poems)} tokens={total_tokens} out={args.out}")
    print(f"locations: A00={n_a} Cxx={n_c} Exx={n_e}")
    print(f"skipped_not_found={skipped_not_found}")


if __name__ == "__main__":
    main()
