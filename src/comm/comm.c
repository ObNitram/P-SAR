#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include "comm.h"
#include "utils/list.h"
#define DISABLE_LOG
#include "utils/logger.h"

//maybe useless....
static const int MAX_EVENT = 10;
static const int MAX_RETRY = 3;
static const int WAIT_RETRY = 100; // in microsecond

/// @brief Structure representing a callback for the event loop.
/// @details Contains the function to execute and the file descriptor that trigger an event.
struct callback {
	void (*callback)(int fd);
	struct list_head list;
	int fd;
};

/// @brief Structure representing the context for this module
/// @details Contains a mutex a list of callback to watch, the main loop thread id, the epoll file descriptor, a file descriptor for terminating, and a state boolean indicate if the module is running.
static struct {
	pthread_mutex_t context_lock;
	struct list_head watcher;
	pthread_t main_loop;
	int epoll;
	int poison;
	bool started;
} context = { .context_lock = PTHREAD_MUTEX_INITIALIZER, .started = false };

static struct callback *create_callback(void (*cb)(int), int fd)
{
	struct callback *new_callback =
		(struct callback *)malloc(sizeof(struct callback));
	if (new_callback == NULL) {
		log_error("malloc failed: %s", strerror(errno));
		return NULL;
	}

	new_callback->callback = cb;
	new_callback->fd = fd;
	INIT_LIST_HEAD(&new_callback->list);
	return new_callback;
}

static void terminate_cb(int fd)
{
	log_debug("terminate main loop");
}

static void remove_callback(struct callback *cb)
{
	list_del(&cb->list);

	if (epoll_ctl(context.epoll, EPOLL_CTL_DEL, cb->fd, NULL) < 0) {
		log_error("epoll_ctl DEL fd=%d: %s", cb->fd, strerror(errno));
	}
	log_debug("close fd=%d", cb->fd);
	close(cb->fd);
	free(cb);
}

static void cleanup_watchers()
{
	// cleanup remaining watchers
	struct callback *cur, *tmp;
	list_for_each_entry_safe(cur, tmp, &context.watcher, list) {
		remove_callback(cur);
	}

	close(context.epoll);
}

static void *main_loop(void *arg)
{
	struct epoll_event events[MAX_EVENT];
	int retry = MAX_RETRY;
	bool running = true;

	log_debug("main_loop started");
	while (running) {
		int n = epoll_wait(context.epoll, events, MAX_EVENT, -1);
		if (n < 0) {
			log_error("epoll_wait failed: %s", strerror(errno));
			if (--retry > 0) {
				usleep(WAIT_RETRY);
				continue;
			}
			pthread_mutex_lock(&context.context_lock);
			cleanup_watchers();
			pthread_mutex_unlock(&context.context_lock);
			log_error("main loop crash");
			return NULL;
		}
		retry = MAX_RETRY;
		for (int i = 0; i < n; i++) {
			pthread_mutex_lock(&context.context_lock);
			struct callback *cur;
			void (*callback)(int fd) = NULL;
			int fd = -1;
			list_for_each_entry(cur, &context.watcher, list) {
				if (cur == events[i].data.ptr) {
					callback = cur->callback;
					fd = cur->fd;
				}
			}
			pthread_mutex_unlock(&context.context_lock);
			if (callback == terminate_cb) {
				running = false;
				break;
			}
			if (callback != NULL) {
				callback(fd);
			}
		}
	}

	return NULL;
}

static int __add_handler_nolock(void (*cb)(int), int fd)
{
	if (fcntl(fd, F_GETFL) < 0 && errno == EBADF) {
		log_error("invalid fd=%d", fd);
		return -1;
	}

	if (cb == NULL) {
		log_error("callback given is NULL");
		return -1;
	}

	struct callback *callback = create_callback(cb, fd);
	if (callback == NULL) {
		return -1;
	}

	struct epoll_event ev = { .events = EPOLLIN, .data.ptr = callback };
	if (epoll_ctl(context.epoll, EPOLL_CTL_ADD, fd, &ev) < 0) {
		log_error("epoll_ctl ADD fd=%d: %s", fd, strerror(errno));
		free(callback);
		return -1;
	}

	list_add(&callback->list, &context.watcher);

	return 0;
}

static int __delete_handler_no_lock(int fd)
{
	struct callback *cur, *tmp;
	list_for_each_entry_safe(cur, tmp, &context.watcher, list) {
		if (cur->fd == fd) {
			remove_callback(cur);

			log_debug("handler deleted for fd=%d", fd);
			return 0;
		}
	}

	log_error("delete_handler: no handler for fd=%d", fd);
	return -1;
}

int init_comm(void)
{
	pthread_mutex_lock(&context.context_lock);
	log_debug("init_comm begin");

	if (context.started) {
		log_error("init_comm: already started");
		pthread_mutex_unlock(&context.context_lock);
		return -1;
	}

	context.epoll = epoll_create1(0);
	if (context.epoll < 0) {
		log_error("epoll_create1: %s", strerror(errno));
		pthread_mutex_unlock(&context.context_lock);
		return -1;
	}

	log_debug("epoll created");

	context.poison = eventfd(0, EFD_NONBLOCK);
	if (context.poison < 0) {
		log_error("eventfd: %s", strerror(errno));
		close(context.epoll);
		pthread_mutex_unlock(&context.context_lock);
		return -1;
	}

	INIT_LIST_HEAD(&context.watcher);

	if (__add_handler_nolock(terminate_cb, context.poison) != 0) {
		log_error("add_handler(poison) failed");
		close(context.poison);
		close(context.epoll);
		pthread_mutex_unlock(&context.context_lock);
		return -1;
	}

	log_debug("poison added");

	int rc = pthread_create(&context.main_loop, NULL, main_loop, NULL);
	if (rc != 0) {
		log_error("pthread_create: %s", strerror(rc));
		__delete_handler_no_lock(context.poison);
		close(context.poison);
		close(context.epoll);
		pthread_mutex_unlock(&context.context_lock);
		return -1;
	}

	context.started = true;
	log_debug("init_comm success");
	pthread_mutex_unlock(&context.context_lock);
	return 0;
}

int exit_comm(void)
{
	pthread_mutex_lock(&context.context_lock);
	log_debug("exit_comm begin");

	if (!context.started) {
		log_error("exit_comm: already exit");
		pthread_mutex_unlock(&context.context_lock);
		return -1;
	}
	context.started = false;

	if (eventfd_write(context.poison, 1) < 0) {
		log_error("eventfd_write: %s", strerror(errno));
	}

	pthread_mutex_unlock(&context.context_lock);
	int rc = pthread_join(context.main_loop, NULL);
	if (rc != 0) {
		log_error("pthread_join: %s", strerror(rc));
	}
	pthread_mutex_lock(&context.context_lock);

	cleanup_watchers();

	context.epoll = -1;
	context.poison = -1;
	log_debug("exit_comm success");

	pthread_mutex_unlock(&context.context_lock);
	return 0;
}

int add_handler(void (*cb)(int), int fd)
{
	pthread_mutex_lock(&context.context_lock);

	if (!context.started) {
		log_error("add_handler called before init");
		pthread_mutex_unlock(&context.context_lock);
		return -1;
	}

	int ret = __add_handler_nolock(cb, fd);

	log_debug("handler added for fd=%d", fd);
	pthread_mutex_unlock(&context.context_lock);
	return ret;
}

int delete_handler(int fd)
{
	pthread_mutex_lock(&context.context_lock);

	log_debug("delete handler for fd=%d", fd);

	if (!context.started) {
		log_error("delete_handler called before init");
		pthread_mutex_unlock(&context.context_lock);
		return -1;
	}

	int ret = __delete_handler_no_lock(fd);

	pthread_mutex_unlock(&context.context_lock);
	return ret;
}

int delete_handlers(void (*cb)(int))
{
	pthread_mutex_lock(&context.context_lock);

	log_debug("delete handlers");

	if (!context.started) {
		log_error("delete_handlers called before init");
		pthread_mutex_unlock(&context.context_lock);
		return -1;
	}

	struct callback *cur, *tmp;
	list_for_each_entry_safe(cur, tmp, &context.watcher, list) {
		if (cur->callback == cb) {
			remove_callback(cur);

			log_debug("handler deleted for fd=%d", cur->fd);
		}
	}

	pthread_mutex_unlock(&context.context_lock);
	return 0;
}