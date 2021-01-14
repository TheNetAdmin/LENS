# LENS: A Low-level NVRAM Profiler

LENS is a low-level non-volatile memory profiler, which is built based on [LATTester](https://github.com/NVSL/OptaneStudy).

LENS is part of our [MICRO 2020 paper](https://github.com/TheNetAdmin/LENS-VANS).

## Introduction

LENS contains two modules, each of them needs to be mounted on a pmem device (in fsdax mode):

**LatencyFS**: LatencyFS (LatFS) is the working area of the tester. A series of memory accesses will be issued to locations within this area.

**ReportFS**: ReportFS (RepFS) is used to store access latency of individual requests, or intermediate data like pointer chasing index. User can gather the result using `dd if=/dev/pmemX of=result bs=<bs> count=<count>`


### Requirement

1. Linux 4.13 kernel (with devel)
2. CPU with clwb/AVX-512 support (i.e., support Optane DIMM)
3. Two real or emulated PMem regions
4. LENS will overwrite the content of the PMem devices

## Usage

### Run LENS

LENS contains three sets of microbenchmarks (three "prober"s):

1. buffer prober: profiles on-DIMM buffers
2. policy prober: profiles control policies
3. performance prober: profiles microarchitecture components performance

Read the paper for more details of these probers.

The scripts of these probers are under `scripts/prober/`.
To run a prober, please use the script `scripts/lens.sh`, e.g.:

```
$ cd scripts
$ ./lens.sh /dev/pmem0 /dev/pmem14 prober/buffer/pointer_chasing.sh
```

In this example, `lens.sh` compiles the source code, runs the `pointer_chasing` job from buffer prober on `/dev/pmem14`, and writes the result in the `results` directory.

Most of the probers have pre-predefined options (e.g. access size or stride size).
Check their script code for more details about how to change the options.

### Parse results

We provide a few Python3 scripts (`scripts/prober/*/parse.py`) to parse the results from LENS.
These scripts use the *f-string* feature to print results, which requires Python 3.6 or later version.
You can modify the code to replace *f-string* with *str.format()* if you are using older versions.

To install dependencies:

```shell
$ pip3 install click numpy
```

An example to parse `pointer_chasing.sh` results:

```shell
$ python3 scripts/prober/buffer/parse.py pointer_chasing \
          --src_file results/your_task_id/stdout.log \
          --out_file pointer_chasing.csv
```

### Compile and load modules

`lens.sh` script can compile the code and launch the tests.
But if you want to compile and load the modules yourself:

```shell
$ cd scripts

# Compile and load modules
$ ./utils/mount.sh /dev/pmem0 /dev/pmem14

# Unload modules
$ ./utils/umount.sh
```

In this example, the script `mount.sh` compiles the source code in `src` directory, and mount `/dev/pmem0` as `ReportFS`, `/dev/pmem14` as `LatencyFS`.

### Launch a test through command line

It is recommended to use `lens.sh` with prober scripts under `scripts/prober/` to launch tests.
You can modify the prober scripts to meet your requirements.

But if you need to manually launch some tests through command line:

```shell
# Compile and load modules
$ ./scripts/utils/mount.sh /dev/pmem0 /dev/pmem14

# Run a pointer chasing test
$ cat "task=7,pc_region_size=4096,pc_block_size=64,message=try_ptr_chasing_test" > /proc/lens
```

To find more details about LENS options:

```
$ cat /proc/lens

LENS task=<task> op=<op> [options]
Tasks:
        1: Basic Latency Test (not configurable)
        2: Strided Latency Test  [access_size] [stride_size]
        3: Strided Bandwidth Test [access_size] [stride_size] [delay] [parallel] [runtime] [global]
        4: Random Size-Bandwidth Test [bwtest_bit] [access_size] [parallel] [runtime] [align_mode]
        5: Overwrite Test [stride_size]
        6: Pointer Chasing Write Test [pc_region_size] [pc_block_size]
        7: Pointer Chasing Read and Write Test [pc_region_size] [pc_block_size]
        8: Pointer Chasing Read after Write Test [pc_region_size] [pc_block_size]
        9: Sequencial Test [access_size], [parallel]
        10: Flush First Test [stride_size] [access_size] [op]

Options:
        [op|o]             Operation: 0=Load, 1=Write(NT) 2=Write(clwb), 3=Write+Flush(clwb+flush) default=0
        [access_size|a]    Access size (bytes), default=64
        [stride_size|s]    Stride size (bytes), default=64
        [pc_region_size|R] Pointer chasing region size (bytes), default=64
        [pc_block_size|B]  Pointer chasing block size (bytes), default=64
        [align_mode|l]     Align mode: 0 = Global (anywhere), 1 = Per-Thread pool (default) 2 = Per-DIMM pool 3=Per-iMC pool
                                       4 = Per-Channel Group, 5 = [NIOnly!!!]: Global, 6 = [NIOnly!!!]: Per-DIMM
        [delay|d]          Delay (cycles), default=0
        [parallel|p]       Concurrent kthreads, default=1
        [bwsize_bit|b]     Aligned size (2^b bytes), default=6
        [runtime|t]        Runtime (seconds), default=10
        [write_addr|w]     For Straight Write: access location, default=0
        [write_size|z]     For Straight Write: access size, default=0
        [fence_rate|f]     Access size before issuing a sfence instruction
        [clwb_rate|c]      Access size before issuing a clwb instruction
        [probe_count|r]    Number of test rounds
        [message|m]        A string as a unique ID for the current job

Available pointer chasing benchmarks:
        No.     Name                    Block Size (Byte)
        0       pointer-chasing-64      64
        1       pointer-chasing-128     128
        2       pointer-chasing-256     256
        3       pointer-chasing-512     512
        4       pointer-chasing-1024    1024
        5       pointer-chasing-2048    2048
        6       pointer-chasing-4096    4096
```


## Configuration

### Hardware

Item | Description
---|---
CPUs | 2
CPU | Intel Xeon Platinum 8260 ES (Cascade Lake SP)
CPUFreq. | 24 Cores at 2.2 GHz base clock
CPU L1 | 32 KB i-Cache, 32 KB d-cache
CPU L2 | 1 MB
CPU L3 | 33 MB (shared)
DRAM | 2x6x32 GB Micron DDR4 2666 MHz (36ASF4G72PZ)
PM | 2x6x256 GB Intel Optane DC 2666 MHz QS (NMA1XXD256GQS)

### Software

Item | Description
---|---
GNU/Linux Distro | Fedora 27
Linux Kernel | 4.13.0
CPU Governor | Performance
HyperThreading (SMP) | Disabled
NVDIMM Firmware | 01.01.00.5253
KASLR | Disabled
CPU mitigations | Off

## Bibliography

```bibtex
@inproceedings{LENS-VANS,
  author={Zixuan Wang and Xiao Liu and Jian Yang and Theodore Michailidis and Steven Swanson and Jishen Zhao},
  booktitle={2020 53rd Annual IEEE/ACM International Symposium on Microarchitecture (MICRO)}, 
  title={Characterizing and Modeling Non-Volatile Memory Systems}, 
  year={2020},
  pages={496-508},
  doi={10.1109/MICRO50266.2020.00049}
}
```

## License

[![](https://img.shields.io/github/license/TheNetAdmin/LENS)](LICENSE)

