#pragma once
#include "network/message.h"
#include "utils/utils.h"
#include <stdlib.h>

struct INFO_DSM_message {
	struct message header;
	unsigned int nb_pages;
	unsigned int nb_nodes;
};

static struct INFO_DSM_message *build_message(size_t *sz_) 
{
    size_t ms_sz = sizeof(struct INFO_DSM_message);
	size_t nd_sz = sizeof(struct node_id);
	size_t pg_ow = nb_pages * nd_sz;
	
	// total size of the mess
	size_t sz = ms_sz  + nb_nodees * nd_sz;
	*sz_ = sz;

	struct INFO_DSM_message *dsm_info = (struct INFO_DSM_message *) malloc(sz);
	dsm_info->header.message_type = INFO_DSM;
	dsm_info->nb_pages = nb_pages;
	dsm_info->nb_nodes = nb_nodees;
	void *addr = dsm_info + 1;

	// copy of all node_id
	struct node_list *nlist = &node_list;
	list_for_each_entry_continue(nlist, &node_list.nlist, nlist) {
		memcpy(addr, &nlist->node, nd_sz);
		addr += nd_sz;
	}
	return dsm_info;
}