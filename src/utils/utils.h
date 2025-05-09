#pragma once

#include "utils/counter_cond_var.h"
#include "network/utils/node_id.h"
#include "utils/list.h"
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define PAGE_SIZE 4096

extern struct node_list node_list;
extern unsigned int nb_nodees;
extern void *dsm;
extern unsigned int nb_pages;
// a mutex used to protect the globals above
extern pthread_mutex_t umtx;

extern const struct node_id EMPTY_NODE;
extern struct node_id me;

struct node_list {
	struct node_id node;
	struct list_head nlist;
};

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

extern struct node_list *add_to_nodes(struct node_list *list, const char *host,
				      const int port);

extern struct node_list *remove_node(struct node_list *list,
				     struct node_id *node);

extern void free_nodes(struct node_list *list);

extern size_t get_page_index(void *adr);
