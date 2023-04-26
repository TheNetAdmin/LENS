#!/bin/bash

set -e

curr_path=$(realpath "$(dirname $0)")
out_path=$(realpath "$curr_path/../../results/tasks")

skip=0
# uc_base=0x4000000000
# uc_size=0x4000000000
# uc_size_mb=262144MB
# uc_type=uncachable

mkdir -p $out_path
{
echo "Before:"
mtrr_before=$(cat /proc/mtrr)
while IFS= read -r line; do
    echo "$line"
	curr_base="$(echo "${line}" | cut -d' ' -f2 | cut -d'=' -f2)"
	curr_size="$(echo "${line}" | cut -d',' -f2 | cut -d'=' -f2)"
	curr_type="$(echo "${line}" | cut -d':' -f3 | tr -d ' ')"
	if [ ${curr_base} == ${uc_base} ] && [ ${curr_size} == ${uc_size_mb} ] && [ ${curr_type} == ${uc_type} ]; then
		skip=1
	fi
done <<< "$mtrr_before"

echo

# echo "disable=0" >| /proc/mtrr

# echo "Mid:"
# cat /proc/mtrr
# echo

set_uncacheable() {
	mtrr_config="base=${1} size=${2} type=${3}"
	echo "${mtrr_config}"
	echo "${mtrr_config}" >| /proc/mtrr
}

if [ ${skip} == '0' ]; then
	sudo bash -c "$(declare -f set_uncacheable); set_uncacheable $uc_base $uc_size $uc_type"
else
	echo "Skip assigning the UC region, because it already exists"
fi

echo "After:"
cat /proc/mtrr
echo

sleep 5

} > >(ts '[%Y-%m-%d %H:%M:%S]' | tee -a "$out_path/stdout.log") \
 2> >(ts '[%Y-%m-%d %H:%M:%S]' | tee -a "$out_path/stderr.log" >&2)
