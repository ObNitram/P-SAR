#include "core.h"
#include "../utils/utils.h"
#include "../network/message.h"
#include "../network/network.h"
#include "../utils/list.h"
#include <stdlib.h>
#include <semaphore.h>

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
	sem_t mutex;
};

static struct core_info *core_info;
static size_t core_size;

/// @brief Structure representing a message for the slsm algorithm
/// @details Contains the type of the message, the identifier of the sender, the page, the mode and the initiator.
struct slsm_message {
	size_t message_type;
	struct node_id sender;
	size_t page;
	enum lock_type mode;
	struct node_id initiator;
};

static inline void add_reader(struct core_info *working_page,
			      struct node_id reader)
{
	struct node_list *list =
		(struct node_list *)malloc(sizeof(struct node_list));
	list->node = reader;
	list_add(&list->nlist, &working_page->read_request.nlist);
}

static inline void del_reader(struct core_info *working_page,
			      struct node_id *reader)
{
	struct node_list *c;
	list_for_each_entry(c, &working_page->read_request.nlist, nlist) {
		if (node_equal(&c->node, reader)) {
			list_del(&c->nlist);
			free(c);
			break;
		}
	}
}

static inline void send_slsm_message(enum message_type msgt,
				     struct node_id *sender, enum lock_type m,
				     size_t pid)
{
	struct slsm_message request = {
		.message_type = msgt,
		.initiator = me,
		.mode = m,
		.page = pid,
	};
	send_message(sender, (struct message *)&request,
		     sizeof(struct slsm_message));
}

void ask_lock(size_t page_id, enum lock_type lock_type)
{
	struct core_info working_page = core_info[page_id];

	sem_wait(&working_page.mutex);

	//mode <- lock_type
	working_page.mode = (enum lock_status)lock_type;

	//send(<ASK_LOCK, i, mode>) to have_token
	send_slsm_message(ASK_LOCK, &working_page.have_token, lock_type,
			  page_id);
	
	sem_post(&working_page.mutex);

	//wait(<GET_LOCK, j, mode>) from j
	struct slsm_message *response =
		(struct slsm_message *)wait_message(GET_LOCK, NULL);

	sem_wait(&working_page.mutex);

	switch (working_page.mode) {
	//if mode = WRITE :
	case WRITE:
		//have_token <- i
		working_page.have_token = me;
		break;
	//if mode = READ :
	case READ:
		//have_token <- j
		working_page.have_token = response->sender;
		break;
	default:
		// log erreur should not be possible
		break;
	}

	sem_post(&working_page.mutex);
}

void unlock(size_t page_id, enum lock_type lock_type)
{
	struct core_info working_page = core_info[page_id];

	sem_wait(&working_page.mutex);

	switch (working_page.mode) {
	//if mode = WRITE :
	case WRITE:
		//if read_request = {} :
		if (!list_empty(&working_page.read_request.nlist)) {
			// send <GET_LOCK, i, READ> to all read_request
			struct slsm_message request;
			request.message_type = GET_LOCK;
			request.initiator = me;
			request.mode = READ;
			request.page = page_id;

			struct node_list *c;
			list_for_each_entry(c, &working_page.read_request.nlist,
					    nlist) {
				send_message(&c->node,
					     (struct message *)&request,
					     sizeof(struct slsm_message));
			}
			// else :
		} else {
			//send <GET_LOCK, i, WRITE> to write_request
			send_slsm_message(GET_LOCK, &working_page.write_request,
					  WRITE, page_id);
			//have_token <- write_request
			working_page.have_token = working_page.write_request;
			//write_request <- 0
			working_page.write_request = EMPTY_NODE;
		}
		break;
	//if mode = READ :
	case READ:
		// send <UNLOCK, i, READ> to have_token
		send_slsm_message(UNLOCK, &working_page.have_token, READ,
				  page_id);
		break;
	default:
		// error => should not append
		break;
	}

	//mode <- NONE
	working_page.mode = NONE;

	sem_post(&working_page.mutex);
}

void handle_ASK_LOCK(struct message *message)
{
	struct slsm_message request = *((struct slsm_message *)message);
	struct core_info working_page = core_info[request.page];

	sem_wait(&working_page.mutex);

	//if write_request != 0 :
	if (&working_page.write_request != &EMPTY_NODE) {
		// send(<ASK_LOCK, j, m>) to write_request
		send_message(&working_page.write_request,
			     (struct message *)&request,
			     sizeof(struct slsm_message));
		//else :
	} else {
		//if have_token = i :
		if (node_equal(&working_page.have_token, &me)) {
			//if mode = NONE :
			if (working_page.mode == NONE) {
				switch (request.mode) {
				//if m = WRITE :
				case WRITE:
					//if read_request = {} :
					if (list_empty(
						    &working_page.read_request
							     .nlist)) {
						//send(<GET_LOCK, i, WRITE>) to j
						send_slsm_message(
							GET_LOCK,
							&request.initiator,
							WRITE, request.page);
						//have_token <- j
						working_page.have_token =
							request.initiator;
					//else :
					} else {
						//write_request <- j
						working_page.write_request =
							request.initiator;
					}
					break;
				//if m = READ :
				case READ:
					//send(<GET_LOCK, i, READ>) to j
					send_slsm_message(GET_LOCK,
							  &request.initiator,
							  READ, request.page);
					//read_request <- read_request U {j}
					add_reader(&working_page,
						   request.initiator);
					break;
				default:
					//should not append
					break;
				}
			//else :
			} else {
				switch (request.mode) {
				//if m = WRITE :
				case WRITE:
					//write_request <- j
					working_page.write_request =
						request.initiator;
					break;
				//if m = WRITE :
				case READ:
					//read_request <- read_request U {j}
					add_reader(&working_page,
						   request.initiator);
					break;
				default:
					//should not append
					break;
				}
			}
		//else :
		} else {
			//send(<ASK_LOCK, j, m>) to have_token
			send_message(&working_page.have_token,
				     (struct message *)&request,
				     sizeof(struct slsm_message));
		}
	}

	sem_post(&working_page.mutex);
}

// void handle_GET_LOCK(struct message *message)
// {
// 	struct slsm_message request = *((struct slsm_message *)message);
// 	// DO NOTHING => because GET_LOCK is waiting by ask_lock function
// }

void handle_UNLOCK(struct message *message)
{
	struct slsm_message request = *((struct slsm_message *)message);
	struct core_info working_page = core_info[request.page];

	sem_wait(&working_page.mutex);

	//read_request <- read_request / {j}
	del_reader(&working_page, &request.initiator);

	//if read_request = {} and write_request != 0 :
	if (list_empty(&working_page.read_request.nlist) &&
	    &working_page.write_request != &EMPTY_NODE) {
		//send(<GET_LOCK,i,WRITE>) to write_request
		send_slsm_message(GET_LOCK, &working_page.write_request, WRITE,
				  request.page);
		//have_token <- write_request
		working_page.have_token = working_page.write_request;
		//write_request = 0
		working_page.write_request = EMPTY_NODE;
	}

	sem_post(&working_page.mutex);
}

void init_core(size_t nbpages, struct node_id owner)
{
	//create structure sauf si dans page_info
	core_info = malloc(sizeof(struct core_info) * nbpages);
	for (int i = 0; i<nbpages; i++) {
		core_info[i].have_token = owner;
		core_info[i].write_request = EMPTY_NODE; // must change
		sem_init(&core_info[i].mutex, 0, 1);
		INIT_LIST_HEAD(&core_info[i].read_request.nlist);
	}
	core_size = nbpages;
	//init handler

	addHandler(ASK_LOCK, NULL, handle_ASK_LOCK);
	addHandler(UNLOCK, NULL, handle_UNLOCK);
}

void clean_core()
{
	free(core_info);
	core_info = NULL;

	deleteHandler(ASK_LOCK, NULL, handle_ASK_LOCK);
	deleteHandler(UNLOCK, NULL, handle_UNLOCK);
}
