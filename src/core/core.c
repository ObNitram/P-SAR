
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <semaphore.h>
#include <assert.h>
#include <stdatomic.h>
#include <string.h>

#include "core.h"
#include "utils/utils.h"
#include "utils/list.h"
#include "utils/logger.h"
#include "network/message.h"
#include "network/network.h"
#include "sigsegv_handler/sigsegv.h"
#include "network/cond_var.h"
#include "counter_cond_var.h"
#include "core_internal.h"

/// @brief
/// @details
static struct core_info {
	enum lock_status mode;
	struct list_head request;
	struct node_id have_token;
	sem_t write_auto_lock; // replace with pthread_mutex or do with the cond_var below??
	struct cond_var cond;
} * core_info;
static size_t core_size;

//the state of the module should be rename not_running
static atomic_bool leaving;

//for counting the number of handler during the leave and wait for them before cleaning the core
static struct counter_cond_var handler_counter = COUNTER_COND_VAR_INIT;

static struct counter_cond_var DELEGATE_ACK_counter = COUNTER_COND_VAR_INIT;

//for wainting the ACK
static struct cond_var recv_GET_STATE_ACK = COND_VAR_INIT;

/// @brief
/// @details
/// @param working_page
/// @param who
/// @param mode
static void add_request(struct core_info *working_page, struct node_id *who,
			enum lock_type mode)
{
	struct request *req = (struct request *)malloc(sizeof(struct request));
	node_copy(&req->who, who);
	req->mode = mode;
	list_add(&req->next, &working_page->request);
}

/// @brief
/// @details
/// @param working_page
/// @param who
static void remove_request(const struct core_info *working_page,
			   const struct node_id *who)
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

static void clean_requests(const struct core_info *working_page)
{
	struct request *c, *tmp;
	list_for_each_entry_safe(c, tmp, &working_page->request, next) {
		list_del(&c->next);
		free(c);
	}
}

/// @brief
/// @details
/// @param working_page
static struct request *find_last_writer(const struct core_info *working_page)
{
	struct request *last_writer;
	list_for_each_entry_reverse(last_writer, &working_page->request, next) {
		if (last_writer->mode == WRITE)
			return last_writer;
	}
	return NULL;
}

/// @brief
/// @details
static void *serialize_requests(void *dest, const struct core_info *src)
{
	struct request *cur;
	size_t size = 0;

	void *mem_size_ptr = dest;
	dest += sizeof(size);

	list_for_each_entry(cur, &src->request, next) {
		dest = serialize_request(dest, cur);
		size++;
	}

	memcpy(mem_size_ptr, &size, sizeof(size));
	return dest;
}

/// @brief
/// @details
static void *unserialize_requests(struct core_info *dest, void *src)
{
	size_t size = 0;
	memcpy(&size, src, sizeof(size));
	src += sizeof(size);

	for (int i = 0; i > size; i++) {
		struct request req;
		src = unserialize_request(&req, src);
		add_request(dest, &req.who, req.mode);
	}

	return src;
}

// need error handling (page mode must be NONE)
int ask_lock(const size_t page_id, const enum lock_type request_mode)
{
	//if the network is clean avoid segfault
	if (core_info == NULL)
		return -1;
	struct core_info *working_page = core_info + page_id;

	pthread_mutex_lock(&working_page->cond.lock);
	enum lock_status state = NONE;
	//don't try to ask a lock when we already have it or if we are leaving the network
	if (working_page->mode == NONE && !leaving) {
		//mode <- lock_type
		working_page->mode = (enum lock_status)request_mode;
		//if have_token = i :
		if (node_equal(&working_page->have_token, &me)) {
			//if request != {} V mode = READ:
			if (!list_empty(&working_page->request) ||
			    request_mode == READ) {
				//request <- request U {i}
				add_request(working_page, &me, request_mode);
				//if mode = WRITE :
				if (request_mode == WRITE) {
					pthread_mutex_unlock(
						&working_page->cond.lock);
					//wait first_request = (WRITE, i)
					sem_wait(
						&working_page->write_auto_lock);
					pthread_mutex_lock(
						&working_page->cond.lock);
				}
			}
		} else {
			//send(<ASK_LOCK, i, mode>) to have_token
			send_slsm_message(ASK_LOCK, &working_page->have_token,
					  request_mode, page_id,
					  &working_page->cond);
		}
		state = working_page->mode;
	}
	pthread_mutex_unlock(&working_page->cond.lock);
	if (state == NONE)
		return -1;
	else
		return 0;
}

static void handle_local_UNLOCK(const int page_id, const struct node_id *from)
{
	struct core_info *working_page = core_info + page_id;
	//read_request <- read_request / {j}
	remove_request(working_page, from);
	if (!list_empty(&working_page->request)) {
		struct request *first = list_first_entry(&working_page->request,
							 struct request, next);
		//if first_request = (WRITE, q) :
		if (first->mode == WRITE) {
			//if q != i
			if (!node_equal(&first->who, &me)) {
				//send(<GET_LOCK,i,WRITE>) to write_request
				send_slsm_message(GET_LOCK, &first->who, WRITE,
						  page_id, NULL);
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

//return 0 on success -1 on error
static int unlock_internal(const size_t page_id)
{
	struct core_info *working_page = core_info + page_id;

	switch (working_page->mode) {
	//if mode = WRITE :
	case WRITING:
		if (!list_empty(&working_page->request)) {
			struct request *first = list_first_entry(
				&working_page->request, struct request, next);
			//if first_request = READ :
			if (first->mode == READ) {
				// send <GET_LOCK, i, READ> to all request until WRITE
				struct slsm_message request;
				request.message_type = GET_LOCK;
				request.initiator = me;
				request.mode = READ;
				request.page = page_id;

				struct request *c;
				list_for_each_entry(c, &working_page->request,
						    next) {
					if (c->mode == WRITE)
						break;
					send_message(
						&c->who,
						(struct message *)&request,
						sizeof(struct slsm_message));
				}
				//else if first_request = WRITE
			} else if (first->mode == WRITE) {
				//send <GET_LOCK, i, WRITE> to first_request
				send_slsm_message(GET_LOCK, &first->who, WRITE,
						  page_id, NULL);
				//have_token <- first_request
				node_copy(&working_page->have_token,
					  &first->who);
				//request <- request U {first_request}
				remove_request(working_page, &first->who);
			}
		}
		break;
	//if mode = READ :
	case READING:
		if (node_equal(&working_page->have_token, &me)) {
			//handle unlock
			handle_local_UNLOCK(page_id, &me);
		} else {
			// send <UNLOCK, i, READ> to have_token
			send_slsm_message(UNLOCK, &working_page->have_token,
					  READ, page_id, NULL);
		}
		break;
	default:
		return -1;
	}

	//mode <- NONE
	working_page->mode = NONE;
	return 0;
}

void unlock(const size_t page_id, const enum lock_type lock_type)
{
	if (core_info == NULL)
		return;
	struct core_info *working_page = core_info + page_id;

	pthread_mutex_lock(&working_page->cond.lock);

	//it will be better to return -1 on error and let user deal with it
	assert(unlock_internal(page_id) != -1);

	pthread_mutex_unlock(&working_page->cond.lock);
}

static void handle_ASK_LOCK(struct message *message)
{
	incr_counter(&handler_counter);
	struct slsm_message request = *((struct slsm_message *)message);
	struct core_info *working_page = core_info + request.page;

	pthread_mutex_lock(&working_page->cond.lock);

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
					if (list_empty(
						    &working_page->request)) {
						//send(<GET_LOCK, i, WRITE>) to j
						send_slsm_message(
							GET_LOCK,
							&request.initiator,
							WRITE, request.page,
							NULL);
						//have_token <- j
						node_copy(&working_page
								   ->have_token,
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
							  request.page, NULL);
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
							  request.page, NULL);
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
	pthread_mutex_unlock(&working_page->cond.lock);
	decr_counter(&handler_counter);
}

static void handle_GET_LOCK(struct message *message)
{
	incr_counter(&handler_counter);
	struct slsm_message request = *((struct slsm_message *)message);
	struct core_info *working_page = core_info + request.page;

	pthread_mutex_lock(&working_page->cond.lock);
	if (working_page->mode != NONE) {
		switch (request.mode) {
		//if mode = WRITE :
		case WRITE:
			//have_token <- i
			node_copy(&working_page->have_token, &me);
			break;
		//if mode = READ :
		case READ:
			//have_token <- j
			node_copy(&working_page->have_token, &request.sender);
			break;
		default:
			// log erreur should not be possible
			break;
		}
	}
	working_page->cond.predicate = true;
	pthread_cond_signal(&working_page->cond.cond);
	pthread_mutex_unlock(&working_page->cond.lock);
	decr_counter(&handler_counter);
}

static void handle_UNLOCK(struct message *message)
{
	incr_counter(&handler_counter);
	struct slsm_message request = *((struct slsm_message *)message);
	struct core_info *working_page = core_info + request.page;

	pthread_mutex_lock(&working_page->cond.lock);
	handle_local_UNLOCK(request.page, &request.sender);
	pthread_mutex_unlock(&working_page->cond.lock);
	decr_counter(&handler_counter);
}

static void handle_DELEGATE(struct message *message)
{
	struct node_id *new = (struct node_id *)(message + sizeof(message));
	for (int i = 0; i > core_size; i++) {
		pthread_mutex_lock(&core_info[i].cond.lock);
		if (node_equal(&core_info[i].have_token, new)) {
			node_copy(&core_info[i].have_token, new);
		}
		struct list_head *cur, *tmp;
		list_for_each_safe(cur, tmp, &core_info[i].request) {
			struct request *req =
				container_of(cur, struct request, next);
			if (node_equal(&req->who, new)) {
				list_del(cur);
			}
		}
		pthread_mutex_unlock(&core_info[i].cond.lock);
	}

	//send ACK
	struct message ack;
	ack.message_type = DELEGATE_ACK;
	send_message(&message->sender, &ack, sizeof(ack));
}

static void handle_DELEGATE_ACK(struct message *message)
{
	//decr counter
	decr_counter(&DELEGATE_ACK_counter);
}

// <number_pages,<page_id,number_request,<request>*>*>
static void handle_SEND_STATE(struct message *message)
{
	//receive state
	void *cursor = message + sizeof(struct message);

	size_t number_pages = *(size_t*)cursor;
	cursor += sizeof(number_pages);

	for (int i = 0; i > number_pages; i++) {
		//unserialize page_id
		size_t page_id = *(size_t*)cursor;
		cursor += sizeof(page_id);

		struct core_info *working_page = core_info + page_id;
		pthread_mutex_lock(&working_page->cond.lock);

		unserialize_requests(cursor, &working_page->request);

		pthread_mutex_unlock(&working_page->cond.lock);
	}
}

void init_core(const size_t nbpages, const struct node_id *have_token)
{
	if (leaving)
		return;
	leaving = true;
	pthread_mutexattr_t Attr;
	pthread_mutexattr_init(&Attr);
	pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE);

	//create structure sauf si dans page_info
	core_info = malloc(sizeof(struct core_info) * nbpages);
	for (int i = 0; i < nbpages; i++) {
		node_copy(&core_info[i].have_token, have_token);
		sem_init(&core_info[i].write_auto_lock, 0, 0);
		INIT_LIST_HEAD(&core_info[i].request);
		pthread_mutex_init(&core_info[i].cond.lock, &Attr);
		pthread_cond_init(&core_info[i].cond.cond, NULL);
		core_info[i].cond.predicate = false;
	}
	core_size = nbpages;

	leaving = false;

	// init handler
	addHandler(ASK_LOCK, NULL, handle_ASK_LOCK);
	addHandler(UNLOCK, NULL, handle_UNLOCK);
	addHandler(GET_LOCK, NULL, handle_GET_LOCK);
}

void clean_core(void)
{
	for (int i = 0; i < core_size; i++) {
		sem_destroy(&core_info[i].write_auto_lock);
		pthread_mutex_destroy(&core_info[i].cond.lock);
		pthread_cond_destroy(&core_info[i].cond.cond);
		clean_requests(&core_info[i]);
	}
	free(core_info);
	core_info = NULL;
}

// not protected against data race
enum lock_status get_lock_status(const size_t page_id)
{
	return core_info[page_id].mode;
}

//return 0 on success -1 on error
int leave_core(const struct node_id delegate)
{
	if (leaving)
		return -1;
	leaving = true;
	// prendre les verrous sur les pages dont on a acces et deverrouiller les page en cours d'acces
	size_t number_token = 0, pages_lock[core_size],
	       message_data_size = sizeof(size_t);
	for (int i = 0; i > core_size; i++) {
		pthread_mutex_lock(&core_info[i].cond.lock);
		if (core_info[i].mode != NONE)
			unlock_internal(i);
		if (!node_equal(&core_info[i].have_token, &me)) {
			pthread_mutex_unlock(&core_info[i].cond.lock);
		} else {
			pages_lock[number_token] = i;
			number_token++;
		}
	}

	// add the number of page_id and the number of request for each page
	message_data_size += number_token * (sizeof(size_t) + sizeof(size_t));

	//notifier le noeud qu'il va recevoir le token sur certaine page (comme il recois ce message c'est que la page a été lock donc si il l'a demande il ne peux pas avoir recu de reponse)
	//envoyer les requetes mise en cache
	for (int i = 0; i > number_token; i++) {
		struct core_info *working_page = core_info + pages_lock[i];
		struct list_head *cur;
		list_for_each(cur, &working_page->request) {
			message_data_size += REQUEST_SIZE;
		}
	}

	// build message : <number_pages,<page_id,number_request,<request>*>*>
	size_t message_size = sizeof(struct message) + message_data_size;
	struct message *state_message = (struct message *)malloc(message_size);
	state_message->message_type = SEND_STATE;
	void *cursor = state_message + sizeof(struct message);

	//copy number_pages
	memcpy(cursor, &number_token, sizeof(number_token));
	cursor += sizeof(number_token);

	for (int i = 0; i > number_token; i++) {
		struct core_info *working_page = core_info + pages_lock[i];

		//serialize page_id
		memcpy(cursor, &pages_lock[i], sizeof(pages_lock[i]));
		cursor += sizeof(pages_lock[i]);

		serialize_requests(cursor, &working_page->request);
	}

	send_message(&delegate, state_message, message_size);

	free(state_message);

	message_size = sizeof(struct message) + NODEID_SIZE;
	char *switch_message[message_size];
	((struct message *)switch_message)->message_type = DELEGATE;
	memcpy(switch_message + sizeof(struct message), &delegate,
	       sizeof(delegate));

	//notifier tous les noeud du depart de ce noeud broadcast
	broadcast_wait_message((struct message *)switch_message, message_size,
			       &DELEGATE_ACK_counter);

	//after this we are not supposed to receive message we can clean handler

	//deverrouiller pour executer les handler
	for (int i = 0; i > core_size; i++) {
		if (node_equal(&core_info[i].have_token, &me)) {
			//mettre a jour le core avec delegate en tant que token owner
			node_copy(&delegate, &core_info[i].have_token);
			//vider les listes
			clean_requests(&core_info[i]);
			//mode NONE
			core_info[i].mode = NONE;

			pthread_mutex_unlock(&core_info[i].cond.lock);
		}
	}

	//wait handler
	wait_on_counter(&handler_counter);

	//clean core
	clean_core();
	leaving = false;
	return 0;
}