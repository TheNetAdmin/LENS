# The clang-format tools on AEP

if [ $(hostname) == "AEP2" ]; then
    export CLANG_FORMAT="/home/morteza/me/bin/clang-format"
    export CLANG_FORMAT_DIFF="python /home/morteza/me/share/clang/clang-format-diff.py"
fi
