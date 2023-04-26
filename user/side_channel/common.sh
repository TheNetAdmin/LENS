# To be sourced by scripts

source ../../../scripts/utils/check_mtrr.sh
check_mtrr cacheable

export host_name=$(hostname | tr -d "[:space:]")
PROJ_ROOT=${PROJ_ROOT:-/home/usenix/}

case "${host_name}" in
	nv-4)
		export dram_dev="${dram_dev:-/dev/pmem0}"
		export pmem_dev="${pmem_dev:-/dev/pmem1}"
		export nvleak_path="$PROJ_ROOT/NVLeak/nvleak"
		export pmdk_libpmemobj_cpp_path="${nvleak_path}/user/side_channel/libpmemobj-cpp"
		dax_attacker=$(ndctl list | jq '.[] | select(.name=="dax-attacker") | .chardev' | tr -d '"')
		[ -z "${dax_attacker}" ] && dax_attacker="dax_dev_not_found"
		export dax_dev="${dax_dev:-/dev/${dax_attacker}}"
	;;
	*)
		echo "Unknown host machine: $(hostname)"
		exit 2
	;;
esac

export dram_mnt="${dram_mnt:-/mnt/dram}"
export pmem_mnt="${pmem_mnt:-/mnt/pmem}"
