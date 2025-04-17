#pragma once

#include "../network/message.h"

enum lock_type { READ, WRITE };

// enum for local status of lock
enum lock_status {
	READING = READ,
	WRITING = WRITE,
	NONE,
};

void ask_lock(size_t page_id, enum lock_type lock_type);

void unlock(size_t page_id, enum lock_type lock_type);

void init_core(size_t nb_pages, struct node_id *have_token);

extern void clean_core(void);

extern int check_core_info_test(void);

extern void *get_core_info(size_t *sz);

enum lock_status get_lock_status(size_t page_id);
