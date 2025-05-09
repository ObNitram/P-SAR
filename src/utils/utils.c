#define DISABLE_LOG

#include "utils.h"
#include "logger.h"

pthread_mutex_t umtx = PTHREAD_MUTEX_INITIALIZER;
void *dsm = NULL;
unsigned int nb_pages = 0;
unsigned int nb_nodees = 0;
struct node_list node_list;

struct node_id me;

struct node_list *add_to_nodes(struct node_list *list, const char *host,
			       const int port)
{
	struct node_list *ndlst = malloc(sizeof(struct node_list));
	size_t sz = INET6_ADDRSTRLEN;
	memcpy(ndlst->node.host, host, sizeof(char) * sz);
	ndlst->node.port = port;
	list_add(&ndlst->nlist, &list->nlist);
	if (list == &node_list)
		nb_nodees++;
	return ndlst;
}

struct node_list *remove_node(struct node_list *list, struct node_id *node)
{
	struct node_list *n1 = list, *n2;
	list_for_each_entry_safe_continue(n1, n2, &list->nlist, nlist) {
		if (node_equal(node, &n1->node)) {
			list_del(&n1->nlist);
			if (list == &node_list)
				nb_nodees--;
			return n1;
		}
	}
	return NULL;
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
	size_t addr = (size_t)adr;
	size_t dsm_addr = (size_t)dsm; // Utiliser la variable globale dsm

	size_t index = (addr - dsm_addr) / PAGE_SIZE;

	return index;
}
