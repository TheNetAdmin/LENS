#!/bin/bash

set -e
set -x

PROJ_ROOT=${PROJ_ROOT:-/home/usenix/}

e2fs="$PROJ_ROOT/e2fsprogs-1.46.4/build/misc/mke2fs" # Used on nv-4

fs=ext4

function prepare_pmem() {
	nvram_bus=$(ndctl list --buses | jq '.[] | select(.provider=="ACPI.NFIT" )| .dev' | tr -d '"')
	pmem_device=$(ndctl list --bus=$nvram_bus --namespaces | grep blockdev | cut -d'"' -f4)
	pmem_device=/dev/$pmem_device

	mnt_point=/mnt/pmem
	mkdir -p "${mnt_point}"
	umount "${mnt_point}" || true
	if [ $fs == 'ext4' ]; then
		$e2fs -O fast_commit -t ext4 "$pmem_device"
		tune2fs -O ^has_journal "$pmem_device"
	elif [ $fs == 'xfs' ]; then
		mkfs.xfs -f "$pmem_device"
	fi
	mount -o dax "$pmem_device" $mnt_point

	pushd $mnt_point || exit 2
	for f in {1..128}; do
		touch "file$f"
	done
	popd || exit 2
}

## DRAM
function prepare_dram() {
	dram_device=/dev/pmem0

	mnt_point=/mnt/dram
	mkdir -p "${mnt_point}"
	umount "${mnt_point}" || true

	if [ $fs == 'ext4' ]; then
		$e2fs -O fast_commit -t ext4 "$dram_device"
		tune2fs -O ^has_journal "$dram_device"
	elif [ $fs == 'xfs' ]; then
		mkfs.xfs -f "$dram_device"
	fi
	mount -o dax "$dram_device" $mnt_point

	pushd $mnt_point || exit 2
	for f in {1..128}; do
		touch "file$f"
	done
	popd || exit 2
}

prepare_dram
prepare_pmem
