#!/usr/bin/awk -f

# Convert hachidaishu-pos.txt to pair input format:
#
#   unit_id surface/lemma/pos/reading ...
#
# Input token forms:
#   surface/pos/reading
#   surface/pos:lemma:lemma_reading/reading

BEGIN {
    OFS = " "
}

NF == 0 {
    next
}

{
    printf "%s", $1

    for (i = 2; i <= NF; i++) {
        n = split($i, token, "/")
        if (n != 3) {
            printf "hachidaishu2pair.awk: line %d: malformed token: %s\n", \
                   NR, $i > "/dev/stderr"
            exit 1
        }

        surface = token[1]
        reading = token[3]

        m = split(token[2], info, ":")
        pos = info[1]
        lemma = (m >= 2 && info[2] != "") ? info[2] : surface

        printf " %s/%s/%s/%s", surface, lemma, pos, reading
    }

    print ""
}
