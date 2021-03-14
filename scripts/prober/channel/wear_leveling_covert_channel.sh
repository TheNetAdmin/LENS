#! /bin/bash

print_help() {
    echo "Usage: $0 "
	echo "Args:  [region_size]"
	echo "       [stride_size]"
	echo "       [delay]"
	echo "       [delay_per_byte]"
	echo "       [sub_op]"
	echo "       [repeat]"
	echo "       [count]"
	echo "       [covert_data_file_id]"
    echo "       region_size:    wear leveling region size in byte"
    echo "       stride_size:    sender receiver distance size in byte"
    echo "       delay:          overwrite delay in cycle"
    echo "       delay_per_byte: overwrite delay in byte"
    echo "       repeat:         total # of writes per iteration (per 1 bit sent)"
}

if [ $# -ne 8 ]; then
    print_help
    exit 1
fi

Slack=${Slack:-0}
SlackURL=${SlackURL:-}

# Use `cat /proc/lens` for more details about [op]
# Search 'this_task_name' in 'src/tasks.c' for more details about [task_name]
op=$(cat /proc/lens | grep 'Wear Leveling Covert Channel Test' | sed -e 's/^[ \t]*//' | cut -d: -f1)
task_name=TASK_WEAR_LEVELING_COVERT_CHANNEL

rep_dev=${rep_dev:-/dev/pmem0}

region_size=${1}
stride_size=${2}
delay=${3}
delay_per_byte=${4}
sub_op=${5}
repeat=${6}
count=${7}
covert_data_file_id=${8}
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

echo Test region sizes:   "$region_size"
echo Test strided sizes:  "$stride_size"
echo Test delay:          "$delay"
echo Test delay per byte: "$delay_per_byte"
echo Test repeat:         "$repeat"
echo Test count:          "$count"
echo Test op index:       "$op"
echo Test access size:    "$access_size"
echo Test covert file:    "$covert_data_file"

msg="$(hostname)-$op-$region_size-$stride_size-$delay-$delay_per_byte-$sub_op-$repeat-$count-$parallel-$access_size"
lens_arg="task=$op,access_size=$access_size,pc_region_size=$region_size,stride_size=$stride_size,delay=$delay,delay_per_byte=$delay_per_byte,op=$sub_op,repeat=$repeat,count=$count,parallel=$parallel,sync_per_iter=$sync_per_iter,message=$msg"

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
