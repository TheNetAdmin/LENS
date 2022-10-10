#!/bin/bash

set -e
set -x

print_help() {
    echo "Usage: $0 [debug|all|estimate]"
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

function estimate_time_hours() {
    # $1: stride_array_size
    # $2: region_array_size
    est_time_pre=$(bc -l <<<"scale=2; $1 * $2 * 1 *   8 / 3600") # Based on previous run time
    est_time_min=$(bc -l <<<"scale=2; $1 * $2 * 1 *   5 / 3600")
    est_time_max=$(bc -l <<<"scale=2; $1 * $2 * 1 *  20 / 3600")
    echo "$est_time_pre ($est_time_min - $est_time_max)"
}

block_size=64
stride_array=(
    # RMB Buffer 16KB --> 256B Entry Size --> 16KB/256B = 64 Entries
    # 16KB / 1 or 2 or 4 or 8 or 16 or ... 64
    # The below AIT Buffer covers 16KB/1 and 16KB/2 and 16KB/4
    $(seq -s ' ' $((2 ** 8)) $((2 ** 8)) $((2 ** 12 - 1)))   # [256B,    4KB) per  256B -> 15
    # AIT Buffer 16MB --> 4KB Entry Size --> 16MB/4KB = 4096 Entries
    # 16MB / 1 or 2 or 4 or 8 or 16 or ... 4096
    # NOTE: latency turn point at:
    #       128KB
    #       2MB
    #       8MB
    $(seq -s ' ' $((2 ** 12)) $((2 ** 12)) $((2 ** 16 - 1))) # [  4KB,  64KB) per   4KB -> 15
    $(seq -s ' ' $((2 ** 16)) $((2 ** 14)) $((2 ** 18 - 1))) # [ 64KB, 256KB) per  16KB -> 12
    $(seq -s ' ' $((2 ** 18)) $((2 ** 16)) $((2 ** 20 - 1))) # [256KB,   1MB) per  64KB -> 12
    $(seq -s ' ' $((2 ** 20)) $((2 ** 18)) $((2 ** 22 - 1))) # [  1MB,   4MB) per 256KB -> 12
    $(seq -s ' ' $((2 ** 22)) $((2 ** 20)) $((2 ** 24 - 1))) # [  4MB,  16MB) per   1MB -> 15
    $(seq -s ' ' $((2 ** 24)) $((2 ** 23)) $((2 ** 26 - 1))) # [ 16MB,  64MB) per   8MB ->  6
)
##If region_skip == 512MB and stride_size == 64MB
# -> region_size / block_size * stride_size == 512MB
# -> region_size = 512MB / 64MB * 8
# -> region_size = 64 = 2**6
##If region_skip == 128GB and stride_size == 64MB
# -> region_size / block_size * stride_size == 128GB
# -> region_size = 128GB / 64MB * 8
# -> region_size = 16K = 16*1024 = 2**14
##region == 2**13 --> 8182 --> more than 4096 entries in ait buffer
region_array=(
    # $((2 **  6)) $((2 **  7)) $((2 **  8)) $((2 **  9))
    # $((2 ** 10)) $((2 ** 11)) $((2 ** 12)) $((2 ** 13))

    # $((2 ** 14)) $((2 ** 16)) $((2 ** 20)) $((2 ** 21))
    # $((2 ** 22)) $((2 ** 23)) $((2 ** 24)) $((2 ** 25))

    # $((2 ** 14))
    # $(seq -s ' ' $((2 ** 15)) $((2 ** 15)) $((2 ** 19 - 1))) # [ 32KB, 512KB) per  32KB ->  15

    $(seq -s ' ' $((2 ** 12)) $((2 ** 11)) $((2 ** 15 - 1))) # [  4KB,  32KB) per   2KB ->  15
    $(seq -s ' ' $((2 ** 15)) $((2 ** 12)) $((2 ** 16 - 1))) # [ 32KB,  64KB) per   4KB ->   7

    # $((2 ** 14)) $((2 ** 15)) $((2 ** 16)) $((2 ** 17))
    # $((2 ** 18)) $((2 ** 19)) $((2 ** 20)) $((2 ** 21))
    # $((2 ** 22)) $((2 ** 23)) $((2 ** 24)) $((2 ** 25))
)
sub_op_array=(0 1)
repeat=16
region_align=4096

fence_strategy_array=(0 2)
fence_freq_array=(1)
# fence_strategy_array=(0 1 2)
# fence_freq_array=(0 1)

job=$1
case $job in
debug)
    region_array=(64)
    stride_array=(256)
    fence_strategy_array=(0)
    fence_freq_array=(0)
    sub_op_array=(0)
    for sub_op in "${sub_op_array[@]}"; do
        for fence_strategy in "${fence_strategy_array[@]}"; do
            for fence_freq in "${fence_freq_array[@]}"; do
                # See following:
                #   - src/Makefile
                #   - src/microbench/chasing.h
                export LENS_MAKE_ARGS="-DCHASING_FENCE_STRATEGY_ID=$fence_strategy -DCHASING_FENCE_FREQ_ID=$fence_freq"
                est_total=$(estimate_time_hours ${#stride_array[@]} ${#region_array[@]})
                est_checkpoint=$(estimate_time_hours ${#stride_array[@]} 1)
                slack_notice $SlackURL "[Start   ] $(basename "$0") [FenceStrategy=$fence_strategy] [FenceFreq=$fence_freq] [Repeat=$repeat] [SubOP=$sub_op] [RegionAlign=$region_align] [EstHours=$est_total)] [EstPerCheckpoint=$est_checkpoint]"
                iter=0
                for region_size in "${region_array[@]}"; do
                    for stride_size in "${stride_array[@]}"; do
                        if (( region_size % block_size != 0 )); then
                            continue
                        fi
                        if (( region_size * stride_size / block_size + region_align >= $((250 * 2 ** 30)) )); then
                            continue
                        fi
                        run_prober "$script_root/prober/buffer/pointer_chasing_strided.sh" "$region_size" "$block_size" "$stride_size" "$fence_strategy" "$fence_freq" "$sub_op" "$repeat" "$region_align"
                    done
                    iter=$((iter + 1))
                    progress=$(bc -l <<< "scale=2; $iter / ${#region_array[@]} * 100")
                    slack_notice $SlackURL "[Progress] $progress% [$iter / ${#region_array[@]}]"
                done
                slack_notice $SlackURL "[End     ] $(basename "$0")"
            done
        done
    done
    slack_notice $SlackURL "[Finish  ] <@U01QVMG14HH> check results"
    ;;
all)
    for sub_op in "${sub_op_array[@]}"; do
        for fence_strategy in "${fence_strategy_array[@]}"; do
            for fence_freq in "${fence_freq_array[@]}"; do
                # See following:
                #   - src/Makefile
                #   - src/microbench/chasing.h
                export LENS_MAKE_ARGS="-DCHASING_FENCE_STRATEGY_ID=$fence_strategy -DCHASING_FENCE_FREQ_ID=$fence_freq"
                est_total=$(estimate_time_hours ${#stride_array[@]} ${#region_array[@]})
                est_checkpoint=$(estimate_time_hours ${#stride_array[@]} 1)
                slack_notice $SlackURL "[Start   ] $(basename "$0") [FenceStrategy=$fence_strategy] [FenceFreq=$fence_freq] [Repeat=$repeat] [SubOP=$sub_op] [RegionAlign=$region_align] [EstHours=$est_total)] [EstPerCheckpoint=$est_checkpoint]"
                iter=0
                for region_size in "${region_array[@]}"; do
                    for stride_size in "${stride_array[@]}"; do
                        if (( region_size % block_size != 0 )); then
                            continue
                        fi
                        if (( region_size * stride_size / block_size >= $((250 * 2 ** 30)) )); then
                            continue
                        fi
                        run_prober "$script_root/prober/buffer/pointer_chasing_strided.sh" "$region_size" "$block_size" "$stride_size" "$fence_strategy" "$fence_freq" "$sub_op" "$repeat" "$region_align"
                    done
                    iter=$((iter + 1))
                    progress=$(bc -l <<< "scale=2; $iter / ${#region_array[@]} * 100")
                    slack_notice $SlackURL "[Progress] $progress% [$iter / ${#region_array[@]}]"
                done
                slack_notice $SlackURL "[End     ] $(basename "$0")"
            done
        done
    done
    slack_notice $SlackURL "[Finish  ] <@U01QVMG14HH> check results"
    ;;
estimate)
    estimate_time_hours ${#stride_array[@]} ${#region_array[@]}
    estimate_time_hours ${#stride_array[@]} 1
    echo "Total tasks: $(( ${#sub_op_array[@]} * ${#fence_strategy_array[@]} * ${#fence_freq_array[@]} ))"
    ;;
esac
