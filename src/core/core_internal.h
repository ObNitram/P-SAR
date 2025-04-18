//this header is not supposed to be include elsewhere this purpose is for helper structure and the associated function
//This function doesnt depends on ANY global state and it for that it can be placed in a header...
#pragma once

#include "core.h"
#include "utils/list.h"
#include <stddef.h>
#include "network/message.h"
#include "utils/utils.h"
#include <string.h>

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
		.initiator = me,
		.mode = m,
		.page = pid,
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
struct departure_message {
	size_t message_type;
	struct node_id sender;
	struct node_id delegate;
};
#define DEPARTURE_MESSAGE_SIZE MESSAGE_SIZE + NODEID_SIZE

/// @brief
/// @details
struct request {
	enum lock_type mode;
	struct node_id who;
	struct list_head next;
};
#define REQUEST_SIZE sizeof(enum lock_type) + NODEID_SIZE

//need to move it in more suitable file
/// @brief
/// @details
static void *serialize_nodeid(void *dest, const struct node_id *src)
{
	memcpy(dest, src->host, sizeof(src->host));
	dest += sizeof(src->host);
	memcpy(dest, &src->port, sizeof(src->port));
	dest += sizeof(src->port);
	return dest;
}

//need to move it in more suitable file
/// @brief
/// @details
static void *unserialize_nodeid(struct node_id *dest, void *src)
{
	memcpy(dest->host, src, sizeof(dest->host));
	src += sizeof(dest->host);
	memcpy(&dest->port, src, sizeof(dest->port));
	src += sizeof(dest->port);
	return src;
}

//copy dest to src return the next adress
/// @brief
/// @details
static void *serialize_request(void *dest, const struct request *src)
{
	memcpy(dest, &src->mode, sizeof(src->mode));
	dest += sizeof(src->mode);
	return serialize_nodeid(dest, &src->who);
}

//copy dest to src return the next adress
/// @brief
/// @details
static void *unserialize_request(struct request *dest, void *src)
{
	memcpy(&dest->mode, src, sizeof(dest->mode));
	src += sizeof(dest->mode);
	return unserialize_nodeid(&dest->who, src);
}