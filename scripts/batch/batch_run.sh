#!/bin/bash

set -e

# BATCH_ID: MM-DD-YYYY-ScriptID-ID

curr_path=$(realpath "$(dirname $0)")
result_path=$(realpath "$curr_path/../../results")
script_path=$(realpath "$curr_path/../")

source "$curr_path/default_config.sh"
source "$script_path/utils/slack.sh"
source "$script_root/utils/check_smt.sh"

operation=$1
shift 1

if [[ $operation != "all" ]] && [[ $operation != "debug" ]] && [[ $operation != "estimate" ]]; then
	echo "Usage: $0 [all|debug|estimate] [task_id|...]"
	exit 1
fi

check_smt

all_script_id=("$@")

while true; do
	echo "Perform operation: $operation"
    echo "These tasks selected: " "${all_script_id[@]}"
    read -r -p "Start to run them [y/n]: " yn
    case $yn in
        [Yy]* ) break;;
        [Nn]* ) exit;;
        * ) echo "Please answer yes or no.";;
    esac
done

slack_notice $SlackURL "\`[Batch Start]\` \`[$host_name]\` $operation ${all_script_id[*]}"

for sid in "${all_script_id[@]}"; do
    if [ -d "$result_path/tasks" ]; then
        echo "Backing up old results"
        mv "$result_path/tasks" "$result_path/tasks_bak_$(date +%Y%m%d_%H%M%S)"
        sleep 1
    fi
    
	mkdir -p "$result_path/tasks"

	echo "Dump kernel dmesg"
	(dmesg -w > "$result_path/tasks/dmesg.log") &
	dmesg_pid=$!

    sfile=$(ls "$curr_path/$sid"_* | head -n 1 | xargs -0 readlink -f)
    bash "$sfile" "$operation" 2>&1 | tee "$result_path/tasks/output.log"

	echo "Finish kernel dmesg dumping"
	kill "${dmesg_pid}"

	if [ -d "$result_path/tasks" ]; then
		result_moved=0
		for id in $(seq 0 99); do
			batch_id="$(date +%m-%d-%Y)-$sid-$id-$host_name"
			if [ -d "$result_path/tasks-$batch_id" ]; then
				continue
			else
				mv "$result_path/tasks" "$result_path/tasks-$batch_id"
				result_moved=1
				break
			fi
		done
		if [ $result_moved -ne 1 ]; then
			echo "More than 100 task folders found, cannot generate more id"
			exit 1
		fi
	fi

    slack_notice $SlackURL "\`[Batch Ckpt]\` \`[$host_name]\` Finished $sfile, ID: tasks-$batch_id"
done

slack_notice $SlackURL "\`[Batch End]\` \`[$host_name]\` $operation ${all_script_id[*]}"

