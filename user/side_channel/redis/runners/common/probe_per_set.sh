
function bench_run() {
	side_channel_iter=$((2 ** 16))
	_bench_run_side_channel_kernel -i ${side_channel_iter} -o probe -p "${1}"
}

function run() {
	for set_idx in {0..255}; do
	{
		bench_echo "Probing at set [${set_idx}]"
		{
			bench_echo "Probe start"
			bench_run "${set_idx}"
			bench_echo "Probe end"
		} &
		pid_run=$!

		{
			sleep 0.25
			bench_echo "Query start"
			bench_redis
			bench_echo "Query end"
		} &
		pid_bench=$!

		wait ${pid_bench}
		wait ${pid_run}
	}
	done
}
