#pragma once

#include "../network/message.h"

extern struct node_id *page_owners;

extern void set_new_owner(size_t page_id, struct node_id *new_owner);

extern void sync_page(size_t index);

extern void init_data_transfer(unsigned int nb_pages, struct node_id *owners);

extern void clean_data_transfer();

extern void get_page_owners(void *dst);