#include "stdio.h"
#include "stdint.h"

int main(int argc, char const *argv[])
{
	uint64_t data[10];
	asm volatile (
		"mov		%[data], %%r9\n\t"
		"xor		%%r12, %%r12\n\t"
		"vmovdqa	%%xmm0, (64 * (0))(%[data], %%r12)\n\t"
		"vmovdqa	(%[data]), %%xmm0\n\t"
		"vmovdqa	%%ymm0, (64 * (0))(%[data], %%r12)\n\t"
		"vmovdqa	(%[data]), %%ymm0\n\t"
		"vmovdqa 	(64 * (0 + 0 + 1))(%%r9, %%r12), %%ymm0\n\t"
		:
		: [data] "r" (data)
	);
	printf("Done\n");
	return 0;
}
