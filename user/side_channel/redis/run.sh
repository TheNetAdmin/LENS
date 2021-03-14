#!/bin/bash

set -e

source ../common.sh
source ../bench.sh
source ../select_runner.sh

bench_bin="./redis-cli"
server_bin="./redis-server"
server_arg="--nvm-maxcapacity 2 --nvm-dir /mnt/pmem/redis --nvm-threshold 1"

function print_config() {
	bench_echo "Print config"
	echo "    side_channel_iter: ${side_channel_iter}"
	echo "    task_dir: ${TaskDir}"
	echo "    runner_script: ${runner_script}"
}

### Run
mkdir -p "${TaskDir}"
{
	# Print
	print_config

	# Prepare
	prepare_redis
	prepare

	# Run
	run

	# Post operations
	post_operations

	# Copy scripts
	cp "${runner_script}" "${TaskDir}/"

	# Cleanup
	cleanup
} > >(ts '[%Y-%m-%d %H:%M:%S]' | tee "${TaskDir}/stdout.log") 2>&1
