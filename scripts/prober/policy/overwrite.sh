#! /bin/bash

print_help() {
    echo "Usage: $0"
}

if [ $# -ne 0 ]; then
    print_help
    exit 1
fi

report_dev=$1
output_dir=$2

Slack=${Slack:-0}
SlackURL=${SlackURL:-}

echo Test op index: $op

op=$(cat /proc/lens | grep 'Overwrite Test' | sed -e 's/^[ \t]*//' | cut -d: -f1)
access_size=256
offset_size=0

msg="$(hostname)-$op-$access_size-$offset_size"
lens_arg="task=$op,access_size=$access_size,stride_size=$offset_size,message=$msg"
task_name=TASK_OVERWRITE

if [ $Slack -ne 0 ]; then
    source $(realpath $(dirname $0))/../../utils/slack.sh
    slack_notice $SlackURL "[$msg] Start"
fi

echo $lens_arg
echo $lens_arg >/proc/lens

while true; do
    sleep 10
    q=$(dmesg | tail -n 50 | grep "${task_name}_END")
    if [[ $q ]]; then
        break
    fi
done

dd if=$rep_dev of=$TaskDir/dump bs=8M count=2

if [ $Slack -ne 0 ]; then
    slack_notice $SlackURL "[$msg] End"
fi
