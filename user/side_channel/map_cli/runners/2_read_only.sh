# To be sourced

side_channel_iter=4096
kv_array=(
	$(seq -s ' ' 16 16 256)
)

function prepare_kv() {
	bench_echo "Prepare kv store: START"
	for kv in "${kv_array[@]}"; do
		${bench_bin} "${map_file}" persistent get $kv
	done
	bench_echo "Prepare kv store: DONE"
}

function bench_kv() {
	bench_echo "Read kv store: START"
	for kv in "${kv_array[@]}"; do
		sleep 0.5
		${bench_bin} "${map_file}" persistent get $kv | grep "ctree_map" 
		sleep 0.5
		${bench_bin} "${map_file}" persistent get $kv | grep "ctree_map" 
	done
	bench_echo "Read kv store: DONE"
}

function post_operations() {
	echo ""
}
