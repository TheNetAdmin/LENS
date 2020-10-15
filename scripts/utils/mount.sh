#!/bin/sh
# Compile code, mount ReportFS and LatencyFS

set -e

if [ $# -ne 2 ]; then
    echo './mount.sh [DRAM_DEV] [PMEM_DEV]'
    echo 'e.g., ./mount.sh /dev/pmem0 /dev/pmem14'
    exit 1
fi

this_script_path=$(realpath $(dirname $0))
pushd $this_script_path/../../src

DRAM_DEV=$1
PMEM_DEV=$2

# Hard lock watchdog at nmi_watchdog
echo 0 > /proc/sys/kernel/soft_watchdog

echo "Compiling"
make clean 2>&1 > /dev/null
make -j 2>&1

echo "Make mount points"
mkdir -p /mnt/latency
mkdir -p /mnt/report

REPFS=`lsmod | grep repfs` || true
LATFS=`lsmod | grep latfs` || true

echo "Check and unmount previous modules"
if [ ! -z "$REPFS" ]; then
	echo Unmounting existing partitions
	$this_script_path/umount.sh
elif [ ! -z "$LATFS" ]; then
	echo Unmounting existing partitions
	$this_script_path/umount.sh
fi

echo "Insert new modules"
insmod repfs.ko
insmod latfs.ko

echo "Mount ReportFS and LatencyFS"
mount -t ReportFS $DRAM_DEV /mnt/report
mount -t LatencyFS $PMEM_DEV /mnt/latency

echo "$0 Finished"

popd
