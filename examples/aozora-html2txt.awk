#!/usr/bin/awk -f
# aozora-html2txt.awk
#
# 青空文庫のXHTMLから、題名と本文を取り出す。
# ルビは親文字だけを残し、読みと括弧を除く。
#
# 入力はUTF-8を前提とする。
#
# Usage:
#   nkf -w -Lu aozora-bochan.html \
#     | awk -f aozora-html2txt.awk \
#     > aozora-bochan.txt

BEGIN {
    in_main = 0
    title_done = 0
    blank = 1
}

# HTML断片をプレーンテキストとして出力する。
function output_text(s,    n, a, i, text) {
    gsub(/\r/, "", s)

    # ルビの読みと括弧を除く。
    # <rb>の中身は、後段のタグ除去によって文字だけが残る。
    gsub(/<rp[^>]*>[^<]*<\/rp>/, "", s)
    gsub(/<rt[^>]*>[^<]*<\/rt>/, "", s)

    # HTMLの改行をテキストの改行へ変換する。
    gsub(/<br[[:space:]]*\/?>/, "\n", s)

    # 残ったHTMLタグを除く。
    gsub(/<[^>]*>/, "", s)

    # 基本的なHTML実体を戻す。
    gsub(/&nbsp;/, " ", s)
    gsub(/&quot;/, "\"", s)
    gsub(/&#39;/, "'", s)
    gsub(/&lt;/, "<", s)
    gsub(/&gt;/, ">", s)
    gsub(/&amp;/, "\\&", s)

    n = split(s, a, "\n")

    for (i = 1; i <= n; i++) {
        text = a[i]

        # HTML整形用のASCII空白だけを除く。
        # 段落頭の全角空白「　」は残す。
        sub(/^[ \t]+/, "", text)
        sub(/[ \t]+$/, "", text)

        if (text == "") {
            if (!blank) {
                print ""
                blank = 1
            }
        } else {
            print text
            blank = 0
        }
    }
}

{
    line = $0
    gsub(/\r/, "", line)

    # 題名だけを取り出す。著者名は本文データに含めない。
    if (!title_done &&
        line ~ /<h1[[:space:]][^>]*class="title"[^>]*>/) {
        output_text(line)

        if (!blank) {
            print ""
            blank = 1
        }

        title_done = 1
    }

    # main_textより前の目次などは捨てる。
    # main_text開始タグより前に同じ行の目次があっても切り落とす。
    if (!in_main) {
        if (match(line,
                  /<div[[:space:]][^>]*class="main_text"[^>]*>/)) {
            line = substr(line, RSTART + RLENGTH)
            in_main = 1
        } else {
            next
        }
    }

    # 書誌情報の開始地点で本文を終了する。
    if (match(line,
              /<div[[:space:]][^>]*class="bibliographical_information"[^>]*>/)) {
        line = substr(line, 1, RSTART - 1)

        if (line != "")
            output_text(line)

        exit
    }

    output_text(line)
}
