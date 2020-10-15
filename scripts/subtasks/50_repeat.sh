#!/bin/bash

#`sed 's/^#define SIZEBT_MACRO.*$/TEST/g'`

cd ../src

rm -f ../src/output.txt > /dev/null
cp -f memaccess.c memaccess.c.bak

#accesssize_array=( 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 )
#startoffset_array=( 16384 32768 49152 65536 131072 16777216 50331648 )
accesssize_array=( 256 )
startoffset_array=( 0 )
hostname=`hostname -s`
TAG=`date +%Y%m%d%H%M%S`

#- StrideBW: AEP1-5-0

export AEPWatch=0
export EMon=1
export EMonEventFile=emon-tlb-events.txt

for offset in ${startoffset_array[@]}; do
	for accesssize in ${accesssize_array[@]}; do
		msg="$hostname-5-$accesssize-$offset-repeat$i-$TAG"
		out_path=rebuttal_overwrite/$TAG
		mkdir -p $out_path
		export TaskID=$msg
		echomsg="task=5,access_size=$accesssize,stride_size=$offset"
		echo $echomsg
		export TaskID=$msg
		echo Report $1 Test $2
		../testscript/umount.sh
		../testscript/mount.sh $1 $2
		echo "sleep 5" > /tmp/aeprun.sh
		echo "echo $echomsg > /proc/lattester" >> /tmp/aeprun.sh
		echo "sleep 5" >> /tmp/aeprun.sh
		chmod +x /tmp/aeprun.sh
		aeprun /tmp/aeprun.sh
		sleep 10
	    dd if=$1 of=/tmp/dump bs=8M count=2
	    mv /tmp/dump $out_path/$msg.dump
		#cp aep_latest/aepwatch.csv $out_path/aepwatch-$msg.csv
		cp aep_latest/emon.dat $out_path/emon-$msg.dat
	done
done
