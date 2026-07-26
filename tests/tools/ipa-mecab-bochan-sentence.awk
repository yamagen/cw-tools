awk '
/。/ {
    s = $0
    sub(/^[[:space:]　]+/, "", s)

    while (match(s, /。/)) {
        sentence = substr(s, 1, RSTART)
        if (sentence != "")
            print sentence

        s = substr(s, RSTART + RLENGTH)
        sub(/^[[:space:]　]+/, "", s)
    }
}
' tmp/bochan.txt > tmp/bochan-sentences.txt
