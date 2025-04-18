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
	pthread_mutex_lock(&(page_cv + page_id)->lock);
	node_copy(page_owners + page_id, new_owner);
	page_state[page_id] = (node_equal(new_owner, &me));
	pthread_mutex_unlock(&(page_cv + page_id)->lock);
}

void sync_page(size_t page_id)
{
	// check if we are already the owner
	pthread_mutex_lock(&(page_cv + page_id)->lock);
	struct node_id *owner = page_owners + page_id;
	if (node_equal(owner, &me) || page_state[page_id]) {
		pthread_mutex_unlock(&(page_cv + page_id)->lock);
		return;
	}
	(page_cv + page_id)->predicate = false;

	// ask for a page and wait until the page is synched
	size_t ms_sz;
	struct message *msg = build_ASK_PAGE_message(page_id, &ms_sz);
	log_info("waiting for page %zu\n", page_id);
	send_wait_message_nolock(owner, msg, ms_sz, page_cv + page_id);
	(page_cv + page_id)->predicate = true;
	pthread_mutex_unlock(&(page_cv + page_id)->lock);
	log_info("synched page %zu\n", page_id);
	free_message(msg);
}

void init_data_transfer(unsigned int nb_pages, struct node_id *owners)
{
	addHandler(RECV_PAGE, NULL, RECV_PAGE_handler);
	addHandler(ASK_PAGE, NULL, ASK_PAGE_handler);
	page_owners = malloc(nb_pages * sizeof(struct node_id));
	page_cv = malloc(nb_pages * sizeof(struct cond_var));
	page_state = malloc(nb_pages * sizeof(bool));

	for (unsigned int i = 0; i < nb_pages; i++) {
		pthread_mutex_init(&(page_cv + i)->lock, NULL);
		pthread_cond_init(&(page_cv + i)->cond, NULL);
		(page_cv + i)->predicate = false;
		page_state[i] = !owners;
		if (!owners)
			node_copy(page_owners + i, &me);
	}
	if (owners)
		memcpy(page_owners, owners, sizeof(struct node_id) * nb_pages);
}

void clean_data_transfer(void)
{
	for (unsigned int i = 0; i < nb_pages; i++) {
		pthread_mutex_destroy(&(page_cv + i)->lock);
		pthread_cond_destroy(&(page_cv + i)->cond);
	}
	free(page_owners);
	free(page_cv);
	free(page_state);
}

void get_page_owners(void *dst)
{
	for (unsigned int i = 0; i < nb_pages; i++)
		pthread_mutex_lock(&(page_cv + i)->lock);
	memcpy(dst, page_owners, nb_pages * sizeof(struct node_id));
	for (unsigned int i = 0; i < nb_pages; i++)
		pthread_mutex_unlock(&(page_cv + i)->lock);
}