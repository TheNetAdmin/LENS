# To be sourced by scripts

source ../../../scripts/utils/check_mtrr.sh
check_mtrr cacheable

export host_name=$(hostname | tr -d "[:space:]")

case "${host_name}" in
	AEP2)
		export dram_dev="${dram_dev:-/dev/pmem0}"
		export pmem_dev="${pmem_dev:-/dev/pmem13}"
	;;
	sdp)
		export dram_dev="${dram_dev:-/dev/pmem0}"
		export pmem_dev="${pmem_dev:-/dev/pmem1}"
		export pmdk_path="/root/PMDK-Libs"
		export pmdk_libpmemobj_cpp_path="${pmdk_path}/libpmemobj-cpp"
		export dax_dev="${dax_dev:-/dev/dax1.2}"
	;;
	nv-4)
		export dram_dev="${dram_dev:-/dev/pmem0}"
		export pmem_dev="${pmem_dev:-/dev/pmem2}"
	;;
	lens)
		# lens vm
		export dram_dev="${dram_dev:-/dev/pmem0}"
		export pmem_dev="${pmem_dev:-/dev/pmem1}"
	;;
	*)
		echo "Unknown host machine: $(hostname)"
		exit 2
	;;
esac

export dram_mnt="${dram_mnt:-/mnt/dram}"
export pmem_mnt="${pmem_mnt:-/mnt/pmem}"
