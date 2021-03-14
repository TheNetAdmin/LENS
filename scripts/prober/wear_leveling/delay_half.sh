#! /bin/bash

print_help() {
    echo "Usage: $0 [region_size] [delay]"
    echo "       region_size: (optional) overwrite region size in byte"
    echo "       delay:       (optional) cycles to delay between each write, multiple of 16"
}

if [ $# -ne 0 ] && [ $# -ne 2 ]; then
    echo "WRONG USAGE: $@"
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
# 3 -- overwrite dealy half
sub_op=3

if [ $# -eq 2 ]; then
    region_array=($1)
    delay_array=($2)
else
    region_array=($((2 **  8)))
    delay_array=($((2 **  29)) $((2 **  28)) $((2 **  30)) $((2 **  31)) $((2 ** 32)) $((2 ** 33)))
fi

echo Test region sizes: "${region_array[@]}"
echo Test delay:        "${delay_array[@]}"

for region_size in ${region_array[@]}; do
    for delay in ${delay_array[@]}; do
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
done
