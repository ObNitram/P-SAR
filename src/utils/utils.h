#pragma once

#include "utils/counter_cond_var.h"
#include "network/utils/node_id.h"
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

/// @brief Represents the type of message.
/// @details This enum defines the available message types.
enum message_type {
	//network module
	NETWORK_JOIN,
	NETWORK_ACK_JOIN,
	NETWORK_NEW_NODE,
	NETWORK_ACK_NEW_NODE,
	NETWORK_JOIN_SUCCESS,
	//lock module
	ASK_LOCK,
	GET_LOCK,
	UNLOCK,
	JOIN_DSM,
	INFO_DSM,
	NEW_NODE,
	ASK_PAGE,
	RECV_PAGE,
	RECV_PAGE_LEAVE,
	DT_LEAVE,
	ACK_DT_LEAVE,
	ACK_RECV_PAGE,
	INVALIDATION,
	SEND_STATE,
	DELEGATE,
	DELEGATE_ACK,
	REQUEST_CS,
	GET_CS,
	ACK_NODE,
	NEW_ROOT_CS,
	RESET_CS,
	ACK_CS,
	LEAVE_CS,
	NUMBER_OF_MSG_TYPE // keep this value to the end it indicate the number of message type in the app
};
extern char *mess_type_str[NUMBER_OF_MSG_TYPE];

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

extern struct node_list *add_to_nodes(struct node_list *list, const char *host,
				      const int port);

extern struct node_list *remove_node(struct node_list *list,
				     struct node_id *node);

extern void free_nodes(struct node_list *list);

extern size_t get_page_index(void *adr);

extern bool node_equal(const struct node_id *node1,
		       const struct node_id *node2);

void node_copy(struct node_id *dst, const struct node_id *src);
