#!/bin/bash

access_64b_4k=$(seq -s ' ' 64 64 4096)
access_4k_16k=$(seq -s ' ' 4352 256 16384)
access_16k_32k=$(seq -s ' ' 17408 1024 32768)
access_64k_1m=$(seq -s ' ' 65536 61440 1048576)
access_1m_4m=$(seq -s ' ' 131072 262144 4194304)

op_array=( 0 1 2 )
stride_array=( 0 )
access_array=("${access_64b_4k[@]}" "${access_4k_16k[@]}" "${access_16k_32k[@]}" "${access_64k_1m[@]}" "${access_1m_4m[@]}")

rm -f output.txt
for accesssize in ${access_array[@]};  do
	for stridesize in ${stride_array[@]}; do
		stridesize=`expr $accesssize + $stridesize`
        for op in ${op_array[@]}; do
		msg="AEP2-13-$accesssize-$stridesize"
		echo "task=13,access_size=$accesssize,stride_size=$stridesize,message=$msg,op=$op" | tee -a output.txt
		echo "task=13,access_size=$accesssize,stride_size=$stridesize,message=$msg,op=$op" > /proc/lattester
		while true; do
			sleep 1
			x=`dmesg | tail -n 50 | grep "LATTester_FF_END: $accesssize-$stridesize-$op"`
			if [[ $x ]]; then
				break;
			fi
		done
		msgstart=`dmesg | grep -n "LATTester_START: $msg" | tail -1 | awk -F':' {' print $1 '}`
		msgend=`dmesg | grep -n "LATTester_FF_END: $accesssize-$stridesize-$op" | tail -1 | awk -F':' {' print $1 '}`
		msgend+="p"
		dmesg | sed -n "$msgstart,$msgend" | grep "cycle" | tee -a output.txt
        done
	done
done
