#!/bin/bash

echo "Turning off smt"
echo off > /sys/devices/system/cpu/smt/control
