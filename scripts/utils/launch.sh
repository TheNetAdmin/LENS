#!/bin/bash
# Launch LENS probers

DefaultConfig=$(realpath $(dirname $0))/default.sh
if [ -f $DefaultConfig ]; then
    source $DefaultConfig
fi

export TaskID=$(date +%Y%m%d%H%M%S)-$(git rev-parse --short HEAD)-${TaskID:-$(hostname)}
export TaskDir="$(realpath $(dirname $0))/../../results/tasks/$TaskID"

# Profiler envs
Profiler=${Profiler:-none}
source $(realpath $(dirname $0))/profiler/$Profiler/profiler.sh

# Slack envs
Slack=${Slack:-0}
SlackURL=${SlackURL:-}

prepare() {
    if [ $Slack -ne 0 ]; then
        source $(realpath $(dirname $0))/slack.sh
        slack_notice $SlackURL "[$TaskID] Start"
    fi

    echo "Start prepare process"

    # Disable DVFS
	if [ "${boost_cpu}" != "n" ]; then
		for line in $(find /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor); do
			echo "set performance mode for $line"
			sudo bash -c "echo 'performance' >$line"
		done
	else
		echo "Skipping boosting cpu"
	fi

    # Disable cache prefetcher
	if [ "${disable_prefetcher}" != "n" ]; then
		echo "Disabling cache prefetcher"
		if ! sudo lsmod | grep -q "msr"; then
			sudo modprobe msr
		fi
		sudo wrmsr -a 0x1a4 0xf
	else
		echo "Skipping disabling cache prefetcher"
	fi

    # Load and start profiler
    echo "Loading profiler [$Profiler]"
    load_profiler

    echo "Finished prepare process"
}

cleanup() {
    # Enable cache prefetcher
	if [ "${disable_prefetcher}" != "n" ]; then
		sudo wrmsr -a 0x1a4 0x0
	fi

    # Stop and unload profiler
    unload_profiler
    
    if [ $Slack -ne 0 ]; then
        slack_notice $SlackURL "[$TaskID] End"
    fi
}

sigint_handler2() {
    trap sigint_handler2 SIGINT
    echo "$0: Be patient..."
    cleanup
    exit 1
}

sigint_handler() {
    trap sigint_handler2 SIGINT
    echo "$0: Received SIGINT, exiting..."
    cleanup
    exit 1
}

trap sigint_handler SIGINT
mkdir -p $TaskDir

prepare

sudo dmesg --read-clear > $TaskDir/dmesg_before.txt

{
    echo \#\#\# LENS: Start running task "$@"
    echo
	echo \#\#\# Hostname: $(hostname)
	echo
    echo \#\#\# LENS info:
    echo
         cat /proc/lens
    echo
    echo \#\#\# LENS MAKE ARGS:
    echo "$LENS_MAKE_ARGS"
    echo
    echo \#\#\# GCC:
    echo gcc --version
    echo
    echo \#\#\# CPUInfo:
    echo
         lscpu
    echo
    echo \#\#\# MTRR:
    echo
        cat /proc/mtrr
    echo
    echo \#\#\# "MSR 0x1a4 (cache prefetcher) status:"
         if ! rdmsr -a 0x1a4; then 
            echo "ERROR: rdmsr failed"
            exit 1
         fi
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
    echo \#\#\# Slack:         $Slack
    echo \#\#\# SlackURL:      $SlackURL
    echo
        print_profiler
    echo
    echo \#\#\# RepDev: $(mount | grep ReportFS | awk '{print $1}')
        if [ -z $rep_dev ]; then
            export rep_dev="$(mount | grep ReportFS | awk '{print $1}')"
        fi
    echo \#\#\# TestDev: $(mount | grep LatencyFS | awk '{print $1}')
    echo
    echo \#\#\# Commit ID: $(git rev-parse --short HEAD)
    echo \#\#\# Commit Status: $(git diff-index --quiet HEAD -- || echo "dirty")

    git diff-index --quiet HEAD --
    if [ $? -ne 0 ]; then
        echo \#\#\# Code base not clean, output diff to diff.txt
        git diff >$TaskDir/diff.txt
    fi

    # echo \#\#\# Copying object files for debugging
    # echo
    #     cp $(realpath $(realpath $(dirname $0))/../../src/tasks.o) $TaskDir/tasks.o
    # echo

    echo
    echo \#\#\# Start profiler: $Profiler
    start_profiler

    echo
    echo \#\#\# TS_Start $(cat /proc/uptime | sed 's/ [0-9.]*//')

    time nice -20 $@
    ExitCode=$?

    echo
    echo \#\#\# TS_End $(cat /proc/uptime | sed 's/ [0-9.]*//')
    echo
    echo  \#\#\# Stop profiler: $Profiler
    stop_profiler
    echo
    echo \#\#\# ExitCode: $ExitCode
} > >(ts '[%Y-%m-%d %H:%M:%S]' | tee -a "$TaskDir/stdout.log") \
 2> >(ts '[%Y-%m-%d %H:%M:%S]' | tee -a "$TaskDir/stderr.log" >&2)

cleanup

sudo dmesg >"$TaskDir/dmesg_after.txt"

exit $ExitCode
