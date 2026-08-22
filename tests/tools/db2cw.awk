# awk -f db2cw.awk all-v02-21daishu.db > hachidaishu-bg.txt

BEGIN {
    OFS = " "
}

$1 ~ /^Not/ { next }
$4 == "77" { next }

{
    split($1, id, ":")
    anthology = id[1] + 0
    poem      = id[2] + 0

    if (anthology < 1 || anthology > 8)
        next

    if ($2 != "A00" && $2 != "B00")
        next

    song_id = sprintf("%d%04d", anthology, poem)

    surface = $5
    gsub(/[〈〉]/, "", surface)

token = surface "/" $6 "/" $4 "/" $9 "/" $3

    if (song_id != prev) {
        if (NR > 1 && prev != "")
            printf "\n"
        printf "%s %s", song_id, token
        prev = song_id
    } else {
        printf " %s", token
    }
}

END {
    if (prev != "")
        printf "\n"
}
