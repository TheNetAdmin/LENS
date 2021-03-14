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

source "$script_root/utils/check_mtrr.sh"
check_mtrr uncacheable

export SlackURL=https://hooks.slack.com/services/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
source "$script_root/utils/slack.sh"

function run_prober() {
	# NOTE: single test takes ~5mins, run it 3 rounds takes too much time
	# export Profiler=none
	# yes | $script_root/lens.sh "${rep_dev}" "${lat_dev}" "$@"
	# export Profiler=emon
	# yes | $script_root/lens.sh "${rep_dev}" "${lat_dev}" "$@"
	export Profiler=aepwatch
	yes | $script_root/lens.sh "${rep_dev}" "${lat_dev}" "$@"
}

function estimate_time_hours() {
	# $1: stride_array_size
	# $2: region_array_size
	est_time_pre=$(bc -l <<<"scale=2; $1 * $2 * 1 *  14 / 3600") # Based on previous run time
	est_time_min=$(bc -l <<<"scale=2; $1 * $2 * 1 *   5 / 3600")
	est_time_max=$(bc -l <<<"scale=2; $1 * $2 * 1 *  20 / 3600")
	echo "$est_time_pre ($est_time_min - $est_time_max)"
}

access_size=64
echo "Update this script due to pointer_chasing_covert_channel.sh removing access_size argument"
exit 2

block_size=64
stride_array=(
	$((2 ** 16))
	$((2 ** 17))
	$((2 ** 18))
	$((2 ** 19))
	$((2 ** 20)) # 2 ** 20 --> 1 MiB == 16MiB/16Way
	$((2 ** 21))
	$((2 ** 22))
	$((2 ** 23))
	$((2 ** 24))
)
region_array=(
	$((2 **  7))
	$((2 **  8))
	# $((2 **  9))
	$(seq -s ' ' $((2 ** 9)) $((2 ** 6)) $((2 ** 10 - 1)))
	$((2 ** 10)) # 16 * 64 --> 16 blocks --> 16 way
	$((2 ** 11))
	$((2 ** 12))
	$((2 ** 13))
)
sub_op_array=(0) # Covert channel: Pointer chasing read and write
repeat_array=(1 2 4 8 16 32)
region_align=4096

fence_strategy_array=(0)
fence_freq_array=(1)
flush_after_load_array=(0)
record_timing_array=(1) # 1: per_reapeat
flush_l1_array=(0 1)

function bench_func() {
	for repeat in "${repeat_array[@]}"; do
		for flush_l1 in "${flush_l1_array[@]}"; do
			for record_timing in "${record_timing_array[@]}"; do
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
									-DCHASING_RECORD_TIMING=$record_timing \
									-DCHASING_FLUSH_L1=$flush_l1
								EOF
								)
								LENS_MAKE_ARGS=$(echo "$LENS_MAKE_ARGS" | tr -s '\t')
								export LENS_MAKE_ARGS

								est_total=$(estimate_time_hours ${#stride_array[@]} ${#region_array[@]})
								est_checkpoint=$(estimate_time_hours ${#stride_array[@]} 1)

								SLACK_MSG=$(cat <<- EOF
									[Start   ] $(basename "$0")
										[FenceStrategy=$fence_strategy]
										[FenceFreq=$fence_freq]
										[FlushAfterLoad=$flush_after_load]
										[Repeat=$repeat]
										[RecordTiming=$record_timing]
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
										if ((region_size % block_size != 0)); then
											continue
										fi
										if ((region_size * stride_size / block_size + region_align >= $((250 * 2 ** 30 - 64 * 2 ** 30)))); then
											continue
										fi
										run_prober \
											"$script_root/prober/covert/pointer_chasing_covert_channel.sh" \
											"$region_size" \
											"$block_size" \
											"$stride_size" \
											"$fence_strategy" \
											"$fence_freq" \
											"$sub_op" \
											"$repeat" \
											"$region_align" \
											"$flush_after_load" \
											"$access_size"
									done
									iter=$((iter + 1))
									progress=$(bc -l <<<"scale=2; 100 * $iter / ${#region_array[@]}")
									slack_notice $SlackURL "[Progress] $progress% [$iter / ${#region_array[@]}]"
								done
								slack_notice $SlackURL "[End     ] $(basename "$0")"
							done
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
	region_array=(256)
	stride_array=(256)
	flush_l1_array=(1)
	repeat_array=(1)
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
