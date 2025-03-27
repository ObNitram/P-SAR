#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include "../network/network.h"

enum lock_type { READ, WRITE };

void ask_lock(size_t page_id, enum lock_type lock_type);

void unlock(size_t page_id, enum lock_type lock_type);

void init_core(size_t nb_pages, void *pages_data);

void clean_core(void);