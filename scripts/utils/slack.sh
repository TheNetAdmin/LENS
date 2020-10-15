#! /bin/bash

slack_notice() {
    # $1 Slack URL
    # $2 Slack message
    echo -n "Sending slack message -- $2"
    curl -X POST -H 'Content-type: application/json' --data "{'text':'$2'}" $1 >/dev/null 2>&1
    echo ""
}
