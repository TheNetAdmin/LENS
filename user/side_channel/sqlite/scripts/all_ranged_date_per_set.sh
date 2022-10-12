#!/bin/bash

rounds=${1:-4}

# bench() {
# 	echo "$@"
# 	for r in $(seq 1 $rounds); do
# 		echo -n -e "  $r:\t"
# 		$*
# 		echo "done"
# 	done
# }


for r in $(seq 1 $rounds); do
	echo "Round: $r"
	export sql_op=range1; ./run.sh both_per_set
	export sql_op=range2; ./run.sh both_per_set
	export sql_op=range3; ./run.sh both_per_set
	export sql_op=range4; ./run.sh both_per_set
done
