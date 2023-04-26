obj-m += repfs.o latfs.o

ccflags-y+=-mmmx -msse2 -mavx -mpreferred-stack-boundary=4
ccflags-y+=-I ${PWD}
repfs-y := rep.o
latfs-y := lat.o proc.o misc.o memaccess.o tasks.o
latfs-y += microbench/chasing.o           \
           microbench/overwrite.o         \
           microbench/channel_covert.o    \
           microbench/channel_side.o      \
           microbench/misc.o
latfs-y += tasks/01_basic_op.o                     \
           tasks/02_strided_lat.o                  \
           tasks/03_strided_bw.o                   \
           tasks/04_size_bw.o                      \
           tasks/05_overwrite.o                    \
           tasks/06_pc_write.o                     \
           tasks/07_pc_read_and_write.o            \
           tasks/08_pc_read_after_write.o          \
           tasks/09_seq.o                          \
           tasks/10_flush_first.o                  \
           tasks/11_wear_leveling.o                \
           tasks/12_pc_strided.o                   \
           tasks/13_buffer_covert_channel.o        \
           tasks/14_debugging.o                    \
           tasks/15_wear_leveling_covert_channel.o \
           tasks/16_wear_leveling_side_channel.o

comp-h := comp_utils/dax_check.h

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

ifeq ($(DEBUG), 1)
	ccflags-y+=-g
endif

ccflags-y+= -DFENCE_MFENCE ${LENS_MAKE_ARGS}

kernel_version=$(shell uname -r)
# kernel_version="4.13.0appdirect+"

# Supress some warnings
# TODO: fix the warnings at source code level, and re-enable these warnings
ccflags-y+= -Wno-unused-function -Wno-unused-variable -Wno-unused-label
ccflags-y+= -Wno-declaration-after-statement

%.o.S: %.o
	objdump -r -d -t $< > $@

comp_utils/%.h: ../scripts/code/compatibility/%.sh
	bash $<

all: module

module: ${comp-h}
	${MAKE} -C /lib/modules/${kernel_version}/build M=`pwd`

clean:
	${MAKE} -C /lib/modules/${kernel_version}/build M=`pwd` clean
	rm -f debug *.pyc *.o.S
	rm -f *.o.ur-safe */*.o.ur-safe
	rm -f ${comp-h}
	rm -rf .kernel_api_check
	rm -rf comp_utils
