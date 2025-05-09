#pragma once

#include <stddef.h>

extern void init_memory(int fd1);

extern void exit_memory(void);

extern void memory_protect(size_t index, int perm);

extern void memory_lock(size_t index);

extern void memory_unlock_read(size_t index);

extern void memory_unlock_write(size_t index);

extern void memory_lock_reset(size_t index);