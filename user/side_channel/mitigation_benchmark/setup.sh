#!/bin/bash

benchmark_bin_path="/root/PMDK-Libs/libpmemobj-cpp/build/benchmarks"
declare -a benchmark_bin_array=(
	benchmark-concurrent_hash_map_insert_open
	benchmark-concurrent_hash_map_insert_open_secure
	benchmark-radix_tree
	benchmark-radix_tree_secure
	benchmark-self_relative_pointer_assignment
	benchmark-self_relative_pointer_assignment_secure
	benchmark-self_relative_pointer_get
	benchmark-self_relative_pointer_get_secure
)

function link_benchmark_bin() {
	echo "Linking benchmark binaries"
	for benchmark_bin in "${benchmark_bin_array[@]}"; do
		if [ ! -f "${benchmark_bin}" ]; then
			ln -s "${benchmark_bin_path}/${benchmark_bin}" .
		fi
	done
}

function _check_and_insert_gitignore() {
	if ! grep -q "${1}" .gitignore; then
		echo "${1}" >> .gitignore
	fi
}

function gitignore_benchmark_bin() {
	echo "Gitignore benchmark binaries"
	touch .gitignore
	for benchmark_bin in "${benchmark_bin_array[@]}"; do
		_check_and_insert_gitignore "${benchmark_bin}"
	done
	_check_and_insert_gitignore results
}

link_benchmark_bin
gitignore_benchmark_bin
