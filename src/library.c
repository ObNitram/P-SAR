#include "library.h"

static int init_my_node_id() 
{
	char *ip = get_server_ip();
	if (!ip) {
		perror("didn't get server ip");
		return 1;
	}
	memcpy(me.host, ip, INET6_ADDRSTRLEN * sizeof(char));
	me.port = get_server_port();
	free(ip);
	return 0;
}

static void JOIN_DSM_handler(struct message *message) 
{
	size_t sz;

	struct INFO_DSM_message * dsm_info = build_message(&sz);

	send_message(&message->sender, (struct message *)dsm_info, sz);
	add_to_nodes(&node_list, message->sender.host, message->sender.port);
	free_message((struct message *)dsm_info);
}

static void INFO_DSM_handler(struct message *message) 
{
	struct INFO_DSM_message *idsm = (struct INFO_DSM_message *)
										message;
	nb_pages = idsm->nb_pages;

	void *addr = idsm + 1;

	struct node_id *n = (struct node_id *) addr;
	for (unsigned int i = 0; i < idsm->nb_nodes; i++) {
		add_to_nodes(&node_list ,n->host, n->port);
		n++;	
	}
}

static void set_all_handlers(void) 
{
	set_sigaction_handler();
	addHandler(JOIN_DSM, NULL, JOIN_DSM_handler);
	addHandler(INFO_DSM, NULL, INFO_DSM_handler);
}

static void exclude_others(void *adr, size_t s, enum lock_type lock_type, void (*exc_func) (size_t, enum lock_type)) 
{
	size_t start_index = get_page_index(adr);
	size_t end_index = get_page_index(adr + s);
	for (size_t page_id = start_index; page_id <= end_index; page_id++) {
		exc_func(page_id, lock_type);
	}
}

void *Init_DSM(size_t size, int port)
{
	start_server(port);
	set_all_handlers();
	
	// init internal data
	if (init_my_node_id()) return NULL;

	// memory init
	nb_pages = (size + PAGE_SIZE - 1)/ PAGE_SIZE;
	dsm = mmap(0, nb_pages * PAGE_SIZE, 
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (dsm == MAP_FAILED) {
		perror("map allocation failed");
		return NULL;
	}

	init_core(nb_pages, &me);
	init_nodes(&node_list);
	return dsm;
}

void free_DSM() 
{
	munmap(dsm, nb_pages * PAGE_SIZE);
}

void *join_DSM(const char *host, int connect_port, int server_port)
{
	size_t msg_sz = sizeof(struct message);

	start_server(server_port);
	set_all_handlers();

	init_nodes(&node_list);
	struct node_id *nd = &add_to_nodes(&node_list, host, connect_port)->node;
	init_core(nb_pages, nd);

	struct message *mess_joining = malloc(msg_sz);
	mess_joining->message_type = JOIN_DSM;
	send_message(nd, mess_joining, msg_sz);
	
	struct message *dsm_info = wait_message(INFO_DSM, NULL) ;
	
	free_message(mess_joining);
	free_message(dsm_info);

	if (init_my_node_id()) return NULL;

	dsm = mmap(0, nb_pages * PAGE_SIZE, 
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (dsm == MAP_FAILED) {
		perror("map allocation failed");
		stop_server();
		free_nodes(&node_list);
		clean_core();
		return NULL;
	}
	return dsm;
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