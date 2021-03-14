# To be sourced

side_channel_iter=2048

function bench_run() {
	if [ ${side_channel_iter} -ne 0 ]; then
		bench_run_side_channel -i ${side_channel_iter} -o probe
	fi
}

function prepare_victim() {
	echo "Prepare victim: SKIP"
}

function bench_victim() {
	sleep 2
	bench_echo "Bench victim: START"
	for i in {0..3}; do
		echo "    iter [$i]"
		LD_LIBRARY_PATH=/mnt/pmem/:$LD_LIBRARY_PATH \
			./victim
		sleep 2
	done
	bench_echo "Bench victim: DONE"
}

function post_operations() {
	echo "Post operations: SKIP"
}
