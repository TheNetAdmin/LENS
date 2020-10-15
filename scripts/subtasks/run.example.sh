#!/bin/bash


rm ../src/output.txt -f

wrmsr -a 0x1a4 0xf


testapp=./31_seqbwfig.sh
repdev=`mount | grep ReportFS | awk {'print \$1'}`
testdev=`mount | grep LatencyFS | awk {'print \$1'}`
commit_id=`git rev-parse --short HEAD`
commit_status=`git diff-index --quiet HEAD -- || echo "-dirty"`

for i in `seq 5`; do
  export TAG=repeatr$i-$commit_id$commit_status
  echo =====Workload description====
  echo Run: $testapp
  echo Tag: $TAG
  echo LatFS on: $testdev
  echo RepFS on: $repdev
  echo Config:
  cat config.json
  echo Press any key to continue...
  read

  $testapp $repdev $testdev
  ./parse_bw.py output.txt 5 1 > $TAG.summary.txt
  mv output.txt ./$TAG.output.txt

done

wrmsr -a 0x1a4 0x0

aeprelease
