#include "library.h"

#include <assert.h>
#include <stdio.h>

void *dsm;
unsigned int nb_pages;

void *Init_DSM(size_t size, int port)
{
	// memory init
	nb_pages = (size + PAGE_SIZE - 1)/ PAGE_SIZE;
	dsm = mmap(0, nb_pages * PAGE_SIZE, 
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (dsm == MAP_FAILED) {
		perror("map allocation failed");
		return NULL;
	}

	// init internal data
	page_info.id = -1;
	INIT_LIST_HEAD(&page_info.list);
	for (int i = 0; i<nb_pages; i++) {
		struct page *p = malloc(sizeof(struct page));
		if (!p) {
			perror("strcut page init failed");
			goto error_exit;
		}
		init_page(p, (ssize_t) (dsm + i*PAGE_SIZE));
		list_add(&p->list, &page_info.list);
	}
	nodes.host = NULL;
	INIT_LIST_HEAD(&nodes.list);

	start_server();

	set_sigaction_handler();
	return dsm;

	error_exit :
		munmap(dsm, nb_pages * PAGE_SIZE);
		free_page_info();
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