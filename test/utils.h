#pragma once

#include <assert.h>
#include <pthread.h>
#include <sys/mman.h>

struct barrier {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int count;
	int objectif;
	bool deleted;
};

static struct barrier *barrier_init(int c)
{
	struct barrier *b = (struct barrier *)mmap(NULL, sizeof(struct barrier),
						   PROT_READ | PROT_WRITE,
						   MAP_SHARED | MAP_ANONYMOUS,
						   -1, 0);
	assert(b != MAP_FAILED);

	pthread_mutexattr_t mattr;
	pthread_condattr_t cattr;
	pthread_mutexattr_init(&mattr);
	pthread_condattr_init(&cattr);
	pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
	pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);

	pthread_mutex_init(&b->mutex, &mattr);
	pthread_cond_init(&b->cond, &cattr);
	b->objectif = c;
	b->count = 0;
	b->deleted = false;

	return b;
}

static void barrier_destroy(struct barrier *b)
{
	b->deleted = true;

	while (b->count != 0) {
		pthread_cond_signal(&b->cond);
		pthread_cond_wait(&b->cond, &b->mutex);
	}

	pthread_mutex_destroy(&b->mutex);
	pthread_cond_destroy(&b->cond);

	munmap(b, sizeof(struct barrier));
}

static void barrier_wait(struct barrier *b)
{
	pthread_mutex_lock(&b->mutex);
	if (++b->count == b->objectif) {
		b->count = 0;
		pthread_cond_broadcast(&b->cond);
	} else {
		while (b->count != 0 && b->deleted == false)
			pthread_cond_wait(&b->cond, &b->mutex);
		if (b->deleted) {
			b->count--;
			pthread_cond_signal(&b->cond);
		};
	}
	pthread_mutex_unlock(&b->mutex);
}

#define EXPECT_TRUE_OR_EXIT(cond)                                              \
	do {                                                                   \
		if (!(cond)) {                                                 \
			std::cerr << "Assertion failed: " << #cond << " at "   \
				  << __FILE__ << ":" << __LINE__ << std::endl; \
			_exit(1);                                              \
		}                                                              \
	} while (0)
