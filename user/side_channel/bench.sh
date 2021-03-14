# This script is to be sourced

# Settings
: "${DISABLE_CPU_PREFETCHERS:=n}"
: "${PARSE_SIDE_CHANNEL:=y}"

# IDs
task_id="$(date +%Y%m%d-%H-%M-%S)"
TaskDir="results/${task_id}"

function bench_echo() {
	echo "###" "$@"
}

function check_and_generate_git_diff() {
	bench_echo "Commit status: $(git diff-index --quiet HEAD -- || echo "dirty")"
	
    if ! git diff-index --quiet HEAD --; then
        bench_echo Code base not clean, generating diff
        git diff > "${TaskDir}/diff.txt"
    fi
}

function disable_prefetcher() {
	case $DISABLE_CPU_PREFETCHERS in
        [Yy]* )
			bench_echo "Disable cache prefetcher"
			wrmsr -a 0x1a4 0xf
		;;
        [Nn]* )
			bench_echo "[SKIP] Disable cache prefetcher"
		;;
        * )
			bench_echo "DISABLE_CPU_PREFETCHERS not recognized: [$DISABLE_CPU_PREFETCHERS]"
		;;
    esac
}

function enable_prefetcher() {
	case $DISABLE_CPU_PREFETCHERS in
        [Yy]* )
			bench_echo "Re-enable cache prefetcher"
			if ! modprobe msr; then
				bench_echo "Failed to load msr module"
				exit 1
			fi
			wrmsr -a 0x1a4 0x0
		;;
        [Nn]* )
			bench_echo "[SKIP] Re-enable cache prefetcher"
		;;
        * )
			bench_echo "DISABLE_CPU_PREFETCHERS not recognized: [$DISABLE_CPU_PREFETCHERS]"
			exit 1
		;;
    esac
}

function set_cpu_performance_mode() {
	bench_echo "Set host CPU into performance mode"
	for line in $(find /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor); do
		echo "performance" >"$line"
	done
}

function drop_system_cache() {
	# Drop caches
	bench_echo "Drop system caches"
	sync
	echo 3 >/proc/sys/vm/drop_caches
}

function prepare() {
	bench_echo "Prepare system"
	check_and_generate_git_diff
	set_cpu_performance_mode
	disable_prefetcher
	drop_system_cache
}

CLEANUP_FINISHED=n

function cleanup() {
	if [ $CLEANUP_FINISHED == n ]; then
		bench_echo "Cleanup system"
		enable_prefetcher

		CLEANUP_FINISHED=y
	fi
}

trap cleanup EXIT

function _run_side_channel() {
	nice -n -19 \
		numactl --physcpubind "4" \
		../nvleak/side_channel "$@"
}

function _parse_side_channel() {
	# $1: dir with results
	if [ $# -ne 1 ]; then
		bench_echo "Error: _parse_side_channel() expects one argument but got $#"
		exit 2
	fi

	if [ $PARSE_SIDE_CHANNEL == n ]; then
		echo "Parse SKIP"	
	else
		pushd "$1" || exit 2
		python3 ../../../nvleak/parse.py
		popd || exit 2
	fi

}

function parse_side_channel() {
	_parse_side_channel "${TaskDir}"
}

function _bench_run_side_channel_kernel() {
	_run_side_channel -f "${dax_dev:-/dev/dax1.1}" "$@" >>"${TaskDir}/side_channel.log" || true
}

function _bench_run_side_channel_no_file_output_kernel() {
	_run_side_channel -f "${dax_dev:-/dev/dax1.1}" "$@" || true
}

function bench_run_side_channel() {
	bench_echo "Making task dir: ${TaskDir}"
	mkdir -p "${TaskDir}"

	bench_echo "Probe start"
	_bench_run_side_channel_kernel "$@"
	bench_echo "Probe done"

	bench_echo "Parse start"
	parse_side_channel
	bench_echo "Parse done"
}

function bench_run_side_channel_no_file_output() {
	bench_echo "Making task dir: ${TaskDir}"
	mkdir -p "${TaskDir}"

	bench_echo "Probe start"
	_bench_run_side_channel_no_file_output_kernel "$@"
	bench_echo "Probe done"

	bench_echo "Parse start"
	parse_side_channel
	bench_echo "Parse done"
}
