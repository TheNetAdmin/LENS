#!/bin/bash
# Compile code, mount ReportFS and LatencyFS

set -e

if [ $# -ne 2 ] && [ $# -ne 3 ]; then
    echo './mount.sh [DRAM_DEV] [PMEM_DEV] [no_remake]'
    echo 'e.g., ./mount.sh /dev/pmem0 /dev/pmem14'
    exit 1
fi

this_script_path=$(realpath $(dirname $0))
echo "Curr script path: ${this_script_path}"
src_path="$(realpath "$this_script_path/../../src")"
echo "Src path: ${src_path}"
pushd "${src_path}"

DRAM_DEV=$1
PMEM_DEV=$2
NO_REMAKE=$3

# Hard lock watchdog at nmi_watchdog
sudo bash -c "echo 0 > /proc/sys/kernel/soft_watchdog"

if test "${NO_REMAKE}" == ""; then
	echo "Compiling"
	make -j 40
else
	echo "Compiling"
	sudo make clean >/dev/null 2>&1
	make -j 40 >/dev/null 2>&1
fi

echo "Make mount points"
sudo mkdir -p /mnt/latency
sudo mkdir -p /mnt/report

REPFS=$(sudo lsmod | grep repfs) || true
LATFS=$(sudo lsmod | grep latfs) || true

echo "Check and unmount previous modules"
if [ ! -z "$REPFS" ]; then
	echo Unmounting existing partitions
	$this_script_path/umount.sh
elif [ ! -z "$LATFS" ]; then
	echo Unmounting existing partitions
	$this_script_path/umount.sh
fi

echo "Insert new modules"
sudo insmod repfs.ko
sudo insmod latfs.ko

echo "Mount ReportFS and LatencyFS"
sudo mount -t ReportFS $DRAM_DEV /mnt/report
sudo mount -t LatencyFS $PMEM_DEV /mnt/latency

echo "$0 Finished"

popd
