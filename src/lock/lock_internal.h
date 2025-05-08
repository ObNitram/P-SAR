//this header is not supposed to be include elsewhere this purpose is for helper structure and the associated function
//This function doesnt depends on ANY global state and it for that it can be placed in a header...
#pragma once

#include "lock.h"
#include "utils/counter_cond_var.h"
#include "utils/list.h"
#include <stddef.h>
#include "network/message.h"
#include "utils/utils.h"
#include <string.h>

//need to move it in more suitable file
/// @brief
/// @details
static void *serialize_nodeid(void *dest, const struct node_id *src)
{
	char *cursor = (char *)dest;
	memcpy(cursor, src->host, sizeof(src->host));
	cursor += sizeof(src->host);
	memcpy(cursor, &src->port, sizeof(src->port));
	cursor += sizeof(src->port);
	return cursor;
}

//need to move it in more suitable file
/// @brief
/// @details
static void *unserialize_nodeid(struct node_id *dest, void *src)
{
	char *cursor = (char *)src;
	memcpy(dest->host, cursor, sizeof(dest->host));
	cursor += sizeof(dest->host);
	memcpy(&dest->port, cursor, sizeof(dest->port));
	cursor += sizeof(dest->port);
	return cursor;
}

/// @brief Structure representing a message for the slsm algorithm
/// @details Contains the type of the message, the identifier of the sender, the page, the mode and the initiator.
struct slsm_message {
	size_t message_type;
	struct node_id sender;
	size_t page;
	enum lock_type mode;
	struct node_id initiator;
};
#define SLSM_SIZE \
	MESSAGE_SIZE + sizeof(size_t) + sizeof(enum lock_type) + NODEID_SIZE

/// @brief
/// @details
/// @param msgt
/// @param sender
/// @param m
/// @param pid
/// @param cond
static void send_slsm_message(const enum message_type msgt,
			      const struct node_id *sender,
			      const enum lock_type m, size_t pid,
			      struct cond_var *cond)
{
	struct slsm_message request = {
		.message_type = msgt,
		.page = pid,
		.mode = m,
		.initiator = me,
	};

	if (cond == NULL)
		send_message(sender, (struct message *)&request,
			     sizeof(struct slsm_message));
	else
		send_wait_message_nolock(sender, (struct message *)&request,
					 sizeof(struct slsm_message), cond);
}

/// @brief
/// @details
struct delegate_message {
	size_t message_type;
	struct node_id sender;
	struct node_id delegate;
};
#define DELEGATE_MESSAGE_SIZE MESSAGE_SIZE + NODEID_SIZE

void serialize_delegate_message(void *dest, const struct delegate_message *src)
{
	*(struct message *)dest = *(struct message *)src;
	serialize_nodeid((char *)dest + sizeof(struct message), &src->delegate);
}

struct delegate_message unserialize_delegate_message(const void *src)
{
	struct message *src_mesg = (struct message *)src;
	struct delegate_message msg = {
		.message_type = src_mesg->message_type,
		.sender = src_mesg->sender,
	};
	unserialize_nodeid(&msg.delegate, src_mesg + 1);
	return msg;
}

/// @brief
/// @details
struct request {
	enum lock_type mode;
	struct node_id who;
	struct list_head next;
};
#define REQUEST_SIZE sizeof(enum lock_type) + NODEID_SIZE

//copy dest to src return the next adress
/// @brief
/// @details
static void *serialize_request(void *dest, const struct request *src)
{
	char *cursor = (char *)dest;
	memcpy(cursor, &src->mode, sizeof(src->mode));
	cursor += sizeof(src->mode);
	return serialize_nodeid(cursor, &src->who);
}

//copy dest to src return the next adress
/// @brief
/// @details
static void *unserialize_request(struct request *dest, void *src)
{
	char *cursor = (char *)src;
	memcpy(&dest->mode, cursor, sizeof(dest->mode));
	cursor += sizeof(dest->mode);
	return unserialize_nodeid(&dest->who, cursor);
}