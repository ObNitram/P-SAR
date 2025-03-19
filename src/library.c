#include "library.h"

#include <assert.h>
#include <stdio.h>

void *dsm;
unsigned int nb_pages;

static void set_all_handlers() {
	set_sigaction_handler();
	// maybe hadnlers for algo in core
}

static void init_nodes() {
	INIT_LIST_HEAD(&node_list.nlist);
	node_list.node.port = -1;
}

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
	page_info = malloc(nb_pages * sizeof(struct page));
	if (!page_info) goto error_exit;
	for (int i = 0; i<nb_pages; i++) init_page(page_info + i);
	init_nodes();
	
	set_all_handlers();
	start_server(port);

	return dsm;

	error_exit :
		munmap(dsm, nb_pages * PAGE_SIZE);
		free_page_info();
	 	return NULL;
}

void *join_DSM(const char *host, int connect_port, int server_port)
{
	init_nodes();
	set_all_handlers();
	start_server(server_port);

	size_t ndlst_sz = sizeof(struct node_list);
	size_t msg_sz = sizeof(struct message);
	struct node_list *ndlst = malloc(ndlst_sz);
	size_t sz = min(strlen(host),INET6_ADDRSTRLEN) ;
	memcpy(ndlst->node.host, host, sizeof(char) * sz);
	ndlst->node.port = connect_port;
	list_add(&ndlst->nlist, &node_list.nlist);

	struct message *mess_joining = malloc(msg_sz);
	memcpy(&mess_joining->sender, &ndlst->node, sizeof(struct node_id));
	mess_joining->message_type = JOIN_DSM;
	send_message(&ndlst->node, mess_joining, msg_sz);

	struct DSM_INFO_message *dsm_info = 
		(struct DSM_INFO_message *)wait_message(DSM_INFO, NULL) ;
	
	free(dsm_info);
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