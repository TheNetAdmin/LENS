#!/bin/bash
# Compile code, mount ReportFS and LatencyFS

set -e

if [ $# -ne 2 ] && [ $# -ne 3 ]; then
    echo './mount.sh [DRAM_DEV] [NUMA_NODE] [no_remake]'
    echo 'e.g., ./mount.sh /dev/pmem0 2'
    exit 1
fi

this_script_path=$(realpath $(dirname $0))
echo "Curr script path: ${this_script_path}"
src_path="$(realpath "$this_script_path/../../src")"
echo "Src path: ${src_path}"
pushd "${src_path}"

DRAM_DEV=$1
NUMA_NODE=$2
NO_REMAKE=$3


if test "${NO_REMAKE}" == ""; then
	echo "Compiling"
	make -j 40
else
	echo "Compiling"
	sudo make clean >/dev/null 2>&1
	make -j 40 >/dev/null 2>&1
fi

echo "Make mount points"
sudo mkdir -p /mnt/report

LENS_CXL_FS=$(sudo lsmod | grep lens_cxl_fs) || true

echo "Check and unmount previous modules"
if [ ! -z "$LENS_CXL_FS" ]; then
	echo Unmounting existing partitions
	sudo umount /mnt/report
	sudo rmmod lens_cxl_fs
	sudo bash -c "echo 1 > /proc/sys/kernel/soft_watchdog"
fi

# Hard lock watchdog at nmi_watchdog
sudo bash -c "echo 0 > /proc/sys/kernel/soft_watchdog"

echo "Insert new modules"
sudo insmod lens_cxl_fs.ko

echo "Mount lens_cxl_fs"
sudo mount -t lens_cxl_fs $DRAM_DEV /mnt/report

echo "$0 Finished"

popd
