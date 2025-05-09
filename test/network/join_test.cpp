#include <gtest/gtest.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>

extern "C" {
#include "utils/logger.h"
#include "comm/comm.h"
#include "network/network.new.h"
}

#define EXPECT_TRUE_OR_EXIT(cond)                                 \
	do {                                                      \
		if (!(cond)) {                                    \
			log_error("Assertion failed: %s", #cond); \
			_exit(1);                                 \
		}                                                 \
	} while (0)

static const int init_port = 2450;

static struct node_id creator = { .host = "127.0.0.1", .port = init_port };
static struct node_id joiner1 = { .host = "127.0.0.1", .port = init_port + 1 };
static struct node_id joiner2 = { .host = "127.0.0.1", .port = init_port + 2 };
static struct node_id joiner3 = { .host = "127.0.0.1", .port = init_port + 3 };
static struct node_id joiner4 = { .host = "127.0.0.1", .port = init_port + 4 };
static struct node_id joiner5 = { .host = "127.0.0.1", .port = init_port + 5 };

static struct node_id all_nodes[6] = { creator, joiner1, joiner2,
				       joiner3, joiner4, joiner5 };

static void wait_all(unsigned int times)
{
	for (unsigned int i = 0; i < times; i++)
		wait(NULL);
}

static bool lookup_nodes(const struct node_id nodes[], size_t size)
{
	unsigned int network_size = 0;
	struct node_id *network = get_network(&network_size);
	if (size > network_size) {
		free(network);
		return false;
	}
	bool found = false;
	for (size_t i = 0; i < size; i++) {
		for (size_t j = 0; j < network_size; j++) {
			if (node_equal(&nodes[i], &network[j])) {
				found = true;
				break;
			}
		}
		if (!found) {
			free(network);
			return false;
		}
		found = false;
	}
	free(network);
	return true;
}

static bool check_none(void)
{
	struct node_id *network = NULL;
	unsigned int network_size = 0;

	network = get_network(&network_size);
	return (network_size == 0 && network == NULL);
}

static bool check_alone(const struct node_id *me)
{
	struct node_id *network = NULL;
	unsigned int network_size = 0;

	network = get_network(&network_size);
	if (network_size != 1) {
		free(network);
		return false;
	}
	bool found = node_equal(&network[0], me);
	free(network);
	return found;
}

static void post_on(sem_t *sem, unsigned int times)
{
	for (unsigned int i = 0; i < times; i++)
		sem_post(sem);
}

static void wait_on(sem_t *sem, unsigned int times)
{
	for (unsigned int i = 0; i < times; i++)
		sem_wait(sem);
}

static void close_all(sem_t *sems[], size_t size)
{
	for (int i = 0; i < size; i++) {
		if (sem_close(sems[i]) == -1) {
			log_error("sem_close: %s", strerror(errno));
			exit(EXIT_FAILURE);
		}
		char name[10];
		sprintf(name, "/sem%d", i);
		if (sem_unlink(name) == -1) {
			log_error("sem_unlink: %s", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
}

TEST(joinNetwork, create_and_leave)
{
	init_logger(stdout);

	log_info("Test : create and leave");
	init_comm();

	// check that there is no one in the network
	ASSERT_TRUE(check_none());

	EXPECT_EQ(join_network(&creator, NULL), 0);
	// check if we are alone in the network
	ASSERT_TRUE(check_alone(&creator));

	EXPECT_EQ(leave_network(), 0);
	// check that there is no one in the network
	ASSERT_TRUE(check_none());

	exit_comm();
	log_info("End of test");
}

TEST(joinNetwork, child_joins_father_then_exits)
{
	init_logger(stdout);

	log_info("Test : child joins father then exits");

	sem_t *sem1, *sem2;

	sem1 = sem_open("/sem1", O_CREAT, 0644, 0);
	sem2 = sem_open("/sem2", O_CREAT, 0644, 0);

	if (sem1 == SEM_FAILED || sem2 == SEM_FAILED) {
		log_error("sem_open: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	pid_t pid = fork();
	if (pid == 0) {
		init_comm();
		// wait until father is set up
		log_info("joiner1 : waiting for creator to set up");
		sem_wait(sem1);
		EXPECT_TRUE_OR_EXIT(join_network(&joiner1, &creator) == 0);
		log_info("joiner1 : joined the network");

		// inform father that we joined
		sem_post(sem2);

		// check network state
		log_info("joiner1 : checking network state");
		const struct node_id to_lookup[2] = { joiner1, creator };
		EXPECT_TRUE_OR_EXIT(lookup_nodes(to_lookup, 1));

		log_info("joiner1: wait until creator allow us to leave");
		sem_wait(sem1);

		// we leave
		log_info("joiner1 : leaving the network");
		EXPECT_TRUE_OR_EXIT(leave_network() == 0);
		log_info("joiner1 : left the network");
		EXPECT_TRUE_OR_EXIT(check_none());

		// inform father that we leaved
		sem_post(sem2);
		exit_comm();
		exit(0);
	} else {
		init_comm();
		// we create the network
		log_info("creator : creating the network");
		EXPECT_EQ(join_network(&creator, NULL), 0);

		// inform child that he can join
		sem_post(sem1);

		// wait until child joined
		log_info("creator : waiting for joiner1 to join");
		sem_wait(sem2);

		// check network state
		log_info("creator : checking network state");
		struct node_id to_lookup[2] = { creator, joiner1 };
		ASSERT_TRUE(lookup_nodes(to_lookup, 2));

		log_info("creator: inform joiner1 leaving");
		sem_post(sem1);

		// wait until child left
		log_info("creator : waiting for joiner1 to leave");
		sem_wait(sem2);

		// check that child really left
		log_info("creator : checking that we are alone");
		ASSERT_TRUE(check_alone(&creator));

		// we leave
		log_info("creator : leaving the network");
		EXPECT_EQ(leave_network(), 0);
		log_info("creator : left the network");

		// check that there is no one in the network
		log_info(
			"creator : checking that there is no one in the network");
		ASSERT_TRUE(check_none());
		exit_comm();
		wait_all(1);
	}
	if (sem_close(sem1) == -1 || sem_close(sem2) == -1) {
		log_error("sem_close: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (sem_unlink("/sem1") == -1 || sem_unlink("/sem2") == -1) {
		log_error("sem_unlink: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

TEST(joinNetwork, two_childs_join_then_nb2_leaves_then_nb1_leaves)
{
	init_logger(stdout);

	log_info("Test : two childs join then one leave");

	sem_t *sem1, *sem2, *sem3, *sem4;

	sem1 = sem_open("/sem1", O_CREAT, 0644, 0);
	sem2 = sem_open("/sem2", O_CREAT, 0644, 0);
	sem3 = sem_open("/sem3", O_CREAT, 0644, 0);
	sem4 = sem_open("/sem4", O_CREAT, 0644, 0);

	if (sem1 == SEM_FAILED || sem2 == SEM_FAILED || sem3 == SEM_FAILED ||
	    sem4 == SEM_FAILED) {
		log_error("sem_open: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	for (int i = 1; i <= 2; i++) {
		pid_t pid = fork();
		if (pid == 0) {
			init_comm();
			// wait until father is set up
			log_info("joiner%d : waiting for creator to set up", i);
			sem_wait(sem1);
			log_info("joiner%d : joining the network", i);
			EXPECT_TRUE_OR_EXIT(
				join_network(&all_nodes[i], &creator) == 0);
			log_info("joiner%d : joined the network", i);

			// inform father that we joined
			sem_post(sem2);

			// wait until every child has joined
			sem_wait(sem3);

			// check network state
			log_info("joiner%d : checking network state", i);
			EXPECT_TRUE_OR_EXIT(lookup_nodes(all_nodes, 3));
			// joiner2 can leave
			if (i == 1)
				sem_post(sem1);

			// we leave
			if (i == 2) {
				// wait until joiner1 checked his state
				sem_wait(sem1);
				log_info("joiner%d : leaving the network", i);
				EXPECT_TRUE_OR_EXIT(leave_network() == 0);
				log_info("joiner%d : left the network", i);
				EXPECT_TRUE_OR_EXIT(check_none());

				// inform others that joiner2 left
				log_info(
					"joiner%d : informing others that I left",
					i);
				post_on(sem4, 2);
				exit_comm();
				exit(0);
			}

			// wait until joiner2 leaves
			sem_wait(sem4);
			// check that joiner2 left
			log_info("joiner%d : checking that joiner2 left", i);
			ASSERT_TRUE(lookup_nodes(all_nodes, 2));

			// wait until creator checked network state
			log_info(
				"joiner%d : waiting for creator to check network state",
				i);
			sem_wait(sem1);

			log_info("joiner%d : leaving the network", i);
			EXPECT_EQ(leave_network(), 0);
			log_info("joiner%d : left the network", i);
			sem_post(sem2);
			ASSERT_TRUE(check_none());

			exit_comm();
			exit(0);
		}
	}

	init_comm();
	// we create the network
	log_info("creator : creating the network");
	EXPECT_EQ(join_network(&creator, NULL), 0);

	// inform child that he can join
	log_info("creator : informing joiners that they can join");
	post_on(sem1, 2);

	// wait until every child joined
	log_info("creator : waiting for joiners to join");
	wait_on(sem2, 2);

	// check network state
	log_info("creator : checking network state");
	ASSERT_TRUE(lookup_nodes(all_nodes, 3));

	// infrom child that everyone has joined
	log_info("creator : informing joiners that everyone has joined");
	post_on(sem3, 2);

	// wait until joiner2 leaves
	log_info("creator : waiting for joiner2 to leave");
	sem_wait(sem4);

	// check that joiner2 really left
	log_info("creator : checking that joiner2 left");
	ASSERT_TRUE(lookup_nodes(all_nodes, 2));

	// inform joiner1 that he can leave
	log_info("creator : informing joiner1 that he can leave");
	sem_post(sem1);

	// wait until joiner1 leaves
	log_info("creator : waiting for joiner1 to leave");
	sem_wait(sem2);

	// check that joiner1 really left
	log_info("creator : checking that joiner1 left");
	ASSERT_TRUE(check_alone(&creator));

	// we leave
	log_info("creator : leaving the network");
	EXPECT_EQ(leave_network(), 0);
	log_info("creator : left the network");

	// check that there is no one in the network
	log_info("creator : checking that there is no one in the network");
	ASSERT_TRUE(check_none());
	exit_comm();
	wait_all(2);

	if (sem_close(sem1) == -1 || sem_close(sem2) == -1 ||
	    sem_close(sem3) == -1 || sem_close(sem4) == -1) {
		log_error("sem_close: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (sem_unlink("/sem1") == -1 || sem_unlink("/sem2") == -1 ||
	    sem_unlink("/sem3") == -1 || sem_unlink("/sem4") == -1) {
		log_error("sem_unlink: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

TEST(joinNetwork, childs_join_father_then_leave)
{
	init_logger(stdout);

	log_info("Test : childs joins father then exits");

	sem_t *sem1, *sem2, *sem3;

	sem1 = sem_open("/sem1", O_CREAT, 0644, 0);
	sem2 = sem_open("/sem2", O_CREAT, 0644, 0);
	sem3 = sem_open("/sem3", O_CREAT, 0644, 0);

	if (sem1 == SEM_FAILED || sem2 == SEM_FAILED || sem3 == SEM_FAILED) {
		log_error("sem_open: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	for (int i = 1; i <= 5; i++) {
		pid_t pid = fork();
		if (pid == 0) {
			init_comm();

			// wait until father is set up
			sem_wait(sem1);
			log_info("joiner%d : joining the network", i);
			EXPECT_TRUE_OR_EXIT(
				join_network(&all_nodes[i], &creator) == 0);
			log_info("joiner%d : joined the network", i);

			log_info("joiner%d : informing creator that I joined",
				 i);
			sem_post(sem2);

			// wait until every child has joined
			sem_wait(sem3);
			log_info("joiner%d : checking network state", i);
			EXPECT_TRUE_OR_EXIT(lookup_nodes(all_nodes, 6));

			// inform creator that we checked the network state
			log_info(
				"joiner%d : informing creator that I checked the network state",
				i);
			sem_post(sem2);

			// wait confirmation to leave
			log_info(
				"joiner%d : waiting for creator to inform me that I can leave",
				i);
			sem_wait(sem1);

			// we leave
			log_info("joiner%d : leaving the network", i);
			EXPECT_TRUE_OR_EXIT(leave_network() == 0);
			log_info("joiner%d : left the network", i);
			EXPECT_TRUE_OR_EXIT(check_none());
			log_info("joiner%d : informing creator that I left", i);
			sem_post(sem2);
			exit_comm();
			exit(0);
		}
	}

	init_comm();
	// we create the network
	log_info("creator : creating the network");
	EXPECT_EQ(join_network(&creator, NULL), 0);

	// inform child that he can join
	log_info("creator : informing joiners that they can join");
	post_on(sem1, 5);

	// wait until every child joined
	log_info("creator : waiting for joiners to join");
	wait_on(sem2, 5);

	// check network state
	log_info("creator : checking network state");
	ASSERT_TRUE(lookup_nodes(all_nodes, 6));

	// inform joiners that every one joined
	log_info("creator : informing joiners that everyone has joined");
	post_on(sem3, 5);

	// wait that every joined checked his network state
	log_info("creator : waiting for joiners to check network state");
	wait_on(sem2, 5);

	// inform joiners that they can leave
	log_info("creator : informing joiners that they can leave");
	post_on(sem1, 5);

	// wait until every child left
	log_info("creator : waiting for joiners to leave");
	wait_on(sem2, 5);

	// check that we are alone
	log_info("creator : checking that we are alone");
	ASSERT_TRUE(check_alone(&creator));

	// we leave
	log_info("creator : leaving the network");
	EXPECT_EQ(leave_network(), 0);
	log_info("creator : left the network");
	// check that there is no one in the network
	ASSERT_TRUE(check_none());

	exit_comm();
	wait_all(5);

	if (sem_close(sem1) == -1 || sem_close(sem2) == -1 ||
	    sem_close(sem3) == -1) {
		log_error("sem_close: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (sem_unlink("/sem1") == -1 || sem_unlink("/sem2") == -1 ||
	    sem_unlink("/sem3") == -1) {
		log_error("sem_unlink: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

TEST(joinNetwork, childs_join_in_a_queue)
{
	init_logger(stdout);

	log_info("Test : any one joins any one");

	sem_t *sems[6];
	for (int i = 0; i < 6; i++) {
		char name[10];
		sprintf(name, "/sem%d", i);
		sems[i] = sem_open(name, O_CREAT, 0644, 0);
		if (sems[i] == SEM_FAILED) {
			log_error("sem_open: %s", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	for (int i = 1; i <= 5; i++) {
		pid_t pid = fork();
		if (pid == 0) {
			init_comm();
			if (i == 1) {
				log_info(
					"joiner%d : waiting for creator to set up",
					i);
			} else {
				log_info(
					"joiner%d : waiting for joiner%d to set up",
					i, i - 1);
			}
			sem_wait(sems[i]);
			EXPECT_TRUE_OR_EXIT(join_network(&all_nodes[i],
							 &all_nodes[i - 1]) ==
					    0);
			log_info("joiner%d : joined the network", i);

			if (i != 5) {
				// inform next that he can join
				log_info(
					"joiner%d : informing joiner%d that he can join",
					i, i + 1);
				sem_post(sems[i + 1]);

				// wait until last one joined
				log_info(
					"joiner%d : waiting for joiner5 to join",
					i);
				sem_wait(sems[i]);
			} else {
				// inform every one that i joined
				log_info(
					"joiner%d : informing every one that I joined",
					i);
				for (int i = 0; i < 5; i++)
					sem_post(sems[i]);
			}

			// check network state
			log_info("joiner%d : checking network state", i);
			EXPECT_TRUE_OR_EXIT(lookup_nodes(all_nodes, 6));

			// inform creator that we have checked our state
			log_info(
				"joiner%d : informing creator that I checked my state",
				i);
			sem_post(sems[0]);

			// wait until we can leave
			sem_wait(sems[i]);

			// we leave
			log_info("joiner%d : leaving the network", i);
			EXPECT_TRUE_OR_EXIT(leave_network() == 0);
			log_info("joiner%d : left the network", i);
			EXPECT_TRUE_OR_EXIT(check_none());

			// inform creator that we left
			log_info("joiner%d : informing creator that I left", i);
			sem_post(sems[0]);
			exit_comm();
			exit(0);
		}
	}

	init_comm();

	// we create the network
	log_info("creator : creating the network");
	EXPECT_EQ(join_network(&creator, NULL), 0);

	// inform child that he can join
	log_info("creator : informing joiner1 that he can join");
	sem_post(sems[1]);

	// wait until joiner5 joined
	log_info("creator : waiting for joiner5 to join");
	sem_wait(sems[0]);

	// check network state
	log_info("creator : checking network state");
	ASSERT_TRUE(lookup_nodes(all_nodes, 6));

	// wait untils every one checked his state
	wait_on(sems[0], 5);

	// inform every one that they can leave
	log_info("creator : informing every one that they can leave");
	for (int i = 1; i <= 5; i++)
		sem_post(sems[i]);

	// wait until every child left
	log_info("creator : waiting for joiners to leave");
	wait_on(sems[0], 5);

	// check that i am alone in the network
	log_info("creator : checking that I am alone in the network");
	ASSERT_TRUE(check_alone(&creator));

	// i leave
	log_info("creator : leaving the network");
	EXPECT_EQ(leave_network(), 0);
	log_info("creator : left the network");

	// check that there is no one in the network
	log_info("creator : checking that there is no one in the network");
	ASSERT_TRUE(check_none());
	exit_comm();
	wait_all(5);

	close_all(sems, 6);
}

TEST(joinNetwork, childs_join_any_one)
{
	init_logger(stdout);

	log_info("Test : any one joins any one");

	sem_t *sems[6];
	for (int i = 0; i < 6; i++) {
		char name[10];
		sprintf(name, "/sem%d", i);
		sems[i] = sem_open(name, O_CREAT, 0644, 0);
		if (sems[i] == SEM_FAILED) {
			log_error("sem_open: %s", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	for (int i = 1; i <= 5; i++) {
		pid_t pid = fork();
		if (pid == 0) {
			init_comm();
			log_info("joiner%d : waiting for joining", i);
			sem_wait(sems[i]);
			log_info("joiner%d : joining the network", i);
			switch (i) {
			case 1: {
				EXPECT_TRUE_OR_EXIT(
					join_network(&all_nodes[i], &creator) ==
					0);
				log_info("joiner%d : joined the network", i);

				// inform joiner2 and joiner3 that they can join
				log_info(
					"joiner%d : informing joiner2 and joiner3 that they can join",
					i);
				sem_post(sems[2]);
				sem_post(sems[3]);
				break;
			}

			case 2: {
				EXPECT_TRUE_OR_EXIT(
					join_network(&all_nodes[i], &joiner1) ==
					0);
				log_info("joiner%d : joined the network", i);

				// inform joiner4 that he can join
				log_info(
					"joiner%d : informing joiner4 that he can join",
					i);
				sem_post(sems[4]);
				break;
			}

			case 3: {
				EXPECT_TRUE_OR_EXIT(
					join_network(&all_nodes[i], &joiner1) ==
					0);
				log_info("joiner%d : joined the network", i);

				// inform joiner5 that he can join
				log_info(
					"joiner%d : informing joiner5 that he can join",
					i);
				sem_post(sems[5]);
				break;
			}

			case 4: {
				EXPECT_TRUE_OR_EXIT(
					join_network(&all_nodes[i], &joiner2) ==
					0);
				log_info("joiner%d : joined the network", i);

				// wait until joiner5 joined
				log_info(
					"joiner%d : waiting for joiner5 to join",
					i);
				sem_wait(sems[i]);

				// inform others creator, joiner1, joiner2, joiner3 that i joined
				log_info(
					"joiner%d : informing creator, joiner1, joiner2, joiner3 that I joined",
					i);
				for (int i = 0; i < 4; i++)
					sem_post(sems[i]);
				goto check_state;
				break;
			}

			default: {
				EXPECT_TRUE_OR_EXIT(
					join_network(&all_nodes[i], &joiner3) ==
					0);
				log_info("joiner%d : joined the network", i);

				// inform all others that i joined
				log_info(
					"joiner%d : informing creator, joiner1, joiner2, joiner3, joiner4 that I joined",
					i);
				for (int i = 0; i < 5; i++)
					sem_post(sems[i]);
				goto check_state;
				break;
			}
			}

			// wait until joiner4 and joiner5 joined
			log_info(
				"joiner%d : waiting for joiner4 and joiner5 to join",
				i);
			wait_on(sems[i], 2);

check_state:
			// check network state
			log_info("joiner%d : checking network state", i);
			EXPECT_TRUE_OR_EXIT(lookup_nodes(all_nodes, 6));

			// inform creator that we have checked our state
			log_info(
				"joiner%d : informing creator that I checked my state",
				i);
			sem_post(sems[0]);

			// wait from creator that we can leave
			log_info(
				"joiner%d : waiting for creator to inform me that I can leave",
				i);
			sem_wait(sems[i]);

			// we leave
			log_info("joiner%d : leaving the network", i);
			EXPECT_TRUE_OR_EXIT(leave_network() == 0);
			log_info("joiner%d : left the network", i);
			EXPECT_TRUE_OR_EXIT(check_none());

			// inform creator that we left
			log_info("joiner%d : informing creator that I left", i);
			sem_post(sems[0]);
			exit_comm();
			exit(0);
		}
	}

	init_comm();

	// we create the network
	log_info("creator : creating the network");
	EXPECT_EQ(join_network(&creator, NULL), 0);

	// inform joiner1 that he can join
	log_info("creator : informing joiner1 that he can join");
	sem_post(sems[1]);

	// wait until joiner4 and joiner5 joined
	log_info("creator : waiting for joiner4 and joiner5 to join");
	wait_on(sems[0], 2);

	// check network state
	log_info("creator : checking network state");
	ASSERT_TRUE(lookup_nodes(all_nodes, 6));

	// wait until every one checked his state
	wait_on(sems[0], 5);

	// inform every one that they can leave
	log_info("creator : informing every one that they can leave");
	for (int i = 1; i <= 5; i++)
		sem_post(sems[i]);

	// wait until every child left
	log_info("creator : waiting for joiners to leave");
	wait_on(sems[0], 5);

	// check that i am alone in the network
	log_info("creator : checking that I am alone in the network");
	ASSERT_TRUE(check_alone(&creator));

	// i leave
	log_info("creator : leaving the network");
	EXPECT_EQ(leave_network(), 0);
	log_info("creator : left the network");

	// check that there is no one in the network
	log_info("creator : checking that there is no one in the network");
	ASSERT_TRUE(check_none());
	exit_comm();
	wait_all(5);

	close_all(sems, 6);
}

TEST(joinNetwork, childs_join_father_then_leave_big_version)
{
	init_logger(stdout);
	const size_t nb_childs = 20;

	log_info("Test : %zu childs joins father then exits", nb_childs);

	struct node_id *nodes_list = (struct node_id *)malloc(
		(nb_childs + 1) * sizeof(struct node_id));
	for (int i = 0; i <= nb_childs; i++) {
		nodes_list[i] = {
			.host = "127.0.0.1",
			.port = 6000 + i,
		};
		log_info("node[%d] %s:%d", i, nodes_list[i].host,
			 nodes_list[i].port);
	}
	struct node_id founder = nodes_list[0];

	sem_t *sem1, *sem2, *sem3;

	sem1 = sem_open("/sem1", O_CREAT, 0644, 0);
	sem2 = sem_open("/sem2", O_CREAT, 0644, 0);
	sem3 = sem_open("/sem3", O_CREAT, 0644, 0);

	if (sem1 == SEM_FAILED || sem2 == SEM_FAILED || sem3 == SEM_FAILED) {
		log_error("sem_open: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	for (int i = 1; i <= nb_childs; i++) {
		pid_t pid = fork();
		if (pid == 0) {
			init_comm();

			// wait until father is set up
			sem_wait(sem1);
			log_info("joiner%d : joining the network", i);
			EXPECT_TRUE_OR_EXIT(
				join_network(&nodes_list[i], &founder) == 0);
			log_info("joiner%d : joined the network", i);

			log_info("joiner%d : informing creator that I joined",
				 i);
			sem_post(sem2);

			// wait until every child has joined
			sem_wait(sem3);
			log_info("joiner%d : checking network state", i);
			EXPECT_TRUE_OR_EXIT(
				lookup_nodes(nodes_list, nb_childs + 1));

			// inform creator that we checked the network state
			log_info(
				"joiner%d : informing creator that I checked the network state",
				i);
			sem_post(sem2);

			// wait confirmation to leave
			log_info(
				"joiner%d : waiting for creator to inform me that I can leave",
				i);
			sem_wait(sem1);

			// we leave
			log_info("joiner%d : leaving the network", i);
			EXPECT_TRUE_OR_EXIT(leave_network() == 0);
			log_info("joiner%d : left the network", i);
			EXPECT_TRUE_OR_EXIT(check_none());
			log_info("joiner%d : informing creator that I left", i);
			sem_post(sem2);
			exit_comm();
			exit(0);
		}
	}

	init_comm();
	// we create the network
	log_info("creator : creating the network");
	EXPECT_EQ(join_network(&founder, NULL), 0);

	// inform child that he can join
	log_info("creator : informing joiners that they can join");
	post_on(sem1, nb_childs);

	// wait until every child joined
	log_info("creator : waiting for joiners to join");
	wait_on(sem2, nb_childs);

	// check network state
	log_info("creator : checking network state");
	ASSERT_TRUE(lookup_nodes(nodes_list, nb_childs + 1));

	// inform joiners that every one joined
	log_info("creator : informing joiners that everyone has joined");
	post_on(sem3, nb_childs);

	// wait that every joined checked his network state
	log_info("creator : waiting for joiners to check network state");
	wait_on(sem2, nb_childs);

	// inform joiners that they can leave
	log_info("creator : informing joiners that they can leave");
	post_on(sem1, nb_childs);

	// wait until every child left
	log_info("creator : waiting for joiners to leave");
	wait_on(sem2, nb_childs);

	// check that we are alone
	log_info("creator : checking that we are alone");
	ASSERT_TRUE(check_alone(&founder));

	// we leave
	log_info("creator : leaving the network");
	EXPECT_EQ(leave_network(), 0);
	log_info("creator : left the network");
	// check that there is no one in the network
	ASSERT_TRUE(check_none());

	exit_comm();
	wait_all(nb_childs);

	if (sem_close(sem1) == -1 || sem_close(sem2) == -1 ||
	    sem_close(sem3) == -1) {
		log_error("sem_close: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (sem_unlink("/sem1") == -1 || sem_unlink("/sem2") == -1 ||
	    sem_unlink("/sem3") == -1) {
		log_error("sem_unlink: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	free(nodes_list);
}