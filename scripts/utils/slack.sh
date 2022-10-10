#! /bin/bash

# By default do not send slack message
Slack=${Slack:-0}

slack_notice() {
    # $1 Slack URL
    # $2 Slack messageiter
	if [ -z "$Slack" ] || [ "$Slack" -eq 0 ]; then
		echo "Skip sending slack message: [$*]"
		return
	fi

	if [ $# -ne 2 ]; then
		echo "slack_notice needs 2 arguments, but got $#"
		exit 1
	fi
    echo -n "Sending slack message -- $2"
    curl -X POST -H 'Content-type: application/json' --data "{'text':'$2'}" $1 >/dev/null 2>&1
    echo ""
}

slack_notice_msg() {
	# $1 Slack message
	slack_notice "$SlackURL" "\`[$host_name]\` $*"
}

_slack_notice_markdown() {
    # $1 Slack URL
	# $* Markdown text
	if [ $# -lt 2 ]; then
		echo "slack_notice_markdown needs more than 2 arguments, but got $#"
		exit 1
	fi

	local slack_url=$1
	shift 1
    
	echo -n "Sending slack message -- $*"
    curl -X POST -H 'Content-type: application/json' --data "{'blocks':[ { 'type': 'section', 'text': { 'type': 'mrkdwn', 'text': '$*' } } ]}" $slack_url >/dev/null 2>&1
    echo ""
}

slack_notice_markdown() {
	# $1 Slack message
	slack_notice "$SlackURL" "\`[$host_name]\`\n $*"
}

slack_notice_beg() {
	slack_notice_msg "\`[Start   ]\` $*"
}

slack_notice_progress() {
	# $1: iter
	# $2: total iter
	if [ $# -ne 2 ]; then
		echo "slack_notice_progress needs 2 arguments, but got $#"
		exit 1
	fi
	iter=$1
	total=$2
	progress=$(bc -l <<<"scale=2; 100 * $iter / $total")
	progress_string=$(printf "\`%8s\` \`[%3s / %-3s]\`" "$progress %" "$iter" "$total")
	slack_notice_msg "\`[Progress]\` $progress_string"
}

slack_notice_end() {
	slack_notice_msg "\`[End     ]\` \`$*\`"
}

slack_notice_finish_and_notify() {
	slack_notice_msg "\`[Finish  ]\` <$SlackUserID> \`$*\`"
}


export -f slack_notice
export -f slack_notice_msg
export -f slack_notice_beg
export -f slack_notice_progress
export -f slack_notice_end
export -f slack_notice_finish_and_notify
