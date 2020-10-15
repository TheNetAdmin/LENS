#! /bin/bash

set -e

print_help() {
    echo "Usage:   $0 rep_dev test_dev job_script [job_arguments]"
    echo "Example: $0 /dev/pmem0 /dev/pmem14 prober/buffer/pointer_chasing.sh write"
    echo "Note:    Set [rep_dev]  as DRAM emulated PMEM device for fast result recording"
    echo "         Set [test_dev] as PMEM device for testing"
}

sigint_handler() {
    echo '$0: SIGINT received, cleaning...'
    kill -2 $(jobs -p)
    wait
}

trap sigint_handler SIGINT

if [ $# -lt 3 ]; then
    print_help
    exit 1
fi

export rep_dev=$1
export test_dev=$2
job_script=$3
shift 3
job_arguments=$@

if [ ! -f $job_script ]; then
    echo Job script not found: $job_script
    exit 1
fi

echo "=====Workload description===="
echo "    rep_dev:       $rep_dev"
echo "    test_dev:      $test_dev"
echo "    job_script:    $job_script"
echo "    job_arguments: $job_arguments"
echo
echo "Press any key to start, or Ctrl-C to exit..."
read

script_path=$(realpath $(dirname $0))

echo "Compiling source and mount LENS"
$script_path/utils/mount.sh $rep_dev $test_dev

echo "Start test job"
$script_path/utils/launch.sh $job_script $job_arguments

echo "Unmount LENS"
$script_path/utils/umount.sh
