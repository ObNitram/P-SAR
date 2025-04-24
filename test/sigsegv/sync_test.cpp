#include <gtest/gtest.h>

extern "C" {
#include "library.h"
#include "utils/utils.h"
#include "core/core.h"
#include "core/data_transfer.h"
#include "utils/logger.h"

#include <stdatomic.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/mman.h>
}

TEST(sync, mmm)
{
	std::size_t size = 4096 * 2;
	int init_port = 9000;
	int join_port = 14001;
	bool parent = true;
	sem_t *sem_a = (sem_t *)mmap(NULL, 2 * sizeof(sem_t),
				     PROT_READ | PROT_WRITE,
				     MAP_ANONYMOUS | MAP_SHARED, 0, 0);
	if (!sem_a) {
		perror("out of memory\n");
		exit(1);
	}
	sem_init(sem_a, 1, 0);

	sem_t *sem_b = sem_a + 1;
	sem_init(sem_b, 1, 0);

	// printf("before fork\n");
	if (!fork()) {
		parent = false;
		// printf("%b, init %i\n", parent, getpid());

		void *dsm = Init_DSM(size, NULL, init_port);
		int *int_dsm = (int *)dsm;
		sem_post(sem_a);
		// printf("%b, post 1\n", parent);

		sem_wait(sem_b);
		// printf("%b, wait 1\n", parent);

		// printf("%b __________ BEFORE LOCK WRITE\n", parent);
		log_info("lock write");
		lock_write(dsm, sizeof(int));
		// printf("%b __________ AFTER LOCK WRITE\n", parent);
		sem_post(sem_a);
		// printf("%b, post 2\n", parent);
		*int_dsm = 42;
		// printf("%b __________ BEFORE UNLOCK WRITE\n", parent);
		unlock_write(dsm, sizeof(int));
		// printf("%b __________ AFTER UNLOCK WRITE\n", parent);

		sem_wait(sem_b);
		// printf("%b, wait 2\n", parent);

		lock_read(dsm, sizeof(int));
		// printf("%b, lock_read\n", parent);
		ASSERT_EQ(*int_dsm, 43);
		// printf("%b, after reading\n", parent);
		unlock_read(dsm, sizeof(int));

		sem_post(sem_a);
		// printf("%b, post 3\n", parent);
		stop_server();
		clean_data_transfer();
		clean_core();
		free_nodes(&node_list);
		munmap(dsm, nb_pages * PAGE_SIZE);
	} else {
		// printf("%b, start %i\n", parent, getpid());
		// wait until INIT is setup
		sem_wait(sem_a);
		// printf("%b, wait 1\n", parent);

		void *dsm = join_DSM("127.0.0.1", init_port, NULL, join_port);
		int *int_dsm = (int *)dsm;

		sem_post(sem_b);
		// printf("%b, post 1\n", parent);

		sem_wait(sem_a);
		// printf("%b, wait 2\n", parent);

		// printf("%b __________ BEFORE LOCK WRITE\n", parent);
		lock_write(dsm, sizeof(int));
		// printf("%b __________ AFTER LOCK WRITE\n", parent);
		int tmp = *int_dsm;
		ASSERT_EQ(tmp, 42);
		(*int_dsm)++;
		unlock_write(dsm, sizeof(int));
		// printf("%b __________ AFTER UNLOCK WRITE\n", parent);

		sem_post(sem_b);
		// printf("%b, post 2\n", parent);

		sem_wait(sem_a);
		// printf("%b, wait 3\n", parent);
		stop_server();
		clean_data_transfer();
		clean_core();
		munmap(dsm, nb_pages * PAGE_SIZE);
	}
	munmap(sem_a, 2 * sizeof(sem_t));
	nb_pages = 0;
	nb_nodees = 0;
}
