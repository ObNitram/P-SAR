#pragma once
#include <stdlib.h>
#include "network/message.h"
#include "utils/utils.h"
#include "core/data_transfer.h"

struct INFO_DSM_message {
	struct message header;
	unsigned int nb_pages;
	unsigned int nb_nodes;
};

static struct INFO_DSM_message *build_INFO_DSM_message(size_t *size) 
{
    size_t ms_sz = sizeof(struct INFO_DSM_message);
	size_t nd_sz = sizeof(struct node_id);
	size_t pg_ow = nb_pages * nd_sz;
	
	// total size of the mess
	size_t sz = ms_sz  + nb_nodees * nd_sz + pg_ow;
	*size = sz;

	struct INFO_DSM_message *dsm_info = (struct INFO_DSM_message *) malloc(sz);
	dsm_info->header.message_type = INFO_DSM;
	dsm_info->nb_pages = nb_pages;
	dsm_info->nb_nodes = nb_nodees;
	struct node_id *addr = (struct node_id *) (dsm_info + 1);

	// copy of all node_id
	struct node_list *nlist = &node_list;
	list_for_each_entry_continue(nlist, &node_list.nlist, nlist) {
		memcpy(addr, &nlist->node, nd_sz);
		addr++;
	}

	// copy th page owners
	get_page_owners(addr);

	return dsm_info;
}

struct NEW_NODE_message {
	struct message header;
	struct node_id new_node;
};

static struct NEW_NODE_message *build_NEW_NODE_message(struct node_id *new_node)
{
	struct NEW_NODE_message *msg = 
		(struct NEW_NODE_message *) malloc(sizeof(struct NEW_NODE_message));
	msg->header.message_type = NEW_NODE;
	node_copy(&msg->new_node, new_node);
	return msg;
}