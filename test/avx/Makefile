CFLAGS += -mmmx -msse2 -mavx -mavx2 -mpreferred-stack-boundary=4

avx: avx.c

avx.disasm.c: avx
	objdump -D $< > $@
