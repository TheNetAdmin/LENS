# To be sourced

side_channel_iter=2948
kv_array=(
	$(seq -s ' ' 64 64 256)
)

function prepare_kv() {
	bench_echo "Prepare kv store: START"
	# for kv in "${kv_array[@]}"; do
	# 	${bench_bin} "${map_file}" persistent get $kv
	# done
	rm -f "${map_file}"
	${bench_bin} "${map_file}" persistent insert 0 0
	bench_echo "Prepare kv store: DONE"
}

function bench_kv() {
	sleep 2
	bench_echo "Bench kv store: START"
	for kv in "${kv_array[@]}"; do
		echo -n "Drop system cache: "
		sync
		echo 3 >/proc/sys/vm/drop_caches
		echo "DONE"

		sleep 2
		echo -n "Insert $kv: "
		${bench_bin} "${map_file}" persistent insert $kv $kv
		echo "DONE"
	done
	bench_echo "Bench kv store: DONE"
}

function post_operations() {
	echo "Post operation: read all kv pairs"
	{
		for kv in "${kv_array[@]}"; do
			${bench_bin} "${map_file}" persistent get $kv | grep "ctree_map" 
		done
	} >"${TaskDir}/memory_layout.log"

	python3 ./tests/data_layout/parse.py \
		"${TaskDir}/memory_layout.log"   \
		"${TaskDir}/memory_layout.csv"   \
	;
}
