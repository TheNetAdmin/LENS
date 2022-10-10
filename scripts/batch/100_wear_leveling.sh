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
        run_prober $script_root/prober/wear_leveling/vanilla.sh 4096
        # # delay each
        # run_prober $script_root/prober/wear_leveling/delay_each.sh 256 $((2 **  17))
        # # two points
        # run_prober $script_root/prober/wear_leveling/two_points.sh 4096
        # # delay half: delay $((2 ** 34)) --> 10s; $((2 ** 35)) --> 20s; $((2 ** 36)) --> 30s
        # run_prober $script_root/prober/wear_leveling/delay_half.sh 256 $((2 **  30))
        # # delay per size: for delay_per_byte $((2 ** 21)), i.e. delay every 2 MiB write
        # #                 delay $((2 ** 27)) --> 20s; $((2 ** 28)) --> 30s
        # run_prober $script_root/prober/wear_leveling/delay_per_size.sh 256 $((2 **  23)) $((2 ** 21))
    ;;
    all)
        # two points
        distance_array=($(seq -s ' ' $((2 **   8)) $((2 **   8)) $((2 **  14 - 1))) \
                        $(seq -s ' ' $((2 **  14)) $((2 **  14)) $((2 **  18 - 1))) \
                        $(seq -s ' ' $((2 **  18)) $((2 **  18)) $((2 **  20    ))))
        for distance in "${distance_array[@]}"; do
            run_prober $script_root/prober/wear_leveling/two_points.sh $distance
        done
        # delay each
        # NOTE: delay $((2 ** 17)) runs 1m32s
        delay_array=($((2 **  10)) $((2 **  11)) $((2 **  12)) $((2 **  13)) \
                     $((2 **  14)) $((2 **  15)) $((2 **  16)) $((2 **  17)))
        for delay in "${delay_array[@]}"; do
            run_prober $script_root/prober/wear_leveling/delay_each.sh $((2 **  8)) $delay
        done
        # vanilla
        region_array=($(seq -s ' ' $((2 **   8)) $((2 **   8)) $((2 **  14 - 1))) \
                      $(seq -s ' ' $((2 **  14)) $((2 **  14)) $((2 **  18 - 1))) \
                      $(seq -s ' ' $((2 **  18)) $((2 **  18)) $((2 **  20    ))))
        for region_size in "${region_array[@]}"; do
            run_prober $script_root/prober/wear_leveling/vanilla.sh $region_size
        done
        # delay half
        # NOTE: delay $((2 ** 35)) runs 20s
        half_delay_array=($((2 ** 32)) $((2 ** 33)) $((2 ** 34)) $((2 ** 35)) \
                          $((2 ** 36)) $((2 ** 37)) $((2 ** 38)) $((2 ** 39)))
        for delay in "${half_delay_array[@]}"; do
            run_prober $script_root/prober/wear_leveling/delay_half.sh $((2 **  8)) $delay
        done
        # delay per size
        # NOTE: delay $((2 ** 27)) runs 20s
        delay_per_byte_array=($((2 ** 18)) $((2 ** 19)) $((2 ** 20)) $((2 ** 21)) \
                          $((2 ** 22)) $((2 ** 23)) $((2 ** 24)) $((2 ** 25)) \
                          $((2 ** 26)) $((2 ** 27)) $((2 ** 28)) $((2 ** 29)))
        for delay_per_byte in "${delay_per_byte_array[@]}"; do
            # Delay every 2 MiB -- $((2 ** 21))
            run_prober $script_root/prober/wear_leveling/delay_per_size.sh $((2 **  8)) $delay_per_byte $((2 ** 21))
        done
    ;;
esac
