#pragma once

#include <pthread.h>
#include <stddef.h>

/// @brief Helping structure for waiting on a counter.
/// @details Contains a mutex, a condition and a counter that need to be zero to unlock the condition.
/// 		 Warning: the count is not atomic and need to be protect by the lock otherwise it is an undefined behaviour
struct counter_cond_var {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	int count;
};

/// @brief Staticaly initiate a variable condition and set the counter to zero.
/// @details Initialize the variable condition with the PTHREAD_MUTEX_INITIALIZER and PTHREAD_COND_INITIALIZER.
///			 The boolean is set to false ready to wait.
#define COUNTER_COND_VAR_INIT                                \
	{                                                    \
		.lock = PTHREAD_MUTEX_INITIALIZER,           \
		.cond = PTHREAD_COND_INITIALIZER, .count = 0 \
	}

/// @brief Wait until the counter is lower or equal to zero.
/// @param counter The counter to work with. The mutex must be unlock.
static void wait_on_counter(struct counter_cond_var *counter)
{
	pthread_mutex_lock(&counter->lock);
	while (counter->count != 0) {
		pthread_cond_wait(&counter->cond, &counter->lock);
	}
	pthread_mutex_unlock(&counter->lock);
}

/// @brief Set the counter the the specific value.
/// @param counter The counter to increment.
/// @param value The value to set. The value is add to the counter for not losing other modification
static void set_counter(struct counter_cond_var *counter, int value)
{
	pthread_mutex_lock(&counter->lock);
	counter->count += value;
	pthread_mutex_unlock(&counter->lock);
}

/// @brief Increment the counter.
/// @param counter The counter to increment.
static void incr_counter(struct counter_cond_var *counter)
{
	pthread_mutex_lock(&counter->lock);
	counter->count++;
	pthread_mutex_unlock(&counter->lock);
}

/// @brief Decrement the counter and notify if the count reach zero.
/// @param counter The counter to decrement.
static void decr_counter(struct counter_cond_var *counter)
{
	pthread_mutex_lock(&counter->lock);
	counter->count--;
	if (counter->count == 0) {
		pthread_cond_signal(&counter->cond);
	}
	pthread_mutex_unlock(&counter->lock);
}