# awk -f db2cw-split.awk all-v02-21daishu.db > hachidaishu-bg-split.txt
#

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

    if (anthology < 1 || anthology > 8)
        return

    song_id = sprintf("%d%04d", anthology, poem)

    if (ne > 0) {
        for (i = 0; i < ne; i++)
            emit_token(es[i], el[i], ec[i], er[i], eb[i])
    }
    else if (nc > 0) {
        for (i = 0; i < nc; i++)
            emit_token(cs[i], cl[i], cc[i], cr[i], cb[i])
    }
    else if (have_a) {
        emit_token(as, al, ac, ar, ab)
    }

    delete es; delete el; delete ec; delete er; delete eb
    delete cs; delete cl; delete cc; delete cr; delete cb
    ne = nc = 0
    have_a = 0
}

BEGIN {
    loc = ""
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
        if (i + 1 > nc)
            nc = i + 1
    }
    else if ($2 ~ /^E[0-9][0-9]$/) {
        i = substr($2, 2) + 0
        eb[i] = $3
        ec[i] = $4
        es[i] = clean_surface($5)
        el[i] = $6
        er[i] = $9
        if (i + 1 > ne)
            ne = i + 1
    }
}

END {
    flush()
    if (prev_song != "")
        printf "\n"
}
