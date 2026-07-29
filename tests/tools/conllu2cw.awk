# conllu2cw.awk - convert CoNLL-U into cw-tools unit lines
#
# Input:
#   CoNLL-U, one token per tab-separated line and one blank line between
#   sentences.  Lemma is column 3, UPOS is column 4, and XPOS is column 5.
#
# Output:
#   unit_id surface/lemma/pos surface/lemma/pos ...
#
# Usage:
#   awk -f conllu2cw.awk input.conllu
#   awk -v keep_punct=1 -f conllu2cw.awk input.conllu
#   awk -v boundaries=1 -f conllu2cw.awk input.conllu
#   awk -v pos=xpos -f conllu2cw.awk input.conllu
#
# Defaults:
#   - punctuation (UPOS=PUNCT) is omitted
#   - sentence-boundary tokens are not added
#   - UPOS is used for the third field
#
# If boundaries=1, <BOS>/<BOS>/BOS and <EOS>/<EOS>/EOS are added.
# CoNLL-U multiword-token rows (for example, the row for "can't") and empty
# nodes are skipped; their component word rows are retained.
#
# CoNLL-U has no gloss column.  The cw-tools fourth field is therefore omitted
# deliberately.  A missing lemma (_) falls back to the surface form.
#
# This program uses only POSIX awk features.

BEGIN {
    FS = "\t"

    if (keep_punct == "")
        keep_punct = 0
    if (boundaries == "")
        boundaries = 0
    if (pos == "")
        pos = "upos"

    if (pos != "upos" && pos != "xpos")
        fail("pos must be either upos or xpos")

    uid = 0
    token_count = 0
    sentence = ""
    fatal = 0
}

function fail(message) {
    print "conllu2cw.awk: " message > "/dev/stderr"
    fatal = 1
    exit 2
}

function flush_sentence(    output) {
    if (token_count == 0) {
        sentence = ""
        return
    }

    uid++
    output = uid

    if (boundaries)
        output = output " <BOS>/<BOS>/BOS"

    output = output sentence

    if (boundaries)
        output = output " <EOS>/<EOS>/EOS"

    print output

    sentence = ""
    token_count = 0
}

# A blank line terminates a CoNLL-U sentence.
/^[[:space:]]*$/ {
    flush_sentence()
    next
}

# CoNLL-U comments include sent_id and the original sentence text.
/^#/ {
    next
}

{
    if (NF < 4)
        fail("line " NR " has fewer than four tab-separated fields")

    token_id = $1

    # Skip multiword-token rows such as 3-4 and empty nodes such as 5.1.
    if (token_id ~ /-/ || token_id ~ /\./)
        next

    form = $2
    lemma = $3
    upos = $4

    if (!keep_punct && upos == "PUNCT")
        next

    if (pos == "xpos")
        tag = $5
    else
        tag = upos

    if (form == "" || form == "_")
        fail("line " NR " has no surface form")
    if (lemma == "" || lemma == "_")
        lemma = form
    if (tag == "" || tag == "_")
        tag = "X"

    # Slash is the cw-tools field separator and cannot occur inside a field.
    if (index(form, "/") || index(lemma, "/") || index(tag, "/"))
        fail("line " NR " contains '/' inside surface, lemma, or POS")

    sentence = sentence " " form "/" lemma "/" tag
    token_count++
}

END {
    if (!fatal)
        flush_sentence()
}
