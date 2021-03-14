#!/bin/bash

set -e

source ../common.sh
source ../bench.sh
source ../select_runner.sh

function print_config() {
	bench_echo "Print config"
	echo "    runner_script: ${runner_script}"
}

### Run
mkdir -p "${TaskDir}"
{
	# Print
	print_config

	# Prepare
	prepare_bench
	prepare

	# Run
	bench_run > >(tee "${TaskDir}/benchmark_result.log")

	# Post operations
	post_operations

	# Copy scripts
	cp "${runner_script}" "${TaskDir}/"

	# Cleanup
	cleanup
} > >(ts '[%Y-%m-%d %H:%M:%S]' | tee "${TaskDir}/stdout.log") 2>&1
