#include <gtest/gtest.h>

extern "C" {
#include <semaphore.h>
#include <fcntl.h>
#include "utils/utils.h"
#include "core/core.h"
#include "utils/logger.h"
#include "network/network.h"
}

#define NB_PAGE 10
struct node_id token_owner {
	.host = "127.0.0.1", .port = 5641
};

TEST(core, try_lock_unlock_write)
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

        sem_post(sem1);
		sem_wait(sem2);

		init_core(NB_PAGE, &token_owner);

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

        sem_post(sem2);
		sem_wait(sem1);

		init_core(NB_PAGE, &token_owner);

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
}

TEST(core, try_lock_unlock_read)
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

        sem_post(sem1);
		sem_wait(sem2);

		init_core(NB_PAGE, &token_owner);

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

        sem_post(sem2);
		sem_wait(sem1);

		init_core(NB_PAGE, &token_owner);

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
}

TEST(core, try_lock_unlock_read_write)
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

        sem_post(sem1);
		sem_wait(sem2);

		init_core(NB_PAGE, &token_owner);

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

        sem_post(sem2);
		sem_wait(sem1);

		init_core(NB_PAGE, &token_owner);

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
}