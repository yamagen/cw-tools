#!/usr/bin/awk -f
# mecab-ipadic2cw.awk
#
# MeCab + IPADIC の縦出力を、
# 句点「。」ごとに一文一行の cw-tools 入力へ変換する。
#
# Input:
#   surface<TAB>POS1,POS2,POS3,POS4,ctype,cform,base,reading,pronunciation
#
# Output:
#   unit_id<TAB>BOS/BOS/記号/BOS<TAB>
#   surface/base/POS1/POS2 ... 。/。/記号/句点<TAB>
#   EOS/EOS/記号/EOS
#
# CRLF由来の CR がMeCabによって不可視の記号として出力された場合も除く。

BEGIN {
    FS = "\t"
    OFS = "\t"

    unit_id = 1
    buffer = ""

    bos = "BOS/BOS/記号/BOS"
    eos = "EOS/EOS/記号/EOS"
}

# CRを除去する。
# CRだけの形態素なら、surfaceが空になるので後段で捨てられる。
{
    gsub(/\r/, "", $0)
}

# MeCabのEOSは入力行の終端であり、cwのunit終端ではない。
$0 == "EOS" {
    next
}

# 正常な形態素行でなければ捨てる。
NF < 2 {
    next
}

{
    surface = $1

    # CRなどの不可視文字だけだった形態素を除く。
    if (surface == "")
        next

    split($2, f, ",")

    pos1 = f[1]
    pos2 = f[2]
    base = f[7]

    if (base == "" || base == "*")
        base = surface

    if (pos1 == "")
        pos1 = "*"

    if (pos2 == "")
        pos2 = "*"

    # cw-toolsでは / がフィールド区切りなので、
    # 実データ中の / は全角に置換する。
    gsub(/\//, "／", surface)
    gsub(/\//, "／", base)
    gsub(/\//, "／", pos1)
    gsub(/\//, "／", pos2)

    token = surface "/" base "/" pos1 "/" pos2

    if (buffer == "")
        buffer = token
    else
        buffer = buffer OFS token

    # 句点までを一つのunitとして出力する。
    if (surface == "。") {
        print unit_id, bos, buffer, eos
        unit_id++
        buffer = ""
    }
}

END {
    # 最後に句点のない断片が残った場合も失わずに出力する。
    if (buffer != "")
        print unit_id, bos, buffer, eos
}
