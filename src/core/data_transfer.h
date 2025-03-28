#pragma once

#include "../network/network.h"
#include "../network/message.h"

extern struct node_id *page_owners;

extern void sync_page(struct node_id *owner, size_t index);

extern void init_data_transfer(unsigned int nb_pages, struct node_id* owners);

extern void clean_data_transfer();