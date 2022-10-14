#! /bin/bash

for i in {1..6}; do
	yes $i | ./run.sh
done
