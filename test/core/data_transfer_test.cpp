#include <cstdio>
#include <gtest/gtest.h>
#include <sys/mman.h>

extern "C" {
#include "library.h"
#include "network/network.h"
#include "network/message.h"
#include "utils/list.h"
#include "utils/logger.h"
#include "core/core.h"
#include "core/data_transfer.h"
#include "utils/utils.h"
#include "sigsegv_handler/sigsegv.h"
#include "memory/memory.h"
#include "comm/comm.h"
#include <semaphore.h>
#include <fcntl.h>
}

static const char *addr_init = "127.0.0.1";
static const int init_port = 2451;
static const int joiner_port = 4321;
static const int nb_pages_ = 10;

struct node_id page_owners_both[10] = {
	{ .host = "127.0.0.1", .port = init_port },
	{ .host = "127.0.0.1", .port = init_port },
	{ .host = "127.0.0.1", .port = init_port },
	{ .host = "127.0.0.1", .port = init_port },
	{ .host = "127.0.0.1", .port = init_port },
	{ .host = "127.0.0.1", .port = init_port },
	{ .host = "127.0.0.1", .port = init_port },
	{ .host = "127.0.0.1", .port = init_port },
	{ .host = "127.0.0.1", .port = init_port },
	{ .host = "127.0.0.1", .port = init_port }
};

#define SIZE_DSM 40960

static int check_page_owners_equality()
{
	for (int i = 0; i < nb_pages_; i++) {
		if (page_owners[i].port != page_owners_both[i].port) {
			return 0;
		}
	}
	return 1;
}

// we have two proc
// one that inits the DSM, the other who will try to join it
TEST(data_transfer, join_then_try_sync_a_page)
{
	init_logger(stdout);

	sem_t *sem1, *sem2;

	log_info("started test\n");
	sem1 = sem_open("/sem1", O_CREAT, 0644, 0);
	sem2 = sem_open("/sem2", O_CREAT, 0644, 0);

	if (sem1 == SEM_FAILED || sem2 == SEM_FAILED) {
		perror("sem_open");
		exit(EXIT_FAILURE);
	}

	pid_t pid = fork();
	int eq = 0;
	if (pid) {
		Init_DSM(SIZE_DSM, LOCALHOST, init_port);
		for (size_t i = 0; i < nb_pages_; i++) {
			memory_unlock_write(i);
		}

		eq = check_page_owners_equality();
		ASSERT_EQ(eq, 1);

		// edit some data
		int *tab = (int *)dsm;
		*tab = 0;
		for (int *i = tab + 1; i < tab + 100; i++) {
			*i = *(i - 1) + (i - tab);
		}

		sem_post(sem1);

		// wait te recv an ASK_PAGE request
		sem_wait(sem2);

		stop_server();
		clean_data_transfer();
		clean_core();
		free_nodes(&node_list);
		munmap(dsm, nb_pages * PAGE_SIZE);
		exit_comm();
		destroy_all_chans();
		wait(NULL);
		nb_pages = 0;
		nb_nodees = 0;

		if (sem_close(sem1) == -1 || sem_close(sem2) == -1) {
			perror("sem_close");
			exit(EXIT_FAILURE);
		}

		if (sem_unlink("/sem1") == -1 || sem_unlink("/sem2") == -1) {
			perror("sem_unlink");
			exit(EXIT_FAILURE);
		}
	} else {
		// wait until parent is setup
		sem_wait(sem1);

		join_DSM(addr_init, init_port, LOCALHOST, joiner_port);
		log_info("joined the DSM\n");

		ASSERT_EQ(nb_pages, nb_pages_);

		int found = 0;

		eq = check_page_owners_equality();
		ASSERT_EQ(eq, 1);

		sync_page(0);
		for (size_t i = 0; i < nb_pages_; i++) {
			memory_unlock_write(i);
		}
		log_info("synced page 0\n");
		sem_post(sem2);

		int *tab = (int *)dsm;
		ASSERT_EQ(*tab, 0);

		eq = 0;

		for (int *i = tab + 1; i < tab + 100; i++) {
			int calculated_val = *(i - 1) + (i - tab);
			int cur_val = *i;
			eq = calculated_val == cur_val;
			if (!eq)
				break;
		}

		ASSERT_EQ(eq, 1);

		stop_server();
		clean_data_transfer();
		clean_core();
		free_nodes(&node_list);
		munmap(dsm, nb_pages * PAGE_SIZE);
		exit_comm();
		destroy_all_chans();
		exit(0);
	}
}
