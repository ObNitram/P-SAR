#include "library.h"

#include <assert.h>
#include <stdio.h>

void *dsm;
unsigned int nb_pages;

static void set_all_handlers(void) {
	set_sigaction_handler();
	// maybe hadnlers for algo in core
}

static void init_nodes(void) {
	INIT_LIST_HEAD(&node_list.nlist);
	node_list.node.port = -1;
}

static struct node_list *add_to_nodes(const char *host, const int port) {
	struct node_list *ndlst = malloc(sizeof(struct node_list));
	size_t sz = min(strlen(host),INET6_ADDRSTRLEN) ;
	memcpy(ndlst->node.host, host, sizeof(char) * sz);
	ndlst->node.port = port;
	list_add(&ndlst->nlist, &node_list.nlist);
	return ndlst;
}

static void free_nodes(void) {
	struct node_list *n1 = &node_list, *n2;
	list_for_each_entry_safe_continue(n1, n2, &node_list.nlist, nlist) {
		free(n1);
	}
}

static void JOIN_DSM_handler(struct message *message) {
	size_t dsminf_sz = sizeof(struct DSM_INFO_message);
	size_t page_sz = sizeof(struct page);
	size_t nd_sz = sizeof(struct node_id);
	size_t sz = dsminf_sz + nb_pages * page_sz + 
				nb_nodees * nd_sz;

	struct DSM_INFO_message *dsm_info = malloc(sz);
	dsm_info->header.message_type = DSM_INFO;
	dsm_info->nb_nodes = nb_nodees;
	dsm_info->nb_pages = nb_pages;
	
	void *addr = dsm_info + dsminf_sz;
	memcpy(addr, page_info, page_sz * nb_pages);
	addr += page_sz * nb_pages;


	struct node_list *nlist = &node_list;
	list_for_each_entry_continue(nlist, &node_list.nlist, nlist) {
		memcpy(addr, &nlist->node, nd_sz);
		addr += nd_sz;
	}

	send_message(&message->sender, (struct message *)dsm_info, sz);
	free(dsm_info);
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
	
	set_all_handlers();
	start_server(port);

	// init internal data
	init_core_info(nb_pages, NULL);
	init_nodes();

	return dsm;
}

void *join_DSM(const char *host, int connect_port, int server_port)
{
	size_t msg_sz = sizeof(struct message);

	set_all_handlers();
	start_server(server_port);

	init_nodes();
	struct node_id *nd = &add_to_nodes(host, connect_port)->node;

	struct message *mess_joining = malloc(msg_sz);
	mess_joining->message_type = JOIN_DSM;
	send_message(nd, mess_joining, msg_sz);
	
	struct DSM_INFO_message *dsm_info = 
		(struct DSM_INFO_message *)wait_message(DSM_INFO, NULL) ;
	
	free(mess_joining);
	free(dsm_info);

	dsm = mmap(0, nb_pages * PAGE_SIZE, 
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (dsm == MAP_FAILED) {
		perror("map allocation failed");
		stop_server();
		free_nodes();
		clean_core();
		return NULL;
	}
	return dsm;
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