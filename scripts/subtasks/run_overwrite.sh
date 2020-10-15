#!/bin/bash


rm ../src/output.txt -f

wrmsr -a 0x1a4 0xf

#export AEPWatch=1
#export EMon=0

#testapp=./50_repeat.sh
testapp1=./51_repeat_rebuttal_emon.sh
testapp2=./52_rebuttal_multirun.sh
testapp3=./53_rebuttal_multirun_emon.sh

repdev=`mount | grep ReportFS | awk {'print \$1'}`
testdev=`mount | grep LatencyFS | awk {'print \$1'}`
commit_id=`git rev-parse --short HEAD`
commit_status=`git diff-index --quiet HEAD -- || echo "-dirty"`

  export TAG=repeatr$i-$commit_id$commit_status
  echo =====Workload description====
  echo Run: $testapp1 $testapp2 $testapp3
  echo Tag: $TAG
  echo LatFS on: $testdev
  echo RepFS on: $repdev

  #$testapp1 $repdev $testdev
  #$testapp2 $repdev $testdev
  $testapp3 $repdev $testdev

wrmsr -a 0x1a4 0x0

