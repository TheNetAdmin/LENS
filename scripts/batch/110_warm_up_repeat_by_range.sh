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


source "$script_root/utils/check_mtrr.sh"
check_mtrr uncacheable

function run_prober() {
    export Profiler=none # none | emon | aepwatch
    yes | ${script_root}/lens.sh "${rep_dev}" "${lat_dev}" "$@"
}

region_array=(
	64 128
	$(seq -s ' ' $((2 ** 8)) $((2 ** 8)) $((2 ** 12 - 1)))
	4096 8192
)
threshold_array=(
	 30000
	 40000
	 60000
	100000
)
repeat_array=(
	2000
)

script=${script_root}/prober/wear_leveling/7_warm_up_repeat_by_range.sh

total_jobs=$(bc -l <<<"${#region_array[@]} * ${#threshold_array[@]} * ${#repeat_array[@]}")
curr_job=0

job=$1
case $job in
    debug)
        run_prober ${script} 256 100000 25
    ;;
    all)
		for region    in "${region_array[@]}"   ; do
		for threshold in "${threshold_array[@]}"; do
		for repeat    in "${repeat_array[@]}"   ; do
			run_prober ${script} \
				${region}        \
				${threshold}     \
				${repeat}        \
			;
			curr_job=$((curr_job + 1))
			progress=$(bc -l <<<"scale=2; 100 * $curr_job / $total_jobs")
			slack_notice $SlackURL "\`[Progress]\` \`[$host_name]\` $progress% [$curr_job / $total_jobs]"
		done
		done
		done
    ;;
esac
