#!/bin/bash

set -e

batch_id="$(date +%Y%m%d-%H-%M-%S)"

usage() {
	echo "USAGE: ${0} send_1|send_0|test|all|debug"
}

if [ $# -ne 1 ]; then
	usage
	exit 1
fi

op="${1}"
case ${op} in
send_1) ;;
send_0) ;;
test) ;;
all) ;;
debug) ;;
*)
	usage
	exit 1
	;;
esac

source ../../scripts/utils/profiler/aepwatch/profiler.sh

make

prepare() {
	echo "Set host CPU into performance mode"
	for line in $(find /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor); do
		echo "performance" >"$line"
	done
	# Goto TaskDir
	mkdir -p $TaskDir
	# Disable cache prefetcher
	echo "Disable cache prefetcher"
	wrmsr -a 0x1a4 0xf
	# Load profiler
	load_profiler
	start_profiler
}

cleanup() {
	# Unload profiler
	stop_profiler || true
	unload_profiler
	# Enable cache prefetcher
	echo "Re-enable cache prefetcher"
	wrmsr -a 0x1a4 0x0
}

trap cleanup EXIT

run_covert() {
	# $1 send | recv
	# $@ other args
	core_id=1
	case $1 in
		send)
			core_id="4-7"
		;;
		recv)
			core_id="8-11"
		;;
		*)
		echo "Wrong usage"
		echo "Usage: run_covert send|recv [... args]"
		;;
	esac

	nice -n -19 \
	numactl --physcpubind "${core_id}" \
		./inode_covert "$@"
}

# args
declare -A covert_args
covert_args[recv_single]="-f /mnt/pmem/file2 -i 20000 -o inode -d 100000"
covert_args[send_single]="-f /mnt/pmem/file1 -i 20000 -o inode -d 10000"
covert_args[test]="-f /mnt/pmem/file3 -i 20000 -o inode -d 10000"

bench_func() {
	prepare
	covert_fid=2
	echo "Covert data file id: ${covert_fid}"

	covert_data_path="$(realpath "$(dirname "${0}")"/covert_data)"
	covert_data_file_pattern="${covert_data_path}/${covert_fid}.*.*.bin"
	if ls ${covert_data_file_pattern} 1>/dev/null 2>&1; then
		covert_data_file="$(readlink -f ${covert_data_file_pattern})"
	else
		echo "Cannot find covert data file with ID: ${covert_fid}"
		exit 1
	fi

	{
		iter=1000
		cycle_ddl=$((100000 * iter))
		dev=pmem

		run_covert send \
			-r sender \
			-f /mnt/$dev/file1 \
			-e /mnt/$dev/file120 \
			-i $iter \
			-d 10000 \
			-o inode \
			-c "$covert_data_file" \
			-l $cycle_ddl \
			> >(tee -a "${TaskDir}/sender.log" >/dev/null) \
			2>&1 \
		&


		run_covert recv \
			-r receiver \
			-f /mnt/$dev/file2 \
			-i $iter \
			-d 100000 \
			-o inode \
			-c "$covert_data_file" \
			-l $cycle_ddl \
			> >(tee -a "${TaskDir}/receiver.log" >/dev/null) \
			2>&1 \
		&

		wait
	}
	cleanup
}

echo "RUNNING op=${op}"
case ${op} in
	test)
	prepare
	./inode_covert ${covert_args[test]}
	cleanup
	;;
	send_0)
	prepare
	{
		run_covert send ${covert_args[send_single]} >"${TaskDir}/send.log" &
		run_covert recv ${covert_args[recv_single]} >"${TaskDir}/recv.log" &
		wait
	}
	cleanup
	;;
	send_1)
	prepare
	{
		run_covert recv ${covert_args[recv_single]} >"${TaskDir}/recv.log" &
		wait
	}
	cleanup
	;;
	debug)
	export TaskDir=results/${op}/${batch_id}
	bench_func
	;;
esac

