#!/bin/bash

if [ "$(hostname)" == "AEP2" ]; then
    export PATH="/home/morteza/me/bin":$PATH
    export CLANG_FORMAT_DIFF="python /home/morteza/me/share/clang/clang-format-diff.py"
fi

gcc -E -CC "$1" \
    | sed 's/\\n\"\s\+\"/\\n\"\n\t\"/g' \
    | sed 's/\"\s\+\"//g' \
    | clang-format \
    > "$1.disasm.c"
