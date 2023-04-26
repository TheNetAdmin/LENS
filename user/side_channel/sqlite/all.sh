#!/bin/bash

rounds=${1:-1}

bench() {
	echo "$@"
	for r in $(seq 1 $rounds); do
		echo -n -e "  $r:\t"
		$*
		echo "done"
	done
}

export sql_op=count       ;  bench ./run.sh both;
export sql_op=sort        ;  bench ./run.sh both;
export sql_op=query       ;  bench ./run.sh both;
export sql_op=insert1000  ;  bench ./run.sh both;
export sql_op=insert10000 ;  bench ./run.sh both;
export sql_op=update100   ;  bench ./run.sh both;
export sql_op=update1000  ;  bench ./run.sh both;

export sql_op=range_month1; ./run.sh both_per_set;
export sql_op=range_month2; ./run.sh both_per_set;
export sql_op=range_month3; ./run.sh both_per_set;
export sql_op=range_month4; ./run.sh both_per_set;
