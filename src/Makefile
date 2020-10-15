obj-m += repfs.o latfs.o

ccflags-y+=-mmmx -msse2 -mavx -mpreferred-stack-boundary=4
repfs-y := rep.o
latfs-y := lat.o proc.o misc.o memaccess.o tasks.o chasing.o

avx_supported := $(call as-instr,vpxor %xmm0$(comma)%xmm0$(comma)%xmm0,yes,no)
avx2_supported := $(call as-instr,vpgatherdd %ymm0$(comma)(%eax$(comma)%ymm1\
		$(comma)4)$(comma)%ymm2,yes,no)

CLWB := $(shell cat /proc/cpuinfo  | grep clwb | wc -l)

ifneq ($(CLWB), 0)
  ccflags-y+= -DCLWB_SUPPORTED
endif

ifeq ($(PERF), 1)
	ccflags-y+=-DUSE_PERF
	latfs-y+=perf_util.o
endif

ccflags-y+= -DFENCE_MFENCE

all:
	make -C /lib/modules/$(shell uname -r)/build M=`pwd`
	gcc -O0 -o debug -mavx -g debug.c memaccess.c  $(ccflags-y)


clean:
	make -C /lib/modules/$(shell uname -r)/build M=`pwd` clean
	rm -f debug *.pyc -f 

