#! /bin/bash

# https://unix.stackexchange.com/questions/527521/how-to-count-the-number-of-bytes-in-a-file-grouping-the-same-bytes

file_name=/mnt/pmem/redis/redis-port-6379-2GB-AEP

# od -t x1 -w1 -v -An ${file_name} | sort | uniq -c
# < ${file_name} xxd -p | fold -w 2 | sort | uniq -c

od -t x1 -v -An ${file_name} |
    tr ' ' '\n' |
    awk '$1 > "" { h[$1]++ } END { for (v in h) {printf "%d\t%s\n", h[v], v} }' |
    sort -k2
