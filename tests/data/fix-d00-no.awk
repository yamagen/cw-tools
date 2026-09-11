#!/usr/bin/awk -f

BEGIN {
    OFS = " "
}

{
    id = $1
    type = $2

    # position が変わったら状態をリセット
    if (id != prev_id) {
        target = 0
        skip_no = 0
        prev_id = id
    }

    # D00 の表層($5)に「の」がないものを候補にする
    if (type == "D00") {
        target = (index($5, "の") == 0)
        print
        next
    }

    # D00に「の」がなく、E01が補われた「の」なら削除
    if (target && type == "E01" && $5 == "の") {
        skip_no = 1
        next
    }

    # 削除したE01の後のE02をE01へ繰り上げる
    if (target && skip_no && type == "E02") {
        $2 = "E01"
        skip_no = 0
        print
        next
    }

    print
}
