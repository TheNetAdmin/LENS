#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <x86intrin.h>

int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Error: expecting only one argument\n");
		exit(1);
	}

	char *file_path = argv[1];
	printf("Opening file: %s\n", file_path);

	int fd = open(file_path, O_RDONLY);
	if (fd < 0) {
		exit(2);
	}

	struct stat statbuf;
	int err = fstat(fd, &statbuf);
	if (err < 0) {
		exit(2);
	}

	printf("file size: %lu\n", statbuf.st_size);
	
	uint8_t *ptr = (uint8_t *)mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);

	close(fd);

	printf("Data: ");
	for (int i = 0; i < 8; i++) {
		printf("0x%02x ", ptr[i]);
		_mm_clflush(&ptr[i]);
	}
	_mm_mfence();
	printf("\n");

	munmap(ptr, statbuf.st_size);

	return 0;
}
