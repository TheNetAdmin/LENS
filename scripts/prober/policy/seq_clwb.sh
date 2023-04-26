#!/bin/bash

print_help() {
    echo "Usage: $0 [access_size] [offset_size]"
    echo "       access_size: (optional) sequential access size in byte"
    echo "       offset_size: (optional) sequential offset size in byte"
    echo
    echo "Note:  Set access_size AND offset_size to run single sequential strided test,"
    echo "       or use the predefined access_size and offset_size in this script"
}

if [ $# -ne 0 ] && [ $# -ne 2 ]; then
    print_help
    exit 1
fi

Slack=${Slack:-0}
SlackURL=${SlackURL:-}

op=$(cat /proc/lens | grep 'Strided Latency Test' | sed -e 's/^[ \t]*//' | cut -d: -f1)
task_name=LATTester_LAT

# NOTE: offset is use to calculate stridesize
#       offset is not directly taken by stride latency test
if [ $# -eq 0 ]; then
    offset_array=(0)

    access_64b_4k=$(seq -s ' ' 64 64 4095)
    access_4k_16k=$(seq -s ' ' 4096 256 16384)
    access_array=("${access_64b_4k[@]}" "${access_4k_16k[@]}")
else
    access_array=($1)
    offset_array=($2)
fi

for accesssize in ${access_array[@]}; do
    for offsetsize in ${offset_array[@]}; do
        stridesize=$(expr $accesssize + $offsetsize)

        msg="$(hostname)-$op-$accesssize-$stridesize"
        lens_arg="task=$op,access_size=$accesssize,stride_size=$stridesize,message=$msg"

        if [ $Slack -ne 0 ]; then
            source $(realpath $(dirname $0))/../../utils/slack.sh
            slack_notice $SlackURL "[$msg] Start"
        fi

        echo $lens_arg
        echo $lens_arg >/proc/lens

        while true; do
            x=$(dmesg | grep "${task_name}_END: $accesssize-$stridesize")
            if [[ $x ]]; then
                break
            fi
            sleep 1
        done

        msgstart=$(dmesg | grep -n "LENS_START" | tail -1 | awk -F':' {' print $1 '})
        msgend=$(dmesg | grep -n "${task_name}_END: $accesssize-$stridesize" | tail -1 | awk -F':' {' print $1 '})
        msgend+="p"
        dmesg | sed -n "$msgstart,$msgend" | grep "average"

        if [ $Slack -ne 0 ]; then
            slack_notice $SlackURL "[$msg] End"
        fi
    done
done
