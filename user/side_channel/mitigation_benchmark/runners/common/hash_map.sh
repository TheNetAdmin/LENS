# To be sourced

hash_map_data_file="${pmem_mnt}/hash_map"
hash_map_arg_mode="create"
hash_map_arg_inserts="100000"
hash_map_arg_threads="4"
hash_map_args="${hash_map_arg_mode} ${hash_map_arg_inserts} ${hash_map_arg_threads}"

function prepare_bench() {
	rm -f "${hash_map_data_file}"
}

function bench_run() {
	echo "Run benchmark"
	echo -n "mode=${hash_map_arg_mode}, n_inserts=${hash_map_arg_inserts}, n_threads=${hash_map_arg_threads}, time="
	${bench_bin} ${hash_map_data_file} ${hash_map_args}
}

function post_operations() {
	echo
}
