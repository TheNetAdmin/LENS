#!/bin/bash

curr_path=$(realpath $(dirname $0))

pushd $1 || exit 1

logs=$(find -name stdout.log)

{
for f in ${logs[@]}; do
    cat $f | grep "\[pointer-chasing" | awk '{ $1=""; $2=""; $3=""; $4=""; print}'
    echo ""
done
} > agg.log

python3 $curr_path/parse.py

popd || exit 1
