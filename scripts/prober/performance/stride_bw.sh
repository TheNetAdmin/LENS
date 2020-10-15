#!/bin/bash

print_help() {
    echo "Usage: $0 access_size offset_size threads subop"
    echo "       access_size: access size in byte"
    echo "       offset_size: offset size in byte"
    echo "       threads    : number of threads running in parallel"
    echo "       subop      : op for stride bandwith test, more details in 'tasks.c:strided_bwjob()' function"
}

if [ $# -ne 4 ]; then
    print_help
    exit 1
fi

Slack=${Slack:-0}
SlackURL=${SlackURL:-}

op=$(cat /proc/lens | grep ' Strided Bandwidth Test' | sed -e 's/^[ \t]*//' | cut -d: -f1)

# NOTE: offset is use to calculate stridesize
#       offset is not directly taken by stride test
access_array=($1)
offset_array=($2)
parallel_array=($3)
subop_array=($4)

for accesssize in ${access_array[@]}; do
    for offsetsize in ${offset_array[@]}; do
        stridesize=$(expr $accesssize + $offsetsize)
        for parallel in ${parallel_array[@]}; do
            for subop in ${subop_array[@]}; do

                msg="$(hostname)-$op-$accesssize-$stridesize-$parallel-$subop"
                lens_arg="task=$op,access_size=$accesssize,stride_size=$stridesize,runtime=5,parallel=$parallel,delay=0,op=$subop,message=$msg"

                if [ $Slack -ne 0 ]; then
                    source $(realpath $(dirname $0))/../../utils/slack.sh
                    slack_notice $SlackURL "[$msg] Start"
                fi

                echo $lens_arg
                echo $lens_arg >/proc/lens

                while true; do
                    x=$(dmesg | tail -n 50 | grep "LENS_END: $msg")
                    if [[ $x ]]; then
                        break
                    fi
                    sleep 1
                done

                msgstart=$(dmesg | grep -n "LENS_START: $msg" | tail -1 | awk -F':' {' print $1 '})
                msgend=$(dmesg | grep -n "LENS_END: $msg" | tail -1 | awk -F':' {' print $1 '})
                msgend+="p"
                dmesg | sed -n "$msgstart,$msgend" | grep "average"

                if [ $Slack -ne 0 ]; then
                    slack_notice $SlackURL "[$msg] End"
                fi
            done
        done
    done
done
