#! /bin/bash

objdump -D -r -t /mnt/pmem/lib/libwolfssl.so.23.0.0 > libwolfssl.S
python3 parse_pages.py
