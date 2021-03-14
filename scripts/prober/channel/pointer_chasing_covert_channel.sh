#! /bin/bash

print_help() {
    echo "Usage: $0 "
	echo "Args:  [region_size]"
	echo "       [block_size]"
	echo "       [stride_size]"
	echo "       [fence_strategy]"
	echo "       [fence_freq]"
	echo "       [sub_op]"
	echo "       [repeat]"
	echo "       [region_align]"
	echo "       [flush_after_load]"
	echo "       [covert_data_file_id]"
    echo "       region_size: pointer chasing region size in byte"
    echo "       block_size :  pointer chasing block  size in byte"
    echo "       stride_size: pointer chasing block  size in byte"
}

if [ $# -ne 10 ]; then
    print_help
    exit 1
fi

Slack=${Slack:-0}
SlackURL=${SlackURL:-}

# Use `cat /proc/lens` for more details about [op]
# Search 'this_task_name' in 'src/tasks.c' for more details about [task_name]
op=$(cat /proc/lens | grep 'Buffer Covert Chasnnel Test' | sed -e 's/^[ \t]*//' | cut -d: -f1)
task_name=TASK_BUFFER_COVERT_CHANNEL

region_size=${1}
block_size=${2}
stride_size=${3}
fence_strategy=${4}
fence_freq=${5}
sub_op=${6}
repeat=${7}
region_align=${8}
flush_after_load=${9}
parallel=2
align_mode=7
covert_data_file_id=${10}

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

echo Test region sizes:  "$region_size"
echo Test block sizes:   "$block_size"
echo Test strided sizes: "$stride_size"
echo Test op index:      "$op"
echo Test access size:   "$access_size"
echo Test covert file:   "$covert_data_file"


# Start LENS job and wait for results
if (($region_size % $block_size != 0)); then
    echo "ERROR: region size ($region_size) % block size ($block_size) != 0"
    exit 1
fi

msg="$(hostname)-$op-$block_size-$region_size-$stride_size-$fence_strategy-$fence_freq-$sub_op-$repeat-$region_align-$flush_after_load-$parallel-$align_mode-$access_size"
lens_arg="task=$op,access_size=$access_size,pc_region_size=$region_size,pc_block_size=$block_size,stride_size=$stride_size,op=$sub_op,repeat=$repeat,pc_region_align=$region_align,parallel=$parallel,align_mode=$align_mode,message=$msg"

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

