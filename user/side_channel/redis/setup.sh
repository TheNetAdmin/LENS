#!/bin/bash

redis_bin_path="$HOME/PMDK-Libs/pmem-redis/src/"
declare -a redis_bin_array=(
	redis-server
	redis-cli
)

function link_redis_bin() {
	echo "Linking redis binaries"
	for redis_bin in "${redis_bin_array[@]}"; do
		if [ ! -f "${redis_bin}" ]; then
			ln -s "${redis_bin_path}/${redis_bin}" .
		fi
	done
}

function _check_and_insert_gitignore() {
	if ! grep -q "${1}" .gitignore; then
		echo "${1}" >> .gitignore
	fi
}

function gitignore_redis_bin() {
	echo "Gitignore redis binaries"
	touch .gitignore
	for redis_bin in "${redis_bin_array[@]}"; do
		_check_and_insert_gitignore "${redis_bin}"
	done
	_check_and_insert_gitignore results
}

link_redis_bin
gitignore_redis_bin
