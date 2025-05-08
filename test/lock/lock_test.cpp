#include <cstdlib>
#include <gtest/gtest.h>

extern "C" {
#include <semaphore.h>
#include <fcntl.h>
#include "utils/utils.h"
#include "lock/lock.h"
#include "utils/logger.h"
#include "network/network.h"
}

#define NB_PAGE 10
const struct node_id token_owner {
	.host = "127.0.0.1", .port = 5641
};

TEST(lock, try_lock_unlock_write)
{
	init_logger(stdout);

	sem_t *sem1, *sem2;

	sem1 = sem_open("/sem1", O_CREAT, 0644, 0);
	sem2 = sem_open("/sem2", O_CREAT, 0644, 0);

	if (sem1 == SEM_FAILED || sem2 == SEM_FAILED) {
		perror("sem_open");
		exit(EXIT_FAILURE);
	}

	pid_t pid = fork();
	if (pid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (pid != 0) {
		start_server(token_owner.port, token_owner.host);
		init_core(NB_PAGE, &token_owner);

		sem_post(sem1);
		sem_wait(sem2);

		log_info("ask WRITE lock");
		ask_lock(0, WRITE);
		log_info("get WRITE lock");
		sleep(1);
		log_info("unlock");
		unlock(0, WRITE);

		//not the best option but we need to wait for the handler to terminate before cleaning
		sleep(1);

		sem_post(sem1);
		sem_wait(sem2);

		clean_core();
		stop_server();

		wait(NULL);

	} else {
		start_server(7845, "127.0.0.1");
		init_core(NB_PAGE, &token_owner);

		sem_post(sem2);
		sem_wait(sem1);

		log_info("ask WRITE lock");
		ask_lock(0, WRITE);
		log_info("get WRITE lock");
		sleep(1);
		log_info("unlock");
		unlock(0, WRITE);

		//not the best option but we need to wait for the handler to terminate before cleaning
		sleep(1);

		sem_post(sem2);
		sem_wait(sem1);

		clean_core();
		stop_server();

		exit(0);
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

TEST(lock, try_lock_unlock_read)
{
	init_logger(stdout);

	sem_t *sem1, *sem2;

	sem1 = sem_open("/sem1", O_CREAT, 0644, 0);
	sem2 = sem_open("/sem2", O_CREAT, 0644, 0);

	if (sem1 == SEM_FAILED || sem2 == SEM_FAILED) {
		perror("sem_open");
		exit(EXIT_FAILURE);
	}

	pid_t pid = fork();
	if (pid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (pid != 0) {
		start_server(token_owner.port, token_owner.host);
		init_core(NB_PAGE, &token_owner);

		sem_post(sem1);
		sem_wait(sem2);

		log_info("ask READ lock");
		ask_lock(0, READ);
		log_info("get READ lock");
		sleep(1);
		log_info("unlock");
		unlock(0, READ);

		//not the best option but we need to wait for the handler to terminate before cleaning
		sleep(1);

		sem_post(sem1);
		sem_wait(sem2);

		clean_core();
		stop_server();

		wait(NULL);

	} else {
		start_server(7845, "127.0.0.1");
		init_core(NB_PAGE, &token_owner);

		sem_post(sem2);
		sem_wait(sem1);

		log_info("ask READ lock");
		ask_lock(0, READ);
		log_info("get READ lock");
		sleep(1);
		log_info("unlock");
		unlock(0, READ);

		//not the best option but we need to wait for the handler to terminate before cleaning
		sleep(1);

		sem_post(sem2);
		sem_wait(sem1);

		clean_core();
		stop_server();

		exit(0);
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

TEST(lock, try_lock_unlock_read_write)
{
	init_logger(stdout);

	sem_t *sem1, *sem2;

	sem1 = sem_open("/sem1", O_CREAT, 0644, 0);
	sem2 = sem_open("/sem2", O_CREAT, 0644, 0);

	if (sem1 == SEM_FAILED || sem2 == SEM_FAILED) {
		perror("sem_open");
		exit(EXIT_FAILURE);
	}

	pid_t pid = fork();
	if (pid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (pid != 0) {
		start_server(token_owner.port, token_owner.host);
		init_core(NB_PAGE, &token_owner);

		sem_post(sem1);
		sem_wait(sem2);

		log_info("ask WRITE lock");
		ask_lock(0, WRITE);
		log_info("get WRITE lock");
		sleep(1);
		log_info("unlock");
		unlock(0, WRITE);

		//not the best option but we need to wait for the handler to terminate before cleaning
		sleep(1);

		sem_post(sem1);
		sem_wait(sem2);

		clean_core();
		stop_server();

		wait(NULL);

	} else {
		start_server(7845, "127.0.0.1");
		init_core(NB_PAGE, &token_owner);

		sem_post(sem2);
		sem_wait(sem1);

		log_info("ask READ lock");
		ask_lock(0, READ);
		log_info("get READ lock");
		sleep(1);
		log_info("unlock");
		unlock(0, READ);

		//not the best option but we need to wait for the handler to terminate before cleaning
		sleep(1);

		sem_post(sem2);
		sem_wait(sem1);

		clean_core();
		stop_server();

		exit(0);
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

TEST(lock, try_leave)
{
	init_logger(stdout);

	sem_t *sem1, *sem2;

	struct node_id join = { .host = "127.0.0.1", .port = 7845 };

	init_nodes(&node_list);

	sem1 = sem_open("/sem1", O_CREAT, 0644, 0);
	sem2 = sem_open("/sem2", O_CREAT, 0644, 0);

	if (sem1 == SEM_FAILED || sem2 == SEM_FAILED) {
		perror("sem_open");
		exit(EXIT_FAILURE);
	}

	pid_t pid = fork();
	if (pid == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	if (pid != 0) {
		add_to_nodes(&node_list, join.host, join.port);
		start_server(token_owner.port, token_owner.host);

		sem_post(sem1);
		sem_wait(sem2);

		init_core(NB_PAGE, &token_owner);

		sem_post(sem1);
		sem_wait(sem2);

		log_info("try to leave the network");
		leave_core(join);
		log_info("the node successfully leave the network");

		sem_post(sem1);
		sem_wait(sem2);

		stop_server();

		log_info("node end");

		free_nodes(&node_list);
		nb_nodees = 0;

		wait(NULL);

	} else {
		start_server(join.port, join.host);
		add_to_nodes(&node_list, token_owner.host, token_owner.port);

		sem_post(sem2);
		sem_wait(sem1);

		init_core(NB_PAGE, &token_owner);

		sem_post(sem2);
		sem_wait(sem1);

		log_info("ask READ lock");
		ask_lock(0, READ);
		log_info("get READ lock");
		sleep(1);
		log_info("unlock");
		unlock(0, READ);

		sem_post(sem2);
		sem_wait(sem1);

		clean_core();
		stop_server();

		log_info("node end");

		free_nodes(&node_list);
		nb_nodees = 0;

		exit(0);
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
