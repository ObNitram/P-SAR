#include <cstdio>
#include <gtest/gtest.h>
#include <sys/mman.h>

extern "C" {
#include "network/utils/node_id.h"
#include "utils/logger.h"
#include "core/data_transfer.h"
#include "utils/utils.h"
#include "memory/memory.h"
#include "comm/comm.h"
#include "network/network.new.h"
#include "notification_chans.h"
#include <semaphore.h>
#include <fcntl.h>
}

static const char *addr_init = "127.0.0.1";
static int nb_pages_ = 10;
static struct node_id creator = { .host = "127.0.0.1", .port = 2450 };
static struct node_id joiner1 = { .host = "127.0.0.1", .port = 2451 };

struct node_id page_owners_both[10] = { creator, creator, creator, creator,
					creator, creator, creator, creator,
					creator, creator };

static int check_page_owners_equality()
{
	for (int i = 0; i < nb_pages_; i++) {
		if (page_owners[i].port != page_owners_both[i].port) {
			return 0;
		}
	}
	return 1;
}

static void alloc_dsm(unsigned int nb_pagess)
{
	nb_pages = nb_pagess;
	dsm = mmap(0, nb_pages * PAGE_SIZE, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (dsm == MAP_FAILED) {
		log_error("map allocation failed");
		exit(EXIT_FAILURE);
	}
}

static void clean_up()
{
	clean_data_transfer();
	munmap(dsm, nb_pages * PAGE_SIZE);
	leave_network();
	exit_comm();
	destroy_all_chans();
}

// we have two proc
// one that inits the DSM, the other who will try to join it
TEST(data_transfer, try_sync_a_page)
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
	if (!pid) {
		alloc_dsm(nb_pages_);
		init_data_transfer(nb_pages_, page_owners_both);
		create_all_chans();
		init_memory(memory_fd1);

		// check that data_transfer is correctly init
		eq = check_page_owners_equality();
		ASSERT_EQ(eq, 1);

		// wait until creator is setup
		log_info("joiner1 : waiting for creator to setup");
		sem_wait(sem1);
		join_network(&joiner1, &creator);

		// wait until creator informs me that i can sync page 0
		log_info("joiner1 : waiting for creator signal to sync page 0");
		sem_wait(sem1);
		sync_page(0);

		// inform creator that i synced page 0
		log_info("joiner1 : informing creator that i synced page 0");
		sem_post(sem2);

		for (size_t i = 0; i < nb_pages_; i++) {
			memory_unlock_write(i);
		}

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

		// exiting
		log_info("joiner1 : exiting");
		clean_up();
		exit(0);
	} else {
		alloc_dsm(nb_pages_);
		init_data_transfer(nb_pages_, NULL);
		create_all_chans();
		init_memory(memory_fd1);

		// check that data_transfer is correctly init
		eq = check_page_owners_equality();
		ASSERT_EQ(eq, 1);

		join_network(&creator, NULL);

		// inform joiner1 that he can join
		log_info("creator : Informing joiner1 that he can join");
		sem_post(sem1);

		// init data with some random values
		for (size_t i = 0; i < nb_pages_; i++) {
			memory_unlock_write(i);
		}

		// edit some data
		int *tab = (int *)dsm;
		*tab = 0;
		for (int *i = tab + 1; i < tab + 100; i++) {
			*i = *(i - 1) + (i - tab);
		}

		// inform joiner that he can sync a page
		log_info("creator : Informing joiner1 that he can sync a page");
		sem_post(sem1);

		// wait until joiner1 synced page 0
		log_info("creator : waiting for joiner1 to sync page 0");
		sem_wait(sem2);

		// exiting
		log_info("creator : exiting");
		clean_up();

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
	}
}
