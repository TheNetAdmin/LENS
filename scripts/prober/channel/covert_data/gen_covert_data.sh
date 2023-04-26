#!/bin/bash

set -e

curr_path="$(realpath "$(dirname "$0")")"
generator_script="$(realpath "${curr_path}/../../utils/gen_covert_data_pattern.py")"

gen_data_file() {
	# $1 file id
	# $2 pattern
	# $3 size (bit)

	if (( $# != 3 )); then
		echo "Wrong number of arguments: $#"
		echo "Arguments: $*"
		exit 1
	fi

	fid="${1}"
	pattern="${2}"
	size="${3}"
	fname="${fid}.${pattern}.${size}.bin"
	if test -f "${fname}"; then
		echo "File exists, skipping: $1"
	elif ls "${fid}".*.*.bin 1> /dev/null 2>&1; then
		echo "Error: file ID conflict"
		echo "       Exists: $(ls "${fid}.*.*.bin")"
		echo "       Expect: ${fname}"
		exit 3
	else
		echo "Generating: ${fname}"
		python3 "${generator_script}" \
			-o "${fname}"             \
			-b ${size}                \
			-p ${pattern}
	fi
}

pushd "${curr_path}" || exit 2

# Filename: ./${file_id}.${pattern}.${file_size_bit}.bin
fid=0
gen_data_file "${fid}" 0   64; ((fid+=1));
gen_data_file "${fid}" 1   64; ((fid+=1));
gen_data_file "${fid}" 2   64; ((fid+=1));
gen_data_file "${fid}" 3   64; ((fid+=1));
gen_data_file "${fid}" 0  256; ((fid+=1));
gen_data_file "${fid}" 1  256; ((fid+=1));
gen_data_file "${fid}" 2  256; ((fid+=1));
gen_data_file "${fid}" 3  256; ((fid+=1));
gen_data_file "${fid}" 0  512; ((fid+=1));
gen_data_file "${fid}" 1  512; ((fid+=1));
gen_data_file "${fid}" 2  512; ((fid+=1));
gen_data_file "${fid}" 3  512; ((fid+=1));
gen_data_file "${fid}" 0 1024; ((fid+=1));
gen_data_file "${fid}" 1 1024; ((fid+=1));
gen_data_file "${fid}" 2 1024; ((fid+=1));
gen_data_file "${fid}" 3 1024; ((fid+=1));
gen_data_file "${fid}" 0 2048; ((fid+=1));
gen_data_file "${fid}" 1 2048; ((fid+=1));
gen_data_file "${fid}" 2 2048; ((fid+=1));
gen_data_file "${fid}" 3 2048; ((fid+=1));

popd
