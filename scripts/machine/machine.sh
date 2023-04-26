#!/bin/bash

set -eu

curr_path=$(realpath $(dirname $0))

case $1 in
    setup)
	shift 1
    $curr_path/grub.sh setup $*
    $curr_path/optane.sh setup appdirect ni
    ;;
    reset)
    $curr_path/grub.sh reset
    $curr_path/optane.sh reset
    ;;
    *)
    echo "$(basename $0) setup|reset"
    ;;
esac
