#include <pthread.h>
#include <stdlib.h>

#include "data_transfer.h"
#include "data_transfer_utils.h"
#include "../network/network.new.h"
#include "../utils/cond_var.h"
#include "../utils/counter_cond_var.h"
#include "../utils/utils.h"
#include "../network/utils/message_type.h"
#include "../memory/memory.h"
// #define DISABLE_LOG
#include "../utils/logger.h"

struct node_id *page_owners;

void set_new_owner(size_t page_id, struct node_id *new_owner)
{
	pthread_mutex_lock(&(page_cv + page_id)->lock);
	node_copy(page_owners + page_id, new_owner);
	page_state[page_id] = (node_equal(new_owner, &me));
	pthread_mutex_unlock(&(page_cv + page_id)->lock);
}

static void handle_INVALIDATION(struct node_id *sender, void *payload)
{
	size_t *page_id = (size_t *)(payload);
	memory_lock(*page_id);
	set_new_owner(*page_id, sender);
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
	cv->predicate = false;

	// ask for a page and wait until the page is synched
	size_t payload_sz;
	void *payload = build_ASK_PAGE_message(page_id, &payload_sz);
	send_message1(ASK_PAGE, owner, payload, payload_sz);

	log_info("waiting for page %zu\n", page_id);
	wait_on_cond(cv, false);
	log_info("synched page %zu\n", page_id);

	pthread_mutex_unlock(&cv->lock);
	free(payload);
}

void init_data_transfer(unsigned int nb_pages, struct node_id *owners)
{
	add_net_handler(ASK_PAGE, handle_ASK_PAGE);
	add_net_handler(RECV_PAGE, handle_RECV_PAGE);
	add_net_handler(RECV_PAGE_LEAVE, handle_RECV_PAGE_LEAVE);
	add_net_handler(ACK_RECV_PAGE, handle_ACK_RECV_PAGE);
	add_net_handler(DT_LEAVE, handle_DT_LEAVE);
	add_net_handler(ACK_DT_LEAVE, handle_ACK_DT_LEAVE);
	add_net_handler(INVALIDATION, handle_INVALIDATION);

	page_owners = malloc(nb_pages * sizeof(struct node_id));
	page_cv = malloc(nb_pages * sizeof(struct cond_var));
	page_state = malloc(nb_pages * sizeof(bool));

	init_counter(&ack_counter);
	init_cond(&leaving_cv);
	for (unsigned int i = 0; i < nb_pages; i++) {
		init_cond(page_cv + i);
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
		destroy_cond(page_cv + i);
	}
	destroy_cond(&leaving_cv);
	destroy_counter(&ack_counter);
	free(page_owners);
	free(page_cv);
	free(page_state);
}

void exit_data_transfer(struct node_id new_owner)
{
	// we wont treat any request further here
	add_net_handler(ASK_PAGE, NULL);
	add_net_handler(RECV_PAGE, NULL);
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

	log_info("Inform new Owner\n");
	size_t payload_sz;
	void *payload = build_multiple_PAGE_message(index_pages, nb_owned_pages,
						    &payload_sz, &new_owner,
						    RECV_PAGE_LEAVE);
	send_message1(RECV_PAGE_LEAVE, &new_owner, payload, payload_sz);
	wait_on_cond(&leaving_cv, false);
	free(payload);
	log_info("ACK recved from new Owner\n");

	// broadcast to each other node, the info about the new owner
	payload = build_DT_LEAVE_message(index_pages, nb_owned_pages,
					 &new_owner, &payload_sz);
	const struct node_id *except[] = { &new_owner, NULL };
	int nb_sent = broadcast_message1(DT_LEAVE, except, payload, payload_sz);
	log_info("Informed %d nodes that i leave\n", nb_sent);

	// wait for all the ACK
	set_counter(&ack_counter, nb_sent, false);
	wait_on_counter(&ack_counter, false);
	log_info("ACKED all, leave done !\n");
	free(payload);

	for (size_t i = 0; i < nb_owned_pages; i++)
		pthread_mutex_unlock(&(page_cv + index_pages[i])->lock);

	clean_data_transfer();
}

void send_invalidation(size_t page_index)
{
	size_t payload_sz = sizeof(size_t);
	void *payload = malloc(payload_sz);
	size_t *page_id = (size_t *)(payload);
	*page_id = page_index;
	broadcast_message1(INVALIDATION, NULL, payload, payload_sz);
	set_new_owner(page_index, &me);
}

void get_page_owners(void *dst)
{
	for (unsigned int i = 0; i < nb_pages; i++)
		pthread_mutex_lock(&(page_cv + i)->lock);
	memcpy(dst, page_owners, nb_pages * sizeof(struct node_id));
	for (unsigned int i = 0; i < nb_pages; i++)
		pthread_mutex_unlock(&(page_cv + i)->lock);
}