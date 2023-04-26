#! /bin/bash

set -e

print_help() {
    echo "Usage: $0 [distance]"
    echo "       distance: (required) overwrite distance between two points"
}

if [ $# -ne 1 ]; then
    print_help
    exit 1
fi

Slack=${Slack:-0}
SlackURL=${SlackURL:-}

task_op=$(cat /proc/lens | grep 'Wear Leveling Test' | sed -e 's/^[ \t]*//' | cut -d: -f1)
echo Task op index: $task_op

# Region size is the max distance allowed
region_size=$((2 ** 20))
delay=0
# 2 -- overwrite two points
sub_op=2

distance_array=($1)

echo Test distance sizes: "${distance_array[@]}"

for distance in ${distance_array[@]}; do

    msg="$(hostname)-$task_op-$region_size-$distance-$delay-$sub_op"
    lens_arg="task=$task_op,access_size=$region_size,stride_size=$distance,delay=$delay,op=$sub_op,message=$msg"
    task_name=TASK_WEAR_LEVELING

    if [ $Slack -ne 0 ]; then
        source $(realpath $(dirname $0))/../../utils/slack.sh
        slack_notice $SlackURL "[$msg] Start"
    fi

    echo "$lens_arg"
    echo "$lens_arg" >/proc/lens

    while true; do
        sleep 1
        q=$(dmesg | grep "${task_name}_END")
        if [[ $q ]]; then
            break
        fi
    done

    # Total count 2*1048586, each latency is size(long)=8, total size 2*1048576*8=16MB
    dd if=$rep_dev of=$TaskDir/dump-$msg bs=8M count=2

    if [ $Slack -ne 0 ]; then
        slack_notice $SlackURL "[$msg] End"
    fi

done
