#pragma once

#include <pthread.h>
#include <stdbool.h>

/// @brief Helping structure for waiting on a predicate.
/// @details Contains a mutex, a condition and a predicate that need to be true to unlock the condition.
/// 		 Warning: the predicate is not atomic and need to be protect by the lock otherwise it is an undefined behaviour
struct cond_var {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	bool predicate;
};

/// @brief Staticaly initiate a variable condition and set the predicate at false.
/// @details Initialize the variable condition with the PTHREAD_MUTEX_INITIALIZER and PTHREAD_COND_INITIALIZER.
///			 The boolean is set to false ready to wait.
///			 Warning : the mutex is not reccurssive if this is needed you need to initialize it yourself.
#define COND_VAR_INIT                                                \
	{                                                            \
		.lock = PTHREAD_MUTEX_INITIALIZER,                   \
		.cond = PTHREAD_COND_INITIALIZER, .predicate = false \
	}
