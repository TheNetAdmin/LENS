#! /bin/bash

if [ $# -ne 0 ]; then
	echo "ERROR: $0: No argument accepted"
    exit 1
fi

Slack=${Slack:-0}
SlackURL=${SlackURL:-}

op=$(cat /proc/lens | grep 'Debugging Test' | sed -e 's/^[ \t]*//' | cut -d: -f1)
task_name=TASK_DEBUGGING


msg="$(hostname)-debugging"
lens_arg="task=$op,op=1,pc_region_size=$((16*1024*1024)),message=$msg"

if [ $Slack -ne 0 ]; then
	source $(realpath $(dirname $0))/../../utils/slack.sh
	slack_notice $SlackURL "[$msg] Start"
fi

echo $lens_arg
echo $lens_arg >/proc/lens

while true; do
	sleep 5
	q=$(dmesg | grep "${task_name}_END:")
	if [[ $q ]]; then
		break
	fi
done

msg_start=$(dmesg | grep -n "LENS_START: $msg" | tail -1 | awk -F':' {' print $1 '})
msg_end=$(dmesg | grep -n "${task_name}_END:" | tail -1 | awk -F':' {' print $1 '})
msg_end+="p"
dmesg | sed -n "$msg_start,$msg_end" | grep "average"

if [ $Slack -ne 0 ]; then
	slack_notice $SlackURL "[$msg] End"
fi
