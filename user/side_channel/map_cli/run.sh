#!/bin/bash

set -e

source ../common.sh
source ../bench.sh
source ./default.sh
source ../select_runner.sh

map_file="${pmem_mnt}/${map_file}"
bench_bin="./${bench_bin}"

map_file_secure="${pmem_mnt}/${map_file_secure}"
bench_bin_secure="./${bench_bin_secure}"

function print_config() {
	bench_echo "Print config"
	echo "    kv_array: (${kv_array[@]})"
	echo "    side_channel_iter: ${side_channel_iter}"
	echo "    task_dir: ${TaskDir}"
	echo "    runner_script: ${runner_script}"
}

function bench_run() {
	if [ ${side_channel_iter} -ne 0 ]; then
		bench_run_side_channel -i ${side_channel_iter} -o probe
	fi
}

### Run
mkdir -p "${TaskDir}"
{
	# Print
	print_config

	# Prepare
	prepare_kv
	prepare

	# Run
	{
		{
			bench_run
		} &
		pid_bench=$!

		{
			bench_kv
		} &
		pid_run=$!

		wait ${pid_bench}
		wait ${pid_run}
	}

	# Post operations
	post_operations

	# Copy scripts
	cp "${runner_script}" "${TaskDir}/"

	# Cleanup
	cleanup
} > >(tee "${TaskDir}/stdout.log") 2>&1
