#! /bin/bash

for i in {1..6}; do
	echo "######## Run $i Job #######"
	yes $i | ./run.sh
	sleep 2
done
