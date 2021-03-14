# To be sourced

side_channel_iter=20

function bench_run() {
	if [ ${side_channel_iter} -ne 0 ]; then
		bench_run_side_channel -i ${side_channel_iter} -o probe_lib -c 9 -l /mnt/pmem/libnvleak_example.so -t 1 -e 0
	fi
}

function prepare_victim() {
	echo "Prepare victim"
	export PARSE_SIDE_CHANNEL=n
}

function bench_victim() {
	sleep 3
	bench_echo "Bench victim: START"
	for i in {0..4}; do
		echo "    iter [$i]"
		LD_LIBRARY_PATH=/mnt/pmem/:$LD_LIBRARY_PATH \
			./victim
		sleep 3
	done
	bench_echo "Bench victim: DONE"
}

function post_operations() {
	echo "Post operations: SKIP"
}
