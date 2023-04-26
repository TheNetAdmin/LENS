#! /bin/bash

print_help() {
    echo "Usage: $0 [region_size] [block_size] [stride_size] [fence_strategy] [fence_freq] [sub_op] [repeat] [region_align] [flush_after_load]"
    echo "       region_size: pointer chasing region size in byte"
    echo "       block_size :  pointer chasing block  size in byte"
    echo "       stride_size: pointer chasing block  size in byte"
}

if [ $# -ne 9 ]; then
    print_help
    exit 1
fi

Slack=${Slack:-0}
SlackURL=${SlackURL:-}

# Use `cat /proc/lens` for more details about [op]
# Search 'this_task_name' in 'src/tasks.c' for more details about [task_name]
op=$(cat /proc/lens | grep 'Pointer Chasing Strided Test' | sed -e 's/^[ \t]*//' | cut -d: -f1)
task_name=TASK_PC_STRIDED

region_size=$1
block_size=$2
stride_size=$3
fence_strategy=$4
fence_freq=$5
sub_op=$6
repeat=$7
region_align=$8
flush_after_load=$9

echo Test region sizes:  "$region_size"
echo Test block sizes:   "$block_size"
echo Test strided sizes: "$stride_size"
echo Test op index:      "$op"


# Start LENS job and wait for results
if (($region_size % $block_size != 0)); then
    echo "ERROR: region size ($region_size) % block size ($block_size) != 0"
    exit 1
fi

msg="$(hostname)-$op-$block_size-$region_size-$stride_size-$fence_strategy-$fence_freq-$sub_op-$repeat-$region_align-$flush_after_load"
lens_arg="task=$op,pc_region_size=$region_size,pc_block_size=$block_size,stride_size=$stride_size,op=$sub_op,repeat=$repeat,pc_region_align=$region_align,message=$msg"

if [ "$Slack" -ne 0 ]; then
    source "$(realpath $(dirname $0))/../../utils/slack.sh"
    slack_notice "$SlackURL" "[$msg] Start"
fi

echo "$lens_arg"
echo "$lens_arg" >/proc/lens

while true; do
    sleep 2
    q=$(dmesg | grep "${task_name}_END: $region_size-$block_size-$stride_size")
    if [[ $q ]]; then
        break
    fi
done

msg_start=$(dmesg | grep -n "LENS_START: $msg" | tail -1 | awk -F':' {' print $1 '})
msg_end=$(dmesg | grep -n "${task_name}_END: $region_size-$block_size-$stride_size" | tail -1 | awk -F':' {' print $1 '})
msg_end+="p"
dmesg | sed -n "$msg_start,$msg_end"

if [ $Slack -ne 0 ]; then
    slack_notice $SlackURL "[$msg] End"
fi
