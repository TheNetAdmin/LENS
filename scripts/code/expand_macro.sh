#!/bin/bash

gcc -E -CC "$1" \
    | sed 's/\\n\"\s\+\"/\\n\"\n\t\"/g' \
    | sed 's/\"\s\+\"//g' \
    | clang-format \
    > "$1.disasm.c"
