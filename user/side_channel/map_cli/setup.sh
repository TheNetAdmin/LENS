#!/bin/bash

set -e

curr_path="$(realpath "$(realpath "$(dirname "$0")")")"
cd "${curr_path}" || exit 2

source ../common.sh
source ./default.sh

function link_bench_bin() {
	bench_bin_origin_path="${pmdk_libpmemobj_cpp_path}/build/examples/${bench_bin}"

	echo -n "Link binary [${bench_bin_origin_path}] to curr dir: "
	rm -f ${bench_bin}
	ln -s "${bench_bin_origin_path}" .
	echo "DONE"
}

function ndctl_status() {
    ndctl list --namespaces
    # ndctl list --regions
}

function _create_fsdax_devdax() {
	echo "### Creating DAX in fs and dev modes"

	nvram_bus=$(ndctl list --buses | jq '.[] | select(.provider=="ACPI.NFIT" )| .dev' | tr -d '"')
	echo "nvram_bus=${nvram_bus}"

	local_region=$(ndctl list --bus "${nvram_bus}" --regions | grep region | cut -d '"' -f 4 | sed 's/region//' | sort -n | head -n 1)
	echo "local_region=${local_region}"

	ndctl create-namespace --mode fsdax  --size 16G --align 1G --region "${local_region}" --name "dax-victim"
	ndctl create-namespace --mode devdax --size 16G --align 1G --region "${local_region}" --name "dax-attacker"
}

function setup_fsdax_devdax() {
	# Code reused from "scripts/machine/optane.sh"
    echo "### Ndctl status before:"
	ndctl_status
	echo ""

	echo "### Do you wish to proceed creating DAX?"
	select yn in "Yes" "No"; do
		case $yn in
			Yes)
				_create_fsdax_devdax
				echo "Creating DAX: DONE"
				break
			;;
			No)
				echo "Creating DAX: SKIP"
				break
			;;
		esac
	done

	
    echo "### Ndctl status after:"
	ndctl_status
	echo ""
}

function mount_pmem() {
	echo "Format and mount pmem"
	echo "Do you wish to format the pmem [${pmem_dev}]?"
	select yn in "Yes" "No"; do
		case $yn in
			Yes)
				mkfs.ext4 ${pmem_dev}
				echo "Formating pmem: DONE"
				break
			;;
			No)
				echo "Formating pmem: SKIP"
				break
			;;
		esac
	done

	echo -n "Mount [${pmem_dev}] to [${pmem_mnt}]:"
	umount "${pmem_mnt}" >/dev/null 2>&1 || true
	mkdir -p "${pmem_mnt}"
	mount -o dax "${pmem_dev}" "${pmem_mnt}"
	echo "DONE"
}

function create_map_file() {
	map_file_path="${pmem_mnt}/${map_file}"
	echo "Create map data file [${map_file_path}]"
	if [ -f "${map_file_path}" ]; then
		echo "File [${map_file_path}] exists, overwrite it?"
		select yn in "Yes" "No"; do
			case $yn in
				Yes)
					rm -f "${map_file_path}"
					break
				;;
				No)
					break
				;;
			esac
		done
	fi

	if [ ! -f "${map_file_path}" ]; then
		truncate "${map_file_path}" --size="${map_file_size}"
	fi

	echo "Create map data file: DONE"
}

function git_ingore_bin() {
	echo "${bench_bin}" >.gitignore
}

function setup() {
	link_bench_bin
	setup_fsdax_devdax
	mount_pmem
	# No need to create map file, the binary executable will create it
	# create_map_file
	git_ingore_bin
}

setup