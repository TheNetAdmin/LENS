function prepare_redis() {
	bench_echo "Prepare redis: COPY"
	cp ../data/redis/redis-port-6379-2GB-AEP /mnt/pmem/redis
	bench_echo "Prepare redis: START"
	${server_bin} ${server_arg} > ${TaskDir}/redis-server.log &
	sleep 8
	bench_echo "Prepare redis: DONE"
}

function bench_redis() {
	bench_echo "Read redis: START"
	for key in "${key_array[@]}"; do
		
		sleep 1

		time ${bench_bin} <<- EOI
		get $key
		EOI

	done
	bench_echo "Read redis: DONE"
}

function post_operations() {
	echo "Stop the redis server"
	${bench_bin} shutdown
}
