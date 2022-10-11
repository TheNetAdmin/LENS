# This file is sourced by `batch_run.sh` thus every sub-batch script should have it

export host_name=$(hostname | tr -d "[:space:]")
export script_root="$(realpath "$(realpath "$(dirname "$0")")"/../)"
export Slack=0
export SlackURL=""
export SlackUserID="" # Optional: your Slack user ID (not your user name) which starts with '@U'

case "${host_name}" in
	AEP2)
		export rep_dev="${rep_dev:-/dev/pmem0}"
		export lat_dev="${lat_dev:-/dev/pmem13}"
		# [    0.000000] user: [mem 0x0000003040000000-0x000001aa3fffffff] persistent (type 7)
		# 0x000001aa3fffffff - 0x0000003040000000 = 17A00000000 = 1512 GB
		export uc_base=0x4000000000
		export uc_size=0x4000000000
		export uc_size_mb=262144MB
		export uc_type=uncachable
		export disable_prefetcher=y
		export boost_cpu=y
		export dimm_size=$((250 * 2 ** 30 - 64 * 2 ** 30))
	;;
	sdp)
		export rep_dev="${rep_dev:-/dev/pmem0}"
		export lat_dev="${lat_dev:-/dev/pmem1}"
		# [    0.000000] user: [mem 0x0000003040000000-0x000001aa3fffffff] persistent (type 7)
		# 0x000001aa3fffffff - 0x0000003040000000 = 17A00000000 = 1512 GB
		export uc_base=0x4000000000
		export uc_size=0x4000000000
		export uc_size_mb=262144MB
		export uc_type=uncachable
		export disable_prefetcher=y
		export boost_cpu=y
		export dimm_size=$((250 * 2 ** 30 - 64 * 2 ** 30))
	;;
	nv-4)
		export rep_dev="${rep_dev:-/dev/pmem0}"
		export lat_dev="${lat_dev:-/dev/pmem1}"
		# [    0.000000] user: [mem 0x0000001840000000-0x000000d53fffffff] persistent (type 7)
		# [6109977.118667] reportfs_fill_super: dev pmem0, phys_addr 0x400000000, virt_addr 00000000a5b7ce1d, size 17179869184
		# [6109977.134381] latencyfs_fill_super: dev pmem2, phys_addr 0x18be200000, virt_addr 00000000ab30e843, size 133175443456
		# 0x18be200000 + 133175443456 = 0x37c0000000
		# 0x1900000000 + 128GiB       = 0x3900000000
		export uc_base=0x2000000000
		export uc_size=0x2000000000
		export uc_size_mb=131072MB
		export uc_type=uncachable
		export disable_prefetcher=y
		export boost_cpu=y
		export dimm_size=$((90 * 2 ** 30))
	;;
	lens)
		# lens vm
		export rep_dev="${rep_dev:-/dev/pmem0}"
		export lat_dev="${lat_dev:-/dev/pmem1}"
		# [    0.000000] user: [mem 0x0000001840000000-0x000000d53fffffff] persistent (type 7)
		# [6109977.118667] reportfs_fill_super: dev pmem0, phys_addr 0x400000000, virt_addr 00000000a5b7ce1d, size 17179869184
		# [6109977.134381] latencyfs_fill_super: dev pmem2, phys_addr 0x18be200000, virt_addr 00000000ab30e843, size 133175443456
		# 0x18be200000 + 133175443456 = 0x37c0000000
		# 0x1900000000 + 128GiB       = 0x3900000000
		# export uc_base=0x2000000000
		# export uc_size=0x2000000000
		# export uc_size_mb=131072MB
		# export uc_type=uncachable
		export disable_prefetcher=n
		export boost_cpu=n
		export dimm_size=$((2 ** 32))
	;;
	*)
		echo "Unknown host machine: $(hostname)"
		exit 2
	;;
esac
