#include <gtest/gtest.h>

extern "C" {
#include "library.h"
#include "network/network.h"
#include "network/message.h"
#include "utils/list.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "core/data_transfer.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include "comm/comm.h"
}

static const int init_port = 2450;

struct node_id creator = { .host = "127.0.0.1", .port = init_port };
struct node_id joiner1 = { .host = "127.0.0.1", .port = init_port + 1 };
struct node_id joiner2 = { .host = "127.0.0.1", .port = init_port + 2 };
struct node_id joiner3 = { .host = "127.0.0.1", .port = init_port + 3 };
struct node_id joiner4 = { .host = "127.0.0.1", .port = init_port + 4 };
struct node_id joiner5 = { .host = "127.0.0.1", .port = init_port + 5 };

struct node_id all_nodes[6] = { creator, joiner1, joiner2,
				joiner3, joiner4, joiner5 };

static void wait_all(unsigned int times)
{
	for (unsigned int i = 0; i < times; i++)
		wait(NULL);
}

int join_network(const struct node_id *me, const struct node_id *father);

int leave_network(void);

int add_net_handler(const unsigned int type,
		    void (*callBack)(struct node_id *, void *));

int send_message1(const unsigned int type, const struct node_id *target,
		  const void *payload, const size_t size);

int broadcast_message1(const unsigned int type, const struct node_id **except,
		       const void *payload, const size_t size);

//WIP
//int multicast_message(const unsigned int type, struct node_id **targets, const void *payload, const size_t size);

struct node_id *get_network(size_t *size);

static bool lookup_nodes(const struct node_id nodes[], size_t size)
{
	size_t network_size = 0;
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
	}
	free(network);
	return true;
}

static bool check_none(void)
{
	struct node_id *network = NULL;
	size_t network_size = 0;

	network = get_network(&network_size);
	return (network_size == 0 && network == NULL);
}

static bool check_alone(const struct node_id *me)
{
	struct node_id *network = NULL;
	size_t network_size = 0;

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
			perror("sem_close");
			exit(EXIT_FAILURE);
		}
		char name[10];
		sprintf(name, "/sem%d", i);
		if (sem_unlink(name) == -1) {
			perror("sem_unlink");
			exit(EXIT_FAILURE);
		}
	}
}

TEST(join, create_and_leave)
{
	init_logger(stdout);

	log_info("Test : create and leave\n");
	init_comm();

	// check that there is no one in the network
	ASSERT_TRUE(check_none());

	join_network(&creator, NULL);
	// check if we are alone in the network
	ASSERT_TRUE(check_alone(&creator));

	leave_network();
	// check that there is no one in the network
	ASSERT_TRUE(check_none());

	exit_comm();
	log_info("End of test\n");
}

TEST(join, child_joins_father_then_exits)
{
	init_logger(stdout);

	log_info("Test : child joins father then exits\n");

	sem_t *sem1, *sem2;

	sem1 = sem_open("/sem1", O_CREAT, 0644, 0);
	sem2 = sem_open("/sem2", O_CREAT, 0644, 0);

	if (sem1 == SEM_FAILED || sem2 == SEM_FAILED) {
		perror("sem_open");
		exit(EXIT_FAILURE);
	}

	pid_t pid = fork();
	if (!pid) {
		init_comm();
		// wait until father is set up
		log_info("joiner1 : waiting for creator to set up\n");
		sem_wait(sem1);
		join_network(&joiner1, &creator);
		log_info("joiner1 : joined the network\n");

		// inform father that we joined
		sem_post(sem2);

		// check network state
		log_info("joiner1 : checking network state\n");
		const struct node_id to_lookup[2] = { joiner1, creator };
		ASSERT_TRUE(lookup_nodes(to_lookup, 1));

		// we leave
		log_info("joiner1 : leaving the network\n");
		leave_network();
		log_info("joiner1 : left the network\n");
		ASSERT_TRUE(check_none());

		// inform father that we leaved
		sem_post(sem2);
		exit_comm();
		exit(0);
	} else {
		init_comm();
		// we create the network
		log_info("creator : creating the network\n");
		join_network(&creator, NULL);

		// infrom child that he can join
		sem_post(sem1);

		// wait until child joined
		log_info("creator : waiting for joiner1 to join\n");
		sem_wait(sem2);

		// check network state
		log_info("creator : checking network state\n");
		struct node_id to_lookup[2] = { creator, joiner1 };
		ASSERT_TRUE(lookup_nodes(to_lookup, 2));

		// wait until child left
		log_info("creator : waiting for joiner1 to leave\n");
		sem_wait(sem2);

		// check that child really left
		log_info("creator : checking that we are alone\n");
		ASSERT_TRUE(check_alone(&creator));

		// we leave
		log_info("creator : leaving the network\n");
		leave_network();
		log_info("creator : left the network\n");

		// check that there is no one in the network
		log_info(
			"creator : checking that there is no one in the network\n");
		ASSERT_TRUE(check_none());
		exit_comm();
		wait_all(1);
	}
	if (sem_close(sem1) == -1 || sem_close(sem2) == -1) {
		perror("sem_close");
		exit(EXIT_FAILURE);
	}

	if (sem_unlink("/sem1") == -1 || sem_unlink("/sem2") == -1) {
		perror("sem_unlink");
		exit(EXIT_FAILURE);
	}
}

TEST(join, two_childs_join_then_nb2_leaves_then_nb1_leaves)
{
	init_logger(stdout);

	log_info("Test : two childs join then one leave\n");

	sem_t *sem1, *sem2, *sem3, *sem4;

	sem1 = sem_open("/sem1", O_CREAT, 0644, 0);
	sem2 = sem_open("/sem2", O_CREAT, 0644, 0);
	sem3 = sem_open("/sem3", O_CREAT, 0644, 0);
	sem4 = sem_open("/sem4", O_CREAT, 0644, 0);

	if (sem1 == SEM_FAILED || sem2 == SEM_FAILED || sem3 == SEM_FAILED ||
	    sem4 == SEM_FAILED) {
		perror("sem_open");
		exit(EXIT_FAILURE);
	}

	for (int i = 1; i <= 2; i++) {
		pid_t pid = fork();
		if (!pid) {
			init_comm();
			// wait until father is set up
			log_info("joiner%d : waiting for creator to set up\n",
				 i);
			sem_wait(sem1);
			log_info("joiner%d : joining the network\n", i);
			join_network(&all_nodes[i], &creator);
			log_info("joiner%d : joined the network\n", i);

			// inform father that we joined
			sem_post(sem2);

			// wait until every child has joined
			sem_wait(sem3);

			// check network state
			log_info("joiner%d : checking network state\n", i);
			ASSERT_TRUE(lookup_nodes(all_nodes, 3));
			// joiner2 can leave
			if (i == 1)
				sem_post(sem1);

			// we leave
			if (i == 2) {
				// wait until joiner1 checked his state
				sem_wait(sem1);
				log_info("joiner%d : leaving the network\n", i);
				leave_network();
				log_info("joiner%d : left the network\n", i);
				ASSERT_TRUE(check_none());

				// inform others that joiner2 left
				log_info(
					"joiner%d : informing others that I left\n",
					i);
				post_on(sem4, 2);
				exit_comm();
				exit(0);
			}

			// wait until joiner2 leaves
			sem_wait(sem4);
			// check that joiner2 left
			log_info("joiner%d : checking that joiner2 left\n", i);
			ASSERT_TRUE(lookup_nodes(all_nodes, 2));

			// wait until creator checked network state
			log_info(
				"joiner%d : waiting for creator to check network state\n",
				i);
			sem_wait(sem1);

			log_info("joiner%d : leaving the network\n", i);
			leave_network();
			log_info("joiner%d : left the network\n", i);
			sem_post(sem2);
			ASSERT_TRUE(check_none());

			exit_comm();
			exit(0);
		}
	}

	init_comm();
	// we create the network
	log_info("creator : creating the network\n");
	join_network(&creator, NULL);

	// inform child that he can join
	log_info("creator : informing joiners that they can join\n");
	post_on(sem1, 2);

	// wait until every child joined
	log_info("creator : waiting for joiners to join\n");
	wait_on(sem2, 2);

	// check network state
	log_info("creator : checking network state\n");
	ASSERT_TRUE(lookup_nodes(all_nodes, 3));

	// infrom child that everyone has joined
	log_info("creator : informing joiners that everyone has joined\n");
	post_on(sem3, 2);

	// wait until joiner2 leaves
	log_info("creator : waiting for joiner2 to leave\n");
	sem_wait(sem4);

	// check that joiner2 really left
	log_info("creator : checking that joiner2 left\n");
	ASSERT_TRUE(lookup_nodes(all_nodes, 2));

	// inform joiner1 that he can leave
	log_info("creator : informing joiner1 that he can leave\n");
	sem_post(sem1);

	// wait until joiner1 leaves
	log_info("creator : waiting for joiner1 to leave\n");
	sem_wait(sem2);

	// check that joiner1 really left
	log_info("creator : checking that joiner1 left\n");
	ASSERT_TRUE(check_alone(&creator));

	// we leave
	log_info("creator : leaving the network\n");
	leave_network();
	log_info("creator : left the network\n");

	// check that there is no one in the network
	log_info("creator : checking that there is no one in the network\n");
	ASSERT_TRUE(check_none());
	exit_comm();
	wait_all(2);

	if (sem_close(sem1) == -1 || sem_close(sem2) == -1 ||
	    sem_close(sem3) == -1 || sem_close(sem4) == -1) {
		perror("sem_close");
		exit(EXIT_FAILURE);
	}

	if (sem_unlink("/sem1") == -1 || sem_unlink("/sem2") == -1 ||
	    sem_unlink("/sem3") == -1 || sem_unlink("/sem4") == -1) {
		perror("sem_unlink");
		exit(EXIT_FAILURE);
	}
}

TEST(join, childs_join_father_then_leave)
{
	init_logger(stdout);

	log_info("Test : childs joins father then exits\n");

	sem_t *sem1, *sem2, *sem3;

	sem1 = sem_open("/sem1", O_CREAT, 0644, 0);
	sem2 = sem_open("/sem2", O_CREAT, 0644, 0);
	sem3 = sem_open("/sem3", O_CREAT, 0644, 0);

	if (sem1 == SEM_FAILED || sem2 == SEM_FAILED || sem3 == SEM_FAILED) {
		perror("sem_open");
		exit(EXIT_FAILURE);
	}

	for (int i = 1; i <= 5; i++) {
		pid_t pid = fork();
		if (!pid) {
			init_comm();

			// wait until father is set up
			sem_wait(sem1);
			log_info("joiner%d : joining the network\n", i);
			join_network(&all_nodes[i], &creator);
			log_info("joiner%d : joined the network\n", i);

			log_info("joiner%d : informing creator that I joined\n",
				 i);
			sem_post(sem2);

			// wait until every child has joined
			sem_wait(sem3);
			log_info("joiner%d : checking network state\n", i);
			ASSERT_TRUE(lookup_nodes(all_nodes, 6));

			// inform creator that we checked the network state
			log_info(
				"joiner%d : informing creator that I checked the network state\n",
				i);
			sem_post(sem2);

			// wait confirmation to leave
			log_info(
				"joiner%d : waiting for creator to inform me that I can leave\n",
				i);
			sem_wait(sem1);

			// we leave
			log_info("joiner%d : leaving the network\n", i);
			leave_network();
			log_info("joiner%d : left the network\n", i);
			ASSERT_TRUE(check_none());
			log_info("joiner%d : informing creator that I left\n",
				 i);
			sem_post(sem2);
			exit_comm();
			exit(0);
		}
	}

	init_comm();
	// we create the network
	log_info("creator : creating the network\n");
	join_network(&creator, NULL);

	// inform child that he can join
	log_info("creator : informing joiners that they can join\n");
	post_on(sem1, 5);

	// wait until every child joined
	log_info("creator : waiting for joiners to join\n");
	wait_on(sem2, 5);

	// check network state
	log_info("creator : checking network state\n");
	ASSERT_TRUE(lookup_nodes(all_nodes, 6));

	// inform joiners that every one joined
	log_info("creator : informing joiners that everyone has joined\n");
	post_on(sem3, 5);

	// wait that every joined checked his network state
	log_info("creator : waiting for joiners to check network state\n");
	wait_on(sem2, 5);

	// inform joiners that they can leave
	log_info("creator : informing joiners that they can leave\n");
	post_on(sem1, 5);

	// wait until every child left
	log_info("creator : waiting for joiners to leave\n");
	wait_on(sem2, 5);

	// check that we are alone
	log_info("creator : checking that we are alone\n");
	ASSERT_TRUE(check_alone(&creator));

	// we leave
	log_info("creator : leaving the network\n");
	leave_network();
	log_info("creator : left the network\n");
	// check that there is no one in the network
	ASSERT_TRUE(check_none());

	exit_comm();
	wait_all(5);

	if (sem_close(sem1) == -1 || sem_close(sem2) == -1 ||
	    sem_close(sem3) == -1) {
		perror("sem_close");
		exit(EXIT_FAILURE);
	}

	if (sem_unlink("/sem1") == -1 || sem_unlink("/sem2") == -1 ||
	    sem_unlink("/sem3") == -1) {
		perror("sem_unlink");
		exit(EXIT_FAILURE);
	}
}

TEST(join, childs_join_in_a_queue)
{
	init_logger(stdout);

	log_info("Test : any one joins any one\n");

	sem_t *sems[6];
	for (int i = 0; i < 6; i++) {
		char name[10];
		sprintf(name, "/sem%d", i);
		sems[i] = sem_open(name, O_CREAT, 0644, 0);
		if (sems[i] == SEM_FAILED) {
			perror("sem_open");
			exit(EXIT_FAILURE);
		}
	}

	for (int i = 1; i <= 5; i++) {
		pid_t pid = fork();
		if (!pid) {
			init_comm();
			if (i == 1) {
				log_info(
					"joiner%d : waiting for creator to set up\n",
					i);
			} else {
				log_info(
					"joiner%d : waiting for joiner%d to set up\n",
					i, i - 1);
			}
			sem_wait(sems[i]);
			join_network(&all_nodes[i], &all_nodes[i - 1]);
			log_info("joiner%d : joined the network\n", i);

			if (i != 5) {
				// inform next that he can join
				log_info(
					"joiner%d : informing joiner%d that he can join\n",
					i, i + 1);
				sem_post(sems[i + 1]);

				// wait until last one joined
				log_info(
					"joiner%d : waiting for joiner5 to join\n",
					i);
				sem_wait(sems[i]);
			} else {
				// inform every one that i joined
				log_info(
					"joiner%d : informing every one that I joined\n",
					i);
				for (int i = 0; i < 5; i++)
					sem_post(sems[i]);
			}

			// check network state
			log_info("joiner%d : checking network state\n", i);
			ASSERT_TRUE(lookup_nodes(all_nodes, 6));

			// wait until we can leave
			sem_wait(sems[i]);

			// we leave
			log_info("joiner%d : leaving the network\n", i);
			leave_network();
			log_info("joiner%d : left the network\n", i);
			ASSERT_TRUE(check_none());

			// inform creator that we left
			log_info("joiner%d : informing creator that I left\n",
				 i);
			sem_post(sems[0]);
			exit_comm();
			exit(0);
		}
	}

	init_comm();

	// we create the network
	log_info("creator : creating the network\n");
	join_network(&creator, NULL);

	// inform child that he can join
	log_info("creator : informing joiner1 that he can join\n");
	sem_post(sems[1]);

	// wait until joiner5 joined
	log_info("creator : waiting for joiner5 to join\n");
	sem_wait(sems[0]);

	// check network state
	log_info("creator : checking network state\n");
	ASSERT_TRUE(lookup_nodes(all_nodes, 6));

	// inform every one that they can leave
	log_info("creator : informing every one that they can leave\n");
	for (int i = 1; i <= 5; i++)
		sem_post(sems[i]);

	// wait until every child left
	log_info("creator : waiting for joiners to leave\n");
	wait_on(sems[0], 5);

	// check that i am alone in the network
	log_info("creator : checking that I am alone in the network\n");
	ASSERT_TRUE(check_alone(&creator));

	// i leave
	log_info("creator : leaving the network\n");
	leave_network();
	log_info("creator : left the network\n");

	// check that there is no one in the network
	log_info("creator : checking that there is no one in the network\n");
	ASSERT_TRUE(check_none());
	exit_comm();
	wait_all(5);

	close_all(sems, 6);
}

TEST(join, childs_join_any_one)
{
	init_logger(stdout);

	log_info("Test : any one joins any one\n");

	sem_t *sems[6];
	for (int i = 0; i < 6; i++) {
		char name[10];
		sprintf(name, "/sem%d", i);
		sems[i] = sem_open(name, O_CREAT, 0644, 0);
		if (sems[i] == SEM_FAILED) {
			perror("sem_open");
			exit(EXIT_FAILURE);
		}
	}

	for (int i = 1; i <= 5; i++) {
		pid_t pid = fork();
		if (!pid) {
			init_comm();
			log_info("joiner%d : waiting for joining\n", i);
			sem_wait(sems[i]);
			log_info("joiner%d : joining the network\n", i);
			switch (i) {
			case 1: {
				join_network(&all_nodes[i], &creator);
				log_info("joiner%d : joined the network\n", i);

				// inform joiner2 and joiner3 that they can join
				log_info(
					"joiner%d : informing joiner2 and joiner3 that they can join\n",
					i);
				sem_post(sems[2]);
				sem_post(sems[3]);
				break;
			}

			case 2: {
				join_network(&all_nodes[i], &joiner1);
				log_info("joiner%d : joined the network\n", i);

				// inform joiner4 that he can join
				log_info(
					"joiner%d : informing joiner4 that he can join\n",
					i);
				sem_post(sems[4]);
				break;
			}

			case 3: {
				join_network(&all_nodes[i], &joiner1);
				log_info("joiner%d : joined the network\n", i);

				// inform joiner5 that he can join
				log_info(
					"joiner%d : informing joiner5 that he can join\n",
					i);
				sem_post(sems[5]);
				break;
			}

			case 4: {
				join_network(&all_nodes[i], &joiner2);
				log_info("joiner%d : joined the network\n", i);

				// wait until joiner5 joined
				log_info(
					"joiner%d : waiting for joiner5 to join\n",
					i);
				sem_wait(sems[i]);

				// inform others creator, joiner1, joiner2, joiner3 that i joined
				log_info(
					"joiner%d : informing creator, joiner1, joiner2, joiner3 that I joined\n",
					i);
				for (int i = 0; i < 4; i++)
					sem_post(sems[i]);
				goto check_state;
				break;
			}

			default: {
				join_network(&all_nodes[i], &joiner3);
				log_info("joiner%d : joined the network\n", i);

				// inform all others that i joined
				log_info(
					"joiner%d : informing creator, joiner1, joiner2, joiner3, joiner4 that I joined\n",
					i);
				for (int i = 0; i < 5; i++)
					sem_post(sems[i]);
				goto check_state;
				break;
			}
			}

			// wait until joiner4 and joiner5 joined
			log_info(
				"joiner%d : waiting for joiner4 and joiner5 to join\n",
				i);
			wait_on(sems[i], 2);

check_state:
			// check network state
			log_info("joiner%d : checking network state\n", i);
			ASSERT_TRUE(lookup_nodes(all_nodes, 6));

			// wait from creator that we can leave
			log_info(
				"joiner%d : waiting for creator to inform me that I can leave\n",
				i);
			sem_wait(sems[i]);

			// we leave
			log_info("joiner%d : leaving the network\n", i);
			leave_network();
			log_info("joiner%d : left the network\n", i);
			ASSERT_TRUE(check_none());

			// inform creator that we left
			log_info("joiner%d : informing creator that I left\n",
				 i);
			sem_post(sems[0]);
			exit_comm();
			exit(0);
		}
	}

	init_comm();

	// we create the network
	log_info("creator : creating the network\n");
	join_network(&creator, NULL);

	// inform joiner1 that he can join
	log_info("creator : informing joiner1 that he can join\n");
	sem_post(sems[1]);

	// wait until joiner4 and joiner5 joined
	log_info("creator : waiting for joiner4 and joiner5 to join\n");
	wait_on(sems[0], 2);

	// check network state
	log_info("creator : checking network state\n");
	ASSERT_TRUE(lookup_nodes(all_nodes, 6));

	// inform every one that they can leave
	log_info("creator : informing every one that they can leave\n");
	for (int i = 1; i <= 5; i++)
		sem_post(sems[i]);

	// wait until every child left
	log_info("creator : waiting for joiners to leave\n");
	wait_on(sems[0], 5);

	// check that i am alone in the network
	log_info("creator : checking that I am alone in the network\n");
	ASSERT_TRUE(check_alone(&creator));

	// i leave
	log_info("creator : leaving the network\n");
	leave_network();
	log_info("creator : left the network\n");

	// check that there is no one in the network
	log_info("creator : checking that there is no one in the network\n");
	ASSERT_TRUE(check_none());
	exit_comm();
	wait_all(5);

	close_all(sems, 6);
}
