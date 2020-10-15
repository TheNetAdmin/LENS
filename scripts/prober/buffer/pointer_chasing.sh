#! /bin/bash

print_help() {
    echo "Usage: $0 job_type [region_size] [block_size]"
    echo "       job_type:    [write | read_and_write | read_after_write]"
    echo "       region_size: (optional) pointer chasing region size in byte"
    echo "       block_size:  (optional) pointer chasing block  size in byte"
    echo
    echo "Note:  Set region_size AND block_size to run single pointer chasing test,"
    echo "       or use the predefined region_size and block_size in this script"
}

if [ $# -ne 1 ] && [ $# -ne 3 ]; then
    print_help
    exit 1
fi

Slack=${Slack:-0}
SlackURL=${SlackURL:-}

# Use `cat /proc/lens` for more details about [op]
# Search 'this_task_name' in 'src/tasks.c' for more details about [task_name]
op=0
task_name=''
case $1 in
write)
    op=$(cat /proc/lens | grep 'Pointer Chasing Write Test' | sed -e 's/^[ \t]*//' | cut -d: -f1)
    task_name=TASK_PC_ST
    ;;
read_and_write)
    op=$(cat /proc/lens | grep 'Pointer Chasing Read and Write Test' | sed -e 's/^[ \t]*//' | cut -d: -f1)
    task_name=TASK_PC_LD_ST
    ;;
read_after_write)
    task_name=TASK_PC_RAW
    op=$(cat /proc/lens | grep 'Pointer Chasing Read after Write Test' | sed -e 's/^[ \t]*//' | cut -d: -f1)
    ;;
*)
    print_help
    exit 1
    ;;
esac

if [ $# -eq 1 ]; then
    region_small=(64 128 256 512 1024 2048 4096 8192)
    region_med=($(seq -s ' ' $((2 ** 14)) $((2 ** 12)) $((2 ** 16))))
    region_large=($(seq -s ' ' $((2 ** 22)) $((2 ** 22)) $((2 ** 26))))

    region_array=("${region_small[@]}" "${region_med[@]}" "${region_large[@]}")
    block_array=(64 256 4096)
else
    region_array=($2)
    block_array=($3)
fi

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
            q=$(dmesg | tail -n 50 | grep "${task_name}_END: $region_size-$block_size")
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
