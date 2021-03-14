#! /bin/bash

print_help() {
    echo "Usage: $0 [region_size] [delay] [delay_per_byte]"
    echo "       region_size: (required) overwrite region size in byte"
    echo "       delay:       (required) cycles to delay between each write, multiple of 16"
    echo "       delay_per_byte:  (required) write size in byte before each delay"
}

if [ $# -ne 3 ]; then
    echo "WRONG USAGE: $*"
    print_help
    exit 1
fi

Slack=${Slack:-0}
SlackURL=${SlackURL:-}

task_op=$(cat /proc/lens | grep 'Wear Leveling Test' | sed -e 's/^[ \t]*//' | cut -d: -f1)
echo Task op index: $task_op

stride_size=0
sub_op=4

if [ $# -eq 3 ]; then
    region_array=($1)
    delay_array=($2)
    delay_per_byte_array=($3)
fi

echo Test region sizes: "${region_array[@]}"
echo Test delay:        "${delay_array[@]}"
echo Test Delay per byte:   "${delay_per_byte_array[@]}"

for region_size in ${region_array[@]}; do
    for delay in ${delay_array[@]}; do
        for delay_per_byte in ${delay_per_byte_array[@]}; do
            msg="$(hostname)-$task_op-$region_size-$stride_size-$delay-$delay_per_byte-$sub_op"
            lens_arg="task=$task_op,access_size=$region_size,stride_size=$stride_size,delay=$delay,delay_per_byte=$delay_per_byte,op=$sub_op,message=$msg"
            task_name=TASK_WEAR_LEVELING

            if [ $Slack -ne 0 ]; then
                source $(realpath $(dirname $0))/../../utils/slack.sh
                slack_notice $SlackURL "[$msg] Start"
            fi

            echo $lens_arg
            echo $lens_arg >/proc/lens

            while true; do
                sleep 5
                q=$(dmesg | grep "${task_name}_END")
                if [[ $q ]]; then
                    break
                fi
            done

            # NOTE "delay per size" test double the result size compared to "vanilla" overwrite
            # Total count 2*1048586, each latency is 2*size(long)=2*8=16, total size 2*1048576*16=32MB
            dd if=$rep_dev of=$TaskDir/dump-$msg bs=16M count=2

            if [ $Slack -ne 0 ]; then
                slack_notice $SlackURL "[$msg] End"
            fi
        done
    done
done
