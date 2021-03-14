
function bench_run() {
	if [ ${side_channel_iter} -ne 0 ]; then
		bench_run_side_channel -i ${side_channel_iter} -o probe
	fi
}

function run() {
	{
		{
			bench_run
		} &
		pid_bench=$!

		{
			bench_redis
		} &
		pid_run=$!

		wait ${pid_bench}
		wait ${pid_run}
	}
}
