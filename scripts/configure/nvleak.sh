#!/bin/bash

set -e
set -u

curr_path=$(realpath "$(dirname $0)")
nvleak_path=$(realpath "${curr_path}/../../")

echo "Initializing NVLeak"

echo -n "Creating the 'results' dir: "
mkdir -p "${nvleak_path}/results"
echo "DONE"