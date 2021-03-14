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
	bench_echo "Read kv store: START"
	for kv in "${bench_kv_array[@]}"; do
		sleep 2
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
