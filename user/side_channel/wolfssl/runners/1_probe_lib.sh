# To be sourced

side_channel_iter=4000

function bench_run() {
	if [ ${side_channel_iter} -ne 0 ]; then
		# filefrag -v shows starting loc 33824, mod 256 gets 32
			# -c 32 \
		bench_run_side_channel \
			-i ${side_channel_iter} \
			-o probe_lib \
			-l /mnt/pmem/lib/libwolfssl.so.23.0.0 \
			-t 43 \
			-e 0 \
		;
	fi
}

function prepare_victim() {
	echo "Prepare victim"
	export PARSE_SIDE_CHANNEL=n
	export LD_LIBRARY_PATH=/mnt/pmem/lib/:$LD_LIBRARY_PATH
}

function bench_victim() {
	bench_echo "This test is not interesting/useful"
	exit 2
	bench_echo "Bench dir: $PWD"
	bench_echo "Bench victim: START"
	./wolf/test_dsa/dsa_sign 4
	bench_echo "Bench victim: DONE"
}

function post_operations() {
	echo "Post operations: SKIP"
}
