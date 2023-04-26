#!/bin/bash

rm -f example.redis
{
	echo "set key1 value1"
	echo "set key2 value2"
	echo "set key3 value3"
	echo "save"
} >example.redis
