# Generate unit_id -> source-text JSON from all-v02-21daishu.db.
#
# Usage:
#   awk -f tests/tools/db2texts.awk tests/data/all-v02-21daishu.db \
#     > tests/data/emit-texts.json
#
# Output shape:
#   {
#     "10001": {"surface": "..."},
#     "10002": {"surface": "..."}
#   }
#
# The source surface is reconstructed from the non-split records used by
# db2cw.awk: A00 for ordinary tokens and B00 for unsplit compound tokens.
# C/D/E child records and POS 77 annotation/symbol records are ignored.

function clean_surface(s) {
    gsub(/[〈〉]/, "", s)
    return s
}

function json_escape(s) {
    gsub(/\\/, "\\\\", s)
    gsub(/"/, "\\\"", s)
    gsub(/\t/, "\\t", s)
    gsub(/\r/, "\\r", s)
    gsub(/\n/, "\\n", s)
    return s
}

function flush() {
    if (unit_id == "")
        return

    if (!first)
        printf ",\n"

    printf "  \"%s\": {\"surface\": \"%s\"}", \
           json_escape(unit_id), json_escape(surface)
    first = 0
}

BEGIN {
    print "{"
    first = 1
    unit_id = ""
    surface = ""
}

$1 ~ /^Not/ { next }
$2 != "A00" && $2 != "B00" { next }
$4 == "77" { next }

{
    split($1, id, ":")
    anthology = id[1] + 0
    poem = id[2] + 0

    if (anthology < 1 || anthology > 8)
        next

    current_id = sprintf("%d%04d", anthology, poem)

    if (unit_id != "" && current_id != unit_id) {
        flush()
        surface = ""
    }

    unit_id = current_id
    surface = surface clean_surface($5)
}

END {
    flush()
    if (!first)
        printf "\n"
    print "}"
}
