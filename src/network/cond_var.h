#pragma once

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

struct cond_var {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	bool predicate;
};

#define COND_VAR_INIT {.lock = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER, .predicate = false}