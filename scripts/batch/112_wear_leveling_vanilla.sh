#!/bin/bash

set -x

print_help() {
    echo "Usage: $0 [debug|all]"
}

if [ $# -ne 1 ]; then
    print_help
    exit 1
fi

script_root=$(realpath $(realpath $(dirname $0))/../)

function run_prober() {
    export Profiler=none # none | emon | aepwatch
    yes | ${script_root}/lens.sh "${rep_dev}" "${lat_dev}" "$@"
}

job=$1
case $job in
    debug)
        # vanilla
        run_prober $script_root/prober/wear_leveling/vanilla.sh 256
    ;;
    all)
        # vanilla
        region_array=(256)
        for region_size in "${region_array[@]}"; do
            run_prober $script_root/prober/wear_leveling/vanilla.sh $region_size
        done
    ;;
esac
