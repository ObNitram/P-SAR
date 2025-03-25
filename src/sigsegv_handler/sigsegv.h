#pragma once
#include <stddef.h>

void * init_sigsegv(size_t size);
void exit_sigsegv(void * mem, size_t size);

void lock_memory(void * addr, size_t size);
void unlock_memory(void * addr, size_t size);
