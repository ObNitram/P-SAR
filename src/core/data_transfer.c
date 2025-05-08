#include <pthread.h>
#include <stdlib.h>

#include "data_transfer.h"
#include "data_transfer_utils.h"
#include "network/network.h"
#include "utils/cond_var.h"
#include "sigsegv_handler/sigsegv.h"
#include "utils/utils.h"
#define DISABLE_LOG
#include "utils/logger.h"

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
	struct cond_var *cv = page_cv + page_id;
	pthread_mutex_lock(&cv->lock);
	struct node_id *owner = page_owners + page_id;
	if (node_equal(owner, &me) || page_state[page_id]) {
		pthread_mutex_unlock(&cv->lock);
		return;
	}
	(page_cv + page_id)->predicate = false;

	// ask for a page and wait until the page is synched
	size_t ms_sz;
	struct message *msg = build_ASK_PAGE_message(page_id, &ms_sz);

	log_info("waiting for page %zu\n", page_id);
	send_wait_message_nolock(owner, msg, ms_sz, page_cv + page_id);

	log_info("synched page %zu\n", page_id);
	(page_cv + page_id)->predicate = true;
	pthread_mutex_unlock(&cv->lock);
	free_message(msg);
}

void init_data_transfer(unsigned int nb_pages, struct node_id *owners)
{
	addHandler(RECV_PAGE, NULL, RECV_PAGE_handler);
	addHandler(ASK_PAGE, NULL, ASK_PAGE_handler);
	addHandler(ACK_RECV_PAGE, NULL, ACK_RECV_PAGE_handler);
	addHandler(RECV_PAGE_LEAVE, NULL, RECV_PAGE_LEAVE_handler);
	addHandler(DT_LEAVE, NULL, DT_LEAVE_handler);

	page_owners = malloc(nb_pages * sizeof(struct node_id));
	page_cv = malloc(nb_pages * sizeof(struct cond_var));
	page_state = malloc(nb_pages * sizeof(bool));

	pthread_mutex_init(&(dt_cv).lock, NULL);
	pthread_cond_init(&(dt_cv).cond, NULL);
	dt_cv.predicate = true;
	for (unsigned int i = 0; i < nb_pages; i++) {
		pthread_mutex_init(&(page_cv + i)->lock, NULL);
		pthread_cond_init(&(page_cv + i)->cond, NULL);
		(page_cv + i)->predicate = true;
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
	pthread_mutex_destroy(&dt_cv.lock);
	pthread_cond_destroy(&dt_cv.cond);
	free(page_owners);
	free(page_cv);
	free(page_state);
}

void leave_data_transfer(const struct node_id new_owner)
{
	// we wont treat any request further here
	addHandler(RECV_PAGE, NULL, NULL);
	addHandler(ASK_PAGE, NULL, NULL);
	size_t nb_owned_pages = 0;
	size_t index_pages[nb_pages];

	for (size_t i = 0; i < nb_pages; i++) {
		// look for the pages we own
		struct cond_var *cv = page_cv + i;
		pthread_mutex_lock(&cv->lock);
		if (node_equal(page_owners + i, &me)) {
			index_pages[nb_owned_pages++] = i;
			continue;
		}
		pthread_mutex_unlock(&cv->lock);
	}
	dt_cv.predicate = false;
	log_info("Inform new Owner\n");
	size_t ms_sz;
	struct message *msg =
		build_multiple_PAGE_message(index_pages, nb_owned_pages, &ms_sz,
					    &new_owner, RECV_PAGE_LEAVE);
	send_wait_message(&new_owner, msg, ms_sz, &dt_cv);
	free_message(msg);
	log_info("ACK recved from new Owner\n");

	// broadcast to each other node, the info about the new owner
	pthread_mutex_lock(&umtx);
	msg = build_DT_LEAVE_message(index_pages, nb_owned_pages, &new_owner,
				     &ms_sz);
	struct node_list *n = &node_list;
	log_info("Informing %u nodes that i leave\n", nb_nodees - 1);
	list_for_each_entry_continue(n, &node_list.nlist, nlist) {
		if (node_equal(&n->node, &new_owner))
			continue;
		dt_cv.predicate = false;
		send_wait_message(&n->node, msg, ms_sz, &dt_cv);
	}
	log_info("ACKED all, leave done !\n");
	free_message(msg);
	pthread_mutex_unlock(&umtx);

	for (size_t i = 0; i < nb_owned_pages; i++)
		pthread_mutex_unlock(&(page_cv + index_pages[i])->lock);

	clean_data_transfer();
}

void get_page_owners(void *dst)
{
	for (unsigned int i = 0; i < nb_pages; i++)
		pthread_mutex_lock(&(page_cv + i)->lock);
	memcpy(dst, page_owners, nb_pages * sizeof(struct node_id));
	for (unsigned int i = 0; i < nb_pages; i++)
		pthread_mutex_unlock(&(page_cv + i)->lock);
}