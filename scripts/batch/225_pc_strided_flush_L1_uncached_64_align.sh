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

script_root="$(realpath $(realpath $(dirname $0))/../)"


function run_prober() {
    export Profiler=none # none | emon | aepwatch
    yes | ${script_root}/lens.sh "${rep_dev}" "${lat_dev}" "$@"
}

function estimate_time_hours() {
	# $1: stride_array_size
	# $2: region_array_size
	est_time_pre=$(bc -l <<<"scale=2; $1 * $2 * 1 *  14 / 3600") # Based on previous run time
	est_time_min=$(bc -l <<<"scale=2; $1 * $2 * 1 *   5 / 3600")
	est_time_max=$(bc -l <<<"scale=2; $1 * $2 * 1 *  20 / 3600")
	echo "$est_time_pre ($est_time_min - $est_time_max)"
}

block_size=64
prev_stride_array=()
stride_array=(
    # RMB Buffer 16KB --> 256B Entry Size --> 16KB/256B = 64 Entries
    # 16KB / 1 or 2 or 4 or 8 or 16 or ... 64
    # The below AIT Buffer covers 16KB/1 and 16KB/2 and 16KB/4
    $((2 ** 8))
    $(seq -s ' ' $((2 ** 9)) $((2 ** 9)) $((2 ** 12 - 1))) # [512B,    4KB) per  512B -> 7

    # AIT Buffer 16MB --> 4KB Entry Size --> 16MB/4KB = 4096 Entries
    # 16MB / 1 or 2 or 4 or 8 or 16 or ... 4096
    # NOTE: latency turn point at:
    #       128KB
    #       2MB
    #       8MB
    $((2 ** 12)) $((2 ** 13))
    $(seq -s ' ' $((2 ** 14)) $((2 ** 14)) $((2 ** 16 - 1))) # [ 16KB,  64KB) per  16KB ->  3
    $(seq -s ' ' $((2 ** 16)) $((2 ** 16)) $((2 ** 18 - 1))) # [ 64KB, 256KB) per  64KB ->  3
    $(seq -s ' ' $((2 ** 18)) $((2 ** 18)) $((2 ** 20 - 1))) # [256KB,   1MB) per 256KB ->  3
    $(seq -s ' ' $((2 ** 20)) $((2 ** 20)) $((2 ** 22 - 1))) # [  1MB,   4MB) per   1MB ->  3
    $(seq -s ' ' $((2 ** 22)) $((2 ** 22)) $((2 ** 24 - 1))) # [  4MB,  16MB) per   4MB ->  3
    $(seq -s ' ' $((2 ** 24)) $((2 ** 24)) $((2 ** 26 - 1))) # [ 16MB,  64MB) per  16MB ->  3
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
# prev_region_array=(
#     $((2 ** 6)) $((2 ** 7)) $((2 ** 8)) $((2 ** 9))
#     $(seq -s ' ' $((2 ** 10)) $((2 ** 9)) $((2 ** 13 - 1)))  # [  1KB,   8KB) per  512B -> 15
#     $(seq -s ' ' $((2 ** 13)) $((2 ** 10)) $((2 ** 14 - 1))) # [  8KB,  16KB) per   1KB ->  7
#     $(seq -s ' ' $((2 ** 14)) $((2 ** 13)) $((2 ** 16 - 1))) # [ 16KB,  64KB) per   8KB ->  6
#     $(seq -s ' ' $((2 ** 16)) $((2 ** 14)) $((2 ** 19 - 1))) # [ 64KB, 512KB) per  32KB -> 15
#     $(seq -s ' ' $((2 ** 19)) $((2 ** 16)) $((2 ** 20 - 1))) # [512KB,   1MB) per  64KB ->  7
#     $((2 ** 20))
# )

prev_region_array=()
region_array=(
    $(seq -s ' ' $((2 **  6)) $((2 ** 6)) $((2 ** 11 - 1)))  # [  64B,   2KB) per   64B -> 31
    $(seq -s ' ' $((2 ** 11)) $((2 ** 8)) $((2 ** 13 - 1)))  # [  2KB,   8KB) per  256B -> 15
    $(seq -s ' ' $((2 ** 13)) $((2 ** 10)) $((2 ** 14 - 1))) # [  8KB,  16KB) per  256B -> 31
    $(seq -s ' ' $((2 ** 14)) $((2 ** 13)) $((2 ** 16 - 1))) # [ 16KB,  64KB) per   8KB ->  6
    $(seq -s ' ' $((2 ** 16)) $((2 ** 14)) $((2 ** 19 - 1))) # [ 64KB, 512KB) per  32KB -> 15
    $(seq -s ' ' $((2 ** 19)) $((2 ** 16)) $((2 ** 20 - 1))) # [512KB,   1MB) per  64KB ->  7
    $((2 ** 20))
)
sub_op_array=(0)
repeat=32
region_align=64

fence_strategy_array=(0)
fence_freq_array=(1)
flush_after_load_array=(1)
per_repeat_timing_array=(1)
flush_l1_array=(1)

function bench_func() {
	for flush_l1 in "${flush_l1_array[@]}"; do
		for per_repeat_timing in "${per_repeat_timing_array[@]}"; do
			for flush_after_load in "${flush_after_load_array[@]}"; do
				for sub_op in "${sub_op_array[@]}"; do
					for fence_strategy in "${fence_strategy_array[@]}"; do
						for fence_freq in "${fence_freq_array[@]}"; do
							# See following:
							#   - src/Makefile
							#   - src/microbench/chasing.h
							LENS_MAKE_ARGS=$(cat <<- EOF
								-DCHASING_FENCE_STRATEGY_ID=$fence_strategy \
								-DCHASING_FENCE_FREQ_ID=$fence_freq \
								-DCHASING_FLUSH_AFTER_LOAD=$flush_after_load \
								-DCHASING_RECORD_TIMING=$per_repeat_timing \
								-DCHASING_FLUSH_L1=$flush_l1
							EOF
							)
							LENS_MAKE_ARGS=$(echo "$LENS_MAKE_ARGS" | tr -s '\t')
							export LENS_MAKE_ARGS

							est_total=$(estimate_time_hours ${#stride_array[@]} ${#region_array[@]})
							est_checkpoint=$(estimate_time_hours ${#stride_array[@]} 1)

							SLACK_MSG=$(cat <<- EOF
								\`[Start   ]\` \`[$host_name]\` $(basename "$0")
								    [FenceStrategy=$fence_strategy]
								    [FenceFreq=$fence_freq]
								    [FlushAfterLoad=$flush_after_load]
								    [Repeat=$repeat]
								    [PerRepeatTiming=$per_repeat_timing]
								    [SubOP=$sub_op]
								    [RegionAlign=$region_align]
								    [EstHours=$est_total)]
								    [EstPerCheckpoint=$est_checkpoint]
							EOF
							)
							slack_notice $SlackURL "$SLACK_MSG"
							iter=0
							for region_size in "${region_array[@]}"; do
								for stride_size in "${stride_array[@]}"; do
									skip_current=0
									for prs in "${prev_region_array[@]}"; do
										for pss in "${prev_stride_array[@]}"; do
											if ((region_size == prs)) && ((stride_size == pss)); then
												echo "Skip previously runned region[$region_size] stride[$stride_size]"
												skip_current=1
												break
											fi
										done
										if ((skip_current == 1)); then
											break
										fi
									done

									if ((skip_current == 1)); then
										continue
									fi
									if ((region_size % block_size != 0)); then
										continue
									fi
									if ((region_size * stride_size / block_size + region_align >= dimm_size)); then
										continue
									fi
									run_prober \
										"$script_root/prober/buffer/pointer_chasing_strided.sh" \
										"$region_size" \
										"$block_size" \
										"$stride_size" \
										"$fence_strategy" \
										"$fence_freq" \
										"$sub_op" \
										"$repeat" \
										"$region_align" \
										"$flush_after_load"
								done
								iter=$((iter + 1))
								progress=$(bc -l <<<"scale=2; $iter / ${#region_array[@]} * 100")
								slack_notice $SlackURL "\`[Progress]\` \`[$host_name]\` $progress% [$iter / ${#region_array[@]}]"
							done
							slack_notice $SlackURL "\`[End     ]\` \`[$host_name]\` $(basename "$0")"
						done
					done
				done
			done
		done
	done
	slack_notice $SlackURL "[Finish  ] <@U01QVMG14HH> check results"
}

job=$1
case $job in
debug)
	region_array=(64)
	stride_array=(256)
	flush_l1_array=(0 1)
	bench_func
	;;
all)
	bench_func
	;;
estimate)
	estimate_time_hours ${#stride_array[@]} ${#region_array[@]}
	estimate_time_hours ${#stride_array[@]} 1
	echo "Total tasks: $((${#sub_op_array[@]} * ${#fence_strategy_array[@]} * ${#fence_freq_array[@]} * ${#flush_after_load_array[@]}))"
	;;
esac
