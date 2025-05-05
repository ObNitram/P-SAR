#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/// @brief Helping structure for waiting on a counter.
/// @details Contains a mutex, a condition and a counter that need to be zero to unlock the condition.
/// 		 Warning: the count is not atomic and need to be protect by the lock otherwise it is an undefined behaviour
struct counter_cond_var {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	int count;
	bool ready;
	bool signal;
};

/// @brief Staticaly initiate a variable condition and set the counter to zero.
/// @details Initialize the variable condition with the PTHREAD_MUTEX_INITIALIZER and PTHREAD_COND_INITIALIZER.
///			 The boolean is set to false ready to wait.
#define COUNTER_COND_VAR_INIT                                                 \
	{                                                                     \
		.lock = PTHREAD_MUTEX_INITIALIZER,                            \
		.cond = PTHREAD_COND_INITIALIZER, .count = 0, .ready = false, \
		.signal = false                                               \
	}

static int init_counter(struct counter_cond_var *counter)
{
	pthread_mutex_init(&counter->lock, NULL);
	pthread_cond_init(&counter->cond, NULL);
	counter->count = 0;
	counter->ready = false;
	counter->signal = false;

	return 0;
}

static int reset_counter(struct counter_cond_var *counter)
{
	counter->count = 0;
	counter->ready = false;
	counter->signal = false;

	return 0;
}

/// @brief Wait until the counter is lower or equal to zero.
/// @param counter The counter to work with. The mutex must be unlock.
static int wait_on_counter(struct counter_cond_var *counter, bool islock)
{
	if (!islock)
		pthread_mutex_lock(&counter->lock);

	while (!counter->ready || counter->count != 0) {
		pthread_cond_wait(&counter->cond, &counter->lock);
	}

	if (!islock)
		pthread_mutex_unlock(&counter->lock);
	return 0;
}

/// @brief Set the counter the the specific value.
/// @param counter The counter to increment.
/// @param value The value to set. The value is add to the counter for not losing other modification
static int set_counter(struct counter_cond_var *counter, int value, bool islock)
{
	if (!islock)
		pthread_mutex_lock(&counter->lock);

	if (counter->signal)
		return -1;

	counter->count += value;
	counter->ready = true;

	if (counter->ready && counter->count == 0) {
		counter->signal = true;
		pthread_cond_broadcast(&counter->cond);
	}

	if (!islock)
		pthread_mutex_unlock(&counter->lock);
	return 0;
}

/// @brief Increment the counter.
/// @param counter The counter to increment.
static int incr_counter(struct counter_cond_var *counter, bool islock)
{
	if (!islock)
		pthread_mutex_lock(&counter->lock);

	if (counter->signal)
		return -1;

	counter->count++;

	if (!islock)
		pthread_mutex_unlock(&counter->lock);
	return 0;
}

/// @brief Decrement the counter and notify if the count reach zero.
/// @param counter The counter to decrement.
static int decr_counter(struct counter_cond_var *counter, bool islock)
{
	if (!islock)
		pthread_mutex_lock(&counter->lock);

	if (counter->signal)
		return -1;

	counter->count--;

	if (counter->ready && counter->count == 0) {
		counter->signal = true;
		pthread_cond_broadcast(&counter->cond);
	}

	if (!islock)
		pthread_mutex_unlock(&counter->lock);
	return 0;
}

/// @brief Decrement the counter and notify if the count reach zero.
/// @param counter The counter to decrement.
static int get_counter(struct counter_cond_var *counter, bool islock)
{
	if (!islock)
		pthread_mutex_lock(&counter->lock);

	int ret = counter->count;

	if (!islock)
		pthread_mutex_unlock(&counter->lock);
	return ret;
}