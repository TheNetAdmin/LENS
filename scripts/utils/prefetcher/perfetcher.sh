# To be sourced
# https://github.com/xmrig/xmrig/blob/7b51e23aa0d7c3f500aec889dcaef312d870ef53/scripts/randomx_boost.sh

check_msr_module() {
	if ! lsmod | grep 'msr'; then
		echo "ERROR: module 'msr' not inserted"
		exit 1
	fi
	if ! which rdmsr; then
		echo "ERROR: command 'rdmsr' not found"
		exit 1
	fi
}

set_prefetcher_intel() {
	mode="$1"
	if [ "$mode" == "off" ]; then
		wrmsr -a 0x1a4 0xf
	else
		wrmsr -a 0x1a4 0x0
	fi
}

set_prefetcher_amd() {
	mode="$1"
	if grep "cpu family[[:space:]]\{1,\}:[[:space:]]25" /proc/cpuinfo >/dev/null; then
		if grep "model[[:space:]]\{1,\}:[[:space:]]97" /proc/cpuinfo >/dev/null; then
			echo "Detected Zen4 CPU"
			if [ "$mode" == "off" ]; then
				wrmsr -a 0xc0011020 0x4400000000000
				wrmsr -a 0xc0011021 0x4000000000040
				wrmsr -a 0xc0011022 0x8680000401570000
				wrmsr -a 0xc001102b 0x2040cc10
				echo "MSR register values for Zen4 applied: OFF"
			else
				echo "ERROR: The script does not handle Zen4 msr register recovery"
			fi
		else
			echo "Detected Zen3 CPU"
			if [ "$mode" == "off" ]; then
				wrmsr -a 0xc0011020 0x4480000000000
				wrmsr -a 0xc0011021 0x1c000200000040
				wrmsr -a 0xc0011022 0xc000000401570000
				wrmsr -a 0xc001102b 0x2000cc10
				echo "MSR register values for Zen3 applied: OFF"
			else
				wrmsr -a 0xc0011020 0x4480000000000
				wrmsr -a 0xc0011021 0x2000000c0
				wrmsr -a 0xc0011022 0xc000000401500000
				wrmsr -a 0xc001102b 0x2000cc15
				echo "MSR register values for Zen3 applied: ON"
			fi
		fi
	else
		echo "Detected Zen1/Zen2 CPU"
		if [ "$mode" == "off" ]; then
			wrmsr -a 0xc0011020 0
			wrmsr -a 0xc0011021 0x40
			wrmsr -a 0xc0011022 0x1510000
			wrmsr -a 0xc001102b 0x2000cc16
			echo "MSR register values for Zen1/Zen2 applied: OFF"
		else
			echo "ERROR: The script does not handle Zen1/2 msr register recovery"
		fi
	fi
}

set_prefetcher() {
	if grep -E 'AMD Ryzen|AMD EPYC' /proc/cpuinfo >/dev/null; then
		set_prefetcher_amd $*
	else
		set_prefetcher_intel $*
	fi
}

export -f check_msr_module
export -f set_prefetcher
