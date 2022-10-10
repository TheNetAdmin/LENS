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

function run_prober() {
    export Profiler=none # none | emon | aepwatch
    yes | ${script_root}/lens.sh "${rep_dev}" "${lat_dev}" "$@"
}

function estimate_time_hours() {
	# $1: block_array_size
	# $2: region_array_size
	est_time_pre=$(bc -l <<<"scale=2; $1 * $2 * 1 *  14 / 3600") # Based on previous run time
	est_time_min=$(bc -l <<<"scale=2; $1 * $2 * 1 *   5 / 3600")
	est_time_max=$(bc -l <<<"scale=2; $1 * $2 * 1 *  20 / 3600")
	echo "$est_time_pre ($est_time_min - $est_time_max)"
}

prev_block_array=()
block_array=(64 256 4096)

prev_region_array=()
region_array=(
    $((2 ** 8))
    $(seq -s ' ' $((2 **  9)) $((2 **  9)) $((2 ** 12 - 1))) # [512B,    4KB) per  512B -> 7
    $((2 ** 12)) $((2 ** 13))
    $(seq -s ' ' $((2 ** 14)) $((2 ** 12)) $((2 ** 16 - 1))) # [ 16KB,  64KB) per   4KB -> 12
    $(seq -s ' ' $((2 ** 16)) $((2 ** 16)) $((2 ** 18 - 1))) # [ 64KB, 256KB) per  64KB ->  3
    $(seq -s ' ' $((2 ** 18)) $((2 ** 18)) $((2 ** 20 - 1))) # [256KB,   1MB) per 256KB ->  3
    $(seq -s ' ' $((2 ** 20)) $((2 ** 20)) $((2 ** 22 - 1))) # [  1MB,   4MB) per   1MB ->  3
    $(seq -s ' ' $((2 ** 22)) $((2 ** 22)) $((2 ** 26 - 1))) # [  4MB,  64MB) per   4MB -> 16
    $(seq -s ' ' $((2 ** 26)) $((2 ** 26)) $((2 ** 30 - 1))) # [ 64MB,   1GB) per  16MB -> 15
)
sub_op_array=(0)
region_align=4096

fence_strategy_array=(0)
fence_freq_array=(1)
flush_after_load_array=(0)
per_repeat_timing_array=(0)
flush_l1_array=(0)
prober_script="$script_root/prober/buffer/pointer_chasing.sh"

function bench_func() {
	slack_notice_beg "$(basename "$0")"
	for flush_l1 in "${flush_l1_array[@]}"; do
		for per_repeat_timing in "${per_repeat_timing_array[@]}"; do
			for flush_after_load in "${flush_after_load_array[@]}"; do
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

						est_total=$(estimate_time_hours ${#block_array[@]} ${#region_array[@]})
						est_checkpoint=$(estimate_time_hours ${#block_array[@]} 1)

						SLACK_MSG=$(cat <<- EOF
							\`$(basename "$0")\`
								\`\`\`
								$(printf "%16s: %-16s" "FenceStrategy"    "$fence_strategy"   )
								$(printf "%16s: %-16s" "FenceFreq"        "$fence_freq"       )
								$(printf "%16s: %-16s" "FlushAfterLoad"   "$flush_after_load" )
								$(printf "%16s: %-16s" "PerRepeatTiming"  "$per_repeat_timing")
								$(printf "%16s: %-16s" "RegionAlign"      "$region_align"     )
								$(printf "%16s: %-16s" "EstHours"         "$est_total"        )
								$(printf "%16s: %-16s" "EstPerCheckpoint" "$est_checkpoint"   )
								\`\`\`
						EOF
						)

						slack_notice_beg "$SLACK_MSG"
						iter=0
						for region_size in "${region_array[@]}"; do
							for block_size in "${block_array[@]}"; do
								skip_current=0
								for prs in "${prev_region_array[@]}"; do
									for pbs in "${prev_block_array[@]}"; do
										if ((region_size == prs)) && ((block_size == pbs)); then
											echo "Skip previously runned region[$region_size] block[$block_size]"
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
								if ((region_size * block_size + region_align >= dimm_size)); then
									continue
								fi
								run_prober           \
									"$prober_script" \
									"read_and_write" \
									"$region_size"   \
									"$block_size"    \
								;
							done
							iter=$((iter + 1))
							slack_notice_progress "$iter" "${#region_array[@]}"
						done
						slack_notice_end "$(basename "$0")"
					done
				done
			done
		done
	done
	slack_notice_finish_and_notify "Check results: $(basename "$0")"
}

job=$1
case $job in
debug)
	region_array=(4096)
	block_array=(64 256)
	bench_func
	;;
all)
	bench_func
	;;
estimate)
	estimate_time_hours ${#block_array[@]} ${#region_array[@]}
	estimate_time_hours ${#block_array[@]} 1
	echo "Total tasks: $((${#sub_op_array[@]} * ${#fence_strategy_array[@]} * ${#fence_freq_array[@]} * ${#flush_after_load_array[@]}))"
	;;
esac
