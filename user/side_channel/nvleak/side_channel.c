#include "chasing.h"
#include "utils.h"
#include <assert.h>
#include <fcntl.h>
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <x86intrin.h>
#include <sys/time.h>
#include <time.h>

typedef struct {
	uint64_t cycles[256];
	uint64_t cycle_beg;
	uint64_t cycle_end;
} iter_result_t;

typedef struct {
	/* General info */
	char *	 buf;		 /* Hard code */
	uint64_t region_size;	 /* Argument  */
	uint64_t region_skip;	 /* Deduct    */
	uint64_t block_size;	 /* Argument  */
	uint64_t repeat;	 /* Argument  */
	uint64_t count;		 /* Hard code */
	uint64_t iter_cycle_ddl; /* Argument  */

	/* Pointer chasing info */
	uint64_t  chasing_func_index; /* Deduct    */
	uint64_t  strided_size;	      /* Argument  */
	uint64_t  region_align;	      /* Argument  */
	uint64_t *cindex;	      /* Hard code */
	uint64_t *timing;	      /* Hard code */

	/* Result */
	uint64_t cycle_global_beg; /* Generated */
} chasing_info_t;

typedef enum { PROBE, SCRATCH, PROBE_SCRATCH, PROBE_LIB } role_t;

typedef struct {
	role_t		role;
	char *		file_path;
	size_t		file_size;
	char *          lib_path;
	size_t          lib_size;
	size_t		iters;
	size_t		warm_up_iters;
	uint8_t *	buf;
	uint8_t *	lib_data;
	uint8_t *	scratch_buf;
	size_t		scratch_at_iter;
	size_t		scratch_set_idx;
	size_t		scratch_size;
	size_t		scratch_op;
	size_t          cache_set_beg;
	long            probe_set_idx;
	long probe_lib_page;
	long probe_lib_page_offset;
	long probe_sleep;
	iter_result_t * ir;
	chasing_info_t *ci;
} side_channel_info_t;

static unsigned long long rdtscp(void)
{
	unsigned int	   aux;
	unsigned long long ret = __rdtscp(&aux);
	asm volatile("mfence\n");

	return ret;
}

static inline uint64_t wait_until_ddl(uint64_t cycle_beg, uint64_t cycle_end, uint64_t cycle_ddl)
{
	unsigned long long cycle_cur;

	/* Target ddl */
	uint64_t cycle_tgt = cycle_beg + cycle_ddl;

	if (cycle_end > cycle_tgt) {
		printf("WARNING: iter_cycle_end [%lu] exceeds iter_cycle_ddl [%lu], skipping wait\n",
		       cycle_end,
		       cycle_tgt);
		return rdtscp();
	}

	do {
		/* 1024 cycle granularity */
		for (int i = 0; i < 1024 / 16; i++) {
			asm volatile("nop\n nop\n nop\n nop\n "
				     "nop\n nop\n nop\n nop\n "
				     "nop\n nop\n nop\n nop\n "
				     "nop\n nop\n nop\n nop\n ");
		}
		cycle_cur = rdtscp();
	} while (cycle_cur < cycle_tgt);

	return rdtscp();
}

static void store_chasing(unsigned set_idx, chasing_info_t *ci)
{
	char *set_addr = ci->buf + set_idx * 4096;
	_mm_mfence();
	chasing_func_list[ci->chasing_func_index].st_func(set_addr,
							  ci->region_size,
							  ci->strided_size,
							  ci->region_skip,
							  ci->count,
							  ci->repeat,
							  ci->cindex,
							  ci->timing);
	_mm_mfence();
}

/* 
 * Use strided pointer chasing to probe one L2 set. This probing will also fill
 * the target set with attacker's pages, thus serving as flushing a target L2
 * set.
 * 
 * Note using multiple probe_l2_set() function will also flush the L1 buffer
 * entirely (16KiB / 256B = 64 entries), so no need to explicitly call
 * flush_l1() if many probe_l2_set() is called.
 */
void probe_l2_set(unsigned set_idx, chasing_info_t *ci, iter_result_t *ir)
{
	char *set_addr = ci->buf + set_idx * 4096;
	_mm_mfence();
	chasing_func_list[ci->chasing_func_index].ld_func(set_addr,
							  ci->region_size,
							  ci->strided_size,
							  ci->region_skip,
							  ci->count,
							  ci->repeat,
							  ci->cindex,
							  ci->timing + ci->repeat * 2);
	_mm_mfence();

	/* Calculate avg cycle */
	uint64_t  avg_cycle = 0;
	uint64_t *timing    = ci->timing + ci->repeat * 2;
	for (uint64_t i = 0; i < ci->repeat; i++) {
		avg_cycle += timing[i + 1];
	}
	ir->cycles[set_idx] = avg_cycle / ci->repeat;
}

void flush_l2_set(unsigned set_idx, chasing_info_t *ci, iter_result_t *ir)
{
	char *set_addr = ci->buf + (set_idx % 256) * 4096;
	
	_mm_mfence();
	chasing_func_list[ci->chasing_func_index].ld_no_timing_func(
		set_addr,
		ci->region_size,
		ci->strided_size,
		ci->region_skip,
		ci->count,
		ci->repeat,
		ci->cindex,
		NULL
	);
	_mm_mfence();
}

uint64_t flush_l1(uint8_t *buf)
{
	uint64_t tmp = 0;
	for (int i = 0; i < 16384; i += 256) {
		tmp += (uint64_t)buf[i];
		// buf[i] = tmp;
		_mm_clflush(&(buf[i]));
	}
	_mm_mfence();
	return tmp;
}

uint64_t flush_l1_double(uint8_t *buf)
{
	uint64_t tmp = 0;
	for (int i = 0; i < 32768; i += 256) {
		tmp += (uint64_t)buf[i];
		// buf[i] = tmp;
		_mm_clflush(&(buf[i]));
	}
	_mm_mfence();
	return tmp;
}

#define SET_STRIDE (1 * 1024 * 1024)

static uint8_t inline scratch(side_channel_info_t *si, size_t curr_iter)
{
	uint8_t tmp;
	_mm_mfence();
	switch (si->scratch_op) {
	case 5: /* Single set, stride size */
		if (curr_iter >= si->scratch_at_iter) {
			size_t	 scratch_set  = si->scratch_set_idx;
			uint8_t *addr	      = si->scratch_buf + scratch_set * 4096;
			size_t	 scratch_size = 0;
			if ((curr_iter - si->scratch_at_iter) % 2 == 0)
				scratch_size = (curr_iter - si->scratch_at_iter) / 2;
			for (size_t i = 0; i < scratch_size; i++) {
				uint8_t *curr_addr = addr + i * SET_STRIDE;
				tmp += curr_addr[0];
				_mm_clflush(curr_addr);
			}
		}
		break;
	case 4: /* Single set, fixed size */
		if (curr_iter >= si->scratch_at_iter) {
			size_t	 scratch_set  = si->scratch_set_idx;
			uint8_t *addr	      = si->scratch_buf + scratch_set * 4096;
			size_t	 scratch_size = si->scratch_size;
			for (size_t i = 0; i < scratch_size; i++) {
				uint8_t *curr_addr = addr + i * SET_STRIDE;
				tmp += curr_addr[0];
				_mm_clflush(curr_addr);
			}
		}
		break;
	case 3: /* Single set, sequential size */
		if (curr_iter >= si->scratch_at_iter) {
			size_t	 scratch_set  = si->scratch_set_idx;
			uint8_t *addr	      = si->scratch_buf + scratch_set * 4096;
			size_t	 scratch_size = curr_iter - si->scratch_at_iter;
			for (size_t i = 0; i < scratch_size; i++) {
				uint8_t *curr_addr = addr + i * SET_STRIDE;
				tmp += curr_addr[0];
				_mm_clflush(curr_addr);
			}
		}
		break;
	case 2: /* Double-sequential scratch */
		if (curr_iter >= si->scratch_at_iter) {
			size_t	 scratch_set  = curr_iter - si->scratch_at_iter;
			uint8_t *addr	      = si->scratch_buf + scratch_set * 4096;
			size_t	 scratch_size = scratch_set % 16;
			for (size_t i = 0; i < scratch_size; i++) {
				uint8_t *curr_addr = addr + i * SET_STRIDE;
				tmp += curr_addr[0];
				_mm_clflush(curr_addr);
			}
		}
		break;
	case 1: /* Sequential scratch */
		if (curr_iter >= si->scratch_at_iter) {
			size_t	 scratch_set = curr_iter - si->scratch_at_iter;
			uint8_t *addr	     = si->scratch_buf + scratch_set * 4096;
			for (size_t i = 0; i < si->scratch_size; i++) {
				uint8_t *curr_addr = addr + i * SET_STRIDE;
				tmp += curr_addr[0];
				_mm_clflush(curr_addr);
			}
		}
		break;
	case 0: /* Static scratch */
	default:
		if (curr_iter == si->scratch_at_iter) {
			uint8_t *addr = si->scratch_buf + si->scratch_set_idx * 4096;
			for (size_t i = 0; i < si->scratch_size; i++) {
				uint8_t *curr_addr = addr + i * SET_STRIDE;
				tmp += curr_addr[0];
			}
		}
		break;
	}
	_mm_mfence();
	return tmp;
}

static void side_channel_iterative(side_channel_info_t *si)
{
	iter_result_t *ir;

	/* Store chasing */
	for (unsigned seti = 0; seti < 256; seti++) {
		store_chasing(seti, si->ci);
	}

	for (size_t i = 0; i < si->warm_up_iters; i++) {
		ir = &si->ir[i];
		/* flush_l1(si->buf); */
		for (unsigned seti = 0; seti < 256; seti++) {
			probe_l2_set(seti, si->ci, ir);
		}
	}

	printf("Start side channel probing\n");
	// fflush(stdout);

	for (size_t i = 0; i < si->iters; i++) {
		if (si->role == PROBE_SCRATCH) {
			scratch(si, i);
		}
		ir = &si->ir[i];
		if (si->probe_set_idx < 0) {
			/* flush_l1(si->buf); */
			ir->cycle_beg = rdtscp();
			for (unsigned seti = 0; seti < 256; seti++) {
				probe_l2_set(seti, si->ci, ir);
			}
		} else {
			flush_l1(si->buf);
			ir->cycle_beg = rdtscp();
			probe_l2_set(si->probe_set_idx, si->ci, ir);
		}
		ir->cycle_end = rdtscp();
		wait_until_ddl(ir->cycle_beg, ir->cycle_end, si->ci->iter_cycle_ddl);
	}
}

static uint8_t side_channel_shared_lib(side_channel_info_t *si)
{
	int num_lib_pages = (si->lib_size + 4095) / 4096;
	size_t *cycles = (size_t *)malloc(sizeof(size_t) * num_lib_pages);
	unsigned long long cycle_beg, cycle_end;
	unsigned long long cycle_iter_beg, cycle_iter_end;
	uint8_t tmp = 0;

	for (size_t iter = 0; iter < si->iters; iter ++) {
		// for (int curr_page = 0; curr_page < num_lib_pages; curr_page ++) {
		// 	_mm_clflush(&si->lib_data[curr_page * 4096]);
		// }
		_mm_mfence();
		cycle_iter_beg = rdtscp();

		si->ci->repeat = 1;
		if (si->probe_lib_page < 0) {
			for (int curr_page = 0; curr_page < num_lib_pages; curr_page ++) {
				_mm_mfence();
				cycle_beg = rdtscp();

				tmp += si->lib_data[curr_page * 4096];

				_mm_mfence();
				cycle_end = rdtscp();

				cycles[curr_page] = cycle_end - cycle_beg;
			}
			// for (int curr_page = 0; curr_page < num_lib_pages; curr_page ++) {
			// 	// _mm_clflush(&si->lib_data[curr_page * 4096]);
			// 	for (int cli = 0; cli * 64 < 4096; cli += 1)
			// 		_mm_clflush(&si->lib_data[curr_page * 4096 + cli * 64]);
			// 	flush_l2_set(curr_page + si->cache_set_beg, si->ci, &si->ir[iter]);
			// }
		} else {
			int curr_page = (int)si->probe_lib_page;

			_mm_mfence();
			cycle_beg = rdtscp();

			tmp += si->lib_data[curr_page * 4096 + si->probe_lib_page_offset];

			_mm_mfence();
			cycle_end = rdtscp();

			cycles[curr_page] = cycle_end - cycle_beg;

			_mm_clflush(&si->lib_data[curr_page * 4096 + si->probe_lib_page_offset]);
			// for (int cli = 0; cli * 64 < 4096; cli += 1)
			// 	_mm_clflush(&si->lib_data[curr_page * 4096 + cli * 64]);
			// flush_l2_set(curr_page + si->cache_set_beg, si->ci, &si->ir[iter]);
		}
		_mm_mfence();
		flush_l1(si->buf);

		cycle_iter_end = rdtscp();

		printf("iter=%lu, iter_cycle=%llu, cycles=", iter, cycle_iter_end - cycle_iter_beg);
		if (si->probe_lib_page < 0) {
			printf("[");
			for (int curr_page = 0; curr_page < num_lib_pages; curr_page ++) {
				printf("%4lu, ", cycles[curr_page]);
			}
			printf("]\n");
		} else {
			int curr_page = (int)si->probe_lib_page;
			printf("%4lu", cycles[curr_page]);
			printf(", page=%d\n", curr_page);
		}
		// fflush(stdout);

		if (si->probe_sleep > 0)
			sleep((unsigned int)si->probe_sleep);
	}

	free(cycles);
	return tmp;
}

void side_channel(side_channel_info_t *si)
{
	switch(si->role) {
		case PROBE:
		case SCRATCH:
		case PROBE_SCRATCH:
			side_channel_iterative(si);
			break;
		case PROBE_LIB:
			side_channel_shared_lib(si);
			break;
		default:
			printf("Error: unrecognized role %d\n", si->role);
			exit(1);
			break;
	}
}

void print_results(side_channel_info_t *si)
{
	printf("role=%u, "
	       "file_path=%s, "
	       "file_size=%lu, "
	       "lib_path=%s, "
	       "lib_size=%lu, "
	       "scratch_op=%lu, "
	       "scratch_at_iter=%lu, "
	       "scratch_set_idx=%lu, "
	       "scratch_size=%lu, "
	       "iters=%lu, "
	       "cache_set_beg=%lu, "
	       "probe_set_index=%ld, "
	       "probe_lib_page=%ld, "
	       "probe_lib_page_offset=%ld "
	       "probe_sleep=%ld"
	       "\n\n",
	       si->role,
	       si->file_path,
	       si->file_size,
	       si->lib_path,
	       si->lib_size,
	       si->scratch_op,
	       si->scratch_at_iter,
	       si->scratch_set_idx,
	       si->scratch_size,
	       si->iters,
	       si->cache_set_beg,
	       si->probe_set_idx,
	       si->probe_lib_page,
	       si->probe_lib_page_offset,
	       si->probe_sleep
	);
	PRINT_INFO(si->ci);
	printf("\n");

	if (si->role == PROBE_LIB) {
		return;
	}

	iter_result_t *ir;
	for (size_t i = 0; i < si->iters; i++) {
		ir = &si->ir[i];
		printf("per_cacheline_lat_iter[%5ld] = [", i);
		if (si->probe_set_idx < 0) {
			for (unsigned seti = 0; seti < 256; seti++) {
				printf("s%03u=%5lu", seti, ir->cycles[seti] * 64 / si->ci->region_size);
				if (seti != 255) {
					printf(", ");
				}
			}

		} else {
			unsigned seti = (unsigned)si->probe_set_idx;
			printf("s%03u=%5lu", seti, ir->cycles[seti] * 64 / si->ci->region_size);
		}
		printf("]");
		printf(", cycle_iter=%lu, cycle_beg=%lu, cycle_end=%lu",
		       ir->cycle_end - ir->cycle_beg,
		       ir->cycle_beg,
		       ir->cycle_end);
		printf("\n");
	}
}

static void usage(void)
{
	printf("Usage: side_channel \n"
	       "       -f pmem_file_path \n"
	       "       -l lib_file_path\n"
	       "       -i iterations\n"
	       "       -s scratch_op\n"
	       "       -o side_channel|scratch\n"
	       "       -c cache_set_beg\n"
	       "       -p probe_set_index (0 to 255)\n"
	);
}

uint8_t *file_mmap(char *file_path, size_t *file_size, int fd_flags, int mmap_flags)
{
	printf("Opening file %s\n", file_path);
	int fd = open(file_path, fd_flags);
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
		printf("Warning: statbuf.st_size==0, force using file_size=7GiB\n");
		*file_size = 7ULL * 1024 * 1024 * 1024;
	}

	uint8_t *ptr = (uint8_t *)mmap(NULL, *file_size, mmap_flags, MAP_SHARED, fd, 0);
	if (ptr == MAP_FAILED) {
		printf("Mapping failed\n");
		exit(2);
	}
	close(fd);

	printf("pre-heat file, tmp=");
	uint8_t tmp = 0;
	for (size_t i = 0; i < *file_size; i += 4096) {
		tmp += ptr[i];
	}
	printf("%u, ptr_end=%px\n", tmp, (ptr + (*file_size) - 4096));

	return ptr;
}

void file_unmmap(uint8_t *ptr, size_t file_size)
{
	int err = munmap(ptr, file_size);
	if (err != 0) {
		printf("Unmapping Failed\n");
		exit(2);
	}
}

static void init_data_structures(side_channel_info_t *si, chasing_info_t *ci)
{
	/* mmap files */
	si->buf = file_mmap(si->file_path, &si->file_size, O_RDWR, PROT_READ | PROT_WRITE);
	if (si->role == PROBE_LIB) {
		si->lib_data = file_mmap(si->lib_path, &si->lib_size, O_RDONLY, PROT_READ);
	} else {
		si->lib_data = NULL;
	}

	/* chasing info */
	si->warm_up_iters      = 3;
	ci->block_size	       = 64;
	ci->region_size	       = 16 * 64;
	ci->region_align       = 4096;
	ci->strided_size       = 1 * 1024 * 1024;
	ci->chasing_func_index = chasing_find_func(ci->block_size);
	ci->buf		       = si->buf + 1 * 1024 * 1024 * 1024;
	ci->count	       = 1;
	ci->repeat	       = 2;

	/* 750 for pointer chasing, 600 for recording results */
	int num_sets = 256;
	if (si->probe_set_idx >= 0)
		num_sets = 1;
	const uint64_t base_ddl = 750 + 600;
	ci->iter_cycle_ddl	= ((base_ddl) * ci->region_size / 64) * ci->repeat * num_sets;
	ci->region_skip		= (ci->region_size / ci->block_size) * ci->strided_size;

	ci->cindex = (uint64_t *)calloc((ci->region_size / ci->block_size), sizeof(uint64_t));
	ci->timing = (uint64_t *)calloc(ci->repeat * 4, sizeof(uint64_t));
	assert(ci->cindex);
	assert(ci->timing);

	init_chasing_index(ci->cindex, ci->region_size / ci->block_size);

	si->ci = ci;

	/* Results */
	si->ir = (iter_result_t *)calloc(si->iters, sizeof(iter_result_t));

	/* Scratch */
	si->scratch_buf	    = si->buf + 2 * 16 * 1024 * 1024;
	si->scratch_at_iter = 0;
	si->scratch_set_idx = 0;
	si->scratch_size    = 2;
}

static void cleanup_data_structures(side_channel_info_t *si, chasing_info_t *ci)
{
	file_unmmap(si->file_path, si->file_size);
	if (si->lib_data != NULL) {
		file_unmmap(si->lib_path, si->lib_size);
	}
	free(si->ir);
}

int main(int argc, char *argv[])
{
	int		    opt;
	side_channel_info_t si;
	chasing_info_t	    ci;

	si.file_path	 = NULL;
	si.lib_path	 = NULL;
	si.iters	 = 0;
	si.role		 = PROBE_SCRATCH;
	si.scratch_op	 = 0;
	si.cache_set_beg = 0;
	si.probe_set_idx = -1;
	si.probe_lib_page= -1;
	si.probe_sleep = 1;
	si.probe_lib_page_offset = 0;
	while ((opt = getopt(argc, argv, "f:i:o:s:l:c:p:t:e:r:")) != -1) {
		switch (opt) {
		case 'f':
			si.file_path = optarg;
			break;
		case 'l':
			si.lib_path = optarg;
			break;
		case 'i':
			si.iters = strtoul(optarg, NULL, 0);
			break;
		case 'o':
			if (0 == strcmp(optarg, "probe")) {
				si.role = PROBE;
			} else if (0 == strcmp(optarg, "scratch")) {
				si.role = SCRATCH;
			} else if (0 == strcmp(optarg, "probe_scratch")) {
				si.role = PROBE_SCRATCH;
			} else if (0 == strcmp(optarg, "probe_lib")) {
				si.role = PROBE_LIB;
			} else {
				usage();
				exit(1);
			}
			break;
		case 's':
			si.scratch_op = strtoul(optarg, NULL, 0);
			break;
		case 'c':
			si.cache_set_beg = strtoul(optarg, NULL, 0);
			break;
		case 'p':
			si.probe_set_idx = strtol(optarg, NULL, 0);
			break;
		case 't':
			si.probe_lib_page = strtol(optarg, NULL, 0);
			break;
		case 'r':
			si.probe_lib_page_offset = strtol(optarg, NULL, 0);
			break;
		case 'e':
			si.probe_sleep = strtol(optarg, NULL, 0);
			break;
		default:
			usage();
			exit(1);
			break;
		}
	}

	if (si.file_path == NULL) {
		printf("Missing argument pmem_file_path\n");
		usage();
		exit(1);
	}

	if (si.iters == 0) {
		printf("Missing argument iters, or it cannot be 0\n");
		usage();
		exit(1);
	}

	if (si.role == PROBE_LIB) {
		if (si.lib_path == NULL) {
			printf("Missing argument lib_path\n");
			usage();
			exit(1);
		}
	}

	if (si.probe_set_idx != -1) {
		if (si.probe_set_idx < 0 || si.probe_set_idx > 255) {
			printf("ERROR: Probe set index [%ld] is invalid.\n", si.probe_set_idx);
			usage();
			exit(1);
		}
	}

	if (si.probe_lib_page_offset < 0 || si.probe_lib_page_offset >= 4096) {
		printf("ERROR: Probe lib page offset [%ld] out of range.\n", si.probe_lib_page_offset);
		usage();
		exit(1);
	}

	init_data_structures(&si, &ci);

	struct timespec tbeg, tend;

	printf("Running side_channel\n");
	
	clock_gettime(CLOCK_MONOTONIC, &tbeg);
	side_channel(&si);
	clock_gettime(CLOCK_MONOTONIC, &tend);
	
	print_results(&si);

	uint64_t tdur = (tend.tv_sec - tbeg.tv_sec) * 1000000000 + (tend.tv_nsec - tbeg.tv_nsec);
	printf("Total time: %lu ns; %.9f sec\n", tdur, (double)tdur / 1e9);

	cleanup_data_structures(&si, &ci);
	return 0;
}
