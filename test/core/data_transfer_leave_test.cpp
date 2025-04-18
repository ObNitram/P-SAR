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

int look_up_init()
{
	struct node_list *ndlist = &node_list;
	list_for_each_entry_continue(ndlist, &node_list.nlist, nlist) {
		if (node_equal(nodes, &ndlist->node)) {
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
			sleep(2);
			join_DSM(nodes[0].host, nodes[0].port, NULL,
				 nodes[i].port);

			log_info("me %d joined the DSM\n", i);

			// wait for init to modify the dsm
			sleep(1);

			log_info("me %d try sync page 0\n", i);
			sync_page(0);

			sleep(1);

			int eq = node_equal(&nodes[1], page_owners);
			ASSERT_EQ(eq, 1);

			ASSERT_EQ(look_up_init(), 0);

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

	leave_data_transfer(&nodes[1]);

	stop_server();
	clean_core();
	free_nodes(&node_list);
	free_DSM();

	for (int i = 0; i < 3; i++)
		wait(NULL);
	nb_pages = 0;
	nb_nodees = 0;
}
