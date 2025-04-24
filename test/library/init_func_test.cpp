#include <gtest/gtest.h>
#include <sys/mman.h>

extern "C" {
#include "library.h"
#include "network/network.h"
#include "network/message.h"
#include "utils/list.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "core/core.h"
#include "core/data_transfer.h"
}

static const char *addr_init = "127.0.0.1";
static const int init_port = 2450;
static const int joiner_port = 4320;
static const int nb_pages_ = 10;
static const int nb_nodes_ = 1;

// the final node_list of JOINER
static struct node_id node_list_joiner[5] = {
	{ .host = "127.0.0.1", .port = init_port },
};

// the final node_list of init
static struct node_id node_list_init[5] = {
	{ .host = "127.0.0.1", .port = joiner_port },
};

#define SIZE_DSM 40960

static int check_node_list_equality(struct node_id nodes[])
{
	struct node_list *n1 = &node_list;
	int found = 0;
	list_for_each_entry_continue(n1, &node_list.nlist, nlist) {
		assert(n1->node.port != -1);
		for (int i = 0; i < nb_nodes_; i++) {
			if (node_equal(&n1->node, &nodes[i])) {
				found = 1;
				break;
			}
		}
		if (!found)
			return 0;
		found = 0;
	}
	return 1;
}

// we have two proc
// one that inits the DSM, the other who will try to join it
TEST(join_init_dsm, try_to_init_then_join_the_dsm)
{
	init_logger(stdout);

	log_info("started test\n");

	pid_t pid = fork();
	if (pid) {
		Init_DSM(SIZE_DSM, LOCALHOST, init_port);

		log_info("INIT :  dsm ready");

		ASSERT_EQ(nb_pages, nb_pages_);

		ASSERT_EQ(list_empty(&node_list.nlist), 1);

		// wait for the node to join
		sleep(2);

		// check that node_list is the same as
		int eq = check_node_list_equality(node_list_init);
		ASSERT_EQ(eq, 1);

		stop_server();
		clean_data_transfer();
		clean_core();
		free_nodes(&node_list);
		munmap(dsm, nb_pages * PAGE_SIZE);
		wait(NULL);
		nb_pages = 0;
		nb_nodees = 0;
	} else {
		// wait until INIT is setup
		sleep(1);

		join_DSM(addr_init, init_port, LOCALHOST, joiner_port);

		ASSERT_EQ(nb_pages, nb_pages_);

		int found = 0;

		ASSERT_EQ(nb_nodees, nb_nodes_);
		int eq = 0;

		eq = check_node_list_equality(node_list_joiner);
		ASSERT_EQ(eq, 1);

		stop_server();
		clean_data_transfer();
		clean_core();
		free_nodes(&node_list);
		munmap(dsm, nb_pages * PAGE_SIZE);

		exit(0);
	}
}

TEST(addr_to_page, get_page_index)
{
	// memory init
	nb_pages = (SIZE_DSM + PAGE_SIZE - 1) / PAGE_SIZE;
	dsm = mmap(0, nb_pages * PAGE_SIZE, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	ASSERT_EQ(dsm != MAP_FAILED, 1);

	size_t index = get_page_index((char *)dsm + 1452);
	ASSERT_EQ(index, 0);

	index = get_page_index((char *)dsm + 4100);
	ASSERT_EQ(index, 1);

	index = get_page_index((char *)dsm + 17376);
	ASSERT_EQ(index, 4);

	index = get_page_index((char *)dsm + 4095);
	ASSERT_EQ(index, 0);

	index = get_page_index((char *)dsm + 4096);
	ASSERT_EQ(index, 1);

	index = get_page_index((char *)dsm + 4090);
	ASSERT_EQ(index, 0);

	munmap(dsm, nb_pages * PAGE_SIZE);
	nb_pages = 0;
}