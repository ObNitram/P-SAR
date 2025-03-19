#include "library.h"

#include <assert.h>
#include <stdio.h>


void *Init_DSM(size_t size, int port)
{
	// init memory
	// init internal data
	// init sigaction
	// TODO: InitNode
	return NULL;
}

void *join_DSM(char *host, int connect_port, int server_port)
{
	// TODO: AddNode
	return NULL;
}

void lock_read(void *adr, size_t s)
{
	// TODO: lock_read
}

void unlock_read(void *adr, size_t s)
{
	// TODO: unlock_read
}

void lock_write(void *adr, size_t s)
{
	// TODO: lock_write
}

void unlock_write(void *adr, size_t s)
{
	// TODO: unlock_write
}