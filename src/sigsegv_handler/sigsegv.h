#pragma once
#include <stdbool.h>
#include <stddef.h>

void *init_sigsegv(void *dsm, size_t nb_page, bool is_owner, int chans[3]);
void clean_sigsegv(void);
