# This file is sourced by `batch_run.sh` thus every sub-batch script should have it

export host_name=$(hostname | tr -d "[:space:]")
export script_root="$(realpath "$(realpath "$(dirname "$0")")"/../)"
export Slack=0
export SlackURL=""
export SlackUserID="" # Optional: your Slack user ID (not your user name) which starts with '@U'

case "${host_name}" in
	nv-4)
		# rep_dev: The DRAM-emulated PMEM device, created by Linux kernel's boot
		#          command `memmap`
		export rep_dev="${rep_dev:-/dev/pmem0}"
		# lat_dev: The NVRAM-based PMEM device, created by `ipmctl` and `ndctl`
		export lat_dev="${lat_dev:-/dev/pmem1}"

		# Example `dmesg` output on this machine when executing `nvleak/scripts/utils/mount.sh`
		#### [    0.000000] user: [mem 0x0000001840000000-0x000000d53fffffff] persistent (type 7)
		#### [6109977.118667] reportfs_fill_super: dev pmem0, phys_addr 0x400000000, virt_addr 00000000a5b7ce1d, size 17179869184
		#### [6109977.134381] latencyfs_fill_super: dev pmem1, phys_addr 0x18be200000, virt_addr 00000000ab30e843, size 133175443456

		# The above output is useful on setting the fields described below, to
		# get this kernel output:
		#   1. Set the `rep_dev` and `lat_dev`
		#   2. Mount the kernel module by
		#      `nvleak/scripts/utils/mount.sh ${rep_dev} ${lat_dev}`
		#   3. Unmount the kernel module by
		#      `nvleak/scripts/utils/umount.sh`
		#      so that next time the nvleak scripts can mount modules without
		#      complaining that they are already mounted
		#   4. Check the `dmesg` output

		# uc_base: Uncacheable region base address, set it to be a power of 2,
		#          and after the start of your first NVRAM's physical address
		# example: From the above `dmesg` output, `pmem2` phys_addr is
		#          0x18be200000, thus us_base should be 0x2000000000, the first
		#          power of 2 number after pmem2 phys_addr
		export uc_base=0x2000000000
		# uc_size: Uncacheable region size, set it to covert the entire first
		#          NVRAM region, must be a power of 2 and it's ok to go over
		#          the NVRAM region
		# example: A single NVRAM DIMM on this machine is 128 GiB, then set
		#          uc_size to 128 GiB, i.e., 0x2000000000
		export uc_size=0x2000000000
		# uc_size_mb: The output from `cat /proc/mtrr` size field, for the
		#             uncacheable region set above. You can get this number
		#             after you set up the above numbers, run `batch_run.sh 001`
		#             then check its result and find the value.
		# usage:      This value is used by 001_setup_mtrr.sh to detect if the
		#             NVRAM uncacheable region is already set up, so it can skip
		#             and not to re-set it up.
		export uc_size_mb=131072MB
		# uc_type: MTRR region type, set it to uncacheable or other valid
		#          options, check MTRR-related kernel doc for more details
		export uc_type=uncachable
		# disable_prefetcher: Tell scripts to disable CPU hardware prefetchers
		#                     before running each task
		export disable_prefetcher=y
		# boost_cpu: If boost CPU scaling governor to the 'performance' mode
		export boost_cpu=y
		# dimm_size: The NVRAM DIMM size. This value is used by scripts under
		#            the current folder to determine if an experiment
		#            configuration will run over a single NVRAM. It's safer to
		#            set it to be smaller than the actual DIMM size, because
		#            experiments start from the `uc_base` which is a bit over
		#            the start address of the NVRAM, thus the effective NVRAM
		#            DIMM size is smaller than the real size. You may make more
		#            precise calculation by calculating how much `uc_base`
		#            passes the NVRAM's `phys_addr`
		export dimm_size=$((90 * 2 ** 30))
	;;
	lens)
		# lens vm -- Inside a virtual machine for lens/nvleak debugging
		# So there's no uncacheable settings, and do NOT run 001_setup_mtrr.sh
		# either manually or using `batch_run.sh 001`, just directly run the 
		# other benchmark scripts
		export rep_dev="${rep_dev:-/dev/pmem0}"
		export lat_dev="${lat_dev:-/dev/pmem1}"
		# export uc_base=
		# export uc_size=
		# export uc_size_mb=
		# export uc_type=
		export disable_prefetcher=n
		export boost_cpu=n
		export dimm_size=$((2 ** 32))
	;;
	*)
		echo "Unknown host machine: $(hostname)"
		exit 2
	;;
esac
