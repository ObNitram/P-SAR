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
#include "network/network.h"
#include "network/message.h"
#include "core/core.h"
#include "utils/utils.h"
#include "utils/logger.h"
#include "library_messages.h"
#include "Naimi_Trehel.h"
#include "notification/notification.h"
#include "memory/memory.h"
#include "comm/comm.h"

// 1 if we have joined the DSM else 0
static struct cond_var cv = COND_VAR_INIT;
static bool in_dsm = false;
static bool acked = false;

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

static void create_all_chans(bool is_owner)
{
	memory_fd1 = create_chan(memory_lock_status_notification);
	init_memory(memory_fd1);
	sigsegv_fd1 = create_chan(sigsegv_sync_page_notification);
	sigsegv_fd2 = create_chan(sigsegv_lock_status_notification);
	sigsegv_fd3 = create_chan(sigsegv_send_invalidation_notification);
	init_sigsegv(dsm, nb_pages, is_owner,
		     (int[]){ sigsegv_fd1, sigsegv_fd2, sigsegv_fd3 });
}

void destroy_all_chans(void)
{
	destroy_chan(memory_lock_status_notification, memory_fd1);
	destroy_chan(sigsegv_sync_page_notification, sigsegv_fd1);
	destroy_chan(sigsegv_lock_status_notification, sigsegv_fd2);
	destroy_chan(sigsegv_send_invalidation_notification, sigsegv_fd3);
}

static void wait_in_dsm(void)
{
	pthread_mutex_lock(&cv.lock);
	while (!in_dsm)
		pthread_cond_wait(&cv.cond, &cv.lock);
	pthread_mutex_unlock(&cv.lock);
}

static void signal_in_dsm(void)
{
	pthread_mutex_lock(&cv.lock);
	in_dsm = true;
	cv.predicate = true;
	pthread_cond_broadcast(&cv.cond);
	pthread_mutex_unlock(&cv.lock);
}

static void wait_acked(void)
{
	pthread_mutex_lock(&cv.lock);
	while (!acked)
		pthread_cond_wait(&cv.cond, &cv.lock);
	acked = false;
	pthread_mutex_unlock(&cv.lock);
}

static void signal_acked(void)
{
	pthread_mutex_lock(&cv.lock);
	acked = true;
	pthread_cond_broadcast(&cv.cond);
	pthread_mutex_unlock(&cv.lock);
}

static void NEW_NODE_handler(struct message *message)
{
	struct node_id *new_node =
		&((struct NEW_NODE_message *)message)->new_node;
	pthread_mutex_lock(&umtx);
	add_to_nodes(&node_list, new_node->host, new_node->port);
	// we ack the addition of the existing node
	struct message msg = { .message_type = ACK_NODE };
	send_message(&message->sender, &msg, sizeof(struct message));
	pthread_mutex_unlock(&umtx);
}

static void ACK_NODE_handler(struct message *message)
{
	signal_acked();
}

static void JOIN_DSM_handler(struct message *message)
{
	// wait until we joined the dsm
	wait_in_dsm();

	request_CS();

	struct NEW_NODE_message *msg = build_NEW_NODE_message(&message->sender);
	struct node_list *n1 = &node_list;

	pthread_mutex_lock(&umtx);
	list_for_each_entry_continue(n1, &node_list.nlist, nlist) {
		send_message(&n1->node, (struct message *)msg,
			     sizeof(struct NEW_NODE_message));
		// wait for the ACK from the node
		wait_acked();
	}
	free_message((struct message *)msg);

	size_t sz = 0;
	struct INFO_DSM_message *dsm_info = build_INFO_DSM_message(&sz);

	send_message(&message->sender, (struct message *)dsm_info, sz);
	add_to_nodes(&node_list, message->sender.host, message->sender.port);
	free_message((struct message *)dsm_info);
	pthread_mutex_unlock(&umtx);

	release_CS();
}

static void INFO_DSM_handler(struct message *message)
{
	struct INFO_DSM_message *idsm = (struct INFO_DSM_message *)message;
	nb_pages = idsm->nb_pages;

	void *addr = idsm + 1;

	pthread_mutex_lock(&umtx);
	struct node_id *n = (struct node_id *)addr;
	for (unsigned int i = 0; i < idsm->nb_nodes; i++) {
		add_to_nodes(&node_list, n->host, n->port);
		n++;
	}
	pthread_mutex_unlock(&umtx);
	dsm = mmap(0, nb_pages * PAGE_SIZE, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	create_all_chans(false);
	init_data_transfer(nb_pages, n);

	// we joined the DSM notify if there is some waiting requests
	signal_in_dsm();
}

static void INVALIDATION_tmp_handler(struct message *message)
{
	wait_in_dsm();
	size_t *page_id = (size_t *)(message + 1);
	set_new_owner(*page_id, &message->sender);
}

static void set_all_handlers(void)
{
	addHandler(NEW_NODE, NULL, NEW_NODE_handler);
	addHandler(JOIN_DSM, NULL, JOIN_DSM_handler);
	addHandler(INFO_DSM, NULL, INFO_DSM_handler);
	addHandler(INVALIDATION, NULL, INVALIDATION_tmp_handler);
	addHandler(ACK_NODE, NULL, ACK_NODE_handler);
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

void *Init_DSM(size_t size, const char *interface, int port)
{
	// memory init
	cv.predicate = false;
	nb_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	dsm = mmap(0, nb_pages * PAGE_SIZE, PROT_READ | PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (dsm == MAP_FAILED) {
		perror("map allocation failed");
		return NULL;
	}

	init_comm();
	start_server(port, interface);
	init_CS(&EMPTY_NODE, 1, 0);
	init_nodes(&node_list);
	set_all_handlers();
	create_all_chans(true);
	init_core(nb_pages, &me);
	init_data_transfer(nb_pages, NULL);

	signal_in_dsm();
	return dsm;
}

static void free_DSM(void)
{
	munmap(dsm, nb_pages * PAGE_SIZE);
	cv.predicate = 0;
}

void *join_DSM(const char *host, int connect_port, const char *interface,
	       int server_port)
{
	cv.predicate = false;
	init_nodes(&node_list);
	init_comm();
	start_server(server_port, interface);
	set_all_handlers();

	struct node_id *nd =
		&add_to_nodes(&node_list, host, connect_port)->node;
	init_CS(nd, 0, 0);

	struct message mess_joining;
	mess_joining.message_type = JOIN_DSM;
	send_wait_message(nd, &mess_joining, sizeof(struct message), &cv);
	init_core(nb_pages, nd);

	if (dsm == MAP_FAILED) {
		perror("map allocation failed");
		stop_server();
		free_nodes(&node_list);
		clean_core();
		return NULL;
	}
	create_all_chans(false);
	return dsm;
}

void *leave_DSM(void)
{
	request_CS();
	if (list_empty(&node_list.nlist)) {
		stop_server();
		clean_core();
		clean_data_transfer();
		clean_CS();
		clean_sigsegv();
		destroy_all_chans();
		exit_comm();
		void *res = malloc(PAGE_SIZE * nb_pages);
		if (!res) {
			perror("unable to allocate memory for res");
			goto exit;
		}
		for (unsigned int i = 0; i < nb_pages; i++)
			memory_unlock_read((size_t)i);
		memcpy(res, dsm, PAGE_SIZE * nb_pages);
exit:
		free_DSM();
		return res;
	}
	struct node_id succ = list_prev_entry(&node_list, nlist)->node;
	leave_core(succ);
	leave_data_transfer(succ);
	leave_CS(succ);
	stop_server();
	clean_sigsegv();
	free_DSM();
	free_nodes(&node_list);
	destroy_all_chans();
	exit_comm();
	return NULL;
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
