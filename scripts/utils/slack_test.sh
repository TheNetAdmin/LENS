#!/bin/bash

set -x
set -e

source ./slack.sh
source ../batch/default_config.sh

slack_notice_markdown \
	"|col0|col1|\n" \
	"|---:|:---|\n" \
	"|val0|val1|" \
;
