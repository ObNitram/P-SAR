#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

#include "library.h"
#include "sigsegv_handler/sigsegv.h"
#include "utils/utils.h"
#include "utils/cond_var.h"
#include "lock/lock.h"
#include "utils/logger.h"
#include "notification/notification.h"
#include "memory/memory.h"
#include "comm/comm.h"
#include "network/network.new.h"
#include "core/data_transfer.h"

static struct cond_var cv = COND_VAR_INIT;
static bool in_dsm = false;

static void wait_in_dsm(void)
{
	pthread_mutex_lock(&cv.lock);
	while (!in_dsm) {
		pthread_cond_wait(&cv.cond, &cv.lock);
	}
	pthread_mutex_unlock(&cv.lock);
}

static void signal_in_dsm(void)
{
	pthread_mutex_lock(&cv.lock);
	in_dsm = true;
	pthread_cond_broadcast(&cv.cond);
	pthread_mutex_unlock(&cv.lock);
}

static void memory_lock_status_notification(int fd)
{
	size_t index = 0;
	log_info("damn we've been called, we'll see\n");
	read(fd, &index, sizeof(size_t));
	log_info("read thy indexoo\n");
	enum lock_status lock_status = get_lock_status(index);
	int prot;
	switch (lock_status) {
	case NONE:
		prot = PROT_NONE;
		break;
	case READING:
	case WRITING:
		prot = PROT_READ;
		break;
	}
	log_info("write res\n");
	write(fd, &prot, sizeof(int));
	log_info("res written\n");
}
static int memory_fd1 = 0;

static void sigsegv_sync_page_notification(int fd)
{
	size_t page_index = 0;
	read(fd, &page_index, sizeof(size_t));
	sync_page(page_index);
	char eof = 0;
	write(fd, &eof, sizeof(char));
}
static int sigsegv_fd1 = 0;

static void sigsegv_lock_status_notification(int fd)
{
	int res = 0;
	size_t page_index = 0;
	bool curr_writing = 0;
	read(fd, &page_index, sizeof(size_t));
	read(fd, &curr_writing, sizeof(bool));
	enum lock_status lock_status = get_lock_status(page_index);
	// You're not allowed to do that you criminal, how dare you
	if (lock_status == NONE)
		res = -1;
	// You don't have the right do to this my dude
	if (lock_status == READING) {
		if (curr_writing != false)
			res = -1;
	} else if (curr_writing && lock_status == WRITING) {
		// Do not put the write permission too soon.
		// We want to know if the user will write and only then send the invalidation
		// And if we put the write permission when the first read happen, we'll just never know if a read happen
		// It's a mystery~
		res = PROT_WRITE;
	}
	write(fd, &res, sizeof(int));
}
static int sigsegv_fd2 = 0;

static void sigsegv_send_invalidation_notification(int fd)
{
	size_t page_index = 0;
	read(fd, &page_index, sizeof(size_t));
	send_invalidation(page_index);
}
static int sigsegv_fd3 = 0;

static void create_all_chans(void)
{
	memory_fd1 = create_chan(memory_lock_status_notification);
	sigsegv_fd1 = create_chan(sigsegv_sync_page_notification);
	sigsegv_fd2 = create_chan(sigsegv_lock_status_notification);
	sigsegv_fd3 = create_chan(sigsegv_send_invalidation_notification);
}

void destroy_all_chans(void)
{
	destroy_chan(memory_lock_status_notification, memory_fd1);
	destroy_chan(sigsegv_sync_page_notification, sigsegv_fd1);
	destroy_chan(sigsegv_lock_status_notification, sigsegv_fd2);
	destroy_chan(sigsegv_send_invalidation_notification, sigsegv_fd3);
}

static void *build_INFO_DSM_message(size_t *sz)
{
	// total size of the mess
	*sz = sizeof(unsigned int) + nb_pages * sizeof(struct node_id);

	void *dsm_info = malloc(*sz);
	*((unsigned int *)(dsm_info)) = nb_pages;

	// copy th page owners
	get_page_owners(dsm_info + sizeof(unsigned int));
	return dsm_info;
}

static void handle_JOIN_DSM(struct node_id *sender, void *payload)
{
	wait_in_dsm();

	size_t sz = 0;
	void *payload = build_INFO_DSM_message(&sz);
	if (send_message1(INFO_DSM, sender, payload, sz) == -1) {
		log_error("fail to send INFO_DSM");
	}
	free(payload);
}

// niveau 2 de profondeur dans le graphe de deps
static void init_lvl2(bool is_owner, void *page_owners)
{
	init_core(nb_pages, &me);
	init_data_transfer(nb_pages, page_owners);
	init_sigsegv(dsm, nb_pages, is_owner,
		     (int[3]){ sigsegv_fd1, sigsegv_fd2, sigsegv_fd3 });
}

static void *exit_lvl2()
{
	exit_sigsegv();
	// correct ?
	unsigned int network_size = 0;
	struct node_id *network = get_network(&network_size);
	void *res = NULL;
	if (network_size == 1) {
		clean_data_transfer();
		clean_core();
		res = malloc(PAGE_SIZE * nb_pages);
		memcpy(res, dsm, PAGE_SIZE * nb_pages);
	} else {
		const struct node_id succ =
			(node_equal(&network[network_size - 1], &me)) ?
				network[0] :
				network[network_size - 1];
		exit_data_transfer(succ);
		exit_core(succ);
	}
	munmap(dsm, nb_pages * PAGE_SIZE);
	dsm = NULL;
	cv.predicate = false;
	free(network);
	return res;
}

// niveau 3 de profondeur dans le graphe de deps
static void init_lvl3(const struct node_id *father)
{
	init_memory(memory_fd1);
	join_network(&me, father);
}

static void exit_lvl3()
{
	leave_network();
	exit_memory();
}

// niveau 4 de profondeur dans le graphe de deps
static void init_lvl4(void)
{
	init_comm();
	create_all_chans();
}

static void exit_lvl4(void)
{
	destroy_all_chans();
	exit_comm();
}

static void handle_INFO_DSM(struct node_id *sender, void *payload)
{
	nb_pages = *((unsigned int *)payload);
	dsm = mmap(0, nb_pages * PAGE_SIZE, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (dsm == MAP_FAILED) {
		log_error("map allocation failed");
		exit(EXIT_FAILURE);
	}
	init_lvl2(false, payload + sizeof(unsigned int));
	signal_in_dsm();
}

static void set_all_handlers(void)
{
	add_net_handler(JOIN_DSM, handle_JOIN_DSM);
	add_net_handler(INFO_DSM, handle_INFO_DSM);
}

static void exclude_others(void *adr, size_t s, enum lock_type lock_type,
			   void (*exc_func)(size_t, enum lock_type))
{
	size_t start_index = get_page_index(adr);
	size_t end_index = get_page_index(adr + s - 1);
	for (size_t page_id = start_index; page_id <= end_index; page_id++) {
		memory_lock(page_id);
		exc_func(page_id, lock_type);
	}
}

static void init_me(const char *interface, int port)
{
	me.port = port;
	strcpy(me.host, interface);
}

void *Init_DSM(size_t size, const char *interface, int port)
{
	nb_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	dsm = mmap(0, nb_pages * PAGE_SIZE, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (dsm == MAP_FAILED) {
		log_error("map allocation failed");
		exit(EXIT_FAILURE);
	}
	set_all_handlers();
	init_me(interface, port);

	init_lvl4();
	init_lvl3(NULL);
	init_lvl2(true, NULL);

	signal_in_dsm();
	return dsm;
}

void *join_DSM(const char *host, int connect_port, const char *interface,
	       int server_port)
{
	set_all_handlers();
	init_me(interface, server_port);

	init_lvl4();

	struct node_id father;
	strcpy(father.host, host);
	father.port = connect_port;
	init_lvl3(&father);

	send_message1(JOIN_DSM, &father, &server_port, sizeof(int));
	wait_in_dsm();
	return dsm;
}

void *leave_DSM(void)
{
	void *res = NULL;
	res = exit_lvl2();
	exit_lvl3();
	exit_lvl4();
	return res;
}

void lock_read(void *adr, size_t s)
{
	exclude_others(adr, s, READ, ask_lock);
}

void unlock_read(void *adr, size_t s)
{
	exclude_others(adr, s, READ, unlock);
}

void lock_write(void *adr, size_t s)
{
	exclude_others(adr, s, WRITE, ask_lock);
}

void unlock_write(void *adr, size_t s)
{
	exclude_others(adr, s, WRITE, unlock);
}
