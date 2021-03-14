#!/bin/bash

cat stdout.log | grep '{1}bit_id' | grep 'summary' > stdout.1.log
cat stdout.log | grep '{2}bit_id' | grep 'summary' > stdout.2.log
