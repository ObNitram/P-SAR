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

static int init_cond(struct cond_var *cond)
{
	if (cond == NULL) {
		// log_error("invalid cond");
		return -1;
	}

	pthread_mutex_init(&cond->lock, NULL);
	pthread_cond_init(&cond->cond, NULL);
	cond->predicate = false;

	return 0;
}

/// @brief Wait until the counter is lower or equal to zero.
/// @param counter The counter to work with. The mutex must be unlock.
static int wait_on_cond(struct cond_var *cond, bool islock)
{
	if (cond == NULL) {
		// log_error("invalid cond");
		return -1;
	}
	if (!islock)
		pthread_mutex_lock(&cond->lock);

	while (!cond->predicate) {
		pthread_cond_wait(&cond->cond, &cond->lock);
	}

	if (!islock)
		pthread_mutex_unlock(&cond->lock);
	return 0;
}

/// @brief Set the cond to true and signal.
/// @param counter The counter to work with. The mutex must be unlock.
static int unlock_cond(struct cond_var *cond, bool islock)
{
	if (cond == NULL) {
		// log_error("invalid cond");
		return -1;
	}
	if (!islock)
		pthread_mutex_lock(&cond->lock);

	// if (cond->predicate)
	// 	log_warning("cond wa already unlock");

	cond->predicate = true;
	pthread_cond_broadcast(&cond->cond);

	if (!islock)
		pthread_mutex_unlock(&cond->lock);
	return 0;
}