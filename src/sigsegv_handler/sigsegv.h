#pragma once
#include <stdbool.h>
#include <stddef.h>

void * init_sigsegv(void * dsm, size_t size, bool is_owner);
void exit_sigsegv();

void memory_protect(size_t index, int perm);
