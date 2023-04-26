# To be sourced

side_channel_iter=$((1200 * 300))
# side_channel_iter=30

function bench_run() {
	if [ ${side_channel_iter} -ne 0 ]; then
		bench_run_side_channel -i ${side_channel_iter} -o probe_lib -c 0 -l /mnt/pmem/libnvleak_example.so -e 0 -t 4
	fi
}

function prepare_victim() {
	echo "Prepare victim"
	export PARSE_SIDE_CHANNEL=n
	export LD_LIBRARY_PATH=/mnt/pmem/:$LD_LIBRARY_PATH
}

function bench_victim() {
	sleep 3
	bench_echo "Bench victim: START"
	# core 4-7 taken by side-channel attacker
	nice -n -19 \
		numactl --physcpubind "8" \
	./victim
	bench_echo "Bench victim: DONE"
}

function post_operations() {
	echo "Post operations: Parse results"
	pushd "${TaskDir}" || exit 2
	python3 ../../../nvleak/parse_shared_lib.py || true
	popd || exit 2
}
