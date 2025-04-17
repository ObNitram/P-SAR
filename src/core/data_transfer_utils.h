#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

#include "data_transfer.h"
#include "../utils/utils.h"
#include "../sigsegv_handler/sigsegv.h"

/// @brief An array that contains a mutex for each page
static pthread_mutex_t *page_mtx;

/// @brief An array that contains a conditoin variable for each page
static pthread_cond_t *page_cond;

/// @brief An array that represents for each page if we are synching it or not
/// @details It takes 'true' if we are actually synching the page 'false' otherwise
static bool *page_in_transit;

/// @brief An array that represents for each page if it's up-to-date or not
/// @details It takes 'true' if the page is up-to-date 'false' otherwise
static bool *page_state;

/// @brief Wait for a page to be synched
/// @param page_id The id of the page to wait for
static void wait_signal(size_t page_id)
{
	pthread_mutex_lock(page_mtx + page_id);
	while (page_in_transit[page_id]) {
		pthread_cond_wait(page_cond + page_id, page_mtx + page_id);
	}
	pthread_mutex_unlock(page_mtx + page_id);
}

/// @brief Signal that a page is synched
/// @param page_id The id of the page to signal
static void signal_page(size_t page_id)
{
	pthread_mutex_lock(page_mtx + page_id);
	page_in_transit[page_id] = false;
	page_state[page_id] = true;
	pthread_cond_broadcast(page_cond + page_id);
	pthread_mutex_unlock(page_mtx + page_id);
}

/// @brief Handler for a message that contains a new version of a page
/// @param message The message that contains the owner, the page_id and the page
/// @details This function is called when a message of type RECV_PAGE or RECV_PAGE_LEAVE
static void PAGE_handler(struct message *message)
{
	size_t *page_id = (size_t *)(message + 1);
	struct node_id *owner = (struct node_id *)(page_id + 1);
	// np => new page | op => old page
	void *addr_np = (void *)(owner + 1);
	void *addr_op = dsm + (*page_id) * PAGE_SIZE;
	bool synching = false;

	pthread_mutex_lock(page_mtx + *page_id);

	if (message->message_type == RECV_PAGE) {
		node_copy(page_owners + *page_id, owner);
		memory_unlock_write(*page_id);
		memcpy(addr_op, addr_np, PAGE_SIZE);
		memory_lock_reset(*page_id);
		// if someone is synching we wake him up
		synching = page_in_transit[*page_id];
		pthread_mutex_unlock(page_mtx + *page_id);
		if (synching)
			signal_page(*page_id);
	} else
		pthread_mutex_unlock(page_mtx + *page_id);
}

/// @brief Build a message that contains a page, it's owner and id
/// @param page_id The id of the page
/// @param sz It will contain the size of the message
/// @param owner The owner of the page
/// @param recv_type The type of the message : expected to be either RECV_PAGE or RECV_PAGE_LEAVE
/// @return A pointer to the message
static struct message *build_PAGE_message(size_t page_id, size_t *sz,
					  struct node_id *owner,
					  enum message_type recv_type)
{
	// contains the page_id, the owner of that page and the page itself
	*sz = sizeof(struct message) + sizeof(size_t) + sizeof(struct node_id) +
	      PAGE_SIZE;
	void *addr_pg = dsm + PAGE_SIZE * page_id;
	struct message *msg = (struct message *)malloc(*sz);
	// set the page_id
	size_t *index_p = (size_t *)(msg + 1);
	*index_p = page_id;
	// set the new owner
	struct node_id *page_owner = (struct node_id *)(index_p + 1);
	node_copy(page_owner, owner);
	// set the page itself
	memory_unlock_read(page_id);
	memcpy(page_owner + 1, addr_pg, PAGE_SIZE);
	memory_lock_reset(page_id);
	msg->message_type = recv_type;
	return msg;
}

/// @brief Handler for the reception of a synched page
/// @param message The message that contains the owner, the page_id and the page
static void RECV_PAGE_handler(struct message *message)
{
	PAGE_handler(message);
}

/// @brief Transfers a page to a requester
/// @details This function is called when a message of type ASK_PAGE is received and we own the last version of the asked page
/// @param requester The node that asked for the page
/// @param page_id The id of the page to transfer
static void transfer_page(struct node_id *requester, size_t page_id)
{
	size_t ms_sz;
	struct message *msg =
		build_PAGE_message(page_id, &ms_sz, &me, RECV_PAGE);
	send_message(requester, msg, ms_sz);
	free_message(msg);
}

/// @brief Build a message that contains a page_id and a node
/// @param page_id The id of the page
/// @param node The node that asked for the page
/// @param sz It will contain the size of the message
/// @return A pointer to the message
static struct message *build_rqst_message(size_t page_id, struct node_id *node,
					  size_t *sz)
{
	// contains a page_id and a node
	*sz = sizeof(struct message) + sizeof(size_t) + sizeof(struct node_id);
	struct message *msg = (struct message *)malloc(*sz);
	size_t *index_p = (size_t *)(msg + 1);
	*index_p = page_id;
	node_copy((struct node_id *)(index_p + 1), node);
	return msg;
}

/// @brief Handler for a message that asks for a page
/// @param message The message that contains the id of the asked page and it's requester
static void ASK_PAGE_handler(struct message *message)
{
	size_t *page_id = (size_t *)(message + 1);
	struct node_id *requester = (struct node_id *)(page_id + 1);

	pthread_mutex_lock(page_mtx + *page_id);
	struct node_id *owner = page_owners + *page_id;
	pthread_mutex_unlock(page_mtx + *page_id);

	if (node_equal(owner, &me)) {
		transfer_page(requester, *page_id);
	} else {
		send_message(owner, message,
			     sizeof(struct message) + sizeof(size_t) +
				     sizeof(struct node_id));
	}
}

/// @brief Build a message that asks for a page
/// @param page_id The id of the page
/// @param sz It will contain the size of the message
/// @return A pointer to the message
static struct message *build_ASK_PAGE_message(size_t page_id, size_t *sz)
{
	struct message *msg = build_rqst_message(page_id, &me, sz);
	msg->message_type = ASK_PAGE;
	return msg;
}