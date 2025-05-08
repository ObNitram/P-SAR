#include "memory.h"
#include "../utils/utils.h"
#include "../utils/logger.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static int lock_status_chan = 0;

void init_memory(int fd1)
{
	lock_status_chan = fd1;
}

// perm: PROT_NONE, PROT_EXEC, PROT_READ, PROT_WRITE
void memory_protect(size_t index, int perm)
{
	int ret = mprotect(dsm + (index * PAGE_SIZE), PAGE_SIZE, perm);
	if (ret == -1) {
		int error = errno;
		if (error == EACCES) {
			fprintf(stderr, "memory_protect: EACCES\n");
		} else if (error == EINVAL) {
			fprintf(stderr, "memory_protect: EINVAL\n");
		} else if (error == ENOMEM) {
			fprintf(stderr, "memory_protect: ENOMEM\n");
		}
		perror("mprotect lock");
		exit(EXIT_FAILURE);
	}
}

void memory_lock(size_t index)
{
	memory_protect(index, PROT_NONE);
}

void memory_unlock_read(size_t index)
{
	memory_protect(index, PROT_READ);
}

void memory_unlock_write(size_t index)
{
	memory_protect(index, PROT_READ | PROT_WRITE);
}

void memory_lock_reset(size_t index)
{
	write(lock_status_chan, &index, sizeof(size_t));
	log_info("written size_t\n");
	int prot;
	log_info("try read int\n");
	read(lock_status_chan, &prot, sizeof(int));
	log_info("read int\n");
	memory_protect(index, prot);
}