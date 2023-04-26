#! /bin/bash

print_help() {
    echo "Usage: $0"
}

if [ $# -ne 0 ]; then
    print_help
    exit 1
fi

Slack=${Slack:-0}
SlackURL=${SlackURL:-}

region_array=(64 256 512 $((2 ** 12)) $((2 ** 14)) $((2 ** 24)) $((2 ** 29)))
block_array=(64 256 512 1024 2048 4096)
op=$(cat /proc/lens | grep 'Pointer Chasing Read and Write Test' | sed -e 's/^[ \t]*//' | cut -d: -f1)

echo Test region sizes: "${region_array[@]}"
echo Test block sizes: "${block_array[@]}"
echo Test op index: $op

for block_size in ${block_array[@]}; do
    for region_size in ${region_array[@]}; do
        if (($region_size % $block_size != 0)); then
            continue
        fi

        msg="$(hostname)-$op-$block_size-$region_size"
        lens_arg="task=$op,pc_region_size=$region_size,pc_block_size=$block_size,message=$msg"

        if [ $Slack -ne 0 ]; then
            source $(realpath $(dirname $0))/../../utils/slack.sh
            slack_notice $SlackURL "[$msg] Start"
        fi

        echo $lens_arg
        echo $lens_arg >/proc/lens

        while true; do
            sleep 5
            q=$(dmesg | grep "${task_name}_END: $region_size-$block_size")
            if [[ $q ]]; then
                break
            fi
        done

        msg_start=$(dmesg | grep -n "LENS_START: $msg" | tail -1 | awk -F':' {' print $1 '})
        msg_end=$(dmesg | grep -n "${task_name}_END: $region_size-$block_size" | tail -1 | awk -F':' {' print $1 '})
        msg_end+="p"
        dmesg | sed -n "$msg_start,$msg_end" | grep "average"

        if [ $Slack -ne 0 ]; then
            slack_notice $SlackURL "[$msg] End"
        fi
    done
done
