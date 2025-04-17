#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>

#include "Naimi_Trehel.h"
#include "utils/utils.h"
#include "network/network.h"
#define DISABLE_LOG
#include "utils/logger.h"

static bool token;
static bool requesting;
static bool leaving;
static bool acked;
static unsigned int users;
static struct node_id father;
static struct node_id next;
// nt for Naimi_Trehel, synchronisation vars to manage some data race and sleep/wakeup
static pthread_mutex_t nt_mtx;
static pthread_cond_t nt_cond;

static void send_request_to_father(struct node_id *requester)
{
	size_t sz = sizeof(struct message) + sizeof(struct node_id);
	struct message *msg = malloc(sz);
	msg->message_type = REQUEST_CS;
	node_copy((struct node_id *)(msg + 1), requester);
	send_message(&father, msg, sz);
	node_copy(&father, &EMPTY_NODE);
	free_message(msg);
}

static void send_token(struct node_id *dst)
{
	struct message msg = { .message_type = GET_CS };
	send_message(dst, &msg, sizeof(struct message));
}

static void REQUEST_CS_handler(struct message *message)
{
	pthread_mutex_lock(&nt_mtx);
	if (leaving)
		goto exit;
	struct node_id *requester = (struct node_id *)(message + 1);
	if (node_equal(&father, &EMPTY_NODE)) {
		if (requesting) {
			node_copy(&next, requester);
		} else {
			token = false;
			send_token(requester);
		}
	} else {
		send_request_to_father(requester);
	}
	node_copy(&father, requester);
exit:
	pthread_mutex_unlock(&nt_mtx);
}

static void GET_CS_handler(struct message *message)
{
	pthread_mutex_lock(&nt_mtx);
	token = true;
	pthread_cond_broadcast(&nt_cond);
	pthread_mutex_unlock(&nt_mtx);
}

void request_CS()
{
	pthread_mutex_lock(&nt_mtx);
	requesting = true;
	if (token) {
		users++;
		pthread_mutex_unlock(&nt_mtx);
		return;
	}
	if (!node_equal(&father, &EMPTY_NODE)) {
		send_request_to_father(&me);
	}
	while (!token)
		pthread_cond_wait(&nt_cond, &nt_mtx);

	users++;
	pthread_mutex_unlock(&nt_mtx);
}

void release_CS()
{
	pthread_mutex_lock(&nt_mtx);
	users--;
	if (!users) {
		requesting = false;
		if (!node_equal(&next, &EMPTY_NODE)) {
			send_token(&next);
			token = false;
			node_copy(&next, &EMPTY_NODE);
		}
	}
	pthread_mutex_unlock(&nt_mtx);
}

static void init_internal_data(const struct node_id *father_init,
			       bool token_init, bool requesting_init)
{
	token = token_init;
	requesting = requesting_init;
	leaving = false;
	acked = false;
	users = 0;
	node_copy(&father, father_init);
	node_copy(&next, &EMPTY_NODE);
}

static void ACK_CS_handler(struct message *message)
{
	pthread_mutex_lock(&nt_mtx);
	acked = true;
	pthread_cond_broadcast(&nt_cond);
	pthread_mutex_unlock(&nt_mtx);
}

static void RESET_CS_handler(struct message *message)
{
	pthread_mutex_lock(&nt_mtx);
	// we reset the internal data as if we called INIT_CS for the first time
	init_internal_data(&message->sender, false, requesting);
	struct message msg = { .message_type = ACK_CS };
	send_message(&message->sender, &msg, sizeof(struct message));
	// request_CS() is a blocking function must be called at the end
	if (requesting) {
		pthread_mutex_unlock(&nt_mtx);
		request_CS();
	} else {
		pthread_mutex_unlock(&nt_mtx);
	}
}

static void NEW_ROOT_CS_handler(struct message *message)
{
	pthread_mutex_lock(&nt_mtx);
	init_internal_data(&EMPTY_NODE, true, requesting);
	if (requesting) {
		pthread_cond_signal(&nt_cond);
	}
	struct message msg = { .message_type = RESET_CS };
	struct node_list *n1 = &node_list;
	list_for_each_entry_continue(n1, &node_list.nlist, nlist) {
		send_message(&n1->node, &msg, sizeof(struct message));
		while (!acked)
			pthread_cond_wait(&nt_cond, &nt_mtx);
	}
	msg.message_type = LEAVE_CS;
	send_message(&message->sender, &msg, sizeof(struct message));
	pthread_mutex_unlock(&nt_mtx);
}

static void LEAVE_CS_handler(struct message *message)
{
	pthread_mutex_lock(&nt_mtx);
	leaving = false;
	pthread_cond_broadcast(&nt_cond);
	pthread_mutex_unlock(&nt_mtx);
}

void init_CS(const struct node_id *father_init, bool token_init,
	     bool requesting_init)
{
	init_internal_data(father_init, token_init, requesting_init);
	pthread_mutex_init(&nt_mtx, NULL);
	pthread_cond_init(&nt_cond, NULL);
	addHandler(REQUEST_CS, NULL, REQUEST_CS_handler);
	addHandler(GET_CS, NULL, GET_CS_handler);
	addHandler(RESET_CS, NULL, RESET_CS_handler);
	addHandler(NEW_ROOT_CS, NULL, NEW_ROOT_CS_handler);
	addHandler(ACK_CS, NULL, ACK_CS_handler);
	addHandler(LEAVE_CS, NULL, LEAVE_CS_handler);
}

void clear_CS()
{
	pthread_mutex_destroy(&nt_mtx);
	pthread_cond_destroy(&nt_cond);
}

int leave_CS()
{
	pthread_mutex_lock(&nt_mtx);
	if (!token) {
		pthread_mutex_unlock(&nt_mtx);
		return -1;
	}
	leaving = true;
	struct node_id *new_root = NULL;
	size_t sz;
	if (!node_equal(&next, &EMPTY_NODE))
		new_root = &next;
	else
		new_root = &list_next_entry(&node_list, nlist)->node;

	// there is no ther person in the network
	if (new_root->port == -1)
		goto exit;

	// we just have to inform him, that he is the new root
	struct message msg = { .message_type = NEW_ROOT_CS };
	send_message(new_root, &msg, sizeof(struct message));

	while (leaving)
		pthread_cond_wait(&nt_cond, &nt_mtx);

exit:
	pthread_mutex_unlock(&nt_mtx);
	clear_CS();
	return 0;
}