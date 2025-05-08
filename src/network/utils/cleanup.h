#pragma once

#include <pthread.h>

#include <stdlib.h>
#include <string.h>

// define helper for cleanup
#define defer(clean_func) __attribute__((cleanup(clean_func)))

/// @brief cleanup function for unlock a mutex at end of a block
static inline void cleanup_mutex_unlock(void *p)
{
	pthread_mutex_t *m = *(pthread_mutex_t **)p;
	if (m) {
		int err = pthread_mutex_unlock(m);
		if (err != 0) {
			// log_error("Erreur unlock mutex: %s\n", strerror(err));
		}
	}
}
//macro for defer unlock of a given mutex
#define defer_unlock_mutex(lock) \
	defer(cleanup_mutex_unlock) pthread_mutex_t *_lock = lock

/// @brief cleanup function for unlock a mutex at end of a block
static inline void cleanup_free(void *p)
{
	void *m = *(void**)p;
	if (m) {
		free(m);
	}
}