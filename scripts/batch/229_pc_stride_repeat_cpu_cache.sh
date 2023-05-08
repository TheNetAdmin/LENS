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
    export Profiler=none # none | emon | aepwatch
    yes | ${script_root}/lens.sh "${rep_dev}" "${lat_dev}" "$@"
}

function debug_prober() {
    export Profiler=none
    yes | $script_root/lens.sh "${rep_dev}" "${lat_dev}" "$@"
}

function estimate_time_hours() {
    # $1: stride_array_size
    # $2: region_array_size
    est_time_pre=$(bc -l <<<"scale=2; $1 * $2 * 2 *  14 / 3600") # Based on previous run time
    est_time_min=$(bc -l <<<"scale=2; $1 * $2 * 2 *   5 / 3600")
    est_time_max=$(bc -l <<<"scale=2; $1 * $2 * 2 *  20 / 3600")
    echo "$est_time_pre ($est_time_min - $est_time_max)"
}

block_size=64
stride_array=(
    $(seq -s ' ' $((2 ** 6)) $((2 ** 6)) $((2 ** 11 - 1)))   # [256B,    4KB) per  256B -> 15
)
region_array=(
    $((2 **  6)) $((2 **  7)) $((2 **  8)) $((2 **  9))
    $((2 ** 10)) $((2 ** 11)) $((2 ** 12)) $((2 ** 13))
    $((2 ** 14)) $((2 ** 15)) $((2 ** 16)) $((2 ** 17))
    $((2 ** 18)) $((2 ** 19)) $((2 ** 20)) $((2 ** 21))
    $((2 ** 22)) $((2 ** 23)) $((2 ** 24)) $((2 ** 25))
    $((2 ** 26)) $((2 ** 27)) $((2 ** 28)) $((2 ** 29))
)
sub_op_array=(0)
repeat=16
region_align=4096

fence_strategy_array=(0)
fence_freq_array=(1)
# fence_strategy_array=(0 1 2)
# fence_freq_array=(0 1)

#flush_after_load_array=(0 1)
flush_after_load_array=(0)

non_temporal=0

job=$1
case $job in
debug)
    region_array=(64)
    stride_array=(256)
    fence_strategy_array=(0)
    fence_freq_array=(0)
    sub_op_array=(0)
    for flush_after_load in "${flush_after_load_array[@]}"; do
        for sub_op in "${sub_op_array[@]}"; do
            for fence_strategy in "${fence_strategy_array[@]}"; do
                for fence_freq in "${fence_freq_array[@]}"; do
                    # See following:
                    #   - src/Makefile
                    #   - src/microbench/chasing.h
                    export LENS_MAKE_ARGS="-DCHASING_FENCE_STRATEGY_ID=$fence_strategy -DCHASING_FENCE_FREQ_ID=$fence_freq -DCHASING_FLUSH_AFTER_LOAD=$flush_after_load -DCHASING_ST_NT=$non_temporal -DCHASING_LD_NT=$non_temporal -DAVX_512=0"
                    est_total=$(estimate_time_hours ${#stride_array[@]} ${#region_array[@]})
                    est_checkpoint=$(estimate_time_hours ${#stride_array[@]} 1)
                    slack_notice $SlackURL "[Start   ] $(basename "$0") [FenceStrategy=$fence_strategy] [FenceFreq=$fence_freq] [FlushAfterLoad=$flush_after_load] [Repeat=$repeat] [SubOP=$sub_op] [RegionAlign=$region_align] [EstHours=$est_total)] [EstPerCheckpoint=$est_checkpoint]"
                    iter=0
                    for region_size in "${region_array[@]}"; do
                        for stride_size in "${stride_array[@]}"; do
                            if (( region_size % block_size != 0 )); then
                                continue
                            fi
                            if (( region_size * stride_size / block_size + region_align >= $((250 * 2 ** 30 - 64 * 2 ** 30)) )); then
                                continue
                            fi
                            run_prober                                                 \
                                "$script_root/prober/buffer/pointer_chasing_strided.sh"\
                                "$region_size"                                         \
                                "$block_size"                                          \
                                "$stride_size"                                         \
                                "$fence_strategy"                                      \
                                "$fence_freq"                                          \
                                "$sub_op"                                              \
                                "$repeat"                                              \
                                "$region_align"                                        \
                                "$flush_after_load"
                        done
                        iter=$((iter + 1))
                        progress=$(bc -l <<< "scale=2; $iter / ${#region_array[@]} * 100")
                        slack_notice $SlackURL "[Progress] $progress% [$iter / ${#region_array[@]}]"
                    done
                    slack_notice $SlackURL "[End     ] $(basename "$0")"
                done
            done
        done
    done
    slack_notice $SlackURL "[Finish  ] <@U01QVMG14HH> check results"
    ;;
all)
    for flush_after_load in "${flush_after_load_array[@]}"; do
        for sub_op in "${sub_op_array[@]}"; do
            for fence_strategy in "${fence_strategy_array[@]}"; do
                for fence_freq in "${fence_freq_array[@]}"; do
                    # See following:
                    #   - src/Makefile
                    #   - src/microbench/chasing.h
                    export LENS_MAKE_ARGS="-DCHASING_FENCE_STRATEGY_ID=$fence_strategy -DCHASING_FENCE_FREQ_ID=$fence_freq -DCHASING_FLUSH_AFTER_LOAD=$flush_after_load -DCHASING_ST_NT=$non_temporal -DCHASING_LD_NT=$non_temporal"
                    est_total=$(estimate_time_hours ${#stride_array[@]} ${#region_array[@]})
                    est_checkpoint=$(estimate_time_hours ${#stride_array[@]} 1)
                    slack_notice $SlackURL "[Start   ] $(basename "$0") [FenceStrategy=$fence_strategy] [FenceFreq=$fence_freq] [FlushAfterLoad=$flush_after_load] [Repeat=$repeat] [SubOP=$sub_op] [RegionAlign=$region_align] [EstHours=$est_total)] [EstPerCheckpoint=$est_checkpoint]"
                    iter=0
                    for region_size in "${region_array[@]}"; do
                        if (( flush_after_load == 0 )) && (( iter < 15 )) ; then
                            iter=$((iter + 1))
                            progress=$(bc -l <<< "scale=2; $iter / ${#region_array[@]} * 100")
                            slack_notice $SlackURL "[Skip] $progress% [$iter / ${#region_array[@]}]"
                            sleep 1
                            continue
                        fi
                        for stride_size in "${stride_array[@]}"; do
                            if (( region_size % block_size != 0 )); then
                                continue
                            fi
                            if (( region_size * stride_size / block_size + region_align >= $((250 * 2 ** 30 - 64 * 2 ** 30)) )); then
                                continue
                            fi
                            run_prober                                                 \
                                "$script_root/prober/buffer/pointer_chasing_strided.sh"\
                                "$region_size"                                         \
                                "$block_size"                                          \
                                "$stride_size"                                         \
                                "$fence_strategy"                                      \
                                "$fence_freq"                                          \
                                "$sub_op"                                              \
                                "$repeat"                                              \
                                "$region_align"                                        \
                                "$flush_after_load"
                        done
                        iter=$((iter + 1))
                        progress=$(bc -l <<< "scale=2; $iter / ${#region_array[@]} * 100")
                        slack_notice $SlackURL "[Progress] $progress% [$iter / ${#region_array[@]}]"
                    done
                    slack_notice $SlackURL "[End     ] $(basename "$0")"
                done
            done
        done
    done
    slack_notice $SlackURL "[Finish  ] <@U01QVMG14HH> check results"
    ;;
estimate)
    estimate_time_hours ${#stride_array[@]} ${#region_array[@]}
    estimate_time_hours ${#stride_array[@]} 1
    echo "Total tasks: $(( ${#sub_op_array[@]} * ${#fence_strategy_array[@]} * ${#fence_freq_array[@]} * ${#flush_after_load_array[@]} ))"
    ;;
esac
