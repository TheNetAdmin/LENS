# To be sourced

side_channel_iter=3172
init_kv_array=(
	$(seq -s ' ' 1 2048)
)
bench_kv_array=(
	$(seq -s ' ' 256 256 1024)
)

function prepare_kv() {
	if [ -f "${map_file}" ]; then
		echo "File exists, skipping creating kv store: ${map_file}"
	else
		bench_echo "Prepare kv store: START"
		for kv in "${init_kv_array[@]}"; do
			${bench_bin} "${map_file}" persistent insert $kv $kv
		done
		bench_echo "Prepare kv store: DONE"
	fi
}

function bench_kv() {
	sleep 2
	bench_echo "Bench kv store: START"
	for kv in "${bench_kv_array[@]}"; do
		# echo -n "Drop system cache: "
		# sync
		# echo 3 >/proc/sys/vm/drop_caches
		# echo "DONE"

		sleep 2
		echo -n "Insert $kv: "
		${bench_bin} "${map_file}" persistent insert $kv $((kv+1))
		echo "DONE"
	done
	bench_echo "Bench kv store: DONE"
}

function post_operations() {
	echo "Post operation: read all kv pairs"
	{
		for kv in "${bench_kv_array[@]}"; do
			${bench_bin} "${map_file}" persistent get $kv | grep "ctree_map" 
		done
	} >"${TaskDir}/memory_layout.log"

	python3 ./tests/data_layout/parse.py \
		"${TaskDir}/memory_layout.log"   \
		"${TaskDir}/memory_layout.csv"   \
	;
}
