# To be sourced

side_channel_iter=4096
kv_array=(
	$(seq -s ' ' 1 2048)
)

function prepare_kv() {
	bench_echo "Prepare kv store: START"
	for kv in "${kv_array[@]}"; do
		${bench_bin} "${map_file}" persistent insert $kv $kv
	done
	bench_echo "Prepare kv store: DONE"
}

function bench_kv() {
	bench_echo "Read kv store: START"
	for kv in "${kv_array[@]}"; do
		sleep 0.5
		${bench_bin} "${map_file}" persistent get $kv | grep "ctree_map" 
	done
	bench_echo "Read kv store: DONE"
}

function post_operations() {
	echo "Post operation: Parse memory layout"
	python3 ./tests/data_layout/parse.py \
		"${TaskDir}/stdout.log"   \
		"${TaskDir}/memory_layout.csv"   \
	;
}
