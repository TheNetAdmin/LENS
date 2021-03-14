#!/bin/bash

source ../common.sh
source ../bench.sh
source ../select_runner.sh

function print_config() {
	bench_echo "Print config"
	echo "    runner_script: ${runner_script}"
}


### Make
make -C ../nvleak

### Run
mkdir -p "${TaskDir}"
{
	# Print
	print_config

	# Prepare
	prepare_victim
	prepare

	# Run
	{
		{
			time bench_run
		} &
		pid_bench=$!

		{
			sleep 2
			time bench_victim
		} &
		pid_run=$!

		wait ${pid_bench}
		wait ${pid_run}
	}

	# Post operations
	post_operations

	# Copy scripts
	# cp "${runner_script}" "${TaskDir}/"

	# Cleanup
	cleanup
} > >(ts '[%Y-%m-%d %H:%M:%S]' | tee "${TaskDir}/stdout.log") 2>&1
