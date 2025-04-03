#include <cstdio>
#include <cstdlib>
#include <gtest/gtest.h>

extern "C" {
#include "network/message.h"
#include "network/network.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "core/core.h"
#include <semaphore.h>
#include <fcntl.h>
}

static const int init_port = 2455;
static const int joiner_port = 4396;

TEST(core, try_lock_and_read_on_same_page)
{
	init_logger(stdout);

	log_info("started test");

	struct node_id init = {
		.host = "127.0.0.1",
		.port = init_port,
	};

	struct node_id joiner = {
		.host = "127.0.0.1",
		.port = joiner_port,
	};
	
	sem_t *lock_init = sem_open("/lock_init", O_CREAT, 0644,0);
	if (!lock_init) {
		perror("sem_open");
		exit(EXIT_FAILURE);
	}

	sem_t *lock_join = sem_open("/lock_join", O_CREAT, 0644,0);
	if (!lock_join) {
		perror("sem_open");
		exit(EXIT_FAILURE);
	}

	pid_t pid = fork();
	if (pid) {
		log_info("proc 1 start");

		me = init;
		start_server(init_port);
		init_core(2, &init);

		ask_lock(1, WRITE);
		log_info("proc 1 enter SC");
		sleep(1);
		log_info("proc 1 leave SC");
		unlock(1, WRITE);

		ask_lock(1, READ);
		log_info("proc 1 read");
		sleep(1);
		log_info("proc 1 finish reading");
		unlock(1, READ);

		sem_post(lock_init);

		//wait
		sem_wait(lock_join);
		sem_close(lock_join);
		sem_unlink("/lock_join");

		stop_server();
		clean_core();

	} else {
		log_info("proc 2 start");

		me = joiner;
		start_server(joiner_port);
		init_core(2, &init);

		ask_lock(1, WRITE);
		log_info("proc 2 enter SC");
		sleep(1);
		log_info("proc 2 leave SC");
		unlock(1, WRITE);

		ask_lock(1, READ);
		log_info("proc 2 read");
		sleep(1);
		log_info("proc 2 finish reading");
		unlock(1, READ);

		sem_post(lock_join);

		//wait
		sem_wait(lock_init);
		sem_close(lock_init);
		sem_unlink("/lock_init");

		stop_server();
		clean_core();

		exit(0);
	}
}

TEST(core, try_lock_and_read_on_different_page)
{
	init_logger(stdout);

	log_info("started test");

	struct node_id init = {
		.host = "127.0.0.1",
		.port = init_port,
	};

	struct node_id joiner = {
		.host = "127.0.0.1",
		.port = joiner_port,
	};
	
	sem_t *lock_init = sem_open("/lock_init", O_CREAT, 0644,0);
	if (!lock_init) {
		perror("sem_open");
		exit(EXIT_FAILURE);
	}

	sem_t *lock_join = sem_open("/lock_join", O_CREAT, 0644,0);
	if (!lock_join) {
		perror("sem_open");
		exit(EXIT_FAILURE);
	}

	pid_t pid = fork();
	if (pid) {
		log_info("proc 1 start");

		me = init;
		start_server(init_port);
		init_core(2, &init);

		ask_lock(0, WRITE);
		log_info("proc 1 enter SC");
		sleep(1);
		log_info("proc 1 leave SC");
		unlock(0, WRITE);

		ask_lock(0, READ);
		log_info("proc 1 read");
		sleep(1);
		log_info("proc 1 finish reading");
		unlock(0, READ);

		sem_post(lock_init);
		
		sem_wait(lock_join);
		sem_close(lock_join);
		sem_unlink("/lock_join");

		stop_server();
		clean_core();

	} else {
		log_info("proc 2 start");

		me = joiner;
		start_server(joiner_port);
		init_core(2, &init);

		ask_lock(1, WRITE);
		log_info("proc 2 enter SC");
		sleep(1);
		log_info("proc 2 leave SC");
		unlock(1, WRITE);

		ask_lock(1, READ);
		log_info("proc 2 read");
		sleep(1);
		log_info("proc 2 finish reading");
		unlock(1, READ);

		sem_post(lock_join);

		sem_wait(lock_init);
		sem_close(lock_init);
		sem_unlink("/lock_init");

		stop_server();
		clean_core();

		exit(0);
	}
}