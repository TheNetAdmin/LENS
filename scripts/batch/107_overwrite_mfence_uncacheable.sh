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

source "$script_root/utils/check_mtrr.sh"
check_mtrr uncacheable


function run_prober() {
    # export Profiler=none
    # yes | $script_root/lens.sh "${rep_dev}" "${lat_dev}" "$@"
    # export Profiler=emon
    # yes | $script_root/lens.sh "${rep_dev}" "${lat_dev}" "$@"
    export Profiler=aepwatch
    yes | $script_root/lens.sh "${rep_dev}" "${lat_dev}" "$@"
}

region_array=(256)
prev_delay_array=()
delay_array=(
    $(seq -s ' ' $((       0)) $((2 **   8)) $((2 **  11 - 1)))
    $(seq -s ' ' $((2 **  11)) $((2 **  11)) $((2 **  14 - 1)))
    $(seq -s ' ' $((2 **  14)) $((2 **  14)) $((2 **  17 - 1)))
    $(seq -s ' ' $((2 **  17)) $((2 **  17)) $((2 **  19    )))
    $(seq -s ' ' $((2 **  19)) $((2 **  19)) $((2 **  24    )))
)
prev_delay_per_byte_array=()
delay_per_byte_array=(
    $((2 **  8)) $((2 **  9)) $((2 ** 10))
	$(seq -s ' ' $((2 **  10)) $((2 **   8)) $((2 **  13 - 1)))
    $((2 ** 13)) $((2 ** 14)) $((2 ** 15)) $((2 ** 16)) $((2 ** 17))
    $((2 ** 18)) $((2 ** 19)) $((2 ** 20)) $((2 ** 21)) $((2 ** 22))
)

timeout=600

job=$1
case $job in
    debug)
        # delay per size: for delay_per_byte $((2 ** 21)), i.e. delay every 2 MiB write
        #                 delay $((2 ** 27)) --> 20s; $((2 ** 28)) --> 30s
        run_prober $script_root/prober/wear_leveling/delay_per_size.sh 256 $((2 **  23)) $((2 ** 21))
    ;;
    all)
        # delay per 256 B - 2MB

        for region_size in "${region_array[@]}"; do
            est_time_hour_min=$(bc -l <<< "scale=2; ${#delay_array[@]} * ${#delay_per_byte_array[@]} * 2 *  5 / 3600")
            est_time_hour_max=$(bc -l <<< "scale=2; ${#delay_array[@]} * ${#delay_per_byte_array[@]} * 2 * 15 / 3600")
            slack_notice $SlackURL "[Start   ] $(basename "$0") [region size: $region_size] est. hours: $est_time_hour_min - $est_time_hour_max"
            ITER=0
            for delay_per_byte in "${delay_per_byte_array[@]}"; do
                overtime_cut=0
                delay_cut_at=0
                for delay in "${delay_array[@]}"; do
					skip_current=0
					for prev_delay in "${prev_delay_array[@]}"; do
						for prev_delay_per_byte in "${prev_delay_per_byte_array[@]}"; do
							if ((delay == prev_delay)) && ((delay_per_byte == prev_delay_per_byte)); then
								echo "Skip previously runned delay[$delay] delay_per_byte[$delay_per_byte]"
								skip_current=1
								break
							fi
						done
						if ((skip_current == 1)); then
							break
						fi
					done
					if ((skip_current == 1)); then
						continue
					fi

                    start_time="$(date -u +%s)"

                    run_prober $script_root/prober/wear_leveling/delay_per_size.sh $region_size $delay $delay_per_byte

                    end_time="$(date -u +%s)"
                    elapsed="$((end_time-start_time))"
                    if [ $elapsed -gt $timeout ]; then
                        overtime_cut=1
                        delay_cut_at=$delay
                        break
                    fi
                done
                ITER=$((ITER + 1))
                PROGRESS=$(bc -l <<< "scale=2; $ITER * 100 / ${#delay_per_byte_array[@]}")
                MSG=""
                if [ $overtime_cut -eq 1 ]; then
                    MSG=" [Delay cut at $delay_cut_at cycles due to long execution time > $timeout sec]"
                fi
                slack_notice $SlackURL "[Progress] $PROGRESS% [$ITER / ${#delay_per_byte_array[@]}] $MSG"
            done
            slack_notice $SlackURL "[End     ] $(basename "$0")"
        done
    ;;
    estimate)
        echo $(bc -l <<< "scale=2; ${#delay_array[@]} * ${#delay_per_byte_array[@]} * 2 *  10 / 3600") hours
    ;;
esac
