#include "library.h"

#include <assert.h>
#include <stdio.h>

void *dsm;
unsigned int nb_pages;
struct node_list node_list;
unsigned int nb_nodees;
const size_t mask = ~(PAGE_SIZE -1);

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
	size_t ms_sz = sizeof(struct INFO_DSM_message);
	size_t nd_sz = sizeof(struct node_id);
	size_t cr_sz;
	void *core_info = get_core_info(&cr_sz);
	
	// total size of the mess
	size_t sz = ms_sz  + nb_nodees * nd_sz + cr_sz;

	struct INFO_DSM_message *dsm_info = malloc(sz);
	dsm_info->header.message_type = INFO_DSM;
	dsm_info->nb_pages = nb_pages;
	dsm_info->nb_nodes = nb_nodees;
	void *addr = dsm_info + ms_sz;

	// copy of all node_id
	struct node_list *nlist = &node_list;
	unsigned int node_counter = 0;
	list_for_each_entry_continue(nlist, &node_list.nlist, nlist) {
		node_counter++;
		memcpy(addr, &nlist->node, nd_sz);
		addr += nd_sz;
	}
	assert(node_counter == nb_nodees);

	// copy the core info
	memcpy(addr, core_info, cr_sz);
	addr += cr_sz;

	send_message(&message->sender, (struct message *)dsm_info, sz);
	add_to_nodes(message->sender.host, message->sender.port);
	free_message((struct message *)dsm_info);
	free_message(message);
}

static void INFO_DSM_handler(struct message *message) {
	struct INFO_DSM_message *idsm = (struct INFO_DSM_message *)
		message;
	nb_pages = idsm->nb_pages;

	void *addr = (void *) (idsm + 1);

	struct node_id *n = (struct node_id *)addr;
	for (unsigned int i = 0; i < idsm->nb_nodes; i++) {
		add_to_nodes(n->host, n->port);
		nb_nodees++;
		n++;
	}

	init_core(nb_pages, (void *) n);
	free_message(message);
}

static void set_all_handlers(void) {
	set_sigaction_handler();
	addHandler(JOIN_DSM, NULL, JOIN_DSM_handler);
	addHandler(INFO_DSM, NULL, INFO_DSM_handler);
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
	init_core(nb_pages, NULL);
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
	
	struct message *dsm_info = wait_message(INFO_DSM, NULL) ;
	
	free_message(mess_joining);
	free_message(dsm_info);

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

static void get_interval_page(void *adr, size_t s, size_t *start_index, size_t *end_index) {
	size_t addr = (size_t) adr;
	size_t dsmm = (size_t) dsmm;
	*start_index = (size_t) ((addr & mask) - (dsmm & mask));
	*end_index = (size_t) (((addr + s) & mask) - (dsmm & mask));
}

static void exclude_others(void *adr, size_t s, enum lock_type lock_type, void (*exc_func) (size_t, enum lock_type)) {
	size_t start_index;
	size_t end_index;
	get_interval_page(adr, s, &start_index, &end_index);
	for (size_t page_id = start_index; page_id <= end_index; page_id++) {
		exc_func(page_id, lock_type);
	}
}

void lock_read(void *adr, size_t s)
{
	exclude_others(adr, s, READ, ask_lock);
}

void unlock_read(void *adr, size_t s)
{
	exclude_others(adr, s, READ, unlock);

}

void lock_write(void *adr, size_t s)
{
	exclude_others(adr, s, WRITE, ask_lock);
}

void unlock_write(void *adr, size_t s)
{
	exclude_others(adr, s, WRITE, unlock);
}