#!/bin/bash

function check_mtrr() {
	# $1: cacheable|uncacheable
	case $(hostname) in
		AEP2) uc_base=0x4000000000 ;;
		sdp) uc_base=0x4000000000 ;;
		nv-4) uc_base=0x2000000000 ;;
		# Zixuan: Stormbreaker CXL memory base address starts at a very inconvenient address, disable the usage of uncachable memory for now
		stormbreaker) echo "skip mtrr check on [stormbreaker]"; return 0 ;;
		lens) echo "skip mtrr check on [lens]"; return 0 ;;
		*) echo "Unknown hostname $(hostname)"; exit 2;;
	esac
	memory_region_type=$(cat /proc/mtrr | grep "base=${uc_base}" | awk '{print $6;}' | tr -d '[:space:]')
	case $1 in
		cacheable)
			if [ -n "$memory_region_type" ] && [ "$memory_region_type" != "cachable" ]; then
				echo "MTRR:"
				cat /proc/mtrr
				echo "Error: The memory region is not cacheable"
				exit 1
			fi
		;;
		uncacheable)
			if [ "$memory_region_type" != "uncachable" ]; then
				echo "MTRR:"
				cat /proc/mtrr
				echo "Error: The memory region is not uncacheable"
				exit 1
			fi
		;;
		*)
			echo "Usage: $0 [cacheable|uncacheable]"
			echo "Wrong argument: $1"
			exit 2
		;;
	esac
}
