#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

#include "data_transfer.h"
#include "../memory/memory.h"
#include "../utils/utils.h"
#include "../utils/cond_var.h"
#include "../utils/counter_cond_var.h"
#include "../network/utils/node_id.h"
#include "../network/network.new.h"
#include "../network/utils/message_type.h"
#define DISABLE_LOG
#include "utils/logger.h"

/// @brief An array of cond_var for each page.
/// @details The predicate is used to indicate if we are synching the page or not.
static struct cond_var *page_cv;

/// @brief An array that represents for each page if it's up-to-date or not
/// @details It takes 'true' if the page is up-to-date 'false' otherwise
static bool *page_state;

static struct cond_var leaving_cv;

static struct counter_cond_var ack_counter;

static bool leaving = false;
static unsigned int wait_for = 0;
static bool acked = false;
static struct node_id *successor = NULL;

static void update_page(size_t page_id, const struct node_id *owner,
			const struct node_id *sender, const void *addr_np,
			enum message_type recv_type)
{
	void *addr_op = dsm + page_id * PAGE_SIZE;
	struct cond_var *cv = page_cv + page_id;

	log_info("here on update page try lock\n");
	pthread_mutex_lock(&cv->lock);
	log_info("here on update page locked\n");

	// if it's a page that we asked or
	// the owner of that page that informs us about the new
	// owner, we copy the new owner
	if (recv_type == RECV_PAGE ||
	    node_equal(sender, page_owners + page_id)) {
		node_copy(page_owners + page_id, owner);
		memory_unlock_write(page_id);
		memcpy(addr_op, addr_np, PAGE_SIZE);
		log_info("try reset\n");
		memory_lock_reset(page_id);
		log_info("reseted\n");
		// if someone is synching we wake him up
		if (!cv->predicate) {
			page_state[page_id] = true;
			unlock_cond(cv, true);
		}
	}
	log_info("tadaaa\n");
	pthread_mutex_unlock(&cv->lock);
}

static void init_PAGE_message(void *addr, const size_t page_id,
			      const struct node_id *owner)
{
	void *addr_pg = dsm + PAGE_SIZE * page_id;
	size_t *index_p = (size_t *)(addr);
	*index_p = page_id;
	// set the new owner
	struct node_id *page_owner = (struct node_id *)(index_p + 1);
	node_copy(page_owner, owner);
	// set the page itself
	memory_unlock_read(page_id);
	memcpy(page_owner + 1, addr_pg, PAGE_SIZE);
	memory_lock_reset(page_id);
}

/// @brief Handler for a message that contains a new version of a page
/// @param message The message that contains the owner, the page_id and the page
/// @details This function is called when a message of type RECV_PAGE or RECV_PAGE_LEAVE
static void handle_PAGE(struct node_id *sender, void *payload,
			enum message_type recv_type)
{
	size_t *page_id = (size_t *)(payload);
	struct node_id *owner = (struct node_id *)(page_id + 1);
	// np => new page | op => one piece
	void *addr_np = (void *)(owner + 1);
	update_page(*page_id, owner, sender, addr_np, recv_type);
}

/// @brief Build a message that contains a page, it's owner and id
/// @param page_id The id of the page
/// @param sz It will contain the size of the message
/// @param owner The owner of the page
/// @param recv_type The type of the message : expected to be either RECV_PAGE or RECV_PAGE_LEAVE
/// @return A pointer to the message
static void *build_PAGE_message(size_t page_id, size_t *sz,
				const struct node_id *owner,
				enum message_type recv_type)
{
	// contains the page_id, the owner of that page and the page itself
	*sz = sizeof(size_t) + sizeof(struct node_id) + PAGE_SIZE;
	void *payload = malloc(*sz);
	init_PAGE_message(payload, page_id, owner);
	return payload;
}

static void *build_multiple_PAGE_message(size_t page_ids[], size_t nb_ids,
					 size_t *sz,
					 const struct node_id *new_owner,
					 enum message_type recv_type)
{
	size_t page_msg_sz =
		sizeof(size_t) + sizeof(struct node_id) + PAGE_SIZE;
	*sz = sizeof(size_t) + nb_ids * page_msg_sz;

	void *payload = malloc(*sz);
	size_t *nb_ids_p = (size_t *)(payload);
	*nb_ids_p = nb_ids;

	void *addr = (void *)(nb_ids_p + 1);
	for (size_t i = 0; i < nb_ids; i++) {
		init_PAGE_message(addr, page_ids[i], new_owner);
		addr += page_msg_sz;
	}
	return payload;
}

/// @brief Handler for the reception of a synched page
/// @param message The message that contains the owner, the page_id and the page
static void handle_RECV_PAGE(struct node_id *sender, void *payload)
{
	handle_PAGE(sender, payload, RECV_PAGE);
}

/// @brief Transfers a page to a requester
/// @details This function is called when a message of type ASK_PAGE is received
/// and we own the last version of the asked page
/// @param requester The node that asked for the page
/// @param page_id The id of the page to transfer
static void transfer_page(struct node_id *requester, size_t page_id)
{
	size_t payload_sz;
	void *payload =
		build_PAGE_message(page_id, &payload_sz, &me, RECV_PAGE);
	send_message1(RECV_PAGE, requester, payload, payload_sz);
	free(payload);
}

/// @brief Build a message that contains a page_id and a node, used for ASK_PAGE and DT_LEAVE
/// @param page_id The id of the page
/// @param node The node that asked for the page
/// @param sz It will contain the size of the message
/// @return A pointer to the message
static void *build_rqst_message(size_t page_id, const struct node_id *node,
				size_t *sz)
{
	// contains a page_id and a node
	*sz = sizeof(size_t) + sizeof(struct node_id);
	void *payload = malloc(*sz);
	size_t *index_p = (size_t *)(payload);
	*index_p = page_id;
	node_copy((struct node_id *)(index_p + 1), node);
	return payload;
}

/// @brief Handler for a message that asks for a page
/// @param message The message that contains the id of the asked page and it's requester
static void handle_ASK_PAGE(struct node_id *sender, void *payload)
{
	size_t *page_id = (size_t *)(payload);
	struct node_id *requester = (struct node_id *)(page_id + 1);

	pthread_mutex_lock(&(page_cv + *page_id)->lock);
	struct node_id *owner = page_owners + *page_id;
	pthread_mutex_unlock(&(page_cv + *page_id)->lock);

	if (node_equal(owner, &me)) {
		transfer_page(requester, *page_id);
	} else {
		send_message1(ASK_PAGE, owner, payload,
			      sizeof(size_t) + sizeof(struct node_id));
	}
}

/// @brief Build a message that asks for a page
/// @param page_id The id of the page
/// @param sz It will contain the size of the message
/// @return A pointer to the message
static void *build_ASK_PAGE_message(size_t page_id, size_t *sz)
{
	void *payload = build_rqst_message(page_id, &me, sz);
	return payload;
}

/// @brief Handler from a node that have been acknowledged that he is the new owner of a page
static void handle_ACK_RECV_PAGE(struct node_id *sender, void *payload)
{
	unlock_cond(&leaving_cv, false);
}

/// @brief Handler for a message that informs us about the new owner of a page
/// @param message Contains the new owner of a given page with it's id
static void handle_DT_LEAVE(struct node_id *sender, void *payload)
{
	log_info("Someone is leaving, gud by...\n");
	const struct node_id *new_owner = (struct node_id *)(payload);
	const size_t *nb_ids = (size_t *)(new_owner + 1);
	const size_t *start_tab = nb_ids + 1;
	const size_t *end_tab = start_tab + *nb_ids;

	for (const size_t *id_p = start_tab; id_p < end_tab; id_p++) {
		size_t page_id = *id_p;
		struct cond_var *cv = page_cv + page_id;

		pthread_mutex_lock(&cv->lock);
		struct node_id *old_owner = page_owners + page_id;

		if (node_equal(old_owner, sender)) {
			node_copy(old_owner, new_owner);
			if (!cv->predicate) {
				size_t payload_sz;
				void *payload2 = build_ASK_PAGE_message(
					page_id, &payload_sz);
				send_message1(ASK_PAGE, new_owner, payload2,
					      payload_sz);
				free(payload2);
			}
		}
		pthread_mutex_unlock(&cv->lock);
	}

	// we ACK the change
	char ack = 1;
	send_message1(ACK_DT_LEAVE, sender, &ack, sizeof(char));
	log_info("ACK leaving sent !\n");
}

static void handle_ACK_DT_LEAVE(struct node_id *sender, void *payload)
{
	decr_counter(&ack_counter, false);
}

/// @brief Build a message that informs us about the new owner of a page
/// @param page_id The id of the page
/// @param new_owner The new owner of the page
/// @param sz It will contain the size of the message
/// @return A pointer to the message
static void *build_DT_LEAVE_message(size_t page_ids[], size_t nb_ids,
				    const struct node_id *new_owner, size_t *sz)
{
	*sz = sizeof(struct node_id) + sizeof(size_t) * (nb_ids + 1);
	void *payload = malloc(*sz);

	// set new owner
	struct node_id *new_owner_p = (struct node_id *)(payload);
	node_copy(new_owner_p, new_owner);

	// number of page sent
	size_t *nb_ids_p = (size_t *)(new_owner_p + 1);
	*nb_ids_p = nb_ids;

	//copy all the ids
	size_t *page_ids_p = (size_t *)(nb_ids_p + 1);
	for (size_t i = 0; i < nb_ids; i++) {
		*(page_ids_p + i) = page_ids[i];
	}
	return payload;
}

/// @brief Handler for a message that informs us that we are the owner of the new page because the sender is leaving and the age itself
/// @param message Contains the page itself and it's id
static void handle_RECV_PAGE_LEAVE(struct node_id *sender, void *payload)
{
	size_t *nb_ids = (size_t *)(payload);
	size_t page_msg_sz =
		sizeof(size_t) + sizeof(struct node_id) + PAGE_SIZE;
	void *addr = (void *)(nb_ids + 1);
	log_info("someone leaving, i'm new owner of %zu pages\n", *nb_ids);

	for (size_t i = 0; i < *nb_ids; i++) {
		const size_t *page_id = (size_t *)(addr);
		const struct node_id *new_owner =
			(struct node_id *)(page_id + 1);
		const void *addr_np = (void *)(new_owner + 1);
		update_page(*page_id, new_owner, sender, addr_np,
			    RECV_PAGE_LEAVE);
		addr += page_msg_sz;
	}

	// we ACK the changes to the leaver
	char ack = 1;
	send_message1(ACK_RECV_PAGE, sender, &ack, sizeof(char));
	log_info("ACK sent from new Owner of %zu pages\n", *nb_ids);

	pthread_mutex_lock(&leaving_cv.lock);
	wait_for--;
	if (!wait_for && leaving)
		pthread_cond_signal(&leaving_cv.cond);
	pthread_mutex_unlock(&leaving_cv.lock);
}

static void handle_ASK_SUCCESSOR(struct node_id *sender, void *payload)
{
	pthread_mutex_lock(&leaving_cv.lock);
	if (leaving) {
		if (node_cmp(&me, sender) < 0) {
accept_faith:
			wait_for++;
			send_message1(ACK_SUCC, sender, NULL, 0);
			pthread_cond_wait(&leaving_cv.cond, &leaving_cv.lock);
			goto exit;
		} else {
			send_message1(NACK_SUCC, sender, NULL, 0);
		}
	} else
		goto accept_faith;

exit:
	pthread_mutex_unlock(&leaving_cv.lock);
}

static void handle_ACK_SUCC(struct node_id *sender, void *payload)
{
	pthread_mutex_lock(&leaving_cv.lock);
	if (successor == NULL) {
		successor = malloc(sizeof(struct node_id));
		node_copy(successor, sender);
		send_message1(YOU_SUCC, sender, NULL, 0);
	} else {
		send_message1(NYOU_SUCC, sender, NULL, 0);
	}
	pthread_mutex_unlock(&leaving_cv.lock);

	decr_counter(&ack_counter, false);
}

static void handle_YOU_SUCC(struct node_id *sender, void *payload)
{
	pthread_mutex_lock(&leaving_cv.lock);
	pthread_cond_signal(&leaving_cv.cond);
	pthread_mutex_unlock(&leaving_cv.lock);
}

static void handle_NYOU_SUCC(struct node_id *sender, void *payload)
{
	pthread_mutex_lock(&leaving_cv.lock);
	wait_for--;
	pthread_cond_signal(&leaving_cv.cond);
	pthread_mutex_unlock(&leaving_cv.lock);
}
