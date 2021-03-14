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
    region_size=512
    block_size=64
    stride_size=4096
    run_prober $script_root/prober/buffer/pointer_chasing_strided.sh $region_size $block_size $stride_size
    # region_size=16384
    # block_size=64
    # stride_size=128
    # run_prober $script_root/prober/buffer/pointer_chasing_strided.sh $region_size $block_size $stride_size
    # region_size=16384
    # block_size=256
    # stride_size=256
    # run_prober $script_root/prober/buffer/pointer_chasing_strided.sh $region_size $block_size $stride_size
    # region_size=16384
    # block_size=256
    # stride_size=1024
    # run_prober $script_root/prober/buffer/pointer_chasing_strided.sh $region_size $block_size $stride_size
;;
all)
    # AIT Buffer 16MB --> 4KB Entry Size --> 16MB/4KB = 4096 Entries
    # 16MB / 1 or 2 or 4 or 8 or 16 or ... 4096
    # region_skip = (region_size / block_size) * stride_size
    # region_skip < DIMM_SIZE
    #   --> region_size <  block_size *DIMM_SIZE / stride_size
    #               1MB    64          256GB       16MB       
    #               2MB    64          256GB        8MB       
    block_size=64
    stride_array=(
        $((4096 * 4096)) $((4096 * 2048)) $((4096 * 1024)) $((4096 * 512))
        $((4096 * 256)) $((4096 *   128)) $((4096 *   64)) $((4096 *  32))
        $((4096 *  24)) $((4096 *    16)) $((4096 *    8)) $((4096 *   7))
        $((4096 *   6)) $((4096 *     5)) $((4096 *    4)) $((4096 *   3))
        $((4096 *   2)) $((4096 *     1)) 
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
            if (($region_size >= $block_size * $((2 ** 38)) / $stride_size )); then
                continue
            fi
            run_prober "$script_root/prober/buffer/pointer_chasing_strided.sh" "$region_size" "$block_size" "$stride_size"
        done
    done
    # RMB Buffer 16KB --> 256B Entry Size --> 16KB/256B = 64 Entries
    # 16KB / 1 or 2 or 4 or 8 or 16 or ... 64
    # The above AIT Buffer already covers 16KB/1 and 16KB/2 and 16KB/4
    block_size=64
    stride_array=(
        $((256 *  1)) $((256 *  2)) $((256 *  3)) $((256 *  4))
        $((256 *  5)) $((256 *  6)) $((256 *  7)) $((256 *  8))
        $((256 *  9)) $((256 * 10)) $((256 * 11)) $((256 * 12))
        $((256 * 13)) $((256 * 14)) $((256 * 15))
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
