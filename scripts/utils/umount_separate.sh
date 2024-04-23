#!/bin/bash

sudo umount /mnt/latency
sudo umount /mnt/report

sudo rmmod latfs
sudo rmmod repfs

sudo bash -c "echo 1 > /proc/sys/kernel/soft_watchdog"
