#include "core.h"
#include "../utils/utils.h"
#include "network/network.h"
#include "utils/list.h"
#include <stdlib.h>

// enum for local status of lock
enum lock_status {
	READING = READ,
	WRITING = WRITE,
	NONE,
};

struct core_info {
	enum lock_status mode;
	struct node_id write_request;
	struct node_list read_request;
	struct node_id have_token;
};

static struct core_info *core_info;
static size_t core_size;

/// @brief Structure representing a message for the slsm algorithm
/// @details Contains the type of the message, the identifier of the sender, the page, the mode and the initiator.
struct slsm_message {
	size_t message_type; ///< The type of the message.
	struct node_id
		sender; ///< The sender of the message it will be overide by send_message.
	size_t page;
	enum lock_type mode;
	struct node_id initiator; // the initiator of the request
};

void init_core(size_t nbpages)
{
	//create structure sauf si dans page_info
	core_info = malloc(sizeof(struct core_info) * nbpages);
	core_size = nbpages;
	//init handler
}

void clean_core()
{
	free(core_info);
	core_info = NULL;
}

void ask_lock(size_t page_id, enum lock_type lock_type)
{
	// if (page->owner == id) {
	//     // TODO
	// } else {
	//     send(page->owner, ASK_OWNER, <id_page, my_id, lock_type>);
	//     wait(ACK_LOCK); // on est maintenant dans la file d'attente
	//     // TODO; deal with negative ack
	//     wait(LOCK_GIVEN; any);
	// }
	// page->my_lock = lock_type;
	// page->id_lock_given_from = lock_giver;
	// if (lock_type == WRITE) {
	//     page->have_token = true;
	// }

	struct core_info working_page = core_info[page_id];
	working_page.mode = (enum lock_status)lock_type;

	struct slsm_message request;
	request.message_type = ASK_LOCK;
	request.initiator = working_page.have_token;
	request.mode = lock_type;
	request.page = page_id;

	send_message(&working_page.have_token, (struct message *)&request,
		     sizeof(struct slsm_message));
	struct slsm_message *response =
		(struct slsm_message *)wait_message(GET_LOCK, NULL);

	switch (working_page.mode) {
	case WRITE:
		working_page.have_token =
			EMPTY_NODE; // EMPTY_NODE is for the current node
		break;
	case READ:
		working_page.have_token = response->sender;
		break;
	default:
		// log erreur should not be possible
		break;
	}
}

void unlock(size_t page_id)
{
	// assert(page->my_lock != NONE);
	// if (page->my_lock == READ) {  // we readed
	//     assert(read_request.empty());
	//     assert(write_request == NULL);
	// send message back to the guy with the tocken
	//     // if (page->id_given_lock_from == my_id) {
	//     handle
	//     } else {
	//             send(page->given_lock_from, UNLOCK, <page_id, my_id, lock_type>);
	//     }
	// } else {
	//     handle_pending_request(page); // gerer les prochains read et gerer prochain right
	// }
	// page->my_lock = NONE;

	struct core_info working_page = core_info[page_id];

	switch (working_page.mode) {
	case WRITE:
		working_page.mode = NONE;
		if (!list_empty(&working_page.read_request.nlist)) {
			struct node_list *c;
			list_for_each_entry(c, &working_page.read_request.nlist,
					    nlist) {
				//send lock to all reader
			}
		} else {
			//send lock to writer
			// remove writer
		}
		break;
	case READ:
		break;
	default:
		break;
	}
}

void handle_ASK_LOCK(struct message *message)
{
	struct slsm_message request = *((struct slsm_message *)message);
	assert(request.message_type == ASK_LOCK);
}

void handle_GET_LOCK(struct message *message)
{
	struct slsm_message request = *((struct slsm_message *)message);
	assert(request.message_type == GET_LOCK);
}

void handle_UNLOCK(struct message *message)
{
	struct slsm_message request = *((struct slsm_message *)message);
	assert(request.message_type == UNLOCK);

	struct core_info working_page = core_info[request.page];

	// if reader list is empty => error
	assert(!list_empty(&working_page.read_request.nlist));

	// remove reader from list
	struct node_list *c;
	list_for_each_entry(c, &working_page.read_request.nlist, nlist) {
		if (node_equal(&c->node, &request.initiator)) {
            list_del(&c->nlist);
            break;
        }
	}
}

void handle_lock_read(struct page *page, struct node_id id_requester)
{
	// if (page->in_chainon == false) {
	//     send(page->data_owner, ASK_LOCK, <id_page, id_requester, READ>);
	// }
	// if (page->write_request != NULL) {
	//     if (page->write_request == me) {
	//         // Who knows
	//     }
	//     send(page_write_request->id, ASK_LOCK, <id_page, id_requester, READ>);
	//     return;
	// } else {
	//     page->read_request.insert(id_requester);
	//     send(id_requester, ACK_LOCK, <id_page, my_id, READ>);
	//     if (page->have_token && page->my_lock != WRITE) {
	//         send(id_requester, GIVEN_LOCK, <id_page, my_id, READ>);
	//     }
	// }
}

void handle_unlock_read(struct page *page, int id_requester)
{
	// page->read_request, id_request);
	// if (page->write_request != NULL && page->read-request.empty()) {
	//     page->have_token = false;
	//     send(page->write_request, GIVEN_LOCK, <id_page, my_id, WRITE>);
	// }
}

void handle_lock_write(struct page *p, int id_requester)
{
}
void handle_unlock_write(struct page *p, int id_requester)
{
}
