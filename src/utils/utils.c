#include "utils.h"

void *dsm;
unsigned int nb_pages;
struct node_list node_list;
unsigned int nb_nodees;


void init_nodes(void) {
	INIT_LIST_HEAD(&node_list.nlist);
	node_list.node.port = -1;
}

struct node_list *add_to_nodes(const char *host, const int port) {
	struct node_list *ndlst = malloc(sizeof(struct node_list));
	size_t sz = min(strlen(host),INET6_ADDRSTRLEN) ;
	memcpy(ndlst->node.host, host, sizeof(char) * sz);
	ndlst->node.port = port;
	list_add(&ndlst->nlist, &node_list.nlist);
	nb_nodees++;
	return ndlst;
}

size_t get_page_index(void *adr) {
    size_t addr = (size_t) adr;
    size_t dsm_addr = (size_t) dsm; // Utiliser la variable globale dsm

    size_t index = (addr - dsm_addr) / PAGE_SIZE;

    return index;
}

void free_nodes(void) {
	struct node_list *n1 = &node_list, *n2;
	list_for_each_entry_safe_continue(n1, n2, &node_list.nlist, nlist) {
		free(n1);
	}
}