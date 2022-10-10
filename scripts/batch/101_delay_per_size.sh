#!/bin/bash

set -x
set -e

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
        # delay each
        run_prober $script_root/prober/wear_leveling/delay_each.sh 256 $((2 **  17))
        # two points
        run_prober $script_root/prober/wear_leveling/two_points.sh 4096
        # delay half: delay $((2 ** 34)) --> 10s; $((2 ** 35)) --> 20s; $((2 ** 36)) --> 30s
        run_prober $script_root/prober/wear_leveling/delay_half.sh 256 $((2 **  30))
        # delay per size: for delay_per_byte $((2 ** 21)), i.e. delay every 2 MiB write
        #                 delay $((2 ** 27)) --> 20s; $((2 ** 28)) --> 30s
        run_prober $script_root/prober/wear_leveling/delay_per_size.sh 256 $((2 **  23)) $((2 ** 21))
    ;;
    all)
        # delay per 256 B
        # NOTE: delay $((2 ** 17)) runs 1m32s
        delay_array=($(seq -s ' ' $((2 **  10)) $((2 **  10)) $((2 **  14 - 1))) \
                     $(seq -s ' ' $((2 **  14)) $((2 **  14)) $((2 **  17 - 1))) \
                     $(seq -s ' ' $((2 **  17)) $((2 **  17)) $((2 **  18    ))))
        delay_per_byte=256
        for delay in "${delay_array[@]}"; do
            run_prober $script_root/prober/wear_leveling/delay_per_size.sh $((2 **  8)) $delay $delay_per_byte
        done
        # delay per 512 B - 2MB
        delay_array=($(seq -s ' ' $((2 **  10)) $((2 **  10)) $((2 **  14 - 1))) \
                     $(seq -s ' ' $((2 **  14)) $((2 **  14)) $((2 **  16 - 1))) \
                     $(seq -s ' ' $((2 **  16)) $((2 **  16)) $((2 **  19    ))))
        delay_per_byte_array=($((2 **  9)) $((2 ** 10)) $((2 ** 11)) $((2 ** 12)) $((2 ** 13))
                          $((2 ** 14)) $((2 ** 15)) $((2 ** 16)) $((2 ** 17)) $((2 ** 18))
                          $((2 ** 19)) $((2 ** 20)) $((2 ** 21)))
        for delay_per_byte in "${delay_per_byte_array[@]}"; do
            for delay in "${delay_array[@]}"; do
                run_prober $script_root/prober/wear_leveling/delay_per_size.sh $((2 **  8)) $delay $delay_per_byte
            done
        done
        # delay per 2 MB
        # NOTE: delay $((2 ** 27)) runs 20s
        delay_array=($((2 ** 18)) $((2 ** 19)) $((2 ** 20)) $((2 ** 21)) \
                     $((2 ** 22)) $((2 ** 23)) $((2 ** 24)) $((2 ** 25)) \
                     $((2 ** 26)) $((2 ** 27)) $((2 ** 28)) $((2 ** 29)))
        delay_per_byte=$((2**21))
        for delay in "${delay_array[@]}"; do
            # Delay every 2 MiB -- $((2 ** 21))
            run_prober $script_root/prober/wear_leveling/delay_per_size.sh $((2 **  8)) $delay $delay_per_byte
        done
    ;;
esac
