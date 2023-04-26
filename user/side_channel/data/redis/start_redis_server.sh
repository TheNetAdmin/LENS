#! /bin/bash

~/PMDK-Libs/pmem-redis/src/redis-server --nvm-maxcapacity 2 --nvm-dir /mnt/pmem/redis --nvm-threshold 1
