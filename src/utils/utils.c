#define DISABLE_LOG

#include "utils.h"
#include "logger.h"

pthread_mutex_t umtx = PTHREAD_MUTEX_INITIALIZER;
void *dsm = NULL;
unsigned int nb_pages = 0;
unsigned int nb_nodees = 0;
struct node_list node_list;

const struct node_id EMPTY_NODE = { .host = "", .port = -1 };
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

bool node_equal(const struct node_id *node1, const struct node_id *node2)
{
	return (node1->port == node2->port) &&
	       (strcmp(node1->host, node2->host) == 0);
}

void node_copy(struct node_id *dst, const struct node_id *src)
{
	memcpy(dst->host, src->host, INET6_ADDRSTRLEN * sizeof(char));
	dst->port = src->port;
}

void broadcast_message(struct message *msg, size_t size)
{
	struct node_list *node = &node_list;
	list_for_each_entry_continue(node, &node_list.nlist, nlist) {
		send_message(&node->node, msg, size);
	}
}

void broadcast_wait_message(struct message *msg, size_t size,
			    struct counter_cond_var *counter)
{
	set_counter(counter, nb_nodees);
	struct node_list *node = &node_list;
	list_for_each_entry_continue(node, &node_list.nlist, nlist) {
		log_info("send message %p to %s:%d", msg, node->node.host,
			 node->node.port);
		send_message(&node->node, msg, size);
	}
	wait_on_counter(counter);
}