# To be sourced

side_channel_iter=512
key_array=(
	1679576722
)

source runners/common/common.sh
source runners/common/probe_all.sh

function bench_redis() {
	bench_echo "Read redis: START"
	for key in "${key_array[@]}"; do
		
		sleep 1

		time ${bench_bin} <<- EOI
		set $key $key
		EOI

	done
	bench_echo "Read redis: DONE"
}
