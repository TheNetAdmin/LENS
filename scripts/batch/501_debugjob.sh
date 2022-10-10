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


if test "${rep_dev}" == "" || test "${rep_dev}" == "${lat_dev}"; then
	echo "ERROR: rep_dev and lat_dev should not be the same"
	echo "       rep_dev: ${rep_dev}"
	echo "       lat_dev: ${lat_dev}"
	exit 1
fi

prober_script=$script_root/prober/debug/debug.sh

function run_prober() {
	# NOTE: single test takes ~5mins, run it 3 rounds takes too much time
	# export Profiler=none
	# yes | $script_root/lens.sh "${rep_dev}" "${lat_dev}" "$@"
	# export Profiler=emon
	# yes | $script_root/lens.sh "${rep_dev}" "${lat_dev}" "$@"
	export Profiler=aepwatch
	yes | $script_root/lens.sh "${rep_dev}" "${lat_dev}" "$@"
}

function bench_func() {
	slack_notice $SlackURL "\`[Start   ]\` $0"
	run_prober "${prober_script}"
	slack_notice $SlackURL "\`[Finish  ]\` <@U01QVMG14HH> check results"
}

job=$1
case $job in
debug)
	bench_func
	;;
all)
	echo "ERROR: Use 'debug' instead of 'all'"
	# bench_func
	;;
esac
