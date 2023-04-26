# To be sourced

radix_data_file="${pmem_mnt}/radix_data"
radix_args="2000000 200 2000"

function prepare_bench() {
	rm -f "${radix_data_file}"
	pmempool create obj --layout=radix -s 4G "${radix_data_file}"
}

function bench_run() {
	echo "Run benchmark"
	${bench_bin} ${radix_data_file} ${radix_args}
}

function post_operations() {
	echo
}
