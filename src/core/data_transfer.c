#include <pthread.h>
#include <stdlib.h>

#include "data_transfer.h"
#include "data_transfer_utils.h"
#include "../core/core.h"
#include "../network/network.h"
#include "../network/cond_var.h"
#include "../sigsegv_handler/sigsegv.h"
#include "../utils/utils.h"
#define DISABLE_LOG
#include "../utils/logger.h"

struct node_id *page_owners;

void set_new_owner(size_t page_id, struct node_id *new_owner)
{
	pthread_mutex_lock(page_mtx + page_id);
	node_copy(page_owners + page_id, new_owner);
	page_state[page_id] = 0;
	pthread_mutex_unlock(page_mtx + page_id);
}

void sync_page(size_t page_id)
{
	// check if we are already the owner
	pthread_mutex_lock(page_mtx + page_id);
	struct node_id *owner = page_owners + page_id;
	if (node_equal(page_owners + page_id, &me) || page_state[page_id]) {
		pthread_mutex_unlock(page_mtx + page_id);
		return;
	}
	page_in_transit[page_id] = 1;
	pthread_mutex_unlock(page_mtx + page_id);

	// ask for a page and wait until the page is synched
	size_t ms_sz;
	struct message *msg = build_ASK_PAGE_message(page_id, &ms_sz);
	send_message(owner, msg, ms_sz);
	log_info("waiting for page %zu\n", page_id);
	wait_signal(page_id);
	log_info("synched page %zu\n", page_id);
	free_message(msg);
}

void init_data_transfer(unsigned int nb_pages, struct node_id *owners)
{
	addHandler(RECV_PAGE, NULL, RECV_PAGE_handler);
	addHandler(ASK_PAGE, NULL, ASK_PAGE_handler);
	page_owners = malloc(nb_pages * sizeof(struct node_id));
	page_mtx = malloc(nb_pages * sizeof(pthread_mutex_t));
	page_cond = malloc(nb_pages * sizeof(pthread_cond_t));
	page_in_transit = malloc(nb_pages * sizeof(bool));
	page_state = malloc(nb_pages * sizeof(bool));

	for (unsigned int i = 0; i < nb_pages; i++) {
		pthread_mutex_init(page_mtx + i, NULL);
		pthread_cond_init(page_cond + i, NULL);
		page_in_transit[i] = 0;
		if (!owners) {
			node_copy(page_owners + i, &me);
			page_state[i] = 1;
		} else
			page_state[i] = 0;
	}
	if (owners) {
		memcpy(page_owners, owners, sizeof(struct node_id) * nb_pages);
	}
}

void clean_data_transfer()
{
	free(page_owners);
	for (unsigned int i = 0; i < nb_pages; i++) {
		pthread_mutex_destroy(page_mtx + i);
		pthread_cond_destroy(page_cond + i);
	}
	free(page_mtx);
	free(page_cond);
	free(page_in_transit);
	free(page_state);
}

void get_page_owners(void *dst)
{
	for (unsigned int i = 0; i < nb_pages; i++)
		pthread_mutex_lock(page_mtx + i);
	memcpy(dst, page_owners, nb_pages * sizeof(struct node_id));
	for (unsigned int i = 0; i < nb_pages; i++)
		pthread_mutex_unlock(page_mtx + i);
}