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


rep_dev="${rep_dev:-/dev/pmem0}"
lat_dev="${lat_dev:-/dev/pmem13}"

if test "${rep_dev}" == "${lat_dev}"; then
	echo "ERROR: rep_dev and lat_dev should not be the same"
	echo "       rep_dev: ${rep_dev}"
	echo "       lat_dev: ${lat_dev}"
	exit 1
fi

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
	est_time_pre=$(bc -l <<<"scale=2; $1 * $2 * 1 *  60 / 3600") # Based on previous run time
	est_time_min=$(bc -l <<<"scale=2; $1 * $2 * 1 *  45 / 3600")
	est_time_max=$(bc -l <<<"scale=2; $1 * $2 * 1 * 480 / 3600")
	echo "$est_time_pre ($est_time_min - $est_time_max)"
}

prober_script=$script_root/prober/channel/wear_leveling_covert_channel.sh
covert_fid_array=(
	2
	6
)
region_array=(
	256
)
stride_array=(
	$((2 **  8))
	$((2 **  9))
	$((2 ** 10))
	$((2 ** 11))
	$((2 ** 12))
	$((2 ** 13))
	$((2 ** 14))
	$((2 ** 15))
	$((2 ** 16))
	$((2 ** 17))
	$((2 ** 18))
	$((2 ** 19))
	$((2 ** 20))
	$((2 ** 21))
	$((2 ** 22))
)
delay_array=(
	$((2 ** 14))
	$((2 ** 15))
	$((2 ** 16))
	$((2 ** 17))
)
delay_per_byte_array=(
	256
)
sub_op_array=(0)
count_array=(20)
repeat_array=(
	$((2 ** 14))
	$((2 ** 15))
	$((2 ** 16))
)

function bench_func() {
	slack_notice $SlackURL "\`[Start   ]\` $0"
	for covert_fid     in "${covert_fid_array[@]}";     do
	for delay          in "${delay_array[@]}";          do
	for delay_per_byte in "${delay_per_byte_array[@]}"; do
	for sub_op         in "${sub_op_array[@]}";         do
	for count          in "${count_array[@]}";          do
	for repeat         in "${repeat_array[@]}";         do
		est_total=$(estimate_time_hours ${#stride_array[@]} ${#region_array[@]})
		est_checkpoint=$(estimate_time_hours ${#stride_array[@]} 1)

		SLACK_MSG=$(cat <<- EOF
			\`\`\`
			[Start   ] $(basename "$0")
				[Delay=$delay]
				[DelayPerByte=$delay_per_byte]
				[SubOP=$sub_op]
				[Count=$count]
				[Repeat=$repeat]
				[CovertFileID=$covert_fid]
				[EstHours=$est_total)]
				[EstPerCheckpoint=$est_checkpoint]
			\`\`\`
		EOF
		)
		slack_notice $SlackURL "$SLACK_MSG"
		iter=0
		for region_size in "${region_array[@]}"; do
		for stride_size in "${stride_array[@]}"; do
			run_prober			 \
				"$prober_script" \
				"$region_size"	 \
				"$stride_size"	 \
				"$delay"		 \
				"$delay_per_byte"\
				"$sub_op"		 \
				"$repeat"		 \
				"$count"		 \
				"$covert_fid"
			iter=$((iter + 1))
			if ((iter % 2 == 0)); then
				progress=$(bc -l <<<"scale=2; 100 * $iter / (${#region_array[@]} * ${#stride_array[@]})}")
				slack_notice $SlackURL "\`\`\`\n[Progress] $progress% [$iter / (${#region_array[@]} * ${#stride_array[@]})]\n\`\`\`"
			fi
		done
		done
		slack_notice $SlackURL "\`\`\`\n[End     ] $(basename "$0")\n\`\`\`"
	done
	done
	done
	done
	done
	done
	slack_notice $SlackURL "\`[Finish  ]\` <@U01QVMG14HH> check results"
}

job=$1
case $job in
debug)
	region_array=(256)
	stride_array=(2048)
	delay_array=(65536)
	delay_per_byte_array=(256)
	repeat_array=(8192)
	count_array=(20)
	covert_fid_array=(2)
	# export sync_per_iter=0
	bench_func
	;;
all)
	bench_func
	;;
estimate)
	estimate_time_hours ${#stride_array[@]} 1
	estimate_time_hours ${#stride_array[@]} ${#region_array[@]}
	echo "Total tasks: $((${#delay_array[@]} * ${#delay_per_byte_array[@]} * ${#sub_op_array[@]} * ${#repeat_array[@]} * ${#count_array[@]} * ${#covert_fid_array[@]}))"
	;;
esac
