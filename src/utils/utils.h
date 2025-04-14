#pragma once

#include "../network/message.h"
#include "../network/network.h"
#include <string.h>
#include <stdlib.h>

#define PAGE_SIZE 4096

struct node_list {
	struct node_id node;
	struct list_head nlist;
};

extern struct node_list node_list;
extern unsigned int nb_nodees;
extern void *dsm;
extern unsigned int nb_pages;

extern const struct node_id EMPTY_NODE;
extern struct node_id me;

/// @brief Represents the type of message.
/// @details This enum defines the available message types.
enum message_type {
	ASK_LOCK,
	GET_LOCK,
	UNLOCK,
	JOIN_DSM,
	INFO_DSM,
	ASK_PAGE,
	RECV_PAGE,
	NUMBER_OF_MSG_TYPE // keep this value to the end it indicate the number of message type in the app
};

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

bool node_equal(const struct node_id *node1, const struct node_id *node2);

void node_copy(struct node_id *dst, struct node_id *src);

extern void init_nodes(struct node_list *list);

extern struct node_list *add_to_nodes(struct node_list *list, const char *host,
				      const int port);

extern void free_nodes(struct node_list *list);

extern size_t get_page_index(void *adr);

extern bool node_equal(const struct node_id *node1, const struct node_id *node2);

extern void node_copy(struct node_id *dst, struct node_id *src);
