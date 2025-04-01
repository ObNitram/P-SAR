#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include "../network/network.h"

enum lock_type { READ, WRITE };

void ask_lock(size_t page_id, enum lock_type lock_type);

void unlock(size_t page_id, enum lock_type lock_type);

void init_core(size_t nb_pages, struct node_id owner);

extern void clean_core(void);

extern int check_core_info_test(void);

extern void *get_core_info(size_t *sz);

extern int node_equal(struct node_id *node1, struct node_id *node2);