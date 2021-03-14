#! /bin/bash

print_help() {
    echo "Usage: $0 "
	echo "Args:  [region_size]        : wear leveling region size in byte"
	echo "       [stride_size]        : sender receiver distance size in byte"
	echo "       [delay]              : overwrite delay in cycle"
	echo "       [delay_per_byte]     : overwrite delay in byte"
	echo "       [sub_op]             : sub op #"
	echo "       [repeat]             : total # of victim accesses per iter (per 1 bit)"
	echo "       [threshold_cycle]    : threshold cycle for long latency"
	echo "       [threshold_iter]     : total # attacker accesses per iter"
	echo "       [warm_up]            : 1=enable 0=disable warm up of overwrite region"
	echo "       [covert_data_file_id]: covert data file id"
	echo "       [block_size]         : overwrite block size"
}

if [ $# -ne 11 ]; then
    print_help
    exit 1
fi

Slack=${Slack:-0}
SlackURL=${SlackURL:-}

# Use `cat /proc/lens` for more details about [op]
# Search 'this_task_name' in 'src/tasks.c' for more details about [task_name]
op=$(cat /proc/lens | grep 'Wear Leveling Side Channel Test' | sed -e 's/^[ \t]*//' | cut -d: -f1)
task_name=TASK_WEAR_LEVELING_SIDE_CHANNEL

rep_dev=${rep_dev:-/dev/pmem0}

region_size=${1}
stride_size=${2}
delay=${3}
delay_per_byte=${4}
sub_op=${5}
repeat=${6}
threshold_cycle=${7}
threshold_iter=${8}
warm_up=${9}
covert_data_file_id=${10}
block_size=${11}
sync_per_iter=${sync_per_iter:-1}

parallel=2

covert_data_path="$(realpath "$(dirname "${0}")"/covert_data)"
covert_data_file_pattern="${covert_data_path}/${covert_data_file_id}.*.*.bin"
if ls ${covert_data_file_pattern} 1>/dev/null 2>&1; then
	covert_data_file="$(readlink -f ${covert_data_file_pattern})"
	file_size_byte="$(stat --format=%s "${covert_data_file}")"
	dd if="${covert_data_file}" of=${rep_dev} bs=${file_size_byte} count=1 conv=fsync
	access_size=$((file_size_byte * 8))
	sleep 2
else
	echo "Cannot find covert data file with ID: ${covert_data_file_id}"
	exit 2
fi

$script_path/utils/umount.sh
$script_path/utils/mount.sh $rep_dev $test_dev no_remake

echo "Test op index:           : $op"
echo "Test region_size         : $region_size"
echo "Test stride_size         : $stride_size"
echo "Test block_size          : $block_size"
echo "Test delay               : $delay"
echo "Test delay_per_byte      : $delay_per_byte"
echo "Test sub_op              : $sub_op"
echo "Test repeat              : $repeat"
echo "Test threshold_cycle     : $threshold_cycle"
echo "Test threshold_iter      : $threshold_iter"
echo "Test warm_up             : $warm_up"
echo "Test covert_data_file_id : $covert_data_file_id"
echo "Test covert file         : $covert_data_file"

msg="$(hostname)-$op-$region_size-$stride_size-$delay-$delay_per_byte-$sub_op-$repeat-$threshold_cycle-$threshold_iter-$warm_up-$parallel-$access_size-$block_size"
lens_arg="task=$op,access_size=$access_size,pc_region_size=$region_size,pc_block_size=$block_size,stride_size=$stride_size,delay=$delay,delay_per_byte=$delay_per_byte,op=$sub_op,repeat=$repeat,threshold_cycle=$threshold_cycle,threshold_iter=$threshold_iter,warm_up=$warm_up,parallel=$parallel,sync_per_iter=$sync_per_iter,message=$msg"

if [ "$Slack" -ne 0 ]; then
    source "$(realpath $(dirname $0))/../../utils/slack.sh"
    slack_notice "$SlackURL" "[$msg] Start"
fi

echo "$lens_arg"
echo "$lens_arg" >/proc/lens

log_msg_beg="LENS_START: $msg"
log_msg_end="LENS_END: $msg"

while true; do
    sleep 2
    q=$(dmesg | grep "${log_msg_end}")
    if [[ $q ]]; then
        break
    fi
done

msg_start=$(dmesg | grep -n "${log_msg_beg}" | tail -1 | awk -F':' {' print $1 '})
msg_end=$(dmesg | grep -n "${log_msg_end}" | tail -1 | awk -F':' {' print $1 '})
msg_end+="p"
dmesg | sed -n "$msg_start,$msg_end"

if [ $Slack -ne 0 ]; then
    slack_notice $SlackURL "[$msg] End"
fi
