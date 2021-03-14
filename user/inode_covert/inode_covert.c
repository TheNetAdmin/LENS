#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "utils.h"
#include <assert.h>
#include <fcntl.h>
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <x86intrin.h>
// #include <utime.h>
#include <sys/time.h>

#define bench_kernel 1

static void usage(void)
{
	printf("Usage: inode_covert \n"
	       "       -f file_path \n"
	       "       -c covert_data_file_path\n"
	       "       -l cycle_ddl\n"
	       "       -i iterations\n"
	       "       -r sender|receiver\n"
	       "       -o file|inode\n");
}

static unsigned long long rdtscp(unsigned int *aux)
{
	unsigned long long ret = __rdtscp(aux);
	asm volatile("mfence\n");

	return ret;
}

static inline uint64_t wait_until_ddl(uint64_t cycle_beg, uint64_t cycle_end, uint64_t cycle_ddl)
{
	int		   aux;
	unsigned long long cycle_cur;

	/* Target ddl */
	uint64_t cycle_tgt = cycle_beg + cycle_ddl;

	if (cycle_end > cycle_tgt) {
		printf("WARNING: iter_cycle_end [%lu] exceeds iter_cycle_ddl [%lu], skipping wait\n",
		       cycle_end,
		       cycle_tgt);
		return rdtscp(&aux);
	}

	do {
		/* 1024 cycle granularity */
		for (int i = 0; i < 1024 / 16; i++) {
			asm volatile("nop\n nop\n nop\n nop\n "
				     "nop\n nop\n nop\n nop\n "
				     "nop\n nop\n nop\n nop\n "
				     "nop\n nop\n nop\n nop\n ");
		}
		cycle_cur = rdtscp(&aux);
	} while (cycle_cur < cycle_tgt);

	return rdtscp(&aux);
}

typedef enum {
	sender,
	receiver,
} role_t;

typedef struct {
	char *	  file_path;
	char *	  file_1_path;
	char *	  covert_data_file_path;
	size_t	  covert_data_file_size;
	uint64_t *covert_data;
	size_t	  iters;
	size_t	  delay_cycle;
	size_t	  delay_per_iter;
	size_t	  cycle_ddl;
	size_t	  cycle_global_beg;
	size_t	  cycle_global_ddl;
	role_t	  role;
} overwrite_info_t;

void init_overwrite_info(overwrite_info_t *oi)
{
	int cycle_aux;

	oi->file_path		  = NULL;
	oi->file_1_path		  = NULL;
	oi->covert_data_file_path = NULL;
	oi->covert_data_file_size = 0;
	oi->covert_data		  = NULL;
	oi->iters		  = 0;
	oi->delay_cycle		  = 0;
	oi->delay_per_iter	  = 1;
	oi->cycle_ddl		  = 0;
	oi->cycle_global_beg	  = rdtscp(&cycle_aux);
	oi->cycle_global_ddl	  = 1000000;
	oi->role		  = sender;
}

void print_overwrite_info(overwrite_info_t *oi)
{
	printf("Args:\n");
	printf("    file_path             : %s\n", oi->file_path);
	printf("    file_1_path           : %s\n", oi->file_1_path);
	printf("    covert_data_file_path : %s\n", oi->covert_data_file_path);
	printf("    covert_data_file_size : %lu\n", oi->covert_data_file_size);
	printf("    covert_data           : %px\n", oi->covert_data);
	printf("    iters                 : %lu\n", oi->iters);
	printf("    delay_cycle           : %lu\n", oi->delay_cycle);
	printf("    delay_per_iter        : %lu\n", oi->delay_per_iter);
	printf("    cycle_ddl             : %lu\n", oi->cycle_ddl);
	printf("    cycle_global_beg      : %lu\n", oi->cycle_global_beg);
	printf("    cycle_global_ddl      : %lu\n", oi->cycle_global_ddl);
	printf("    role                  : %u\n", oi->role);
	printf("    bench_kernel          : %u\n", bench_kernel);
}

uint64_t *file_mmap(char *file_path, size_t *file_size)
{
	printf("Opening file %s\n", file_path);
	int fd = open(file_path, O_RDWR);
	if (fd < 0) {
		printf("Could not open file: %s\n", file_path);
		exit(2);
	}

	struct stat statbuf;
	int	    err = fstat(fd, &statbuf);
	if (err < 0) {
		printf("Could not stat file: %s\n", file_path);
		exit(2);
	}

	*file_size = statbuf.st_size;
	if (*file_size == 0) {
		printf("Warning: statbuf.st_size==0, force using file_size=1GiB\n");
		*file_size = 1 * 1024 * 1024 * 1024;
	}

	uint64_t *ptr = (uint64_t *)mmap(NULL, *file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (ptr == MAP_FAILED) {
		printf("Mapping failed\n");
		exit(2);
	}
	close(fd);

	return ptr;
}

void file_unmmap(uint64_t *ptr, size_t file_size)
{
	int err = munmap(ptr, file_size);
	if (err != 0) {
		printf("Unmapping Failed\n");
		exit(2);
	}
}

static inline __attribute__((always_inline)) void delay_overwrite(overwrite_info_t *oi, size_t curr_iter)
{
	if ((curr_iter + 1) % oi->delay_per_iter == 0) {
		for (size_t i = 0; i < 1 + (oi->delay_cycle / 16); i++) {
			asm volatile("nop\n nop\n nop\n nop\n"
				     "nop\n nop\n nop\n nop\n"
				     "nop\n nop\n nop\n nop\n"
				     "nop\n nop\n nop\n nop\n");
		}
	}
}

static void file_overwrite(overwrite_info_t *oi)
{
	__m512i		    val;
	unsigned long long  cycle_beg, cycle_end;
	unsigned int	    aux;
	overwrite_latency_t latency;
	char		    buf[LATENCY_MSG_BUF_SIZE];
	size_t		    file_size;
	uint64_t *	    ptr;

	ptr = file_mmap(oi->file_path, &file_size);

	init_overwrite_latency(&latency);

	for (uint64_t i = 0; i < oi->iters; i++) {
		cycle_beg = __rdtscp(&aux);
		_mm512_stream_si512(((void *)(uint8_t *)ptr + 0 * 64), val);
		_mm512_stream_si512(((void *)(uint8_t *)ptr + 1 * 64), val);
		_mm512_stream_si512(((void *)(uint8_t *)ptr + 2 * 64), val);
		_mm512_stream_si512(((void *)(uint8_t *)ptr + 3 * 64), val);
		// _mm_clflush(ptr);
		_mm_mfence();
		cycle_end = __rdtscp(&aux);
		count_overwrite_latency(&latency, cycle_end - cycle_beg);
		delay_overwrite(oi, i);
	}

	print_latency(buf, &latency);
	printf("%s\n", buf);

	file_unmmap(ptr, file_size);
}

static void assert_file_exists(char *file_path)
{
	struct stat statbuf;

	int fd = open(file_path, O_RDWR);
	if (fd < 0) {
		printf("Could not open file: %s\n", file_path);
		exit(2);
	}
	int err = fstat(fd, &statbuf);
	if (err < 0) {
		printf("Could not stat file: %s\n", file_path);
		exit(2);
	}
	close(fd);
}

#if bench_kernel == 0
#define overwrite_func(fd)                                                                                             \
	do {                                                                                                           \
		futimes(fd, times);                                                                                    \
	} while (0)
#elif bench_kernel == 1
#define overwrite_func(fd)                                                                                             \
	do {                                                                                                           \
		ftruncate(fd, i % 4096);                                                                               \
	} while (0)
#elif bench_kernel == 2
#define overwrite_func(origin, prev, next)                                                                             \
	do {                                                                                                           \
		next[0] = '\n';                                                                                        \
		sprintf(next, "%s.%06d", origin, i);                                                                     \
		assert(0 == rename(prev, next));                                                                       \
		strcpy(prev, next);                                                                                    \
	} while (0)
#endif

static void inode_overwrite(overwrite_info_t *oi)
{
	unsigned long long   cycle_beg, cycle_end;
	unsigned int	     aux;
	char		     buf[LATENCY_MSG_BUF_SIZE];
	uint64_t *	     ptr;
	int		     fd;
	int		     fd1;
	struct stat	     statbuf;
	int		     err;
	overwrite_latency_t *latencies;
	overwrite_latency_t *latency;
	size_t		     covert_send_bits;
	size_t		     curr_bit;

	struct timeval times[2] = { 0 };

	covert_send_bits = oi->covert_data_file_size * 8;
	latencies	 = (overwrite_latency_t *)malloc(sizeof(overwrite_latency_t) * covert_send_bits);

	/* Test if inode file exist */
	assert_file_exists(oi->file_path);

	/* Global ddl */
	wait_until_ddl(oi->cycle_global_beg, rdtscp(&aux), oi->cycle_global_ddl);

#if bench_kernel == 0 || bench_kernel == 1
	fd = open(oi->file_path, O_DIRECT | O_SYNC | O_RDWR);
	if (oi->role == sender) {
		fd1 = open(oi->file_1_path, O_DIRECT | O_SYNC | O_RDWR);
	}
#define bench_recv_0 overwrite_func(fd); fsync(fd);
#define bench_send_0 overwrite_func(fd); fsync(fd);
#define bench_send_1 overwrite_func(fd1);fsync(fd1);
#elif bench_kernel == 2
	char prev_file_name[1024];
	char next_file_name[1024];
	char prev_file_1_name[1024];
	char next_file_1_name[1024];

	strcpy(prev_file_name, oi->file_path);
	if (oi->role == sender) {
		strcpy(prev_file_1_name, oi->file_1_path);
	}

	fd = open("/mnt/pmem/", O_DIRECT | O_SYNC | O_RDWR);

#define bench_recv_0 fsync(fd); overwrite_func(oi->file_path, prev_file_name, next_file_name);
#define bench_send_0 fsync(fd); overwrite_func(oi->file_path, prev_file_name, next_file_name);
#define bench_send_1 fsync(fd); overwrite_func(oi->file_1_path, prev_file_1_name, next_file_1_name);
#endif

	for (curr_bit = 0; curr_bit < covert_send_bits; curr_bit++) {
		latency			     = &latencies[curr_bit];
		latency->cycle_without_delay = 0;
		latency->cycle_beg	     = rdtscp(&aux);
		init_overwrite_latency(latency);

		int bit_data = (oi->covert_data[curr_bit / 64] >> curr_bit) & 0x1;

		for (uint64_t i = 0; i < oi->iters; i++) {
			times[1].tv_sec = i;
			times[2].tv_sec = i;
			cycle_beg	= rdtscp(&aux);

			if (oi->role == receiver) {
				bench_recv_0
			} else {
				if (0 == bit_data) {
					bench_send_0
				} else {
					bench_send_1
				}
			}

			cycle_end = rdtscp(&aux);
			count_overwrite_latency(latency, cycle_end - cycle_beg);
			latency->cycle_without_delay += cycle_end - cycle_beg;

			delay_overwrite(oi, i);
		}

		latency->cycle_end = rdtscp(&aux);

		latency->cycle_after_ddl = wait_until_ddl(latency->cycle_beg, latency->cycle_end, oi->cycle_ddl);
	}

#if bench_kernel == 0 || bench_kernel == 1
	close(fd);
	if (oi->role == sender) {
		close(fd1);
	}
#elif bench_kernel == 2
	assert(0 == rename(prev_file_name, oi->file_path));
	if (oi->role == sender) {
		assert(0 == rename(prev_file_1_name, oi->file_1_path));
	}
	close(fd);
#endif

	for (curr_bit = 0; curr_bit < covert_send_bits; curr_bit++) {
		int bit_data = (oi->covert_data[curr_bit / 64] >> curr_bit) & 0x1;
		if (oi->role == sender) {
			printf("Sending bit_id=%u, bit_data=%u\n", curr_bit, bit_data);
		} else {
			printf("Recving bit_id=%u\n", curr_bit);
		}
		latency = &latencies[curr_bit];
		print_latency(buf, latency);
		printf("%s\n", buf);
	}

	printf("Latency Summary:\n");
	
	for (curr_bit = 0; curr_bit < covert_send_bits; curr_bit++) {
		latency = &latencies[curr_bit];
		print_latency_summary(buf, latency);
		printf("%s", buf);
	}
}

static void write_file_pmem(uint64_t *ptr, char *file_path, size_t file_size)
{
	int fd = open(file_path, O_RDONLY);
	if (fd < 0) {
		printf("Could not open file: %s\n", file_path);
		exit(2);
	}

	struct stat statbuf;
	int	    err = fstat(fd, &statbuf);
	if (err < 0) {
		printf("Could not stat file: %s\n", file_path);
		exit(2);
	}

	if (file_size > statbuf.st_size) {
		printf("File size mismatch, need %lu byte, but file only has %lu bytes\n", file_size, statbuf.st_size);
		exit(2);
	}

	uint8_t *in_ptr = (uint8_t *)mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
	if (in_ptr == MAP_FAILED) {
		printf("Mapping failed\n");
		exit(2);
	}
	close(fd);

	uint8_t *out_ptr = (uint8_t *)ptr;

	for (size_t i = 0; i < file_size; i++) {
		out_ptr[i] = in_ptr[i];
		if (i % 64 == 63 || i == file_size - 1) {
			_mm_clflush((void *)out_ptr);
			_mm_mfence();
		}
	}

	err = munmap(in_ptr, file_size);
	if (err != 0) {
		printf("Unmapping Failed\n");
		exit(2);
	}
}

int main(int argc, char *argv[])
{
	int opt;

	overwrite_info_t oi;
	enum { FILE_OVERWRITE,
	       INODE_OVERWRITE,
	} operation = FILE_OVERWRITE;

	init_overwrite_info(&oi);

	while ((opt = getopt(argc, argv, "f:e:c:r:l:i:o:d:b:")) != -1) {
		switch (opt) {
		case 'f':
			oi.file_path = optarg;
			break;
		case 'e':
			oi.file_1_path = optarg;
			break;
		case 'c':
			oi.covert_data_file_path = optarg;
			break;
		case 'r':
			if (0 == strcmp(optarg, "sender")) {
				oi.role = sender;
			} else if (0 == strcmp(optarg, "receiver")) {
				oi.role = receiver;
			} else {
				usage();
				exit(1);
			}
			break;
		case 'l':
			oi.cycle_ddl = strtoul(optarg, NULL, 0);
			break;
		case 'i':
			oi.iters = strtoul(optarg, NULL, 0);
			break;
		case 'o':
			if (0 == strcmp(optarg, "file")) {
				operation = FILE_OVERWRITE;
			} else if (0 == strcmp(optarg, "inode")) {
				operation = INODE_OVERWRITE;
			} else {
				usage();
				exit(1);
			}
			break;
		case 'd':
			oi.delay_cycle = strtoul(optarg, NULL, 0);
			break;
		case 'b':
			oi.delay_per_iter = strtoul(optarg, NULL, 0);
			break;
		default:
			usage();
			exit(1);
			break;
		}
	}

	if (oi.file_path == NULL) {
		printf("ERROR: missing argument file_path\n");
		usage();
		exit(1);
	}

	if (oi.role == sender && oi.file_1_path == NULL) {
		printf("ERROR: missing argument file_1_path\n");
		usage();
		exit(1);
	}

	if (oi.covert_data_file_path == NULL) {
		printf("ERROR: missing argument file_path\n");
		usage();
		exit(1);
	}

	oi.covert_data = file_mmap(oi.covert_data_file_path, &oi.covert_data_file_size);

	print_overwrite_info(&oi);

	switch (operation) {
	case FILE_OVERWRITE:
		file_overwrite(&oi);
		break;
	case INODE_OVERWRITE:
		inode_overwrite(&oi);
		break;
	default:
		break;
	}

	file_unmmap(oi.covert_data, oi.covert_data_file_size);

	return 0;
}
