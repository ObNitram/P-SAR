#pragma once
#include <stdbool.h>
#include <stddef.h>

void * init_sigsegv(void * dsm, size_t size, bool is_owner);
void exit_sigsegv();

void memory_lock(size_t index);
void memory_unlock_read(size_t index);
void memory_unlock_write(size_t index);
void memory_lock_reset(size_t index);
