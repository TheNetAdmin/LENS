#!/bin/bash

function check_smt() {
    smt_on=$(cat /sys/devices/system/cpu/smt/active)
    if [ "$smt_on" == 1 ]; then
        echo "Error: SMT is turned on, please turn it off!"
        exit 1
    fi
}
