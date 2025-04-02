#include "core.h"
#include "../utils/utils.h"
#include "../network/message.h"
#include "../network/network.h"
#include "../utils/list.h"
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>

// enum for local status of lock
enum lock_status {
	READING = READ,
	WRITING = WRITE,
	NONE,
};

struct request {
	enum lock_type mode;
	struct node_id who;
	struct list_head next;
};

struct core_info {
	enum lock_status mode;
	struct list_head request;
	struct node_id have_token;
	sem_t write_auto_lock;
	pthread_mutex_t mutex;
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

static inline void add_request(struct core_info *working_page,
			       struct node_id *who, enum lock_type mode)
{
	struct request *req = (struct request *)malloc(sizeof(struct request));
	node_copy(&req->who, who);
	req->mode = mode;
	list_add(&req->next, &working_page->request);
}

static inline void remove_request(struct core_info *working_page,
				  struct node_id *who)
{
	struct request *c, *tmp;
	list_for_each_entry_safe(c, tmp, &working_page->request, next) {
		if (node_equal(&c->who, who)) {
			list_del(&c->next);
			free(c);
			break;
		}
	}
}

static inline struct request *find_last_writer(struct core_info *working_page)
{
	struct request *last_writer;
	list_for_each_entry(last_writer, &working_page->request, next) {
		if (last_writer->mode == WRITING)
			return last_writer;
	}
	return NULL;
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

void ask_lock(size_t page_id, enum lock_type mode)
{
	struct core_info *working_page = core_info + page_id;

	pthread_mutex_lock(&working_page->mutex);

	//mode <- lock_type
	working_page->mode = (enum lock_status)mode;
	//if have_token = i :
	if (node_equal(&working_page->have_token, &me)) {
		//if request != {} V mode = READ:
		if (!list_empty(&working_page->request) || mode == READ) {
			//request <- request U {i}
			add_request(working_page, &me, mode);
			//if mode = WRITE :
			if (mode == WRITE) {
				pthread_mutex_unlock(&working_page->mutex);
				//wait first_request = (WRITE, i)
				sem_wait(&working_page->write_auto_lock);
				pthread_mutex_lock(&working_page->mutex);
			}
		}
	} else {
		//send(<ASK_LOCK, i, mode>) to have_token
		send_slsm_message(ASK_LOCK, &working_page->have_token, mode,
				  page_id);

		pthread_mutex_unlock(&working_page->mutex);
		//wait(<GET_LOCK, j, mode>) from j
		struct slsm_message *response =
			(struct slsm_message *)wait_message(GET_LOCK, NULL);

		pthread_mutex_lock(&working_page->mutex);

		switch (mode) {
		//if mode = WRITE :
		case WRITE:
			//have_token <- i
			node_copy(&working_page->have_token, &me);
			break;
		//if mode = READ :
		case READ:
			//have_token <- j
			node_copy(&working_page->have_token, &response->sender);
			break;
		default:
			// log erreur should not be possible
			break;
		}
	}
	pthread_mutex_unlock(&working_page->mutex);
}

static void handle_local_UNLOCK(int page_id, struct node_id *from)
{
	struct core_info *working_page = core_info+page_id;
	//read_request <- read_request / {j}
	remove_request(working_page, from);
	if (!list_empty(&working_page->request)) {
		struct request *first = list_first_entry(&working_page->request,
							 struct request, next);
		//if first_request = (WRITE, q) :
		if (first->mode == WRITING) {
			//if q != i
			if (!node_equal(&first->who, &me)) {
				//send(<GET_LOCK,i,WRITE>) to write_request
				send_slsm_message(GET_LOCK, &first->who, WRITE,
						  page_id);
				//have_token <- q
				node_copy(&working_page->have_token,
					  &first->who);
			} else {
				//unlock me
			}
			//request <- request / {q}
			remove_request(working_page, &first->who);
		}
	}
}

void unlock(size_t page_id, enum lock_type lock_type)
{
	struct core_info *working_page = core_info + page_id;

	pthread_mutex_lock(&working_page->mutex);

	switch (working_page->mode) {
	//if mode = WRITE :
	case WRITE:
		if (!list_empty(&working_page->request)) {
			struct request *first = list_first_entry(
				&working_page->request, struct request, next);
			//if first_request = READ :
			if (first->mode == READING) {
				// send <GET_LOCK, i, READ> to all request until WRITE
				struct slsm_message request;
				request.message_type = GET_LOCK;
				request.initiator = me;
				request.mode = READ;
				request.page = page_id;

				struct request *c;
				list_for_each_entry(c, &working_page->request,
						    next) {
					if (c->mode == WRITING)
						break;
					send_message(
						&c->who,
						(struct message *)&request,
						sizeof(struct slsm_message));
				}
				//else if first_request = WRITE
			} else if (first->mode == WRITING) {
				//send <GET_LOCK, i, WRITE> to first_request
				send_slsm_message(GET_LOCK, &first->who, WRITE,
						  page_id);
				//have_token <- first_request
				node_copy(&working_page->have_token,
					  &first->who);
				//request <- request U {first_request}
				remove_request(working_page, &first->who);
			}
		}
		break;
	//if mode = READ :
	case READ:
		if (node_equal(&working_page->have_token, &me)) {
			//handle unlock
			handle_local_UNLOCK(page_id, &me);
		} else {
			// send <UNLOCK, i, READ> to have_token
			send_slsm_message(UNLOCK, &working_page->have_token,
					  READ, page_id);
		}
		break;
	default:
		// error => should not append
		break;
	}

	//mode <- NONE
	working_page->mode = NONE;

	pthread_mutex_unlock(&working_page->mutex);
}

static void handle_ASK_LOCK(struct message *message)
{
	struct slsm_message request = *((struct slsm_message *)message);
	struct core_info *working_page = core_info + request.page;

	pthread_mutex_lock(&working_page->mutex);

	struct request *last_writer = find_last_writer(working_page);

	//if last_write_request = i :
	if (last_writer != NULL && node_equal(&last_writer->who, &me)) {
		//request <- request U {j}
		add_request(working_page, &request.initiator, request.mode);
		//if last_writer != 0 :
	} else if (last_writer != NULL) {
		//send(<ASK_LOCK, j, m>) to last_writer
		send_message(&last_writer->who, (struct message *)&request,
			     sizeof(struct slsm_message));
		//else :
	} else {
		//if have_token = i
		if (node_equal(&working_page->have_token, &me)) {
			//if mode = NONE
			if (working_page->mode == NONE) {
				//if m == WRITE :
				if (request.mode == WRITE) {
					//if request = {}
					if (list_empty(&working_page->request)) {
						//send(<GET_LOCK, i, WRITE>) to j
						send_slsm_message(
							GET_LOCK,
							&request.initiator,
							WRITE, request.page);
						//have_token <- j
						node_copy(
							&working_page->have_token,
							&request.initiator);
						//else :
					} else {
						//request <- request U {j}
						add_request(working_page,
							    &request.initiator,
							    request.mode);
					}
				} else if (request.mode == READ) {
					//send(<GET_LOCK, i, READ>) to j
					send_slsm_message(GET_LOCK,
							  &request.initiator,
							  request.mode,
							  request.page);
					//request <- request U {j}
					add_request(working_page,
						    &request.initiator,
						    request.mode);
				}
				//else :
			} else {
				//request <- request U {j}
				add_request(working_page, &request.initiator,
					    request.mode);
				//if mode = READ and m = READ :
				if (working_page->mode == READING &&
				    request.mode == READ) {
					//send(<GET_LOCK, i, READ>) to j
					send_slsm_message(GET_LOCK,
							  &request.initiator,
							  request.mode,
							  request.page);
				}
			}
			//else :
		} else {
			//send(<ASK_LOCK, j, m>) to have_token
			send_message(&working_page->have_token,
				     (struct message *)&request,
				     sizeof(struct slsm_message));
		}
	}
	pthread_mutex_unlock(&working_page->mutex);
}

static void handle_UNLOCK(struct message *message)
{
	struct slsm_message request = *((struct slsm_message *)message);
	struct core_info *working_page = core_info + request.page;

	pthread_mutex_lock(&working_page->mutex);
	handle_local_UNLOCK(request.page, &request.sender);
	pthread_mutex_unlock(&working_page->mutex);
}

void init_core(size_t nbpages, struct node_id *have_token)
{
	//create structure sauf si dans page_info
	core_info = malloc(sizeof(struct core_info) * nbpages);
	for (int i = 0; i < nbpages; i++) {
		node_copy(&core_info[i].have_token, have_token);
		sem_init(&core_info[i].write_auto_lock, 0, 0);
		INIT_LIST_HEAD(&core_info[i].request);
		pthread_mutex_init(&core_info[i].mutex, NULL);
	}
	core_size = nbpages;

	//init handler
	addHandler(ASK_LOCK, NULL, handle_ASK_LOCK);
	addHandler(UNLOCK, NULL, handle_UNLOCK);
}

void clean_core()
{
	struct request *c, *tmp;
	for (int i = 0; i < core_size; i++) {
		sem_destroy(&core_info[i].write_auto_lock);
		pthread_mutex_destroy(&core_info[i].mutex);
		list_for_each_entry_safe(c, tmp, &core_info[i].request, next) {
			list_del(&c->next);
			free(c);
		}
	}
	free(core_info);
	core_info = NULL;
}
