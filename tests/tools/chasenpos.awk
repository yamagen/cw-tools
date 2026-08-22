$1 !~ /^Not/ {
    pos[$4]++
    if (!(($4 SUBSEP $6) in seen)) {
        example[$4] = example[$4] " " $6
        seen[$4 SUBSEP $6] = 1
    }
}
END {
    for (p in pos)
        print p, pos[p], example[p]
}
