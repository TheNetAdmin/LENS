#!/bin/bash

parse() {
	# $1 op
	op="${1}"
	echo "" >"${op}.log"
	for f in ${op}/*/recv.log; do
		echo "Parsing $f"
		grep latencies "${f}" >> "${op}.log"
	done
}

pushd results || exit 2

parse send_0
parse send_1

popd || exit 2
