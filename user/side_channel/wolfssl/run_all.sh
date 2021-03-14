#!/bin/bash

# Run wolfssl many times to see how accurate we can get

for i in {0..100}; do
	echo "Repeat [$i]" | tee "run_all_progress.log"
	yes 2 | ./run.sh
done
