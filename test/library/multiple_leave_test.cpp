#include <gtest/gtest.h>

extern "C" {
#include <semaphore.h>
#include <fcntl.h>

#include "library.h"
#include "network/network.h"
#include "network/message.h"
#include "utils/list.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "core/core.h"
#include "core/data_transfer.h"
#include <sys/mman.h>
}

static const char *addr_init = "127.0.0.1";
static const int init_port = 2450;
static const int nb_nodes_ = 1;

static struct node_id creator = { .host = "127.0.0.1", .port = init_port };
static struct node_id joiner1 = { .host = "127.0.0.1", .port = init_port + 1 };

static int check_sum(void *res)
{
	int *tab = (int *)res;
	int sum = 0;
	int final_sum = 0;
	for (int i = 0; i < 10; i++) {
		final_sum += i;
		sum += *(tab + i);
	}
	return sum == final_sum;
}

static void wait_all(void)
{
	for (unsigned int i = 0; i < nb_nodes_; i++)
		wait(NULL);
}

TEST(multiple_leave, leave_the_dsm)
{
	init_logger(stdout);

	sem_t *sem1, *sem2;

	sem1 = sem_open("/sem1", O_CREAT, 0644, 0);
	sem2 = sem_open("/sem2", O_CREAT, 0644, 0);

	if (sem1 == SEM_FAILED || sem2 == SEM_FAILED) {
		perror("sem_open");
		exit(EXIT_FAILURE);
	}

	log_info("started test\n");
	if (!fork()) {
		Init_DSM(PAGE_SIZE, LOCALHOST, creator.port);
		log_info("DSM ready\n");
		sem_post(sem1);
		sem_wait(sem2);
		log_info("I will edit data\n");
		lock_write(dsm, PAGE_SIZE);
		int *tab = (int *)dsm;
		for (int i = 0; i < 10; i++) {
			*(tab + i) = i;
		}
		unlock_write(dsm, PAGE_SIZE);
		void *res = leave_DSM();
		log_info("Creator left\n");
		sem_post(sem1);
		ASSERT_EQ(res == NULL, 1);
		exit(0);
	}
	sem_wait(sem1);
	join_DSM(creator.host, creator.port, LOCALHOST, joiner1.port);
	log_info("Joined DSM\n");
	sem_post(sem2);
	sem_wait(sem1);
	log_info("Joiner will leave\n");
	void *res = leave_DSM();
	int eq = check_sum(res);
	ASSERT_EQ(eq, 1);
	free(res);
	wait_all();
	nb_nodees = 0;
	if (sem_close(sem1) == -1 || sem_close(sem2) == -1) {
		perror("sem_close");
		exit(EXIT_FAILURE);
	}

	if (sem_unlink("/sem1") == -1 || sem_unlink("/sem2") == -1) {
		perror("sem_unlink");
		exit(EXIT_FAILURE);
	}
}