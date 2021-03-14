#!/bin/bash

set -ex

curr_path="$(realpath "$(realpath "$(dirname "$0")")")"
test_root="$(realpath "${curr_path}/../../")"

source "${test_root}/../common.sh"
source "${test_root}/default.sh"

map_file="${pmem_mnt}/${map_file}"
bench_bin="${test_root}/${bench_bin}"

echo "Clean up files"
rm -f "${map_file}"
rm -f out.txt

# NOTE:
#   - 2048 entries -> 3m35s runtime
kv_array=(
	$(seq -s ' ' 1 2048)
)

echo "Insert"
for kv in "${kv_array[@]}"; do
	${bench_bin} "${map_file}" persistent insert $kv $kv
done

echo "Get"
for kv in "${kv_array[@]}"; do
	${bench_bin} "${map_file}" persistent get $kv | grep "ctree_map" | tee -a "out.txt"
done
