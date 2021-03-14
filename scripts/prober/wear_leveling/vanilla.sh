#! /bin/bash

print_help() {
    echo "Usage: $0 [region_size]"
    echo "       region_size: (optional) overwrite region size in byte"
}

if [ $# -ne 0 ] && [ $# -ne 1 ]; then
    print_help
    exit 1
fi

report_dev=$1
output_dir=$2

Slack=${Slack:-0}
SlackURL=${SlackURL:-}


task_op=$(cat /proc/lens | grep 'Wear Leveling Test' | sed -e 's/^[ \t]*//' | cut -d: -f1)
echo Task op index: $task_op

stride_size=0
delay=0
# 0 -- vanilla overwrite
sub_op=0

if [ $# -eq 1 ]; then
    region_array=($1)
else
    region_array=($((2 **  8)) $((2 **  9)) $((2 ** 10)) $((2 ** 11)) \
                  $((2 ** 12)) $((2 ** 13)) $((2 ** 14)) $((2 ** 15)) \
                  $((2 ** 16)) $((2 ** 17)) $((2 ** 18)) $((2 ** 19)))
fi

echo Test region sizes: "${region_array[@]}"

for region_size in ${region_array[@]}; do

    msg="$(hostname)-$task_op-$region_size-$stride_size-$delay-$sub_op"
    lens_arg="task=$task_op,access_size=$region_size,stride_size=$stride_size,delay=$delay,op=$sub_op,message=$msg"
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
    dd if=$rep_dev of=$TaskDir/dump-$msg bs=8M count=2

    if [ $Slack -ne 0 ]; then
        slack_notice $SlackURL "[$msg] End"
    fi

done
