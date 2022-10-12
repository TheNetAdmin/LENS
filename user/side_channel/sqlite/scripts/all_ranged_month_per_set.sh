#!/bin/bash

rounds=${1:-4}

echo "Total rounds: $rounds"

for r in $(seq 1 $rounds); do
	echo "Round: $r"
	export sql_op=range_month1; ./run.sh both_per_set
	export sql_op=range_month2; ./run.sh both_per_set
	export sql_op=range_month3; ./run.sh both_per_set
	export sql_op=range_month4; ./run.sh both_per_set
done
