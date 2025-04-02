#include <gtest/gtest.h>

extern "C" {
#include "network/message.h"
#include "network/network.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "core/core.h"
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

	pid_t pid = fork();
	if (pid) {
		me = init;
		start_server(init_port);
		init_core(2, &init);

        log_info("proc 1 try lock");

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
	} else {
		me = joiner;
		start_server(joiner_port);
		init_core(2, &init);

        log_info("proc 2 try lock");

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
	}

	stop_server();
	clean_core();
}