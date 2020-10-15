#!/bin/sh
umount /mnt/latency
umount /mnt/report


rmmod latfs
rmmod repfs

echo 1 > /proc/sys/kernel/soft_watchdog
