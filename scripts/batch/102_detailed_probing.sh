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

script_root=$(realpath "$(realpath "$(dirname "$0")")"/../)


export SlackURL=https://hooks.slack.com/services/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
source "$script_root/utils/slack.sh"

function run_prober() {
    # export Profiler=none
    # yes | $script_root/lens.sh "${rep_dev}" "${lat_dev}" "$@"
    export Profiler=emon
    yes | $script_root/lens.sh "${rep_dev}" "${lat_dev}" "$@"
    export Profiler=aepwatch
    yes | $script_root/lens.sh "${rep_dev}" "${lat_dev}" "$@"
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
        # delay per 256 B - 2MB
        delay_array=(
            $(seq -s ' ' $((2 **   8)) $((2 **   7)) $((2 **  11 - 1)))
            $(seq -s ' ' $((2 **  11)) $((2 **   9)) $((2 **  14 - 1)))
            $(seq -s ' ' $((2 **  14)) $((2 **  13)) $((2 **  17 - 1)))
            $(seq -s ' ' $((2 **  17)) $((2 **  16)) $((2 **  19    )))
        )
        delay_per_byte_array=(
            $((2 **  8)) $((2 **  9)) $((2 ** 10)) $((2 ** 11)) $((2 ** 12))
            $((2 ** 13)) $((2 ** 14)) $((2 ** 15)) $((2 ** 16)) $((2 ** 17))
            $((2 ** 18)) $((2 ** 19)) $((2 ** 20)) $((2 ** 21)) $((2 ** 22))
        )
        est_time_hour_min=$(bc -l <<< "scale=2; ${#delay_array[@]} * ${#delay_per_byte_array[@]} * 2 *  5 / 3600")
        est_time_hour_max=$(bc -l <<< "scale=2; ${#delay_array[@]} * ${#delay_per_byte_array[@]} * 2 * 60 / 3600")
        slack_notice $SlackURL "[Start   ] $(basename "$0") est. hours: $est_time_hour_min - $est_time_hour_max"
        ITER=0
        for delay_per_byte in "${delay_per_byte_array[@]}"; do
            overtime_cut=0
            delay_cut_at=0
            for delay in "${delay_array[@]}"; do
                start_time="$(date -u +%s)"

                run_prober $script_root/prober/wear_leveling/delay_per_size.sh $((2 **  8)) $delay $delay_per_byte

                end_time="$(date -u +%s)"
                elapsed="$((end_time-start_time))"
                if [ $elapsed -gt 120 ]; then
                    overtime_cut=1
                    delay_cut_at=$delay
                    break
                fi
            done
            ITER=$((ITER + 1))
            PROGRESS=$(bc -l <<< "scale=2; $ITER / ${#delay_per_byte_array[@]} * 100")
            MSG=""
            if [ $overtime_cut -eq 1 ]; then
                MSG=" [Delay cut at $delay_cut_at cycles due to long execution time >60s]"
            fi
            slack_notice $SlackURL "[Progress] $PROGRESS% [$ITER / ${#delay_per_byte_array[@]}] $MSG"
        done
        slack_notice $SlackURL "[End     ] $(basename "$0")"
    ;;
esac
