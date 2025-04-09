#include "utils.h"

void *dsm;
unsigned int nb_pages;
struct node_list node_list;
unsigned int nb_nodees;

const struct node_id EMPTY_NODE = {"", -1};
struct node_id me;

void init_nodes(struct node_list *list) 
{
	INIT_LIST_HEAD(&list->nlist);
	list->node.port = -1;
}

struct node_list *add_to_nodes(struct node_list *list, const char *host,
							   const int port) 
{
	struct node_list *ndlst = malloc(sizeof(struct node_list));
	size_t sz = min(strlen(host),INET6_ADDRSTRLEN) + 1;
	memcpy(ndlst->node.host, host, sizeof(char) * sz);
	ndlst->node.port = port;
	list_add(&ndlst->nlist, &list->nlist);
	if (list == &node_list) nb_nodees++;
	return ndlst;
}

void free_nodes(struct node_list *list) 
{
	struct node_list *n1 = list, *n2;
	list_for_each_entry_safe_continue(n1, n2, &list->nlist, nlist) {
		free(n1);
	}
}

size_t get_page_index(void *adr) 
{
    size_t addr = (size_t) adr;
    size_t dsm_addr = (size_t) dsm; // Utiliser la variable globale dsm

    size_t index = (addr - dsm_addr) / PAGE_SIZE;

    return index;
}

int node_equal(struct node_id *node1, struct node_id *node2){
    return node1->port == node2->port && strcmp(node1->host, node2->host) == 0;
}

void node_copy(struct node_id* dst, struct node_id *src) {
	memcpy(dst->host, src->host, INET6_ADDRSTRLEN * sizeof(char));
	dst->port = src->port;
}
