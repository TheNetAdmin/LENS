#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <immintrin.h>
#include <x86intrin.h>
#include "nvleak_example.h"

int secret[64] = {
	0, 1, 0, 1, 0, 1, 0, 1,
	0, 1, 0, 1, 0, 1, 0, 1,
	0, 1, 0, 1, 0, 1, 0, 1,
	0, 1, 0, 1, 0, 1, 0, 1,
	0, 1, 0, 1, 0, 1, 0, 1,
	0, 1, 0, 1, 0, 1, 0, 1,
	0, 1, 0, 1, 0, 1, 0, 1,
	0, 1, 0, 1, 0, 1, 0, 1,
};

static unsigned long long rdtscp(void)
{
	unsigned int	   aux;
	unsigned long long ret = __rdtscp(&aux);
	asm volatile("mfence\n");

	return ret;
}


static void flush_page(char *addr) {
	char * page_start = (char *)((((unsigned long long)addr) >> 12) << 12);
	int cli = 0;
	for (cli = 0; cli < 4096; cli += 64) {
		asm volatile(
			"clflush (%[addr])\n\t"
			:
			: [addr] "r" (page_start + cli)
		);
	}
	asm volatile("mfence");
}

static size_t sleep_us_victim(size_t usec)
{
	struct timespec tbeg, tcur;
	uint64_t tdur;
	uint64_t counter = 0;

	clock_gettime(CLOCK_MONOTONIC, &tbeg);
	while(1) {
		for (int i = 0; i < 100; i++) {
			counter += 1;
		}
		clock_gettime(CLOCK_MONOTONIC, &tcur);
		
		tdur = (tcur.tv_sec - tbeg.tv_sec) * 1000000000 + (tcur.tv_nsec - tbeg.tv_nsec);
		if ((tdur / 1000) > usec) {
			break;
		}
	}

	return tdur;
}


static size_t sleep_ns_victim(size_t nsec)
{
	struct timespec tbeg, tcur;
	uint64_t tdur;
	uint64_t counter = 0;

	clock_gettime(CLOCK_MONOTONIC, &tbeg);
	while(1) {
		for (int i = 0; i < 10; i++) {
			counter += 1;
		}
		clock_gettime(CLOCK_MONOTONIC, &tcur);
		
		tdur = (tcur.tv_sec - tbeg.tv_sec) * 1000000000 + (tcur.tv_nsec - tbeg.tv_nsec);
		if (tdur > nsec) {
			break;
		}
	}

	return tdur;
}

size_t warm_up_cpu_core(size_t iters)
{
	size_t i;
	size_t res = 0;
	for (i = 0; i < iters; i++) {
		res += i;
	}
	return res;
}

int main(void)
{
	int i, j;
	int internal_loop = 1;
	unsigned long long cycle_beg, cycle_end;
	struct timespec tbeg, tend;
	size_t warm_up_res;
	
	warm_up_res = warm_up_cpu_core(20000);

	// printf("This is nvleak victim\n");


	clock_gettime(CLOCK_MONOTONIC, &tbeg);
	for (i = 0; i < 32; i++) {
		cycle_beg = rdtscp();
		internal_loop = 5;
		for (j = 0; j < internal_loop; j++) {
			if (secret[i] == 0) {
				nvleak_secret_func_1();
			} else {
				nvleak_secret_func_2();
			}
			
			// sleep_us_victim(10 / internal_loop);
			sleep_ns_victim(10000 / internal_loop);

			_mm_clflush((void *)((char *)nvleak_secret_func_1 +  0));
			_mm_clflush((void *)((char *)nvleak_secret_func_1 + 64));
			_mm_clflush((void *)((char *)nvleak_secret_func_2 +  0));
			_mm_clflush((void *)((char *)nvleak_secret_func_2 + 64));
			_mm_mfence();
			// flush_page((void *)nvleak_secret_func_1);
			// flush_page((void *)nvleak_secret_func_2);
		}

		cycle_end = rdtscp();
		sleep_us_victim(5);
		// usleep(200);
		// sleep(2);
		// printf("Iter=%d, cycle=%llu\n", i, cycle_end - cycle_beg);
	}
	clock_gettime(CLOCK_MONOTONIC, &tend);

	uint64_t tdur = (tend.tv_sec - tbeg.tv_sec) * 1000000000 + (tend.tv_nsec - tbeg.tv_nsec);
	printf("Total time: %lu ns; %.9f sec\n", tdur, (double)tdur / 1e9);
	printf("Iter cycle: %llu\n", cycle_end - cycle_beg);

	return 0;
}
