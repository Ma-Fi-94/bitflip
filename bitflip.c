#define SIZE 4e9
#define SEED 0

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <time.h>
#include <stdarg.h>
#include <sys/mman.h>

int T0;

// Timestamped printing
void fprintf_ts (FILE* fs, const char *format, ...) {
	fprintf(fs, "[%10d] ", (int) time(NULL) - T0);

	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
}

// Allocate the block of memory
uint32_t* alloc(long size) {
	// mmap instead of malloc, to guarantee page-alignment for mlock
	uint32_t* ptr = (uint32_t*) mmap(NULL,
		size,
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANON, // MAP_ANON: no file, ignore fd
		-1, // fd
		0); // offset

	// Check if allocation was successful
	if ((long) ptr == -1) {
		fprintf_ts(stderr,
			"Couldn't allocate %ld bytes. Error code: %i.\n",
			size, errno);
		exit(-1);
	}

	// Lock so that memory isn't swapped out.
	if (mlock(ptr, size) == -1) {
		fprintf_ts(stderr,
			"Couldn't lock memory at %p of size %ld. Error code: %i.\n",
			ptr, size, errno);
		exit(-1);
	}

	return ptr;
}

// Fill block of memory with pseudorandom numbers
void populate(uint32_t* ptr, long n_uint32) {
	srand(SEED);
	for (long i = 0; i < n_uint32; i++) {
		ptr[i] = rand();
	}
	return;
}

// Check if memory content has stayed identical
bool check(uint32_t* ptr, long n_uint32) {
	srand(SEED);
	for (long i = 0; i < n_uint32; i++) {
		if (ptr[i] != rand()) {
			return false;
		}
	}	
	return true;
}

int main() {
	// We only need this for the timestamps of the output.
	T0 = (int) time(NULL);

	// We need to make sure to allocate complete pages, otherwise we
	// could run into problems after mlock-ing.
	long page_size = sysconf(_SC_PAGESIZE);
	long nb_pages = SIZE / page_size;
	long size_corr = nb_pages * page_size;
	uint32_t* ptr = alloc(size_corr);
	fprintf_ts(stdout,
		"Allocated %ld bytes of memory in %ld pages à %ld bytes.\n",
		size_corr, nb_pages, page_size);

	// We cannot guarantee that the amount of allocated amount is
	// an exact multiple of the size of uint32_t.
	long n_uint32 = size_corr / sizeof(uint32_t);
	fprintf_ts(stdout, "This corresponds to %ld 32-bit integers.\n", n_uint32);

	// Fill memory with pseudorandom number
	populate(ptr, n_uint32);
	fprintf_ts(stdout, "Populated memory.\n");

	// Keep checking for changes hourly
	while (true) {
		if (check(ptr, n_uint32)) {
			fprintf_ts(stdout, "Check passed.\n");
		} else {
			fprintf_ts(stdout, "Check failed, bit-flip.\n");
			return -1;
		}
		sleep(3600);
	}
	
	return 0;
}