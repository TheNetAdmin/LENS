#!/bin/bash

rounds=${1:-5}
for r in $(seq 1 $rounds); do
	echo "Top Round: $r"
	bash ./all_ranged_date_per_set.sh  1
	bash ./all_ranged_month_per_set.sh 1
done
