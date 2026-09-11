#!/usr/bin/awk -f
# awk -f db2cw-split.awk all-v02-21daishu.db > hachidaishu-bg-split.txt
#
# Split cw input, by location:
#   D/E unit -> E00, E01, ...
#   B/C unit -> C00, C01, ...
#   otherwise -> A00
#
# The DB itself is assumed to contain the corrected E hierarchy.  Thus a
# compound such as 立田川 is already E00=立田, E01=川; this converter does
# not add/remove particles or otherwise repair the source analysis.

function clean_surface(s) {
    gsub(/[〈〉]/, "", s)
    return s
}

function emit_token(surface, lemma, cls, reading, bg) {
    token = surface "/" lemma "/" cls "/" reading "/" bg

    if (song_id != prev_song) {
        if (prev_song != "")
            printf "\n"
        printf "%s %s", song_id, token
        prev_song = song_id
    } else {
        printf " %s", token
    }
}

function flush(    i) {
    if (loc == "")
        return

    split(loc, id, ":")
    anthology = id[1] + 0
    poem      = id[2] + 0

    if (anthology >= 1 && anthology <= 8) {
        song_id = sprintf("%d%04d", anthology, poem)

        if (ne > 0) {
            for (i = 0; i <= maxe; i++)
                if (i in es)
                    emit_token(es[i], el[i], ec[i], er[i], eb[i])
        }
        else if (nc > 0) {
            for (i = 0; i <= maxc; i++)
                if (i in cs)
                    emit_token(cs[i], cl[i], cc[i], cr[i], cb[i])
        }
        else if (have_a) {
            emit_token(as, al, ac, ar, ab)
        }
    }

    delete es; delete el; delete ec; delete er; delete eb
    delete cs; delete cl; delete cc; delete cr; delete cb
    ne = nc = 0
    maxe = maxc = -1
    have_a = 0
}

BEGIN {
    loc = ""
    maxe = maxc = -1
}

$1 ~ /^Not/ { next }
$4 == "77" { next }

{
    if (loc != "" && $1 != loc)
        flush()

    loc = $1

    if ($2 == "A00") {
        have_a = 1
        ab = $3
        ac = $4
        as = clean_surface($5)
        al = $6
        ar = $9
    }
    else if ($2 ~ /^C[0-9][0-9]$/) {
        i = substr($2, 2) + 0
        cb[i] = $3
        cc[i] = $4
        cs[i] = clean_surface($5)
        cl[i] = $6
        cr[i] = $9
        nc++
        if (i > maxc)
            maxc = i
    }
    else if ($2 ~ /^E[0-9][0-9]$/) {
        i = substr($2, 2) + 0
        eb[i] = $3
        ec[i] = $4
        es[i] = clean_surface($5)
        el[i] = $6
        er[i] = $9
        ne++
        if (i > maxe)
            maxe = i
    }
}

END {
    flush()
    if (prev_song != "")
        printf "\n"
}
