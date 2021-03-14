#! /bin/bash

print_help() {
    echo "Usage: $0 [region_size] [threshold_cycle] [repeat]"
    echo "       region_size:     overwrite region size in byte"
    echo "       threshold_cycle: cycle threshold for long latency"
}

if [ $# -ne 3 ]; then
    print_help
    exit 1
fi

Slack=${Slack:-0}
SlackURL=${SlackURL:-}


task_op=$(cat /proc/lens | grep 'Wear Leveling Test' | sed -e 's/^[ \t]*//' | cut -d: -f1)
echo Task op index: $task_op

sub_op=6

stride_size=0
delay=0
region_array=($1)
threshold_cycle=$2
repeat=$3

echo Test region sizes: "${region_array[@]}"

for region_size in ${region_array[@]}; do

    msg="$(hostname)-$task_op-$region_size-$stride_size-$delay-$sub_op"
    lens_arg="task=$task_op,access_size=$region_size,stride_size=$stride_size,delay=$delay,op=$sub_op,threshold_cycle=$threshold_cycle,repeat=$repeat,message=$msg"
    task_name=TASK_WEAR_LEVELING

    if [ $Slack -ne 0 ]; then
        source $(realpath $(dirname $0))/../../utils/slack.sh
        slack_notice $SlackURL "[$msg] Start"
    fi

    echo $lens_arg
    echo $lens_arg >/proc/lens

    while true; do
        sleep 1
        q=$(dmesg | grep "${task_name}_END")
        if [[ $q ]]; then
            break
        fi
    done

    # Total count 2*1048586, each latency is size(long)=8, total size 2*1048576*8=16MB
    # Warm up repeat does not have to dump data
    # dd if=$rep_dev of=$TaskDir/dump-$msg bs=8M count=2

    if [ $Slack -ne 0 ]; then
        slack_notice $SlackURL "[$msg] End"
    fi

done
