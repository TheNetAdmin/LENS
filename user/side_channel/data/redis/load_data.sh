#!/bin/bash

if [ $# -ne 1 ]; then
	echo "Usage: $0 db_name"
fi

redis_cli_bin=~/PMDK-Libs/pmem-redis/src/redis-cli
${redis_cli_bin} < ${1}
${redis_cli_bin} shutdown
