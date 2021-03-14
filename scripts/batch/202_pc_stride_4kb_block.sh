#!/bin/bash

set -x

print_help() {
    echo "Usage: $0 [debug|all]"
}

if [ $# -ne 1 ]; then
    print_help
    exit 1
fi

script_root=$(realpath $(realpath $(dirname $0))/../)

function run_prober() {
    # NOTE: single test takes ~5mins, run it 3 rounds takes too much time
    # export Profiler=none
    # yes | $script_root/lens.sh "${rep_dev}" "${lat_dev}" "$@"
    # export Profiler=emon
    # yes | $script_root/lens.sh "${rep_dev}" "${lat_dev}" "$@"
    export Profiler=aepwatch
    yes | $script_root/lens.sh "${rep_dev}" "${lat_dev}" "$@"
}

function debug_prober() {
    export Profiler=none
    yes | $script_root/lens.sh "${rep_dev}" "${lat_dev}" "$@"
}

job=$1
case $job in
debug)
    region_size=16384
    block_size=64
    stride_size=64
    run_prober $script_root/prober/buffer/pointer_chasing_strided.sh $region_size $block_size $stride_size
    block_size=64
    stride_size=128
    run_prober $script_root/prober/buffer/pointer_chasing_strided.sh $region_size $block_size $stride_size
    block_size=256
    stride_size=256
    run_prober $script_root/prober/buffer/pointer_chasing_strided.sh $region_size $block_size $stride_size
    block_size=256
    stride_size=1024
    run_prober $script_root/prober/buffer/pointer_chasing_strided.sh $region_size $block_size $stride_size
    ;;
all)
    block_size=4096
    stride_array=(
        $(($block_size * 1)) $(($block_size * 2)) $(($block_size * 3)) $(($block_size * 4))
        $(($block_size * 5)) $(($block_size * 6)) $(($block_size * 7)) $(($block_size * 8))
    )
    region_array=(
        64 128 256 512 1024 2048 4096 8192
        $(seq -s ' ' $((2 ** 14)) $((2 ** 12)) $((2 ** 16)))
        $(seq -s ' ' $((2 ** 22)) $((2 ** 22)) $((2 ** 26)))
    )
    for stride_size in "${stride_array[@]}"; do
        for region_size in "${region_array[@]}"; do
            if (($region_size % $block_size != 0)); then
                continue
            fi
            run_prober "$script_root/prober/buffer/pointer_chasing_strided.sh" "$region_size" "$block_size" "$stride_size"
        done
    done
    ;;
esac
