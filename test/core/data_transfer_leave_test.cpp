#include <gtest/gtest.h>

extern "C" {
#include "library.h"
#include "network/network.h"
#include "network/message.h"
#include "utils/list.h"
#include "utils/logger.h"
#include "core/core.h"
#include "core/data_transfer.h"
#include "utils/utils.h"
#include <semaphore.h>
#include <fcntl.h>
}

static const int init_port = 2450;
static const int nb_pages_ = 1;
static struct node_id nodes[5] = {
	{ .host = "127.0.0.1", .port = init_port },
	{ .host = "127.0.0.1", .port = init_port + 1 },
	{ .host = "127.0.0.1", .port = init_port + 2 },
	{ .host = "127.0.0.1", .port = init_port + 3 }
};

#define SIZE_DSM 4096

int look_up_node(const struct node_id *node)
{
	struct node_list *ndlist = &node_list;
	list_for_each_entry_continue(ndlist, &node_list.nlist, nlist) {
		if (node_equal(node, &ndlist->node)) {
			return 1;
		}
	}
	return 0;
}

TEST(data_transfer_leave, join_then_try_sync_a_page)
{
	init_logger(stdout);

	log_info("started test\n");

	for (int i = 1; i <= 3; i++) {
		pid_t pid = fork();
		if (!pid) {
			sleep(1);
			join_DSM(nodes[0].host, nodes[0].port, NULL,
				 nodes[i].port);

			log_info("me %d joined the DSM\n", i);

			log_info("me %d try sync page 0\n", i);
			sync_page(0);

			sleep(1);

			int eq = node_equal(&nodes[1], page_owners);
			ASSERT_EQ(eq, 1);

			ASSERT_EQ(look_up_node(&nodes[0]), 0);

			stop_server();
			clean_data_transfer();
			clean_core();
			free_nodes(&node_list);
			free_DSM();
			exit(0);
		}
	}

	Init_DSM(SIZE_DSM, LOCALHOST, init_port);

	// wait until every one joined
	while (nb_nodees < 3) {
	}

	log_info("init all joined\n");

	leave_data_transfer(nodes[1]);

	stop_server();
	clean_core();
	free_nodes(&node_list);
	free_DSM();

	for (int i = 0; i < 3; i++)
		wait(NULL);
	nb_pages = 0;
	nb_nodees = 0;
}

TEST(data_transfer_leave, leave_without_owning_a_page)
{
	init_logger(stdout);

	log_info("started test\n");

	sem_t *sem1, *sem2;

	sem1 = sem_open("/sem1", O_CREAT, 0644, 0);
	sem2 = sem_open("/sem2", O_CREAT, 0644, 0);

	if (sem1 == SEM_FAILED) {
		perror("sem_open");
		exit(EXIT_FAILURE);
	}

	for (int i = 1; i <= 3; i++) {
		pid_t pid = fork();
		if (!pid) {
			sleep(1);
			join_DSM(nodes[0].host, nodes[0].port, NULL,
				 nodes[i].port);
			sem_post(sem2);
			log_info("me %d joined the DSM\n", i);

			sleep(1);
			if (i != 1) {
				log_info("me %d try sync page 0\n", i);
				sync_page(0);
				log_info("me %d synced page 0\n", i);
			} else {
				log_info("me %d leaving the DSM\n", i);
				leave_data_transfer(nodes[0]);
				log_info("me %d left the DSM\n", i);
			}

			if (i != 1) {
				log_info("me %d waiting for 1 to leave\n", i);
				sem_wait(sem1);
				log_info("me %d woke up\n", i);

				int eq = node_equal(&nodes[0], page_owners);
				ASSERT_EQ(eq, 1);

				ASSERT_EQ(look_up_node(&nodes[1]), 0);
				sem_post(sem2);
			}

			if (i == 1) {
				log_info("me %d posting\n", i);
				for (int i = 0; i < 3; i++)
					sem_post(sem1);
				log_info("me %d posted\n", i);
			}

			stop_server();
			if (i != 1)
				clean_data_transfer();
			clean_core();
			free_nodes(&node_list);
			free_DSM();
			exit(0);
		}
	}

	Init_DSM(SIZE_DSM, LOCALHOST, init_port);

	// wait until every one joined
	for (int i = 0; i < 3; i++)
		sem_wait(sem2);

	log_info("init all joined\n");

	sem_wait(sem1);
	ASSERT_EQ(look_up_node(&nodes[1]), 0);

	for (int i = 0; i < 2; i++)
		sem_wait(sem2);
	stop_server();
	clean_core();
	free_nodes(&node_list);
	free_DSM();

	if (sem_close(sem1) == -1 || sem_close(sem2) == -1) {
		perror("sem_close");
		exit(EXIT_FAILURE);
	}

	if (sem_unlink("/sem1") == -1 || sem_unlink("/sem2") == -1) {
		perror("sem_unlink");
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < 3; i++)
		wait(NULL);
	nb_pages = 0;
	nb_nodees = 0;
}
