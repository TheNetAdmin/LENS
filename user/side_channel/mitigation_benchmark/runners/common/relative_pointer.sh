# To be sourced

ptr_data_file="${pmem_mnt}/rel_ptr"

function prepare_bench() {
	rm -f "${ptr_data_file}"
}

function bench_run() {
	echo "Run benchmark"
	${bench_bin} ${ptr_data_file}
}

function post_operations() {
	echo
}
