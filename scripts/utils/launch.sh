#!/bin/bash
# Launch LENS probers

export TaskID=${TaskID:-$(hostname)-$(date +%Y%m%d%H%M%S)}
export TaskDir="$(realpath $(dirname $0))/../../results/$TaskID"

EMon=${EMon:-0}
EMonEventFile=${EMonEventFile:emon/events.txt}

Slack=${Slack:-0}
SlackURL=${SlackURL:-}

load_emon() {
    if [ "$EMon" = "1" ]; then
        source /opt/intel/emon/sep_vars.sh /dev/null 2>&1
        (cd $SEPDRV_DIR && ./insmod-sep)
    fi
}

unload_emon() {
    if [ "$EMon" = "1" ]; then
        (cd $SEPDRV_DIR && ./rmmod-sep 2>&1 >/dev/null)
    fi
}

prepare() {
    if [ $Slack -ne 0 ]; then
        source $(realpath $(dirname $0))/slack.sh
        slack_notice $SlackURL "[$TaskID] Start"
    fi

    # Disable DVFS
    for line in $(find /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor); do
        echo "set performance mode for $line"
        echo "performance" >$line
    done

    # Disable cache prefetcher
    wrmsr -a 0x1a4 0xf

    # Load and start EMon
    if [ "$EMon" = "1" ]; then
        load_emon
        emon -i $EMonEventFile -f $TaskDir/emon.dat &
    fi
}

cleanup() {
    # Enable cache prefetcher
    wrmsr -a 0x1a4 0x0

    # Stop and unload EMon
    if [ "$EMon" = "1" ]; then
        # Stop emon if exists
        emon -stop 2>&1 >/dev/null
        unload_emon
    fi
    
    if [ $Slack -ne 0 ]; then
        slack_notice $SlackURL "[$TaskID] End"
    fi
}

sigint_handler2() {
    trap sigint_handler2 SIGINT
    echo "$0: Be patient..."
    cleanup
    exit 0
}

sigint_handler() {
    trap sigint_handler2 SIGINT
    echo "$0: Received SIGINT, exiting..."
    cleanup
    exit 0
}

trap sigint_handler SIGINT
mkdir -p $TaskDir

prepare

dmesg --read-clear > $TaskDir/dmesg_before.txt

{
    echo \#\#\# LENS: Start running task $@
    echo
    echo \#\#\# CPUInfo:
         cat /proc/cpuinfo
    echo
         lscpu
    echo
    echo \#\#\# "MSR 0x1a4 (cache prefetcher) status:"
         rdmsr -a 0x1a4 | tr '\n' ' '
    echo
    echo \#\#\# ndctl list:
         ndctl list
    echo \#\#\# ipmctl:
    echo
        ipmctl show -system -capabilities
    echo
        ipmctl show -memoryresources
    echo
        ipmctl show -preferences
    echo
        ipmctl show -firmware
    echo
        ipmctl show -region
    echo
    echo \#\#\# Mount:
        mount | grep pmem
    echo
    echo \#\#\# NUMA Status:
        numactl -H
    echo
    echo \#\#\# zoneinfo:
        cat /proc/zoneinfo | grep 'Node\|start_pfn'
    echo
    echo \#\#\# TaskID:        $TaskID
    echo \#\#\# TaskDir:       $TaskDir
    echo \#\#\# EMon:          $EMon
    echo \#\#\# EMonEventFile: $EMonEventFile
    echo \#\#\# Slack:         $Slack
    echo \#\#\# SlackURL:      $SlackURL
    echo
    echo \#\#\# RepDev: `mount | grep ReportFS | awk {'print \$1'}`
    echo \#\#\# TestDev: `mount | grep LatencyFS | awk {'print \$1'}`
    echo
    echo \#\#\# Commit ID: $(git rev-parse --short HEAD)
    echo \#\#\# Commit Status: $(git diff-index --quiet HEAD -- || echo "dirty")

    git diff-index --quiet HEAD --
    if [ $? -ne 0 ]; then
        echo \#\#\# Code base not clean, output diff to diff.txt
        git diff >$TaskDir/diff.txt
    fi

    echo
    echo \#\#\# TS_Start $(cat /proc/uptime | sed 's/ [0-9.]*//')

    time nice -20 $@
    ExitCode=$?

    echo
    echo \#\#\# TS_End $(cat /proc/uptime | sed 's/ [0-9.]*//')
    echo \#\#\# ExitCode: $ExitCode
} > >(tee -a $TaskDir/stdout.log) 2> >(tee -a $TaskDir/stderr.log >&2)

cleanup

dmesg >$TaskDir/dmesg_after.txt

exit $ExitCode
